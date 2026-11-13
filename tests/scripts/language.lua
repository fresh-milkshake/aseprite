-- Copyright (c) 2026 fresh-milkshake. MIT license; see LICENSE.txt.
-- Also run against the extracted Windows package, to check bundled resources.
local previous=app.language
local i18n=package.loaded["minecraft-tools/minecraft_tools_i18n"]
assert(i18n,"The bundled Minecraft extension must load")
app.language="ru"
assert(app.translate("_.display_name")=="Русский")
assert(app.translate("general.cancel"):find("Отмена"))
assert(i18n.tr("New Texture / Sheet...")=="Новая текстура / лист...")
assert(i18n.tr("Selected variant: 3 / 6")=="Выбран вариант: 3 / 6")
assert(i18n.tr("Cannot save custom/path.png"):find("custom/path.png",1,true))
assert(i18n.tr("user_sprite.aseprite")=="user_sprite.aseprite")
local valid,reason=pcall(function() i18n.assert(false,"Open or create a sprite first") end)
assert(not valid and reason:find("Сначала"),reason)
local sprite=Sprite(16,16)
local red=Color{r=220,g=30,b=20}.rgbaPixel
sprite.cels[1].image:putPixel(0,0,red)
app.command.MinecraftOffset()
assert(sprite.cels[1].image:getPixel(8,8)==red)
app.undo()
assert(sprite.cels[1].image:getPixel(0,0)==red)
sprite:close()
app.language="en"
assert(app.translate("_.display_name")=="English")
assert(i18n.tr("Selected variant: 3 / 6")=="Selected variant: 3 / 6")
assert(not pcall(function() app.language="missing_language" end))
app.language=previous
