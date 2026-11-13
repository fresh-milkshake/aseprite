-- Copyright (c) 2026 fresh-milkshake. MIT license; see LICENSE.txt.
local M = {}
local channels = app.pixelColor
local pixel = channels.rgba

M.faces = {
  {"North", {.5,-.5,-.5}, {-1,0,0}, {0,1,0}, {0,0,-1}},
  {"South", {-.5,-.5,.5}, {1,0,0}, {0,1,0}, {0,0,1}},
  {"West", {-.5,-.5,-.5}, {0,0,1}, {0,1,0}, {-1,0,0}},
  {"East", {.5,-.5,.5}, {0,0,-1}, {0,1,0}, {1,0,0}},
  {"Up", {-.5,-.5,-.5}, {1,0,0}, {0,0,1}, {0,-1,0}},
  {"Down", {-.5,.5,.5}, {1,0,0}, {0,0,-1}, {0,1,0}},
}

function M.backdrop(scene, light)
  local width, height = 144, 96
  local image = Image(width, height, ColorMode.RGB)
  local cave = scene == "Stone cave"
  local dusk = light == "Dusk"
  local brightness = dusk and .60 or 1
  for p in image:pixels() do
    local x, y = p.x, p.y
    local noise = ((x * 17 ~ y * 41 ~ x * y * 3) % 13) - 6
    local r, g, b
    if cave then
      local seam = ((x + (y // 10 % 2) * 11) % 23 < 2 or y % 10 == 0) and -9 or 0
      local depth = math.max(0, 22 - math.abs(x - 72) // 3)
      if y < 70 then
        r, g, b = 53 + depth + seam, 53 + depth + seam, 60 + depth + seam
      else
        r, g, b = 108 + seam, 103 + seam, 101 + seam
      end
      if (x < 26 or x > 116) and y < 78 and (y > 9 or x < 15 or x > 129) then
        r, g, b = 71 + seam, 67 + seam, 68 + seam
      end
    elseif y >= 69 + (x // 18 % 3) then
      local grass = y < 84 or ((x // 4 + y) % 13 < 3 and y < 88)
      if grass then r, g, b = 99, 137, 62 else r, g, b = 116, 82, 52 end
      if y > 72 and y < 78 and x % 11 < 3 then r, g, b = 76, 112, 49 end
    else
      r, g, b = 130 + y // 3, 184 + y // 4, 223 + y // 4
      if light == "Overcast" then r, g, b = 141 + y // 4, 157 + y // 4, 170 + y // 4 end
      local ridge = 53 + ((x // 12 * 3) % 14)
      if y > ridge then r, g, b = 103, 146, 94 end
      if x >= 113 and x < 119 and y >= 38 and y <= 71 then r, g, b = 103, 72, 43 end
      if (x >= 101 and x < 131 and y >= 30 and y < 48) or
         (x >= 107 and x < 126 and y >= 22 and y < 54) then
        r, g, b = 53, 103, 48
      end
      if y >= 14 and y < 19 and x > 15 and x < 46 then r, g, b = 209, 226, 228 end
    end
    p(pixel(math.floor((r + noise) * brightness), math.floor((g + noise) * brightness),
            math.floor((b + noise) * brightness), 255))
  end
  return image
end

local function shade(light, normal, enabled)
  if not enabled then return 1 end
  if light == "Dusk" then
    return .48 + .28 * math.max(0, normal[1] * .85 - normal[2] * .25 + normal[3] * .42)
  elseif light == "Overcast" then
    return .73 + .19 * math.max(0, -normal[2] * .75 + normal[3] * .65)
  end
  return .59 + .41 * math.max(0, -normal[1] * .34 - normal[2] * .83 + normal[3] * .44)
end

function M.draw(gc, faces, view)
  local a, b = math.rad(view.yaw), math.rad(view.pitch)
  local ca, sa, cb, sb = math.cos(a), math.sin(a), math.cos(b), math.sin(b)
  local function rotate(v)
    local x, z = v[1]*ca + v[3]*sa, -v[1]*sa + v[3]*ca
    return {x, v[2]*cb - z*sb, v[2]*sb + z*cb}
  end
  local scale = math.min(gc.width, gc.height) * .47 * view.zoom / 100
  local centerX, centerY = gc.width / 2, gc.height / 2
  if view.environment ~= "Studio" then
    local bottom = (math.abs(cb) + math.abs(sb*sa) + math.abs(sb*ca)) / 2
    centerY = gc.height * .77 - bottom * scale
    if view.shading and view.pitch <= 0 then
      gc.color = Color{r=18,g=19,b=20,a=85}
      gc:beginPath()
      gc:oval(Rectangle(math.floor(centerX-scale*.56), math.floor(gc.height*.77-scale*.10),
                        math.ceil(scale*1.12), math.ceil(scale*.20)))
      gc:fill()
    end
  end

  for index, face in ipairs(M.faces) do
    local normal = rotate(face[5])
    if normal[3] > .001 then
      local source = faces[index].image
      local origin, u, v = rotate(face[2]), rotate(face[3]), rotate(face[4])
      local x0, y0 = centerX + origin[1]*scale, centerY + origin[2]*scale
      local ux, uy, vx, vy = u[1]*scale, u[2]*scale, v[1]*scale, v[2]*scale
      local left = math.max(0, math.floor(x0 + math.min(0,ux) + math.min(0,vx)))
      local top = math.max(0, math.floor(y0 + math.min(0,uy) + math.min(0,vy)))
      local right = math.min(gc.width, math.ceil(x0 + math.max(0,ux) + math.max(0,vx)))
      local bottom = math.min(gc.height, math.ceil(y0 + math.max(0,uy) + math.max(0,vy)))
      if right > left and bottom > top then
        local image = Image(right-left,bottom-top,ColorMode.RGB)
        local inv = 1 / (ux*vy - vx*uy)
        local factor = shade(view.light, normal, view.shading)
        for p in image:pixels() do
          local x, y = left+p.x+.5-x0, top+p.y+.5-y0
          local s, t = (x*vy-y*vx)*inv, (y*ux-x*uy)*inv
          if s >= 0 and s < 1 and t >= 0 and t < 1 then
            local value = source:getPixel(math.floor(s*source.width), math.floor(t*source.height))
            local edge = view.edges and math.min(s,1-s,t,1-t)*scale < 1
            if edge then
              p(pixel(238, 200, 122, 255))
            elseif factor == 1 then
              p(value)
            else
              p(pixel(math.floor(channels.rgbaR(value)*factor), math.floor(channels.rgbaG(value)*factor),
                      math.floor(channels.rgbaB(value)*factor), channels.rgbaA(value)))
            end
          end
        end
        gc.color = Color{r=255,g=255,b=255}
        gc:drawImage(image,left,top)
      end
    end
  end
end

return M
