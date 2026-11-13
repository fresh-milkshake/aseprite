-- Copyright (c) 2026 fresh-milkshake. MIT license; see LICENSE.txt.
local i18n=package.loaded["minecraft-tools/minecraft_tools_i18n"] or require "minecraft_tools_i18n"
local Dialog,alert,assert,error,tr=i18n.dialog,i18n.alert,i18n.assert,i18n.error,i18n.tr
local core = require "minecraft_tools_core"
local exporter = require "minecraft_tools_export"
local quickExport = require "minecraft_tools_quick_export"
local armorUI = require "minecraft_tools_armor_ui"
local sheetUI = require "minecraft_tools_sheet_ui"
local cubeUI = require "minecraft_tools_cube_ui"
local resultPreview = require "minecraft_tools_preview"
local sheet = require "minecraft_tools_sheet"
local dialogs = {}
local prefs

local function guarded(fn)
  return function()
    local ok, message = pcall(fn)
    if not ok then alert { title = "Minecraft Java Tools", text = tostring(message):gsub("^.-:%d+: ", "") } end
  end
end

local function sprite()
  return assert(app.sprite, "Open or create a sprite first")
end

local function templateGuide(kind,size)
  local armor=kind:match("^Armor")
  local templateWidth,templateHeight=armor and size*4 or size,armor and size*2 or size
  local guide = Image(templateWidth,templateHeight,ColorMode.RGB)
  local colors = {
    Color { r=95, g=185, b=225, a=95 }, Color { r=225, g=170, b=95, a=95 },
    Color { r=185, g=95, b=225, a=95 }, Color { r=95, g=225, b=150, a=95 },
    Color { r=225, g=95, b=125, a=95 }, Color { r=215, g=225, b=95, a=95 } }
  local function box(u, v, w, h, depth)
    local scale = size / 16
    for i, face in ipairs(core.cuboidUV(u, v, w, h, depth)) do
      for y = face[3] * scale, (face[3] + face[5]) * scale - 1 do
        for x = face[2] * scale, (face[2] + face[4]) * scale - 1 do
          if x < guide.width and y < guide.height then guide:drawPixel(x, y, colors[i]) end
        end
      end
    end
  end
  if armor then
    if kind == "Armor layer 1" then box(0, 0, 8, 8, 8); box(40, 16, 4, 12, 4) end
    box(16, 16, 8, 12, 4); box(0, 16, 4, 12, 4)
  elseif kind == "Weapon / tool" then
    for p in guide:pixels() do
      if math.abs(p.x + p.y - (size - 1)) < math.max(1, size / 16) then
        p(app.pixelColor.rgba(180, 195, 215, 95))
      end
    end
  else
    for p in guide:pixels() do
      if p.x == math.floor(size / 2) or p.y == math.floor(size / 2) then
        p(app.pixelColor.rgba(150, 180, 210, 75))
      end
    end
  end
  return guide
end

