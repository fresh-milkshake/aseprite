-- Copyright (c) 2026 fresh-milkshake. MIT license; see LICENSE.txt.
local i18n=package.loaded["minecraft-tools/minecraft_tools_i18n"] or require "minecraft_tools_i18n"
local Dialog,alert,assert,error,tr=i18n.dialog,i18n.alert,i18n.assert,i18n.error,i18n.tr
local core=require "minecraft_tools_core"
local sheet=require "minecraft_tools_sheet"
local exporter=require "minecraft_tools_export"
local preview=require "minecraft_tools_preview"
local M,dialogs={},{}
local function active() return assert(app.sprite,"Open a sprite sheet first") end
local function guard(fn) return function(...) local ok,e=pcall(fn,...); if not ok then alert(tostring(e):gsub("^.-:%d+: ","")) end end end
local function add(sprite,b,image,name)
  assert(sprite.colorMode==ColorMode.RGB,"Convert to RGB first")
  app.transaction(tr(name),function()
    local layer=sprite:newLayer(); layer.name=tr(name)
    sprite:newCel(layer,app.frame,image,Point(b.x,b.y))
  end)
  app.refresh()
end
local function raw(sprite,b)
  local cel=assert(app.cel,"Select an image cel")
  assert(sprite.colorMode==ColorMode.RGB,"Convert to RGB first")
  local out=Image(b.width,b.height,ColorMode.RGB)
  for p in out:pixels() do
    local x,y=b.x+p.x-cel.position.x,b.y+p.y-cel.position.y
    if x>=0 and y>=0 and x<cel.image.width and y<cel.image.height then p(cel.image:getPixel(x,y)) end
  end
  return out
