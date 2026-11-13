-- Actual Open File command route, not direct model manipulation.
local root = app.fs.joinPath(app.fs.currentPath,"gui-results","fixtures")
assert(app.fs.makeAllDirectories(root))
for i=1,16 do
  local s = i==16 and Sprite(512,256) or Sprite(16,16)
  local image = Image(s.spec)
  for p in image:pixels() do
    local n = (p.x * 31 + p.y * 13 + i * 7) % 30
    if i==16 then
      local cx,cy=p.x//16,p.y//16
      local visible=(cx+cy)%3==0 and p.x%16>2 and p.x%16<13 and p.y%16>2 and p.y%16<13
      local border=p.x%16==3 or p.x%16==12 or p.y%16==3 or p.y%16==12
      p(visible and (border and app.pixelColor.rgba(8,8,12,255) or app.pixelColor.rgba(60+cx*5,70+cy*9,210-cx*4,255)) or 0)
    else p(app.pixelColor.rgba(45 + n,105 + i*5 + n,70 + n,255)) end
  end
  s.cels[1].image = image
  s:saveAs(app.fs.joinPath(root,string.format("texture-%02d.aseprite",i)))
  s:close()
end
for i=1,16 do
  app.command.OpenFile { filename=app.fs.joinPath(root,string.format("texture-%02d.aseprite",i)) }
  assert(app.sprite)
  app.sprite:close()
end
