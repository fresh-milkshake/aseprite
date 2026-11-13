-- Copyright (c) 2026 fresh-milkshake. MIT license; see LICENSE.txt.
-- Run on Windows with the complete CLI/Lua regression suite.
local extension = app.fs.joinPath(app.fs.filePath(app.fs.appPath),
  "data", "extensions", "minecraft-tools")
-- appPath is an executable filename; packaged and build trees share bin/data.
local core = dofile(app.fs.joinPath(extension,"minecraft_tools_core.lua"))
local exporter = dofile(app.fs.joinPath(extension,"minecraft_tools_export.lua"))
local function fails(fn)
  assert(not pcall(fn), "Expected input rejection")
end

local s = Sprite(16,16,ColorMode.RGB)
local red = app.pixelColor.rgba(255,0,0,255)
local green = app.pixelColor.rgba(0,255,0,255)
s.cels[1].image:putPixel(0,0,red)
s.cels[1].position = Point(3,4)
local l = s:newLayer()
local overlay = Image(2,2,ColorMode.RGB)
overlay:putPixel(0,0,green)
s:newCel(l,1,overlay,Point(8,9))
local combined = core.capture(s,1)
assert(combined:getPixel(3,4) == red and combined:getPixel(8,9) == green)
assert(combined:getPixel(0,0) == 0)
local cropped = core.capture(s,1,Rectangle(3,4,6,6))
assert(cropped:getPixel(0,0) == red and cropped:getPixel(5,5) == green)
app.command.NewLayer { name="Guide",reference=true }
s:newCel(app.layer,1,overlay,Point(0,0))
assert(core.capture(s,1):getPixel(0,0) == 0, "Reference guides must not export")

local shifted = core.offset(combined,8,8)
assert(shifted:getPixel(11,12) == red)
assert(core.offset(shifted,8,8):isEqual(combined))
local flat = Image(4,4,ColorMode.RGB)
flat:clear(app.pixelColor.rgba(128,128,128,255))
local normal = core.normalMap(flat,4,true)
assert(normal:getPixel(2,2) == app.pixelColor.rgba(128,128,255,255))
assert(core.seams(flat).edgeDifferences == 0)
local ramp = core.ramp(Color{r=0,g=0,b=0},Color{r=255,g=255,b=255},3)
assert(ramp[1].red == 0 and ramp[2].red == 128 and ramp[3].red == 255)

assert(core.assetId("my_mod","tools/copper_sword") == "my_mod:tools/copper_sword")
for _, path in ipairs({"../escape","/absolute","a//b","a/./b","UPPER","a.png","a/"}) do
  fails(function() core.assetId("mod",path) end)
end
fails(function() core.assetId("Bad Namespace","a") end)
for _, namespace in ipairs({".","..","mod.","nul","com1"}) do
  fails(function() core.assetId(namespace,"a") end)
end
for _, path in ipairs({"dir./file","con","aux.texture","nested/lpt9"}) do
  fails(function() core.assetId("mod",path) end)
end

s:newEmptyFrame()
s.frames[1].duration, s.frames[2].duration = 0.1,0.15
local strip, metadata = core.animation(s)
assert(strip.width == 16 and strip.height == 32)
assert(strip:getPixel(3,4) == red)
assert(metadata.animation.frames[1].index == 0 and metadata.animation.frames[2].time == 3)
s.frames[2].duration = 0.075
fails(function() core.animation(s) end)
s.frames[2].duration = 0.15

local temp = app.fs.joinPath(app.fs.tempPath,"aseprite-minecraft-" .. tostring(os.time()))
local options = { root=temp, namespace="test_mod", path="tools/sword",kind="Weapon / tool",modern=true,models=true,animate=true }
local files = exporter.plan(core,s,options)
assert(#files == 4)
assert(json.decode(files[3].text).parent == "minecraft:item/handheld")
assert(json.decode(files[4].text).model.model == "test_mod:item/tools/sword")
assert(exporter.write(files,false) == 4)
assert(Image{fromFile=files[1].path}:isEqual(strip))
assert(#exporter.collisions(files) == 4)
fails(function() exporter.write(files,false) end)
assert(exporter.write(files,true) == 4)
options.animate = false
fails(function() exporter.plan(core,s,options) end) -- stale animation sidecar
options.path, options.kind, options.animate = "copper_block","Block",false
local block = exporter.plan(core,s,options)
assert(#block == 5)
assert(json.decode(block[2].text).textures.all == "test_mod:block/copper_block")
assert(json.decode(block[3].text).variants[""].model == "test_mod:block/copper_block")
-- A failed staging operation preserves all existing outputs.
local saved = assert(io.open(files[3].path,"rb"))
local original = saved:read("*a"); saved:close()
local invalid = { {path=files[3].path,text="must not replace original"},
  {path=app.fs.joinPath(files[1].path,"impossible.json"),text="invalid parent"} }
fails(function() exporter.write(invalid,true) end)
local input = assert(io.open(files[3].path,"rb"))
assert(input:read("*a") == original); input:close()
for _, file in ipairs(files) do os.remove(file.path) end
s:close()
print("Minecraft image capture, guides, timing, paths and transactional export passed")

-- Verify startup registration too: image-helper tests alone cannot catch a
-- malformed extension manifest that silently leaves the entire menu missing.
local edit = Sprite(4,4,ColorMode.RGB)
edit.cels[1].image:putPixel(0,0,red)
local before = Image(edit.cels[1].image)
app.command.MinecraftOffset()
assert(edit.cels[1].image:getPixel(2,2) == red, "Built-in Minecraft commands must be registered")
app.undo()
assert(edit.cels[1].image:isEqual(before), "Minecraft editing must undo in one step")
edit:close()
