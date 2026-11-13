-- Copyright (c) 2026 fresh-milkshake. MIT license; see LICENSE.txt.
local i18n=package.loaded["minecraft-tools/minecraft_tools_i18n"] or require "minecraft_tools_i18n"
local Dialog,alert,assert,error,tr=i18n.dialog,i18n.alert,i18n.assert,i18n.error,i18n.tr
-- Shared non-destructive preview. The confirmed image is the same cached result
-- shown by the canvas; changing controls never touches a document or its Undo.
local M={}
function M.compare(before,after)
  if not before then return after end
  local out=Image(before.width+8+after.width,math.max(before.height,after.height),ColorMode.RGB)
  out:drawImage(before,Point(0,0));out:drawImage(after,Point(before.width+8,0))
  return out
end
function M.new(dialog,before,render,options)
  options=options or {}
  local self={dialog=dialog,ready=false,accepted=false,result=nil,target=nil}
  local timer
  local function errorText(reason)
    return tr((tostring(reason):gsub("^.-:%d+: ","")))
  end
  function self:update()
    local ok,image,status,display=pcall(render,dialog.data)
    if ok and image then
      self.result=image;self.display=M.compare(before,display or image)
      self.status=status or (before and "Before / Result" or "Result preview")
    else
      self.result=nil;self.display=nil;self.status=errorText(image or "Adjust the options")
    end
    if self.ready then
      dialog:modify{id="preview_status",text=self.status}
      dialog:modify{id=self.confirmId,enabled=self.result~=nil}
      dialog:repaint()
    end
    return self.result~=nil
  end
  self.changed=function()
    if not self.ready then return end
    dialog:modify{id=self.confirmId,enabled=false}
    timer:stop();timer:start()
  end
  timer=Timer{interval=.08,ontick=function() timer:stop();self:update() end}
  local width=math.max(64,math.min(288,math.floor(app.window.width/app.uiScale)-48))
  local height=math.max(32,math.min(104,math.floor(app.window.height/app.uiScale)-230))
  dialog:canvas{id="result_preview",width=width,height=height,framed=true,fullrow=true,onpaint=function(ev)
    local gc=ev.context
    for y=0,gc.height-1,6 do for x=0,gc.width-1,6 do
      gc.color=(x//6+y//6)%2==0 and Color{r=104,g=106,b=112} or Color{r=92,g=94,b=100}
      gc:fillRect(Rectangle(x,y,math.min(6,gc.width-x),math.min(6,gc.height-y)))
    end end
    local image=self.display
    if not image then return end
    local fit=math.min(gc.width/image.width,gc.height/image.height)
    local scale=fit>=1 and math.floor(fit) or fit
    local w,h=math.max(1,math.floor(image.width*scale)),math.max(1,math.floor(image.height*scale))
    local target=Rectangle((gc.width-w)//2,(gc.height-h)//2,w,h)
    self.target=target
    gc:drawImage(image,Rectangle(0,0,image.width,image.height),target)
    if options.selection then
      local b=options.selection()
      if b then
        gc.color=Color{r=255,g=190,b=65}
        gc:strokeRect(Rectangle(target.x+math.floor(b.x*w/image.width),target.y+math.floor(b.y*h/image.height),
          math.max(1,math.floor(b.width*w/image.width)),math.max(1,math.floor(b.height*h/image.height))))
      end
    end
  end,onmousedown=function(ev)
    if not options.pick or not self.target or not self.display then return end
    local b=self.target
    if ev.x<b.x or ev.y<b.y or ev.x>=b.x+b.width or ev.y>=b.y+b.height then return end
    options.pick((ev.x-b.x)*self.display.width/b.width,(ev.y-b.y)*self.display.height/b.height)
    self.changed()
  end}
  dialog:label{id="preview_status",text=before and "Before / Result" or "Result preview"}
  dialog:tab{id="config_tab",text="Options"};dialog:newrow()
  function self:show(id,text)
    self.confirmId=id
    dialog:endtabs{selected="config_tab"};dialog:newrow()
    dialog:button{id=id,text=text,focus=true,onclick=function()
      timer:stop()
      if self:update() then self.accepted=true;dialog:close() end
    end}
    dialog:button{id="cancel_preview",text="Cancel"}
    self.ready=true;self:update()
    dialog:show{autoscrollbars=true}
    self.ready=false;timer:stop()
    return self.accepted
  end
  return self
end
return M
