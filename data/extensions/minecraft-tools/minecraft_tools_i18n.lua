-- Copyright (c) 2026 fresh-milkshake. MIT license; see LICENSE.txt.
-- Presentation adapter: widget IDs, resource IDs, paths and enum values stay
-- canonical. Only displayed text passes through Aseprite's native string table.
local catalog=package.loaded["minecraft-tools/minecraft_tools_catalog"] or require "minecraft_tools_catalog"
local M={}
local nativeDialog,nativeAlert,nativeAssert,nativeError=Dialog,app.alert,assert,error
local templates,prefixes={},{}
local function escape(s) return (s:gsub("([%(%)%.%%%+%-%*%?%[%]%^%$])","%%%1")) end
for text,id in pairs(catalog) do
  if text:find("%%[ds]") then
    local pattern,types,position="^",{},1
    while true do
      local a,b,kind=text:find("%%([ds])",position)
      if not a then pattern=pattern .. escape(text:sub(position)) .. "$";break end
      pattern=pattern .. escape(text:sub(position,a-1)) .. (kind=="d" and "(%-?%d+)" or "(.*)")
      types[#types+1]=kind;position=b+1
    end
    templates[#templates+1]={source=text,id=id,pattern=pattern,types=types}
  elseif text:match(": $") or text=="Cannot save " then prefixes[#prefixes+1]=text end
end
table.sort(templates,function(a,b) return #a.source>#b.source end)
table.sort(prefixes,function(a,b) return #a>#b end)
function M.key(text) return catalog[text] end
function M.tr(text,...)
  if type(text)~="string" then return text end
  local id=catalog[text]
  local translated
  if id then
    translated=app.translate(id)
    -- INI values trim outer whitespace; restore deliberate separators.
    local left,right=text:match("^(%s*)"),text:match("(%s*)$")
    if left~="" and translated:sub(1,#left)~=left then translated=left .. translated end
    if right~="" and translated:sub(-#right)~=right then translated=translated .. right end
  else
    for _,item in ipairs(templates) do
      local values={text:match(item.pattern)}
      if #values>0 then
        for i,kind in ipairs(item.types) do if kind=="d" then values[i]=tonumber(values[i]) end end
        return string.format(app.translate(item.id),table.unpack(values))
      end
    end
    for _,prefix in ipairs(prefixes) do
      if text:sub(1,#prefix)==prefix then return M.tr(prefix) .. text:sub(#prefix+1) end
    end
    for _,face in ipairs({"North","South","East","West","Up","Down"}) do
      if text:sub(1,#face+2)==face .. ": " then return M.tr(face) .. text:sub(#face+1) end
    end
    return text -- User filenames, identifiers and external technical details.
  end
  if select('#',...)>0 then return string.format(translated,...) end
  return translated
end
function M.assert(value,message)
  return nativeAssert(value,M.tr(message))
end
function M.error(message,level) nativeError(M.tr(message),(level or 1)+1) end
function M.alert(options)
  if type(options)=="string" then return nativeAlert(M.tr(options)) end
  local copy={};for k,v in pairs(options) do copy[k]=v end
  copy.title=M.tr(copy.title)
  if type(copy.text)=="table" then
    copy.text={};for i,v in ipairs(options.text) do copy.text[i]=M.tr(v) end
  else copy.text=M.tr(copy.text) end
  if copy.buttons then copy.buttons={};for i,v in ipairs(options.buttons) do copy.buttons[i]=M.tr(v) end end
  return nativeAlert(copy)
end
function M.dialog(options)
  local config=options
  if type(options)=="string" then config=M.tr(options)
  elseif type(options)=="table" then
    config={};for k,v in pairs(options) do config[k]=v end
    config.title=M.tr(config.title)
  end
  local native=nativeDialog(config)
  local schema,reverse={},{}
  local proxy={}
  local function translatedOptions(method,options)
    if type(options)~="table" then return options end
    local copy={};for k,v in pairs(options) do copy[k]=v end
    local kind=method=="modify" and schema[copy.id] or method
    if copy.id and method~="modify" then schema[copy.id]=method end
    if copy.label then copy.label=M.tr(copy.label) end
    if copy.title then copy.title=M.tr(copy.title) end
    if copy.text and kind~="entry" and kind~="number" then copy.text=M.tr(copy.text) end
    if kind=="combobox" then
      if copy.options then
        copy.options={};local lookup={}
        for i,value in ipairs(options.options) do
          local display=M.tr(value);copy.options[i]=display;lookup[display]=value
        end
        reverse[copy.id]=lookup
      end
      if copy.option then copy.option=M.tr(copy.option) end
    end
    return copy
  end
  return setmetatable(proxy,{
    __index=function(_,key)
      if key=="data" then
        local data=native.data
        for id,lookup in pairs(reverse) do if data[id]~=nil then data[id]=lookup[data[id]] or data[id] end end
        return data
      end
      local value=native[key]
      if type(value)=="function" then return function(_,options)
        local result=value(native,translatedOptions(key,options))
        return result==native and proxy or result
      end end
      return value
    end,
    __newindex=function(_,key,value) native[key]=value end})
end
return M
