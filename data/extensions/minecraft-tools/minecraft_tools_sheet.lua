-- Copyright (c) 2026 fresh-milkshake. MIT license; see LICENSE.txt.
local i18n=package.loaded["minecraft-tools/minecraft_tools_i18n"] or require "minecraft_tools_i18n"
local Dialog,alert,assert,error,tr=i18n.dialog,i18n.alert,i18n.assert,i18n.error,i18n.tr
local M={}
function M.nonempty(image)
  for p in image:pixels() do if app.pixelColor.rgbaA(p())>0 then return true end end
  return false
end
function M.cellSize(core,sprite)
  local grid=sprite.gridBounds
  local size=grid.width
  if core.isCellSize(size) and grid.height==size and sprite.width>=size and sprite.height>=size then
    return size
  end
  if not sprite.selection.isEmpty then
    local bounds=sprite.selection.bounds
    size=bounds.width
    if core.isCellSize(size) and bounds.height==size and bounds.x%size==0 and bounds.y%size==0 then
      return size
    end
  end
  return 16
end
function M.rectangle(core,sprite,x,y,size,w,h)
  for _,n in ipairs({x,y,size,w,h}) do
    assert(type(n)=="number" and n==n and n%1==0,"Use whole-number cell coordinates and dimensions")
  end
  assert(core.isCellSize(size) and x>=0 and y>=0 and w>=1 and h>=1,"Invalid grid region")
  assert((x+w)*size<=sprite.width and (y+h)*size<=sprite.height,"Region lies outside the sheet")
  return Rectangle(x*size,y*size,w*size,h*size)
