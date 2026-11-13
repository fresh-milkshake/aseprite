-- This file is released under the terms of the MIT license.
-- Read LICENSE.txt for more information.

-- Run with an isolated ASEPRITE_USER_FOLDER (also used for the ZIP smoke test).
assert(app.preferences.theme.selected == (app.params["expected-theme"] or "default-dark"))
assert(app.preferences.undo.allow_nonlinear_history == false)
assert(app.preferences.general.recent_items == 16)

-- Users can still explicitly select the light theme.
app.preferences.theme.selected = "default"
assert(app.preferences.theme.selected == "default")
app.preferences.theme.selected = "default-dark"

if app.params["expected-version"] then
  assert(tostring(app.version) == app.params["expected-version"])
  local extension=app.fs.joinPath(app.fs.filePath(app.fs.appPath),"data","extensions","minecraft-tools")
  assert(not app.fs.isFile(app.fs.joinPath(extension,"__pref.lua")),
    "The portable package must start without test/user extension settings")
end

-- Also validate bundled extension startup in the extracted release package.
-- A missing sheet/cube module prevents command registration and fails this test.
local atlas=Sprite(64,32)
atlas.selection:select(Rectangle(16,0,16,16))
local marker=Color{r=230,g=70,b=40}.rgbaPixel
atlas.cels[1].image:putPixel(16,0,marker)
app.command.MinecraftOffset()
assert(atlas.cels[1].image:getPixel(24,8)==marker,"Bundled Minecraft sheet commands must load")
assert(atlas.cels[1].image:getPixel(0,0)==0)
app.undo()
assert(atlas.cels[1].image:getPixel(16,0)==marker)
atlas:close()
