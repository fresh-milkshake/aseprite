-- Copyright (c) 2026 fresh-milkshake. MIT license; see LICENSE.txt.
local i18n=package.loaded["minecraft-tools/minecraft_tools_i18n"] or require "minecraft_tools_i18n"
local Dialog,alert,assert,error,tr=i18n.dialog,i18n.alert,i18n.assert,i18n.error,i18n.tr
local core = require "minecraft_tools_core"
local armor = require "minecraft_tools_armor"
local exporter = require "minecraft_tools_export"
local M = {}
local openDialogs = {}

local function choose(title, prefs, exporting)
  local documents,names,lookup = {},{},{}
  local babyNames = {"(none)"}
  local function source(name,s,bounds)
    if not bounds then return end
    local w,h=bounds.width,bounds.height
    if w>=64 and w<=512 and (h*2==w or h==w) then
      local value={sprite=s,bounds=bounds}
      if h*2==w then documents[#documents+1]=value;names[#names+1]=name
      else babyNames[#babyNames+1]=name end
      lookup[name]=value
    end
  end
  for index,s in ipairs(app.sprites) do
    local name=index .. ": " .. app.fs.fileName(s.filename)
    source(name,s,Rectangle(0,0,s.width,s.height))
    if not s.selection.isEmpty then source(name .. " [selection]",s,s.selection.bounds) end
    for i,slice in ipairs(s.slices) do source(name .. " [" .. i .. ": " .. slice.name .. "]",s,slice.bounds) end
  end
  assert(#documents>=1,"Select or name a 64 x 32 armor region in your sheet (Cell Navigator: 4 x 2 cells)")
  local d = Dialog(title)
  d:combobox {id="upper",label="Upper / boots:",options=names,option=names[1]}
  d:combobox {id="leggings",label="Leggings:",options=names,option=names[2] or names[1]}
  if exporting then
    d:combobox {id="profile",label="Java version:",options={"Legacy (up to 1.21.1)",
      "Equipment (1.21.2-1.21.3)","Equipment (1.21.4-1.21.11)","Equipment (26.1+)"},option="Equipment (1.21.4-1.21.11)"}
    d:combobox {id="baby",label="Baby 64 x 64:",options=babyNames,option=babyNames[2] or babyNames[1]}
    d:entry {id="root",label="Resource root:",text=prefs.root or ""}
    d:entry {id="namespace",label="Namespace:",text=prefs.namespace or "mymod"}
    d:entry {id="path",label="Armor asset:",text="custom_armor"}
    d:label {text="26.1+: supply a separate 64 x 64 baby atlas."}
  else d:label {text="Sheet selections / slices: adult front and back."} end
  d:button {id="confirm",text=exporting and "Export" or "Preview",focus=true}
  d:button {text="Cancel"}; d:show{autoscrollbars=true}
  if not d.data.confirm then return end
  return d.data,lookup[d.data.upper],lookup[d.data.leggings],lookup[d.data.baby]
end

function M.preview()
  local _,upper,leggings = choose("Armor Preview",{},false)
  if not upper then return end
  local upperImage,leggingsImage = armor.atlas(core,upper),armor.atlas(core,leggings)
  local image = armor.paperdoll(upperImage,leggingsImage)
  local d
  d = Dialog {title="Java Armor Preview",onclose=function() openDialogs[d]=nil end}
  openDialogs[d] = true
  local height=math.max(56,math.min(224,math.floor(app.window.height/app.uiScale)-165))
  local width=math.max(96,math.min(320,math.floor(app.window.width/app.uiScale)-48,math.floor(height*1.4)))
  local function update()
    image = armor.paperdoll(upperImage,leggingsImage,d.data.armor_upper,d.data.armor_leggings)
    d:repaint()
  end
  d:canvas {id="armor_preview",width=width,height=height,framed=true,fullrow=true,onpaint=function(ev)
    local gc = ev.context
    gc.color = d.data.armor_background
    gc:fillRect(Rectangle(0,0,gc.width,gc.height))
    local mode = d.data.armor_view
    local sourceWidth = mode == "Front and back" and image.width or image.width//2
    local sourceX = mode == "Back" and sourceWidth or 0
    local fit=math.min(gc.width/sourceWidth,(gc.height-16)/image.height)
    local scale=fit>=1 and math.floor(fit) or fit
    local w,h = math.max(1,math.floor(sourceWidth*scale)),math.max(1,math.floor(image.height*scale))
    local x,y = (gc.width-w)//2,math.max(0,(gc.height-16-h)//2)
    gc:drawImage(image,Rectangle(sourceX,0,sourceWidth,image.height),Rectangle(x,y,w,h))
    local function caption(text,cx)
      text=tr(text)
      local labelWidth=gc:measureText(text).width
      local color=d.data.armor_background
      gc.color=(color.red+color.green+color.blue>420) and Color{r=28,g=29,b=32} or Color{r=238,g=238,b=238}
      gc:fillText(text,cx-labelWidth//2,gc.height-14)
    end
    if mode == "Front and back" then
      caption("Front",x+w//4)
      caption("Back",x+w*3//4)
    else
      caption(mode,gc.width//2)
    end
  end}
  d:combobox {id="armor_view",label="View:",options={"Front and back","Front","Back"},onchange=function() d:repaint() end}
  d:color {id="armor_background",label="Background:",color=Color{r=42,g=44,b=50},onchange=function() d:repaint() end}
  d:check {id="armor_upper",text="Upper / boots",selected=true,onclick=update}
  d:check {id="armor_leggings",text="Leggings",selected=true,onclick=update}
  d:newrow()
  d:button {id="refresh_preview",text="Refresh",onclick=function()
    local ok,top,bottom = pcall(function()
      local a,b=armor.atlas(core,upper),armor.atlas(core,leggings)
      armor.paperdoll(a,b,false,false)
      return a,b
    end)
    if ok then upperImage,leggingsImage=top,bottom;update() else alert(tostring(top)) end
  end}
  d:button {id="close_preview",text="Close",onclick=function() d:close() end}; d:show{wait=false,autoscrollbars=true}
end

function M.export(prefs)
  local opts,upper,leggings,baby = choose("Export Java Armor",prefs,true)
  if not opts then return end
  local files = armor.plan(core,upper,leggings,baby,opts)
  local collisions = exporter.collisions(files)
  if #collisions > 0 then
    local text = {"Replace these armor resources?"}
    for _,path in ipairs(collisions) do text[#text+1]=path end
    if alert{title="Replace Java Armor",text=text,buttons={"Replace","Cancel"}} ~= 1 then return end
  end
  exporter.write(files,#collisions > 0)
  prefs.root,prefs.namespace=opts.root,opts.namespace
  alert {title="Java Armor Exported",text=#files .. " files written. Configure the matching material/equippable asset in your mod."}
end

function M.close()
  for d in pairs(openDialogs) do d:close() end
end
return M
