-- Copyright (c) 2026 fresh-milkshake. MIT license; see LICENSE.txt.
local i18n=package.loaded["minecraft-tools/minecraft_tools_i18n"] or require "minecraft_tools_i18n"
local Dialog,alert,assert,error,tr=i18n.dialog,i18n.alert,i18n.assert,i18n.error,i18n.tr
-- Image operations shared by commands and Windows integration tests.
local M = { cellSizes = { "16", "32", "64", "128" } }

function M.isCellSize(size)
  for _, option in ipairs(M.cellSizes) do
    if size == tonumber(option) then return true end
  end
  return false
end

function M.bounds(sprite, selection)
  local b = selection and not sprite.selection.isEmpty and sprite.selection.bounds
    or Rectangle(0, 0, sprite.width, sprite.height)
  local x, y = math.max(0, b.x), math.max(0, b.y)
  local right, bottom = math.min(sprite.width, b.x + b.width), math.min(sprite.height, b.y + b.height)
  assert(right > x and bottom > y, "The selection does not intersect the canvas")
  return Rectangle(x, y, right - x, bottom - y)
end

-- All texture commands share one explicit region. Large sheets never silently
-- become a single texture: a selection or native named slice identifies the cell.
function M.cell(sprite)
  if not sprite.selection.isEmpty then
    local b = M.bounds(sprite,true)
    assert(b == sprite.selection.bounds, "Keep the cell selection inside the canvas")
    return b
  end
  if sprite.width <= 32 and sprite.height <= 32 then
    return Rectangle(0,0,sprite.width,sprite.height)
  end
  local grid = sprite.gridBounds
  if M.isCellSize(grid.width) and grid.width == grid.height and
     sprite.width == grid.width and sprite.height == grid.height then
    return Rectangle(0, 0, sprite.width, sprite.height)
  end
  error("Select a cell or use Minecraft > Cell Navigator")
end

function M.region(sprite, x, y, size)
  assert(M.isCellSize(size), "Choose 16, 32, 64 or 128 pixel cells")
  assert(x >= 0 and y >= 0 and x % size == 0 and y % size == 0,
    "Cell coordinates must align to the chosen grid")
  assert(x+size <= sprite.width and y+size <= sprite.height,"Cell lies outside the sheet")
  return Rectangle(x,y,size,size)
end

function M.capture(sprite, frame, bounds)
  bounds = bounds or Rectangle(0, 0, sprite.width, sprite.height)
  -- Rendering in RGB handles indexed palettes, cel offsets, groups and opacity.
  -- Reference layers are excluded by Aseprite's normal export renderer.
  assert(bounds.width * bounds.height <= 16777216,"Texture operation exceeds 16 million pixels")
  local image = Image(bounds.width, bounds.height, ColorMode.RGB)
  image:drawSprite(sprite, frame or app.frame, Point(-bounds.x, -bounds.y))
  return image
end

function M.square(image)
  assert(image.width == image.height, "Use a square texture or square selection")
  assert(image.width <= 512, "Preview textures must be at most 512 x 512")
  return image
end

function M.offset(image, dx, dy)
  local out = Image(image.spec)
  for pixel in out:pixels() do
    pixel(image:getPixel((pixel.x - dx) % image.width, (pixel.y - dy) % image.height))
  end
  return out
end

function M.seams(image)
  local edges, opaque, transparent, partial = 0, 0, 0, 0
  local palette, colors = {},0
  local function different(a,b)
    if app.pixelColor.rgbaA(a)==0 and app.pixelColor.rgbaA(b)==0 then return false end
    return a~=b
  end
  for y = 0, image.height - 1 do
    if different(image:getPixel(0,y),image:getPixel(image.width-1,y)) then edges=edges+1 end
  end
  for x = 0, image.width - 1 do
    if different(image:getPixel(x,0),image:getPixel(x,image.height-1)) then edges=edges+1 end
  end
  for p in image:pixels() do
    local alpha=app.pixelColor.rgbaA(p())
    if alpha==255 then opaque=opaque+1 else transparent=transparent+1 end
    if alpha>0 and alpha<255 then partial=partial+1 end
    if alpha>0 and colors<=4096 and not palette[p()] then palette[p()]=true;colors=colors+1 end
  end
  return {edgeDifferences=edges,opaque=opaque,transparent=transparent,partial=partial,colors=colors}
end

