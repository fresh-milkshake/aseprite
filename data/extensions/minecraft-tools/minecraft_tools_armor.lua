-- Copyright (c) 2026 fresh-milkshake. MIT license; see LICENSE.txt.
local i18n=package.loaded["minecraft-tools/minecraft_tools_i18n"] or require "minecraft_tools_i18n"
local Dialog,alert,assert,error,tr=i18n.dialog,i18n.alert,i18n.assert,i18n.error,i18n.tr
local M = {}

function M.atlas(core, sprite, baby)
  assert(sprite, "Choose an open armor atlas or named sheet region")
  local bounds
  if type(sprite)=="table" then bounds=sprite.bounds; sprite=sprite.sprite end
  bounds=bounds or Rectangle(0,0,sprite.width,sprite.height)
  local width,height=bounds.width,bounds.height
  assert(bounds.x>=0 and bounds.y>=0 and bounds.x+width<=sprite.width
    and bounds.y+height<=sprite.height,"Armor region is outside the sheet")
  assert(width >= 64 and width <= 512 and width % 64 == 0,
    "Armor atlas width must be 64, 128, 256 or 512")
  assert(baby and height == width or not baby and height * 2 == width,
    baby and "Baby armor uses a 64 x 64 atlas (or a scaled multiple)" or
      "Adult armor uses a 64 x 32 atlas (or a scaled multiple)")
  local scale = width / 64
  assert(scale == 1 or scale == 2 or scale == 4 or scale == 8, "Armor resolution must be a power-of-two scale")
  assert(#sprite.frames == 1, "Equipped armor export requires a single static frame")
  return core.capture(sprite, 1,bounds)
end

function M.plan(core, upper, leggings, baby, opts)
  local id = core.assetId(opts.namespace, opts.path)
  assert(opts.root and opts.root ~= "", "Choose a resource root")
  assert(opts.profile == "Legacy (up to 1.21.1)" or opts.profile == "Equipment (1.21.2-1.21.3)"
    or opts.profile == "Equipment (1.21.4-1.21.11)"
    or opts.profile == "Equipment (26.1+)", "Choose a supported Java equipment profile")
  local a,b = M.atlas(core,upper), M.atlas(core,leggings)
  assert(a.width == b.width, "Upper armor and leggings must use the same resolution")
  local assets = app.fs.joinPath(opts.root,"assets",opts.namespace)
  if opts.profile == "Legacy (up to 1.21.1)" then
    return {
      { path=app.fs.joinPath(assets,"textures/models/armor",opts.path .. "_layer_1.png"), image=a },
      { path=app.fs.joinPath(assets,"textures/models/armor",opts.path .. "_layer_2.png"), image=b } }
  end
  local files = {
    { path=app.fs.joinPath(assets,"textures/entity/equipment/humanoid",opts.path .. ".png"), image=a },
    { path=app.fs.joinPath(assets,"textures/entity/equipment/humanoid_leggings",opts.path .. ".png"), image=b } }
  local layers = { humanoid={{texture=id}}, humanoid_leggings={{texture=id}} }
  if opts.profile == "Equipment (26.1+)" then
    local c = M.atlas(core,baby,true)
    assert(c.width == a.width, "Baby armor must use the same atlas resolution")
    files[#files+1] = { path=app.fs.joinPath(assets,"textures/entity/equipment/humanoid_baby",opts.path .. ".png"),image=c }
    layers.humanoid_baby = {{texture=id}}
  end
  files[#files+1] = { path=app.fs.joinPath(assets,opts.profile == "Equipment (1.21.2-1.21.3)" and "models/equipment" or "equipment",opts.path .. ".json"),text=json.encode({layers=layers}) .. "\n" }
  return files
end

function M.paperdoll(upper, leggings, showUpper, showLeggings)
  local scale = upper.width / 64
  assert(leggings.width == upper.width and upper.height == upper.width / 2
    and leggings.height == upper.height, "Preview requires matching 64 x 32 armor atlases (or scaled multiples)")
  local out = Image(48 * scale,40 * scale,ColorMode.RGB)
  local skin = app.pixelColor.rgba(135,140,150,255)
  local function rectangle(x,y,w,h)
    for py=y*scale,(y+h)*scale-1 do for px=x*scale,(x+w)*scale-1 do out:putPixel(px,py,skin) end end
  end
  -- Draw a neutral mannequin, then the UV samples for front and back.
  for view=0,1 do
    local ox = view*24
    rectangle(ox+8,4,8,8); rectangle(ox+8,12,8,12)
    rectangle(ox+4,12,4,12); rectangle(ox+16,12,4,12)
    rectangle(ox+8,24,4,12); rectangle(ox+12,24,4,12)
  end
  local function patch(source,u,v,w,h,dx,dy,flip)
    local image = Image(w*scale,h*scale,ColorMode.RGB)
    for p in image:pixels() do
      local sx = flip and (w*scale-1-p.x) or p.x
      p(source:getPixel(u*scale+sx,v*scale+p.y))
    end
    out:drawImage(image,Point(dx*scale,dy*scale))
  end
  for view=0,1 do
    local ox,back = view*24,view==1
      local layers = {}
      if showLeggings ~= false then layers[#layers+1] = leggings end
      if showUpper ~= false then layers[#layers+1] = upper end
      for _,source in ipairs(layers) do
      patch(source,back and 32 or 20,20,8,12,ox+8,12)
      patch(source,back and 12 or 4,20,4,12,ox+8,24)
      patch(source,back and 12 or 4,20,4,12,ox+12,24,true)
    end
      if showUpper ~= false then
        patch(upper,back and 24 or 8,8,8,8,ox+8,4)
        patch(upper,back and 52 or 44,20,4,12,ox+4,12)
        patch(upper,back and 52 or 44,20,4,12,ox+16,12,true)
      end
  end
  return out
end

return M
