-- Copyright (c) 2026 fresh-milkshake. MIT license; see LICENSE.txt.
local i18n=package.loaded["minecraft-tools/minecraft_tools_i18n"] or require "minecraft_tools_i18n"
local Dialog,alert,assert,error,tr=i18n.dialog,i18n.alert,i18n.assert,i18n.error,i18n.tr
local core=require "minecraft_tools_core"
local sheet=require "minecraft_tools_sheet"
local scene=require "minecraft_tools_block_scene"
local M,dialogs={},{}
local function guard(fn) return function(...) local ok,e=pcall(fn,...);if not ok then alert(tostring(e):gsub("^.-:%d+: ","")) end end end
function M.show(prefs)
  prefs=prefs or {}
  local saved=prefs.cube_view or {}
  local s=assert(app.sprite,"Open a sprite sheet first")
  local function capture(bounds, selection)
    local image = core.square(core.capture(s,app.frame,bounds))
    return sheet.mask(s,bounds,image,selection)
  end
  local function currentSelection()
    local mask = Selection()
    mask:add(s.selection)
    return mask
  end
  local b=core.cell(s)
  local initial,mask=capture(b),currentSelection()
  local faces,names={},{}
  for i,def in ipairs(scene.faces) do
    faces[i]={bounds=b,image=initial,mask=mask}
    names[i]=def[1]
  end
  local yaw,pitch,zoom=saved.yaw or 35,saved.pitch or -25,saved.zoom or 100
  local dragging,lastx,lasty=false,0,0
  local d, backdrop, backdropKey
  local function paint(ev)
    local gc, view = ev.context, d.data
    local area = Rectangle(0,0,gc.width,gc.height)
    if view.environment == "Studio" then
      gc.color = view.background
      gc:fillRect(area)
    else
      local key = view.environment .. view.light
      if key ~= backdropKey then
        backdrop = scene.backdrop(view.environment,view.light)
        backdropKey = key
      end
      gc.color = Color{r=255,g=255,b=255}
      gc:drawImage(backdrop,backdrop.bounds,area)
    end
    view.yaw,view.pitch,view.zoom=yaw,pitch,zoom
    scene.draw(gc,faces,view)
  end
  d=Dialog{title="Java Block 3D Preview",onclose=function()
    local view=d.data
    prefs.cube_view={yaw=yaw,pitch=pitch,zoom=zoom,shading=view.shading,edges=view.edges,
      environment=view.environment,light=view.light}
    dialogs[d]=nil
  end};dialogs[d]=true
  local height=math.max(72,math.min(240,math.floor(app.window.height/app.uiScale)-230))
  local width=math.max(96,math.min(360,math.floor(app.window.width/app.uiScale)-48,height*3//2))
  local function setAngles(y,p)
    yaw,pitch=y%360,math.max(-89,math.min(89,p))
    d:modify{id="yaw",value=yaw}
    d:modify{id="pitch",value=pitch}
    d:repaint()
  end
  d:canvas{id="preview",width=width,height=height,framed=true,fullrow=true,onpaint=paint,
    onmousedown=function(ev) if ev.button==1 then dragging=true;lastx,lasty=ev.x,ev.y end end,
    onmouseup=function() dragging=false end,
    onmousemove=function(ev)
      if dragging then
        setAngles(yaw+ev.x-lastx,pitch-(ev.y-lasty))
        lastx,lasty=ev.x,ev.y
      end
    end,
    onwheel=function(ev) zoom=math.max(30,math.min(180,zoom-ev.deltaY*10));d:modify{id="zoom",value=zoom};d:repaint() end}
  d:tab{id="view_tab",text="View"}
  d:newrow()
  d:combobox{id="angle",label="Camera:",options={"Isometric","Front","Side","Top"},
    onchange=function()
      local angles={Isometric={35,-25},Front={0,0},Side={90,0},Top={0,-89}}
      setAngles(table.unpack(angles[d.data.angle]))
    end}
  d:slider{id="yaw",label="Yaw:",min=0,max=359,value=yaw,onchange=function() yaw=d.data.yaw;d:repaint() end}
  d:slider{id="pitch",label="Pitch:",min=-89,max=89,value=pitch,onchange=function() pitch=d.data.pitch;d:repaint() end}
  d:slider{id="zoom",label="Zoom:",min=30,max=180,value=zoom,onchange=function() zoom=d.data.zoom;d:repaint() end}
  d:tab{id="scene_tab",text="Scene"}
  d:newrow()
  d:combobox{id="environment",label="Environment:",options={"Studio","Grass and tree","Stone cave"},
    option=saved.environment or "Studio",onchange=function()
      d:modify{id="background",enabled=d.data.environment=="Studio"}
      d:repaint()
    end}
  d:combobox{id="light",label="Lighting:",options={"Daylight","Overcast","Dusk"},
    option=saved.light or "Daylight",onchange=function() d:repaint() end}
  d:color{id="background",label="Background:",color=Color{r=42,g=44,b=50},
    enabled=not saved.environment or saved.environment=="Studio",onchange=function() d:repaint() end}
  d:check{id="shading",text="Shade block",selected=saved.shading~=false,onclick=function() d:repaint() end}
  d:check{id="edges",text="Show face edges",selected=saved.edges==true,onclick=function() d:repaint() end}
  d:tab{id="faces_tab",text="Faces"}
  d:newrow()
  local function showMapping()
    for i,name in ipairs(names) do
      if name==d.data.face then
        local b=faces[i].bounds
        d:modify{id="mapping",text=name .. ": " .. b.x .. "," .. b.y .. " / " .. b.width .. "px"}
      end
    end
  end
  d:combobox{id="face",label="Assign face:",options=names,onchange=showMapping}
  d:label{id="mapping",text="North: " .. b.x .. "," .. b.y .. " / " .. b.width .. "px"}
  d:button{id="assign_face",text="Use Selected Cell",onclick=guard(function()
    assert(app.sprite==s,"Activate the source sheet first")
    local bounds=core.cell(s)
    local image=capture(bounds)
    for i,name in ipairs(names) do
      if name==d.data.face then faces[i]={bounds=bounds,image=image,mask=currentSelection()} end
    end
    showMapping();d:repaint()
  end)}
  d:newrow()
  d:button{id="assign_all",text="Use Cell on All Faces",onclick=guard(function()
    assert(app.sprite==s,"Activate the source sheet first")
    local bounds=core.cell(s)
    local image,mask=capture(bounds),currentSelection()
    for i=1,#faces do faces[i]={bounds=bounds,image=image,mask=mask} end
    showMapping();d:repaint()
  end)}
  d:endtabs{selected="view_tab"}
  d:newrow()
  d:button{id="refresh_preview",text="Refresh",onclick=guard(function()
    assert(app.sprite==s,"Activate the source sheet first")
    for _,face in ipairs(faces) do
      face.image=capture(face.bounds,face.mask)
    end
    d:repaint()
  end)}
  d:button{id="reset_view",text="Reset View",onclick=function()
    yaw,pitch,zoom=35,-25,100
    d:modify{id="yaw",value=yaw};d:modify{id="pitch",value=pitch};d:modify{id="zoom",value=zoom}
    d:repaint()
  end}
  d:button{id="close_preview",text="Close",onclick=function() d:close() end}
  d:show{wait=false,autoscrollbars=true}
end
function M.close() for d in pairs(dialogs) do d:close() end end
return M