function M.assetName(sprite,bounds)
  for _,slice in ipairs(sprite.slices) do
    if slice.bounds==bounds then return slice.name end
  end
  if sprite.width==bounds.width and sprite.height==bounds.height then
    local name=app.fs.fileTitle(sprite.filename):lower():gsub("[^a-z0-9_./-]","_")
    return name~="" and name or "texture"
  end
  local size=bounds.width
  return string.format("r%03d_c%03d",bounds.y//size+1,bounds.x//size+1)
end

function M.assetId(namespace, path)
  assert(namespace and namespace:match("^[a-z0-9_.-]+$"),
    "Namespace must use lowercase a-z, 0-9, _, . or -")
  assert(path and path:match("^[a-z0-9_./-]+$") and path:sub(1, 1) ~= "/"
    and path:sub(-1) ~= "/" and not path:find("//", 1, true),
    "Asset path must use lowercase names separated by /")
  local function portablePart(part)
    assert(part:sub(-1) ~= ".", "Windows resource names cannot end in a dot")
    local stem = part:match("^[^.]+") or part
    assert(stem ~= "con" and stem ~= "prn" and stem ~= "aux" and stem ~= "nul"
      and not stem:match("^com[1-9]$") and not stem:match("^lpt[1-9]$"),
      "Reserved Windows resource name: " .. part)
  end
  portablePart(namespace)
  for part in path:gmatch("[^/]+") do
    portablePart(part)
    assert(part ~= "." and part ~= "..", "Asset path cannot contain . or .. segments")
  end
  assert(not path:match("%.png$"), "Enter the asset path without .png")
  return namespace .. ":" .. path
end

function M.animation(sprite, bounds)
  bounds = bounds or Rectangle(0, 0, sprite.width, sprite.height)
  assert(bounds.width == bounds.height, "Animated Java textures require square frames here")
  assert(bounds.width * bounds.height * #sprite.frames <= 16777216,
    "Animation export exceeds 16 million pixels; export fewer or smaller frames")
  local strip = Image(bounds.width, bounds.height * #sprite.frames, ColorMode.RGB)
  local frames = {}
  for i, frame in ipairs(sprite.frames) do
    local ticks = frame.duration * 20
    local rounded = math.floor(ticks + 0.5)
    assert(rounded >= 1 and math.abs(ticks - rounded) < 0.001,
      "Frame " .. i .. ": duration must be a positive multiple of 50 ms (one Java tick)")
    strip:drawImage(M.capture(sprite, frame, bounds), Point(0, (i - 1) * bounds.height))
    frames[i] = { index = i - 1, time = rounded }
  end
  return strip, { animation = { frametime = 1, interpolate = false, frames = frames } }
end

function M.ramp(first, last, count, hsv)
  local out = {}
  for i = 0, count - 1 do
    local t = count == 1 and 0 or i / (count - 1)
    if hsv then
      local h1,h2=first.hsvHue,last.hsvHue
      if first.hsvSaturation<0.001 then h1=h2 end
      if last.hsvSaturation<0.001 then h2=h1 end
      local delta=(h2-h1+180)%360-180
      out[#out+1]=Color{h=(h1+delta*t)%360,
        s=first.hsvSaturation+(last.hsvSaturation-first.hsvSaturation)*t,
        v=first.hsvValue+(last.hsvValue-first.hsvValue)*t,
        a=math.floor(first.alpha+(last.alpha-first.alpha)*t+0.5)}
    else
      out[#out + 1] = Color {
        r = math.floor(first.red + (last.red - first.red) * t + 0.5),
        g = math.floor(first.green + (last.green - first.green) * t + 0.5),
        b = math.floor(first.blue + (last.blue - first.blue) * t + 0.5),
        a = math.floor(first.alpha + (last.alpha - first.alpha) * t + 0.5) }
    end
  end
  return out
end

function M.normalMap(image, strength, wrap, flipGreen)
  local function height(x, y)
    if wrap then x, y = x % image.width, y % image.height
    else x, y = math.max(0, math.min(image.width - 1, x)), math.max(0, math.min(image.height - 1, y)) end
    local p = image:getPixel(x, y)
    return (app.pixelColor.rgbaR(p) * 0.2126 + app.pixelColor.rgbaG(p) * 0.7152
      + app.pixelColor.rgbaB(p) * 0.0722) / 255
  end
  local out = Image(image.spec)
  for p in out:pixels() do
    local x = (height(p.x - 1, p.y) - height(p.x + 1, p.y)) * strength
    local y = (height(p.x, p.y - 1) - height(p.x, p.y + 1)) * strength
    local length = math.sqrt(x * x + y * y + 1)
    p(app.pixelColor.rgba(math.floor((x / length + 1) * 127.5 + 0.5),
      math.floor(((flipGreen and -y or y) / length + 1) * 127.5 + 0.5), math.floor((1 / length + 1) * 127.5 + 0.5),
      app.pixelColor.rgbaA(image:getPixel(p.x, p.y))))
  end
  return out
end

-- Standard Java cuboid UV unwrap: top/bottom above west/north/east/south.
function M.cuboidUV(u, v, w, h, d)
  return {
    { "Top", u + d, v, w, d }, { "Bottom", u + d + w, v, w, d },
    { "West", u, v + d, d, h }, { "North", u + d, v + d, w, h },
    { "East", u + d + w, v + d, d, h }, { "South", u + 2 * d + w, v + d, w, h } }
end

return M