end
function M.step(core,sprite,x,y,size,w,h,direction)
  M.rectangle(core,sprite,x,y,size,w,h)
  local firstX,firstY=x%w,y%h
  local lastX=firstX+((sprite.width//size-w-firstX)//w)*w
  local lastY=firstY+((sprite.height//size-h-firstY)//h)*h
  x=x+direction*w
  if x<firstX then x=lastX;y=y-h end
  if x>lastX then x=firstX;y=y+h end
  if y<firstY then y=lastY end
  if y>lastY then y=firstY end
  M.rectangle(core,sprite,x,y,size,w,h)
  return x,y
end
function M.nameCell(core,sprite,bounds,name)
  core.assetId("mymod",name)
  local found
  for _,slice in ipairs(sprite.slices) do
    if slice.bounds==bounds then found=slice
    else assert(slice.name:lower()~=name:lower(),"Another cell already uses this name: " .. name) end
  end
  app.transaction(tr("Name Minecraft cell"),function()
    local slice=found or sprite:newSlice(bounds);slice.name=name
  end)
end
function M.displayEdit(sprite,bounds,before,result)
  local out=Image(before)
  for pixel in result:pixels() do
    if sprite.selection.isEmpty or sprite.selection:contains(Point(bounds.x+pixel.x,bounds.y+pixel.y)) then
      out:putPixel(pixel.x,pixel.y,pixel())
    end
  end
  return out
end
function M.mask(sprite,bounds,image,selection)
  selection=selection or sprite.selection
  local out=Image(image)
  if not selection.isEmpty then
    for p in out:pixels() do
      if not selection:contains(Point(bounds.x+p.x,bounds.y+p.y)) then p(0) end
    end
  end
  return out
end
function M.cells(core,sprite,size,bounds)
  assert(core.isCellSize(size),"Choose 16, 32, 64 or 128 pixel cells")
  bounds=bounds or Rectangle(0,0,sprite.width,sprite.height)
  assert(bounds.width>0 and bounds.height>0,"Select at least one cell")
  assert(bounds.x%size==0 and bounds.y%size==0 and bounds.width%size==0 and bounds.height%size==0,
    "The sheet or selection must align to the chosen cell size")
  assert(bounds.width/size*bounds.height/size<=4096,"Export at most 4096 cells at once")
  local cells={}
  for y=bounds.y,bounds.y+bounds.height-size,size do
    for x=bounds.x,bounds.x+bounds.width-size,size do
      local b=core.region(sprite,x,y,size)
      local name=string.format("r%03d_c%03d",y//size+1,x//size+1)
      for _,slice in ipairs(sprite.slices) do
        if slice.bounds==b then name=slice.name; break end
      end
      cells[#cells+1]={bounds=b,name=name}
    end
  end
  return cells
end
function M.plan(core,exporter,sprite,opts)
  local cells=M.cells(core,sprite,tonumber(opts.size),opts.bounds)
  local files,manifest,total={}, {},0
  for _,cell in ipairs(cells) do
    local nonempty=not opts.skip
    for _,frame in ipairs(sprite.frames) do
      if nonempty then break end
      nonempty=M.nonempty(core.capture(sprite,frame,cell.bounds))
      if not opts.animate then break end
    end
    if nonempty then
      local cfg={}
      for k,v in pairs(opts) do cfg[k]=v end
      cfg.bounds=cell.bounds
      local prefix=opts.prefix or ""
      cfg.path=(prefix~="" and prefix .. "/" or "") .. cell.name
      cfg.frame=1 -- Static sheet always uses first frame, independent of UI state.
      local planned=exporter.plan(core,sprite,cfg)
      for _,file in ipairs(planned) do
        if file.image then total=total+file.image.width*file.image.height end
        assert(total<=16777216,"Batch exceeds 16 million pixels; select a smaller sheet region")
        files[#files+1]=file
      end
      manifest[#manifest+1]={asset=core.assetId(opts.namespace,cfg.path),x=cell.bounds.x,y=cell.bounds.y,
        size=cell.bounds.width,animated=not not opts.animate}
    end
  end
  assert(#manifest>0,"No nonempty cells to export")
  exporter.collisions(files)
  return files,manifest
end
-- Locate a contiguous row of genuinely empty cells without overwriting art.
function M.space(image,size,count)
  local columns,rows=image.width//size,image.height//size
  if columns<count then return nil end
  local occupied={}
  for p in image:pixels() do
    if app.pixelColor.rgbaA(p())>0 and p.x//size<columns and p.y//size<rows then
      occupied[(p.y//size)*columns+p.x//size]=true
    end
  end
  for y=0,rows-1 do
    local run=0
    for x=0,columns-1 do
      run=occupied[y*columns+x] and 0 or run+1
      if run>=count then return Rectangle((x-count+1)*size,y*size,count*size,size) end
    end
  end
  return nil
end
function M.transform(image,mode)
  local out=Image(image.spec)
  assert(image.width==image.height,"Select a square cell")
  local n=image.width
  for p in out:pixels() do
    local x,y=p.x,p.y
    if mode=="Rotate clockwise" then x,y=y,n-1-x
    elseif mode=="Rotate counterclockwise" then x,y=n-1-y,x
    elseif mode=="Flip horizontal" then x=n-1-x
    elseif mode=="Flip vertical" then y=n-1-y
    else error("Unknown cell transform") end
    p(image:getPixel(x,y))
  end
  return out
end
function M.outline(image,color,inside,diagonal)
  local out=Image(image.spec)
  local function opaque(x,y)
    return x>=0 and y>=0 and x<image.width and y<image.height and app.pixelColor.rgbaA(image:getPixel(x,y))>0
  end
  for p in out:pixels() do
    local filled=opaque(p.x,p.y)
    if filled==inside then
      for dy=-1,1 do for dx=-1,1 do
        if (dx~=0 or dy~=0) and (diagonal or dx==0 or dy==0) and opaque(p.x+dx,p.y+dy)~=filled then
          p(color.rgbaPixel)
        end
      end end
    end
  end
  return out
end
function M.alpha(image,threshold)
  local out=Image(image.spec)
  for p in out:pixels() do
    local pixelValue=image:getPixel(p.x,p.y)
    local c=Color{r=app.pixelColor.rgbaR(pixelValue),g=app.pixelColor.rgbaG(pixelValue),
      b=app.pixelColor.rgbaB(pixelValue),a=app.pixelColor.rgbaA(pixelValue)}
    c.alpha=c.alpha>=threshold and 255 or 0
    p(c.rgbaPixel)
  end
  return out
end
function M.hue(image,degrees,saturation,value,preserve)
  if degrees%360==0 and saturation==0 and value==0 then return Image(image) end
  local out=Image(image.spec)
  for p in out:pixels() do
    local pixelValue=image:getPixel(p.x,p.y)
    local c=Color{r=app.pixelColor.rgbaR(pixelValue),g=app.pixelColor.rgbaG(pixelValue),
      b=app.pixelColor.rgbaB(pixelValue),a=app.pixelColor.rgbaA(pixelValue)}
    if c.alpha>0 and not (preserve and c.hsvSaturation<0.05) then
      c.hsvHue=(c.hsvHue+degrees)%360
      c.hsvSaturation=math.max(0,math.min(1,c.hsvSaturation+saturation/100))
      c.hsvValue=math.max(0,math.min(1,c.hsvValue+value/100))
    end
    p(c.rgbaPixel)
  end
  return out
end
-- Replace only selected pixels in the active cel, retaining off-canvas/raw RGB.
function M.replace(sprite,bounds,image,name)
  assert(sprite.colorMode==ColorMode.RGB,"Convert to RGB first")
  local layer=assert(app.layer,"Select an image layer")
  assert(layer.isEditable and not layer.isReference and not layer.isTilemap,"Select an unlocked image layer")
  local cel=app.cel
  local x=math.min(bounds.x,cel and cel.position.x or bounds.x)
  local y=math.min(bounds.y,cel and cel.position.y or bounds.y)
  local w=math.max(bounds.x+bounds.width,cel and cel.position.x+cel.image.width or 0)-x
  local h=math.max(bounds.y+bounds.height,cel and cel.position.y+cel.image.height or 0)-y
  assert(w*h<=16777216,"Edited cel exceeds 16 million pixels")
  local out=Image(w,h,ColorMode.RGB)
  if cel then for p in cel.image:pixels() do out:putPixel(p.x+cel.position.x-x,p.y+cel.position.y-y,p()) end end
  for p in image:pixels() do
    local px,py=bounds.x+p.x,bounds.y+p.y
    if sprite.selection.isEmpty or sprite.selection:contains(Point(px,py)) then out:putPixel(px-x,py-y,p()) end
  end
  app.transaction(tr(name),function()
    if cel then cel.image=out; cel.position=Point(x,y) else sprite:newCel(layer,app.frame,out,Point(x,y)) end
  end)
  app.refresh()
end
return M
