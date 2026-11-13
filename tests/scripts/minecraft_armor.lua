-- Copyright (c) 2026 fresh-milkshake. MIT license; see LICENSE.txt.
local folder = app.fs.joinPath(app.fs.filePath(app.fs.appPath),"data/extensions/minecraft-tools")
local core = dofile(app.fs.joinPath(folder,"minecraft_tools_core.lua"))
local armor = dofile(app.fs.joinPath(folder,"minecraft_tools_armor.lua"))
local upper,leggings,baby = Sprite(64,32),Sprite(64,32),Sprite(64,64)
local red,green = Color{r=230,g=50,b=45}.rgbaPixel,Color{r=50,g=220,b=90}.rgbaPixel
upper.cels[1].image:putPixel(8,8,red)
leggings.cels[1].image:putPixel(4,20,green)
local doll = armor.paperdoll(core.capture(upper,1),core.capture(leggings,1))
assert(doll.width == 48 and doll.height == 40)
assert(doll:getPixel(8,4) == red, "Helmet front must sample UV (8,8)")
assert(doll:getPixel(8,24) == green, "Leggings must sample UV (4,20)")
local opts = {root=app.fs.tempPath,namespace="mymod",path="armor/copper",profile="Legacy (up to 1.21.1)"}
local old = armor.plan(core,upper,leggings,nil,opts)
assert(#old == 2 and old[1].path:find("copper_layer_1.png",1,true))
opts.profile = "Equipment (1.21.4-1.21.11)"
local modern = armor.plan(core,upper,leggings,nil,opts)
assert(#modern == 3)
local data = json.decode(modern[3].text)
assert(data.layers.humanoid[1].texture == "mymod:armor/copper")
assert(data.layers.humanoid_leggings[1].texture == "mymod:armor/copper")
opts.profile = "Equipment (1.21.2-1.21.3)"
local transitional = armor.plan(core,upper,leggings,nil,opts)
assert(transitional[3].path:gsub("\\","/"):find("/models/equipment/armor/copper.json",1,true))
opts.profile = "Equipment (26.1+)"
assert(not pcall(armor.plan,core,upper,leggings,nil,opts), "26.1+ requires a separate baby texture")
local newest = armor.plan(core,upper,leggings,baby,opts)
assert(#newest == 4 and json.decode(newest[4].text).layers.humanoid_baby)
local invalid = Sprite(64,64)
assert(not pcall(armor.atlas,core,invalid))
upper:newEmptyFrame()
assert(not pcall(armor.atlas,core,upper))
upper:close(); leggings:close(); baby:close(); invalid:close()