local function newTexture()
  local saved=prefs.new_texture or {}
  local d = Dialog("New Java Texture")
  local visual=resultPreview.new(d,nil,function(opts)
    local size=tonumber(opts.size)
    local columns,rows=tonumber(opts.columns),tonumber(opts.rows)
    local isSheet=opts.kind=="Sprite sheet"
    d:modify{id="size",label=isSheet and "Cell size:" or "Resolution:"}
    d:modify{id="columns",enabled=isSheet};d:modify{id="rows",enabled=isSheet}
    d:modify{id="guide",enabled=not isSheet};d:modify{id="insert",enabled=not isSheet and app.sprite~=nil}
    if isSheet then
      assert(columns and rows and columns>=1 and rows>=1 and columns%1==0 and rows%1==0
        and columns*rows*size*size<=16777216,"Sheet size must fit 16 million pixels")
      local w,h=math.min(columns,32)*4,math.min(rows,32)*4
      local image=Image(w,h,ColorMode.RGB)
      for pixel in image:pixels() do
        if pixel.x%4==0 or pixel.y%4==0 then pixel(app.pixelColor.rgba(160,165,175,180)) end
      end
      return image,string.format("Grid preview: %d x %d pixels",columns*size,rows*size)
    end
    local image=templateGuide(opts.kind,size)
    if not opts.guide then image:clear() end
    if opts.insert then
      local source=sprite();local bounds=core.cell(source)
      assert(opts.guide,"Enable the reference guide")
      assert(source.colorMode==ColorMode.RGB,"Convert the sheet to RGB first")
      assert(bounds.width==image.width and bounds.height==image.height,"Select a matching template region")
    end
    return image,string.format("Template: %d x %d pixels",image.width,image.height)
  end)

  d:combobox { id = "kind", label = "Template:", option=saved.kind or "Sprite sheet", options = {
    "Block", "Item / ingredient", "Weapon / tool", "Armor layer 1", "Armor layer 2", "Sprite sheet" },onchange=visual.changed }
  d:combobox { id = "size", label = "Resolution:", options = core.cellSizes,option=saved.size or "16",onchange=visual.changed }
  d:number {id="columns",label="Sheet columns:",text=tostring(saved.columns or 16),decimals=0,onchange=visual.changed}
  d:number {id="rows",label="Sheet rows:",text=tostring(saved.rows or 16),decimals=0,onchange=visual.changed}
  d:newrow()
  d:check { id = "guide", text = "Add an export-safe reference guide", selected = saved.guide~=false,onclick=visual.changed }
  d:newrow()
  d:check {id="insert",text="Insert template guide in active sheet",selected=false,enabled=app.sprite~=nil,onclick=visual.changed}
  if not visual:show("create","Create") then return end
  local kind, size = d.data.kind, tonumber(d.data.size)
  local armor = kind:match("^Armor")
  local columns,rows=tonumber(d.data.columns) or saved.columns or 16,tonumber(d.data.rows) or saved.rows or 16
  local sheet = kind == "Sprite sheet"
  local templateWidth,templateHeight=armor and size*4 or size,armor and size*2 or size
  local s,origin,paintLayer,frame
  if d.data.insert and not sheet then
    s=sprite()
    local b=core.cell(s)
    assert(b.width==templateWidth and b.height==templateHeight,"Select a matching template region first")
    origin=Point(b.x,b.y);paintLayer=app.layer;frame=app.frame
    assert(s.colorMode==ColorMode.RGB,"Convert the sheet to RGB first")
    assert(d.data.guide,"Enable the reference guide to insert a template")
  else
    s=Sprite(sheet and columns*size or templateWidth,sheet and rows*size or templateHeight)
    s.filename=kind:lower():gsub(" / ","-"):gsub(" ","-") .. ".aseprite"
    s.gridBounds=Rectangle(0,0,size,size)
    s.layers[1].name=tr("Texture")
    origin=Point(0,0);paintLayer=s.layers[1];frame=s.frames[1]
    if sheet then s.selection:select(Rectangle(0,0,size,size)) end
  end
  if d.data.guide and not sheet then
    app.transaction(tr("Minecraft texture guide"), function()
      app.command.NewLayer { name = tr("UV guide (not exported)"), reference = true, ui = false }
      local layer = app.layer
      local guide = visual.result
      local cel = layer:cel(frame)
      if cel then cel.image=guide;cel.position=origin else s:newCel(layer,frame,guide,origin) end
      layer.isEditable = false
      app.layer=paintLayer
    end)
  end
  prefs.new_texture={kind=kind,size=tostring(size),columns=columns,rows=rows,guide=d.data.guide}
  app.refresh()
end