end
function M.navigator(prefs)
  prefs=prefs or {}
  local s=active()
  assert(s.width>=16 and s.height>=16,"Sheet must contain at least one 16 x 16 cell")
  local image=core.capture(s,app.frame)
  local size=sheet.cellSize(core,s)
  local initial=not s.selection.isEmpty and s.selection.bounds
  if initial and (initial.x<0 or initial.y<0 or initial.x+initial.width>s.width or
                  initial.y+initial.height>s.height or initial.x%size~=0 or initial.y%size~=0) then
    initial=nil
  end
  local x,y=initial and initial.x//size or 0,initial and initial.y//size or 0
  x,y=math.min(x,s.width//size-1),math.min(y,s.height//size-1)
  local cellWidth=initial and initial.width%size==0 and math.max(1,initial.width//size) or 1
  local cellHeight=initial and initial.height%size==0 and math.max(1,initial.height//size) or 1
  local d
  local function bounds(cx,cy)
    local w=tonumber(d.data.width) or 1
    local h=tonumber(d.data.height) or 1
    return sheet.rectangle(core,s,cx or x,cy or y,size,w,h)
  end
  local function selectCell()
    assert(app.sprite==s,"Activate the source sheet before selecting a cell")
    local b=bounds()
    s.selection:select(b)
    d:modify{id="column",text=tostring(x+1)}; d:modify{id="row",text=tostring(y+1)}
    d:modify{id="name",text=core.assetName(s,b)}
    d:modify{id="location",text=string.format("Column %d, row %d / %d x %d",x+1,y+1,b.width,b.height)}
    app.refresh(); d:repaint()
  end
  d=Dialog{title="Minecraft Cell Navigator",onclose=function() dialogs[d]=nil end}; dialogs[d]=true
  local width=math.max(32,math.min(320,math.floor(app.window.width/app.uiScale)-48))
  local height=math.max(32,math.min(220,math.floor(app.window.height/app.uiScale)-145))
  local scale
  d:tab{id="sheet_tab",text="Sheet"}
  d:canvas{id="sheet",width=width,height=height,framed=true,fullrow=true,onpaint=function(ev)
    local gc=ev.context
    gc.color=Color{r=42,g=44,b=50}; gc:fillRect(Rectangle(0,0,gc.width,gc.height))
    scale=math.min(gc.width/image.width,gc.height/image.height)
    local w,h=math.floor(image.width*scale),math.floor(image.height*scale)
    gc:drawImage(image,Rectangle(0,0,image.width,image.height),Rectangle(0,0,w,h))
    if d.data.grid and scale*size>=4 then
      gc.color=Color{r=80,g=84,b=92}
      for gx=0,s.width,size do gc:strokeRect(Rectangle(math.floor(gx*scale),0,1,h)) end
      for gy=0,s.height,size do gc:strokeRect(Rectangle(0,math.floor(gy*scale),w,1)) end
    end
    gc.color=Color{r=255,g=190,b=70}
    gc:strokeRect(Rectangle(math.floor(x*size*scale),math.floor(y*size*scale),math.max(1,math.floor(size*(tonumber(d.data.width) or 1)*scale)),math.max(1,math.floor(size*(tonumber(d.data.height) or 1)*scale))))
  end,onmousedown=guard(function(ev)
    if not scale then return end
    local cx,cy=math.floor(ev.x/scale/size),math.floor(ev.y/scale/size)
    if not pcall(bounds,cx,cy) then return end -- Ignore margins/partial edge cells.
    x,y=cx,cy;selectCell()
  end)}
  d:newrow()
  d:check{id="grid",text="Show cell grid",selected=prefs.navigator_grid~=false,onclick=function()
    prefs.navigator_grid=d.data.grid;d:repaint()
  end}
  d:newrow()
  d:label{id="location",text=string.format("Column %d, row %d",x+1,y+1)}
  d:tab{id="cells_tab",text="Cell"}
  d:newrow()
  local sizes={}
  for _,option in ipairs(core.cellSizes) do
    if tonumber(option)<=math.min(s.width,s.height) then sizes[#sizes+1]=option end
  end
  d:combobox{id="size",label="Cell size:",options=sizes,option=tostring(size),onchange=guard(function()
    local previous=size
    size=tonumber(d.data.size)
    if not pcall(bounds,0,0) then size=previous;d:modify{id="size",option=tostring(previous)};return end
    x,y=0,0;selectCell()
  end)}
  d:number{id="column",label="Column (1+):",text=tostring(x+1),decimals=0}
  d:number{id="row",label="Row (1+):",text=tostring(y+1),decimals=0}
  d:number{id="width",label="Width (cells):",text=tostring(cellWidth),decimals=0}
  d:number{id="height",label="Height (cells):",text=tostring(cellHeight),decimals=0}
  d:tab{id="name_tab",text="Name"}
  d:newrow()
  d:entry{id="name",label="Asset name:",text=initial and core.assetName(s,initial) or "my_texture"}
  d:button{id="name_cell",text="Name Cell (Slice)",onclick=guard(function()
    local b=bounds()
    assert(app.sprite==s,"Activate the source sheet first")
    sheet.nameCell(core,s,b,d.data.name)
  end)}
  d:endtabs{selected="sheet_tab"}
  d:newrow()
  d:button{id="select",text="Select",onclick=guard(function()
    local cx,cy=tonumber(d.data.column),tonumber(d.data.row)
    assert(cx and cy and cx%1==0 and cy%1==0,"Use whole-number coordinates")
    bounds(cx-1,cy-1) -- Validate before changing navigator state.
    x,y=cx-1,cy-1;selectCell()
  end)}
  local function step(direction)
    assert(app.sprite==s,"Activate the source sheet first")
    x,y=sheet.step(core,s,x,y,size,tonumber(d.data.width),tonumber(d.data.height),direction)
    selectCell()
  end
  d:button{id="previous",text="Prev",onclick=guard(function() step(-1) end)}
  d:button{id="next_cell",text="Next",onclick=guard(function() step(1) end)}
  d:button{text="Refresh",onclick=guard(function() assert(app.sprite==s,"Activate the source sheet first"); image=core.capture(s,app.frame); d:repaint() end)}
  d:button{id="close_preview",text="Close",onclick=function() d:close() end}
  d:show{wait=false,autoscrollbars=true}
end
function M.export(prefs)
  local s=active();local saved=prefs.sheet_export or {}
  local d=Dialog("Export Minecraft Sheet Cells")
  d:tab{id="resources_tab",text="Resources"}
  d:newrow()
  d:entry{id="root",label="Resource root:",text=prefs.root or ""}
  d:entry{id="namespace",label="Namespace:",text=prefs.namespace or "mymod"}
  d:entry{id="prefix",label="Asset folder:",text=saved.prefix or "sheet"}
  d:combobox{id="kind",label="Type:",options={"Block","Item / ingredient","Weapon / tool"},option=saved.kind or "Block"}
  d:newrow()
  d:check{id="models",text="Generate models / blockstates",selected=saved.models~=false}
  d:newrow()
  d:check{id="modern",text="Include 1.21.4+ item definitions",selected=saved.modern~=false}
  d:newrow()
  d:label{text="Names: slices, otherwise r001_c001."}
  d:tab{id="source_tab",text="Cells"}
  d:newrow()
  d:combobox{id="size",label="Cell size:",options=core.cellSizes,option=tostring(sheet.cellSize(core,s))}
  d:newrow()
  d:check{id="selection",text="Only selected grid rectangle",selected=not s.selection.isEmpty}
  d:newrow()
  d:check{id="skip",text="Skip empty cells",selected=saved.skip~=false}
  d:newrow()
  d:check{id="animate",text="Animate across timeline",selected=#s.frames>1}
  d:newrow()
  d:check{id="interpolate",text="Interpolate animation frames",selected=saved.interpolate==true}
  d:endtabs{selected="resources_tab"}
  d:newrow()
  d:button{id="export",text="Export",focus=true}; d:button{text="Cancel"}; d:show{autoscrollbars=true}
  if not d.data.export then return end
  local opts=d.data
  if opts.selection then assert(not s.selection.isEmpty,"Select a grid rectangle first"); opts.bounds=core.bounds(s,true) end
  local files,manifest=sheet.plan(core,exporter,s,opts)
  local existing=exporter.collisions(files)
  local summary={#manifest .. " cells; " .. #files .. " output files.",#existing .. " existing files will be replaced."}
  for i=1,math.min(6,#manifest) do summary[#summary+1]=manifest[i].asset end
  if #manifest>6 then summary[#summary+1]="..." end
  if alert{title="Confirm Sheet Export",text=summary,buttons={"Export","Cancel"}}~=1 then return end
  exporter.write(files,#existing>0)
  prefs.root,prefs.namespace=opts.root,opts.namespace
  prefs.sheet_export={prefix=opts.prefix,kind=opts.kind,skip=opts.skip,
    models=opts.models,modern=opts.modern,interpolate=opts.interpolate}
  alert{title="Minecraft Sheet Exported",text=#manifest .. " cells exported."}
end
function M.variants(prefs)
  prefs=prefs or {}
  local saved=prefs.variants or {}
  local s=active(); local b=core.cell(s)
  assert(b.width==b.height,"Select a square cell")
  assert(b.width<=512,"Preview textures must be at most 512 x 512")
  local source=sheet.mask(s,b,core.capture(s,app.frame,b))
  local d=Dialog("Cell Color Variants")
  local chosen=1
  local function generate(opts)
    local image=Image(b.width*opts.count,b.height,ColorMode.RGB)
    image:drawImage(source,Point(0,0))
    for i=1,opts.count-1 do
      image:drawImage(sheet.hue(source,i*opts.hue,opts.saturation,opts.value,opts.grays),Point(i*b.width,0))
    end
    chosen=math.min(chosen,opts.count)
    local result=image
    if opts.which=="Selected variant" then
      result=Image(b.width,b.height,ColorMode.RGB)
      result:drawImage(image,Point(-(chosen-1)*b.width,0))
    end
    return result,"Selected variant: " .. chosen .. " / " .. opts.count,image
  end
  local visual=preview.new(d,nil,generate,{
    pick=function(x) chosen=math.max(1,math.min(d.data.count,math.floor(x/b.width)+1)) end,
    selection=function() return Rectangle((chosen-1)*b.width,0,b.width,b.height) end})
  d:slider{id="count",label="Variants:",min=2,max=8,value=saved.count or 6,onchange=visual.changed}
  d:slider{id="hue",label="Hue step:",min=0,max=180,value=saved.hue or 30,onchange=visual.changed}
  d:slider{id="saturation",label="Saturation:",min=-100,max=100,value=saved.saturation or 0,onchange=visual.changed}
  d:slider{id="value",label="Brightness:",min=-100,max=100,value=saved.value or 0,onchange=visual.changed}
  d:newrow()
  d:check{id="grays",text="Preserve neutral grays",selected=saved.grays~=false,onclick=visual.changed}
  d:combobox{id="placement",label="Output:",options={"Empty cells in this sheet","New sheet row"},
    option=saved.placement or "New sheet row",onchange=visual.changed}
  d:combobox{id="which",label="Create:",options={"All variants","Selected variant"},option=saved.which or "All variants",onchange=visual.changed}
  d:newrow()
  d:label{text="First tile: original. Click to choose."}
  if not visual:show("create","Create") then return end
  local opts=d.data
  local image=visual.result
  if opts.placement=="Empty cells in this sheet" then
    local target=assert(sheet.space(core.capture(s,app.frame),b.width,image.width//b.width),
      "No empty row fits these variants. Reduce count, expand the sheet, or choose New sheet row.")
    add(s,target,image,"Minecraft color variants")
  else
    local out=Sprite(image.width,image.height,ColorMode.RGB)
    app.transaction(tr("Minecraft color variants"),function() out.cels[1].image=image end)
    out.gridBounds=Rectangle(0,0,b.width,b.height);out.filename="color-variants.aseprite"
  end
  prefs.variants={count=opts.count,hue=opts.hue,saturation=opts.saturation,value=opts.value,grays=opts.grays,placement=opts.placement,which=opts.which}
end
function M.outline(prefs)
  prefs=prefs or {}
  local saved=prefs.outline or {}
  local s=active();local b=core.cell(s);local source=core.capture(s,app.frame,b)
  local d=Dialog("Cell Outline")
  local visual=preview.new(d,source,function(opts)
    local layer=sheet.mask(s,b,sheet.outline(source,opts.color,opts.inside,opts.diagonal))
    local combined=Image(source);combined:drawImage(layer)
    return layer,"Before / Result",combined
  end)
  d:color{id="color",label="Color:",color=app.fgColor,onchange=visual.changed}
  d:newrow()
  d:check{id="inside",text="Inside silhouette",selected=saved.inside==true,onclick=visual.changed}
  d:newrow()
  d:check{id="diagonal",text="Include diagonal neighbors",selected=saved.diagonal==true,onclick=visual.changed}
  d:newrow()
  d:label{text="New layer, clipped to selection."}
  if visual:show("create","Create") then
    add(s,b,visual.result,"Minecraft cell outline")
    prefs.outline={inside=d.data.inside,diagonal=d.data.diagonal}
  end
end
function M.transform(prefs)
  prefs=prefs or {}
  local saved=prefs.transform or {}
  local s=active();local b=core.cell(s);local original=raw(s,b);local source=sheet.mask(s,b,original)
  local d=Dialog("Transform Cell")
  local visual=preview.new(d,original,function(opts)
    local result=sheet.transform(source,opts.mode)
    return result,"Before / Result",sheet.displayEdit(s,b,original,result)
  end)
  d:combobox{id="mode",label="Transform:",options={"Rotate clockwise","Rotate counterclockwise","Flip horizontal","Flip vertical"},option=saved.mode or "Rotate clockwise",onchange=visual.changed}
  d:newrow()
  d:label{text="Active cel; neighbors stay unchanged."}
  if visual:show("apply","Apply") then
    sheet.replace(s,b,visual.result,"Minecraft cell transform")
    prefs.transform={mode=d.data.mode}
  end
end
function M.alpha(prefs)
  prefs=prefs or {}
  local saved=prefs.alpha or {}
  local s=active();local b=core.cell(s);local source=raw(s,b)
  local d=Dialog("Cell Alpha Cleanup")
  local visual=preview.new(d,source,function(opts)
    local result=sheet.alpha(source,opts.threshold)
    return result,"Before / Result",sheet.displayEdit(s,b,source,result)
  end)
  d:slider{id="threshold",label="Cutoff:",min=1,max=255,value=saved.threshold or 128,onchange=visual.changed}
  d:newrow()
  d:label{text="Alpha below cutoff: 0; otherwise: 255."}
  d:newrow()
  d:label{text="Keeps RGB, including transparent pixels."}
  if visual:show("apply","Apply") then
    sheet.replace(s,b,visual.result,"Minecraft cell alpha cleanup")
    prefs.alpha={threshold=d.data.threshold}
  end
end
function M.close() for d in pairs(dialogs) do d:close() end end
return M
