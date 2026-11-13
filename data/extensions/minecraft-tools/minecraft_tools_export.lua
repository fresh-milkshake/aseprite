-- Copyright (c) 2026 fresh-milkshake. MIT license; see LICENSE.txt.
local i18n=package.loaded["minecraft-tools/minecraft_tools_i18n"] or require "minecraft_tools_i18n"
local Dialog,alert,assert,error,tr=i18n.dialog,i18n.alert,i18n.assert,i18n.error,i18n.tr
local M = {}

local function metadataAt(path)
  if not app.fs.isFile(path) then return nil end
  local stream=assert(io.open(path,"rb"),"Cannot read texture metadata: " .. path)
  local text=stream:read(1048577) or "";stream:close()
  assert(#text<=1048576,"Texture metadata exceeds 1 MB")
  text=text:gsub("^\239\187\191","")
  assert(text:match("^%s*{"),"Texture metadata must be a JSON object: " .. path)
  local ok,value=pcall(json.decode,text)
  -- Aseprite returns a native JSON userdata, not a Lua table. Keep it intact
  -- so unrelated sections (including nulls/arrays) retain their exact structure.
  assert(ok and value~=nil,"Invalid texture metadata: " .. path)
  return value
end

function M.plan(core, sprite, opts)
  local id = core.assetId(opts.namespace, opts.path)
  assert(opts.root and opts.root ~= "", "Choose a resource root (the folder containing assets)")
  local assets = app.fs.joinPath(opts.root, "assets", opts.namespace)
  local kind = opts.kind == "Block" and "block" or "item"
  local texture = app.fs.joinPath(assets, "textures", kind, opts.path .. ".png")
  local bounds = opts.bounds or core.cell(sprite)
  assert(bounds.width == bounds.height, "Select a square cell or effect region")
  local image, metadata
  if opts.animate then
    image, metadata = core.animation(sprite,bounds)
    metadata.animation.interpolate=not not opts.interpolate
  else image = core.capture(sprite, opts.frame or app.frame,bounds) end
  local previous=metadataAt(texture .. ".mcmeta")
  if metadata and previous then
    previous.animation=metadata.animation
    metadata=previous
  end
  local files = { { path = texture, image = image } }
  local function jsonFile(path, value)
    files[#files + 1] = { path = app.fs.joinPath(assets, path), text = json.encode(value) .. "\n" }
  end
  if metadata then files[#files + 1] = { path = texture .. ".mcmeta", text = json.encode(metadata) .. "\n" }
  else
    -- A stale animation sidecar changes how Minecraft reads a static texture.
    assert(not previous or previous.animation==nil,
      "An animation .mcmeta already exists. Remove it or use a different asset path for static export.")
  end
  if opts.models then
    if kind == "block" then
      jsonFile("models/block/" .. opts.path .. ".json", {
        parent = "minecraft:block/cube_all", textures = { all = id:gsub(":", ":block/", 1) } })
      jsonFile("blockstates/" .. opts.path .. ".json", {
        variants = { [""] = { model = opts.namespace .. ":block/" .. opts.path } } })
      jsonFile("models/item/" .. opts.path .. ".json", { parent = opts.namespace .. ":block/" .. opts.path })
    else
      jsonFile("models/item/" .. opts.path .. ".json", {
        parent = opts.kind == "Weapon / tool" and "minecraft:item/handheld" or "minecraft:item/generated",
        textures = { layer0 = opts.namespace .. ":item/" .. opts.path } })
    end
    if opts.modern then
      jsonFile("items/" .. opts.path .. ".json", {
        model = { type = "minecraft:model", model = opts.namespace .. ":item/" .. opts.path } })
    end
  end
  return files
end

function M.collisions(files)
  local out,seen = {},{}
  for _, file in ipairs(files) do
    local key=file.path:lower():gsub("\\","/")
    assert(not seen[key],"Duplicate output path: " .. file.path)
    seen[key]=true
    if app.fs.isFile(file.path) then out[#out + 1] = file.path end
    assert(not app.fs.isDirectory(file.path), "A directory occupies an output filename: " .. file.path)
  end
  return out
end

-- Stage the complete set before replacing anything. Keep backups until every
-- rename succeeds so a write failure cannot leave half of a resource set.
function M.write(files, overwrite)
  local existing = M.collisions(files)
  assert(overwrite or #existing == 0, "Output files already exist")
  local token = ".mc-tools-" .. tostring(os.time()) .. "-" .. tostring(math.random(100000, 999999))
  local staged, backups, installed = {}, {}, {}
  local ok, errorMessage = pcall(function()
    for i, file in ipairs(files) do
      assert(app.fs.makeAllDirectories(app.fs.filePath(file.path)), "Cannot create output directory")
      local stage = file.path .. token .. (file.image and ".png" or ".tmp")
      staged[i] = stage
      if file.image then assert(file.image:saveAs(stage), "Cannot save " .. file.path)
      else
        local stream = assert(io.open(stage, "wb"))
        local wrote, reason = stream:write(file.text)
        local closed, closeReason = stream:close()
        assert(wrote and closed, reason or closeReason)
      end
    end
    for i, file in ipairs(files) do
      if app.fs.isFile(file.path) then
        local backup = file.path .. token .. ".bak"
        assert(os.rename(file.path, backup))
        backups[i] = backup
      end
      assert(os.rename(staged[i], file.path))
      staged[i], installed[i] = nil, true
    end
  end)
  if not ok then
    for i = #files, 1, -1 do
      if installed[i] then os.remove(files[i].path) end
      if backups[i] then os.rename(backups[i], files[i].path) end
      if staged[i] then os.remove(staged[i]) end
    end
    error(errorMessage)
  end
  for _, backup in pairs(backups) do os.remove(backup) end
  return #files
end

return M