local function preview(mode)
  local source = sprite()
  local sourceId = source.id
  local function capture()
    assert(app.sprite == source, "Activate the source sheet first")
    local bounds = core.cell(source)
    local image = sheet.mask(source, bounds, core.capture(source, app.frame, bounds))
    if mode == "Tiling" then core.square(image) end
    return image
  end
  local image = capture()
  local d
  local refreshTimer, siteListener, spriteListener
  local function sourceIsOpen()
    for _, open in ipairs(app.sprites) do
      if open.id == sourceId then return true end
    end
    return false
  end
  local function paint(ev)
    local gc = ev.context
    gc.color = Color { r = 42, g = 44, b = 50 }
    gc:fillRect(Rectangle(0, 0, gc.width, gc.height))
    if not image then return end
    local function tile(x, y, w, h, background)
      gc.color = background or (d.data.light_background and Color{r=210,g=210,b=215} or Color { r = 72, g = 74, b = 80 })
      gc:fillRect(Rectangle(x, y, w, h))
      gc:drawImage(image, Rectangle(0, 0, image.width, image.height), Rectangle(x, y, w, h))
    end
    if mode == "Tiling" then
      local count = tonumber(d.data.tile_repeat) or 3
      local n = math.floor(math.min(gc.width, gc.height) / count)
      local ox,oy = (gc.width-n*count)//2,(gc.height-n*count)//2
      for y = 0, count-1 do for x = 0, count-1 do tile(ox+x*n,oy+y*n,n,n) end end
      if d.data.tile_grid then
        gc.color = Color{r=0,g=0,b=0,a=100}
        for i=1,count-1 do
          gc:fillRect(Rectangle(ox+i*n,oy,1,n*count))
          gc:fillRect(Rectangle(ox,oy+i*n,n*count,1))
        end
      end
      if d.data.mark_center then
        gc.color = Color { r = 245, g = 170, b = 80 }
        local mid = n*(count//2)
        gc:strokeRect(Rectangle(ox+mid,oy+mid,n,n))
      end
    else
      local backgrounds = { Color {r=195,g=195,b=195}, Color {r=30,g=30,b=35},
        Color {r=104,g=135,b=78}, Color {r=114,g=79,b=53} }
      local n = math.floor(math.min(gc.width, gc.height) / 2)
      for i, bg in ipairs(backgrounds) do
        local x, y = ((i - 1) % 2) * n, math.floor((i - 1) / 2) * n
        gc.color = bg
        gc:fillRect(Rectangle(x, y, n, n))
        local fit = math.min((n-8)/image.width,(n-28)/image.height)
        local scale = fit >= 1 and math.floor(fit) or fit
        local w,h = math.max(1,math.floor(image.width*scale)),math.max(1,math.floor(image.height*scale))
        tile(x+math.floor((n-w)/2),y+24+math.floor((n-28-h)/2),w,h,bg)
        -- A 16-pixel inventory sample remains useful for high-resolution art too.
        local iconScale = math.min(16/image.width,16/image.height)
        gc:drawImage(image,Rectangle(0,0,image.width,image.height),
          Rectangle(x+4,y+4,math.max(1,math.floor(image.width*iconScale)),math.max(1,math.floor(image.height*iconScale))))
      end
    end
  end
  d = Dialog { title="Java " .. mode .. " Preview", onclose=function()
    if refreshTimer then
      refreshTimer:stop()
      app.events:off(siteListener)
      if sourceIsOpen() then source.events:off(spriteListener) end
    end
    dialogs[d]=nil
  end }
  dialogs[d] = true
  local screen = app.window
  local size = math.max(32, math.min(256,
    math.floor(screen.width/app.uiScale)-48, math.floor(screen.height/app.uiScale)-(mode=="Tiling" and 192 or 106)))
  d:canvas { id = "preview", width = size, height = size, framed=true,fullrow=true, onpaint = paint }
  d:label { id="item_preview_status", fullrow=true, text = "Merged frame or selection bounds." }
  if mode=="Tiling" then
    d:combobox{id="tile_repeat",label="Tiles per side:",options={"3","5","7"},option=prefs.tile_repeat or "3",
      onchange=function() prefs.tile_repeat=d.data.tile_repeat;d:repaint() end}
    d:check{id="light_background",text="Light background",selected=prefs.tile_light==true,onclick=function()
      prefs.tile_light=d.data.light_background;d:repaint()
    end}
    d:newrow()
    d:check{id="mark_center",text="Mark center tile",selected=prefs.tile_center~=false,onclick=function()
      prefs.tile_center=d.data.mark_center;d:repaint()
    end}
    d:newrow()
    d:check{id="tile_grid",text="Show tile grid",selected=prefs.tile_grid==true,onclick=function()
      prefs.tile_grid=d.data.tile_grid;d:repaint()
    end}
    d:newrow()
  end
  d:button { id="refresh_preview", text = "Refresh", onclick = guarded(function()
    image = capture()
    d:modify { id="item_preview_status", text="Merged frame or selection bounds." }
    d:repaint()
  end) }
  d:button { id="close_preview", text = "Close", onclick = function() d:close() end }
  d:show { wait = false, autoscrollbars = true }
  do
    refreshTimer = Timer { interval=.12, ontick=function()
      refreshTimer:stop()
      if not sourceIsOpen() then d:close(); return end
      if app.sprite ~= source then return end
      local ok, updated = pcall(capture)
      if ok then
        image = updated
        d:modify { id="item_preview_status", text="Merged frame or selection bounds." }
      else
        image = nil
        d:modify { id="item_preview_status", text=tostring(updated):gsub("^.-:%d+: ", "") }
      end
      d:repaint()
    end }
    local function queueRefresh()
      refreshTimer:stop()
      refreshTimer:start()
    end
    siteListener = app.events:on("sitechange", queueRefresh)
    spriteListener = source.events:on("change", queueRefresh)
  end
end

local function addLayer(name, image, position)
  local s = sprite()
  assert(s.colorMode == ColorMode.RGB, "Convert to RGB first (Sprite > Color Mode > RGB)")
  app.transaction(tr(name), function()
    local layer = s:newLayer()
    layer.name = tr(name)
    s:newCel(layer, app.frame, image, position or Point(0,0))
  end)
  app.refresh()
end

local function offset()
  local s = sprite()
  local b = core.cell(s)
  assert(s.colorMode == ColorMode.RGB, "Convert to RGB first (Sprite > Color Mode > RGB)")
  local cel = assert(app.cel, "Select an image cel first")
  assert(not app.layer.isReference and not app.layer.isTilemap, "Select a regular image layer")
  assert(app.layer.isEditable, "Unlock the active layer first")
  local minx,miny=math.min(0,cel.position.x),math.min(0,cel.position.y)
  local width=math.max(s.width,cel.position.x+cel.image.width)-minx
  local height=math.max(s.height,cel.position.y+cel.image.height)-miny
  assert(width*height <= 4194304,"Offset exceeds four million pixels")
  local original = Image(width,height,ColorMode.RGB)
  for p in cel.image:pixels() do
    original:putPixel(p.x+cel.position.x-minx,p.y+cel.position.y-miny,p())
  end
  local selected = Image(b.width,b.height,ColorMode.RGB)
  for p in selected:pixels() do
    p(original:getPixel(b.x-minx+p.x,b.y-miny+p.y))
  end
  local unmasked=Image(selected)
  if not s.selection.isEmpty then
    for p in selected:pixels() do
      if not s.selection:contains(Point(b.x+p.x,b.y+p.y)) then p(0) end
    end
  end
  local dx,dy=math.floor(b.width/2),math.floor(b.height/2)
  local shifted
  if app.isUIAvailable then
    local d=Dialog("Offset Cell with Wrap")
    local visual=resultPreview.new(d,unmasked,function(opts)
      local x,y=tonumber(opts.x),tonumber(opts.y)
      assert(x and y and x%1==0 and y%1==0,"Use whole-pixel offsets")
      local result=core.offset(selected,x,y)
      return result,"Before / Result",sheet.displayEdit(s,b,unmasked,result)
    end)
    d:number{id="x",label="Horizontal:",text=tostring(dx),decimals=0,onchange=visual.changed}
    d:number{id="y",label="Vertical:",text=tostring(dy),decimals=0,onchange=visual.changed}
    d:label{text="Wrapped inside the selected region."}
    if not visual:show("apply","Apply") then return end
    shifted=visual.result
  else shifted=core.offset(selected,dx,dy) end
  -- Replace pixels, including transparency; compositing would retain old pixels.
  for p in shifted:pixels() do
    local x,y = b.x+p.x,b.y+p.y
    if s.selection.isEmpty or s.selection:contains(Point(x,y)) then original:putPixel(x-minx,y-miny,p()) end
  end
  app.transaction(tr("Minecraft wrapped offset"),function()
    cel.image = original
    cel.position = Point(minx,miny)
  end)
  app.refresh()
end

local function validate()
  local s = sprite()
  local image = current()
  local report = core.seams(image)
  local messages = { "Texture: " .. image.width .. " x " .. image.height,
    "Pixels with transparency: " .. report.transparent,
    "Partially transparent pixels: " .. report.partial,
    "Visible colors: " .. (report.colors>4096 and "4096+" or report.colors),
    "Opposite-edge differences: " .. report.edgeDifferences .. " (diagnostic, not always an error)" }
  if image.width ~= image.height then messages[#messages + 1] = "Block/item frames should be square." end
  local function power(n) while n > 1 and n % 2 == 0 do n = n / 2 end; return n == 1 end
  if not power(image.width) or not power(image.height) then messages[#messages+1] = "Non-power-of-two size: check your target atlas." end
  if #s.frames > 1 then
    local ok, reason = pcall(core.animation, s, core.cell(s))
    messages[#messages+1] = ok and "Animation timing: valid Java ticks." or tostring(reason):gsub("^.-:%d+: ", "")
  end
  alert { title = "Java Texture Check", text = messages }
end

local function normalMap()
  local saved=prefs.normal or {}
  local s=sprite();local b=core.cell(s);local source=core.capture(s,app.frame,b)
  local d = Dialog("Normal Map from Height")
  local visual=resultPreview.new(d,source,function(opts)
    return sheet.mask(s,b,core.normalMap(source,opts.strength,opts.wrap,opts.flip_green))
  end)
  d:slider { id = "strength", label = "Strength:", min = 1, max = 16, value = saved.strength or 4,onchange=visual.changed }
  d:check { id = "wrap", text = "Wrap edges for seamless textures", selected = saved.wrap~=false,onclick=visual.changed }
  d:newrow()
  d:check {id="flip_green",text="Flip green channel (-Y)",selected=saved.flip_green==true,onclick=visual.changed}
  d:label { text = "RGB normal map; not LabPBR." }
  if visual:show("create","Create") then
    addLayer("Minecraft normal map", visual.result,Point(b.x,b.y))
    prefs.normal={strength=d.data.strength,wrap=d.data.wrap,flip_green=d.data.flip_green}
  end
end

local function ramp()
  local saved=prefs.ramp or {}
  local s=sprite();local b=core.cell(s)
  local d = Dialog("Material Ramp")
  local visual=resultPreview.new(d,nil,function(opts)
    local colors=core.ramp(opts.shadow,opts.light,opts.steps,opts.space=="HSV (shortest hue)")
    assert(#colors<=b.width,"Too many colors for the selected cell")
    local image=Image(b.width,math.min(4,b.height),ColorMode.RGB)
    for pixel in image:pixels() do pixel(colors[math.min(#colors,math.floor(pixel.x*#colors/b.width)+1)].rgbaPixel) end
    return sheet.mask(s,b,image)
  end)
  d:color { id = "shadow", label = "Shadow:", color = app.bgColor,onchange=visual.changed }
  d:color { id = "light", label = "Highlight:", color = app.fgColor,onchange=visual.changed }
  d:slider { id = "steps", label = "Colors:", min = 2, max = 16, value = saved.steps or 5,onchange=visual.changed }
  d:combobox{id="space",label="Blend:",options={"RGB","HSV (shortest hue)"},option=saved.space or "RGB",onchange=visual.changed}
  if not visual:show("create","Create swatch") then return end
  addLayer("Minecraft material ramp",visual.result,Point(b.x,b.y))
  prefs.ramp={steps=d.data.steps,space=d.data.space}
end

local function exportJava()
  local s = sprite()
  local saved=prefs.single_export or {}
  local d = Dialog("Export Java Resources")
  d:entry { id = "root", label = "Resource root:", text = prefs.root or "" }
  d:label { text = "Choose the folder containing assets." }
  d:entry { id = "namespace", label = "Namespace:", text = prefs.namespace or "mymod" }
  d:entry { id = "path", label = "Asset path:", text = core.assetName(s,core.cell(s)) }
  d:combobox { id = "kind", label = "Type:", options = {"Block","Item / ingredient","Weapon / tool"},option=saved.kind or "Block" }
  d:newrow()
  d:check { id = "models", text = "Generate model JSON", selected = saved.models~=false }
  d:newrow()
  d:check { id = "modern", text = "Include item definition (Java 1.21.4+)", selected = saved.modern~=false }
  d:newrow()
  d:check { id = "animate", text = "Animate cell across all frames", selected = #s.frames > 1 }
  d:newrow()
  d:check{id="interpolate",text="Interpolate animation frames",selected=saved.interpolate==true}
  d:label { text = "Confirm replacement of existing resources." }
  d:button { id = "export", text = "Export", focus = true }; d:button { text = "Cancel" }; d:show { autoscrollbars = true }
  if not d.data.export then return end
  local files = exporter.plan(core,s,d.data)
  local existing = exporter.collisions(files)
  if #existing > 0 then
    local text = {"Replace " .. #existing .. " existing resource file(s)?"}
    for i = 1, math.min(#existing,8) do text[#text+1] = existing[i] end
    if alert { title = "Replace Java Resources", text = text, buttons = {"Replace","Cancel"} } ~= 1 then return end
  end
  local count = exporter.write(files,#existing > 0)
  prefs.root, prefs.namespace = d.data.root,d.data.namespace
  prefs.single_export={kind=d.data.kind,models=d.data.models,modern=d.data.modern,interpolate=d.data.interpolate}
  alert { title = "Java Resources Exported", text = count .. " files written to " .. d.data.root }
end

function init(plugin)
  prefs = plugin.preferences
  local menuTitle = "&Minecraft"
  plugin:newMenuGroup { id = "minecraft_menu", title = menuTitle, titleId = i18n.key(menuTitle),
    group = "help_menu_root" }
  local function command(id,title,callback,needsSprite,group)
    plugin:newCommand { id = id, title = title, titleId=i18n.key(title), group = group,
      onenabled = function() return not needsSprite or app.sprite ~= nil end, onclick = guarded(callback) }
  end
  local menu = "minecraft_menu"
  local function submenu(id, title)
    plugin:newMenuGroup { id=id, title=title, titleId=i18n.key(title), group=menu }
  end
  command("MinecraftQuickExport","Export PNG",function() quickExport.run(prefs,false) end,true,menu)
  command("MinecraftQuickExportAs","Export PNG As...",function() quickExport.run(prefs,true) end,true,menu)
  plugin:newMenuSeparator { group=menu }
  command("MinecraftColorVariants","Color Variants...",function() sheetUI.variants(prefs) end,true,menu)
  command("MinecraftItemPreview","Item Preview...",function() preview("Inventory") end,true,menu)
  command("MinecraftCellNavigator","Cell Navigator...",function() sheetUI.navigator(prefs) end,true,menu)
  command("MinecraftNewTexture","New Texture / Sheet...",newTexture,false,menu)
  plugin:newMenuSeparator { group=menu }
  submenu("minecraft_preview", "Block and Armor Previews")
  command("MinecraftTilePreview","Tiling...",function() preview("Tiling") end,true,"minecraft_preview")
  command("MinecraftBlockPreview","Block (3D)...",function() cubeUI.show(prefs) end,true,"minecraft_preview")
  command("MinecraftArmorPreview","Armor (Front / Back)...",armorUI.preview,true,"minecraft_preview")
  submenu("minecraft_java", "Java Edition Resources")
  command("MinecraftBatchExport","Export Sheet Cells...",function() sheetUI.export(prefs) end,true,"minecraft_java")
  command("MinecraftExport","Export Java Resources...",exportJava,true,"minecraft_java")
  command("MinecraftExportArmor","Export Java Armor...",function() armorUI.export(prefs) end,true,"minecraft_java")
  command("MinecraftValidate","Check Java Texture...",validate,true,"minecraft_java")
  submenu("minecraft_edit", "Texture Tools")
  command("MinecraftAlphaCleanup","Cell Alpha Cleanup...",function() sheetUI.alpha(prefs) end,true,"minecraft_edit")
  command("MinecraftOffset","Offset Active Cel with Wrap...",offset,true,"minecraft_edit")
  command("MinecraftMaterialRamp","Material Ramp...",ramp,true,"minecraft_edit")
  command("MinecraftNormalMap","Normal Map from Height...",normalMap,true,"minecraft_edit")
  -- Preserve the commands for existing shortcuts and scripts.
  command("MinecraftOutline","Cell Outline...",function() sheetUI.outline(prefs) end,true)
  command("MinecraftTransform","Transform Cell...",function() sheetUI.transform(prefs) end,true)
end

function exit(plugin)
  armorUI.close()
  sheetUI.close()
  cubeUI.close()
  for d in pairs(dialogs) do d:close() end
  dialogs = {}
end
