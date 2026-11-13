-- Copyright (c) 2026 fresh-milkshake. MIT license; see LICENSE.txt.
local i18n = require "minecraft_tools_i18n"
local core = require "minecraft_tools_core"
local exporter = require "minecraft_tools_export"
local sheet = require "minecraft_tools_sheet"
local Dialog, alert, tr = i18n.dialog, i18n.alert, i18n.tr
local assert = i18n.assert
local M = {}

local function pathKey(path)
  local normalized = app.fs.normalizePath(path)
  return app.fs.pathSeparator == "\\" and normalized:lower() or normalized
end

local function absolutePath(path)
  if app.fs.pathSeparator == "\\" then
    return path:match("^%a:[/\\]") or path:match("^[/\\][/\\]")
  end
  return path:sub(1, 1) == "/"
end

local function exportRegion(sprite)
  if not sprite.selection.isEmpty then
    return core.cell(sprite)
  end
  return sprite.bounds
end

local function regionKey(bounds)
  return string.format("%d,%d,%d,%d", bounds.x, bounds.y, bounds.width, bounds.height)
end

local function outputPath(sprite, filename)
  assert(filename and absolutePath(filename) and filename:match("[^/\\]$"),
         "Choose a full path for the PNG file")
  local extension = app.fs.fileExtension(filename):lower()
  if extension == "" then filename = filename .. ".png" end
  assert(extension == "" or extension == "png", "Choose a PNG file")
  filename = app.fs.normalizePath(filename)
  assert(app.fs.isDirectory(app.fs.filePath(filename)), "Choose an existing destination folder")
  assert(not sprite.hasAssociatedFile or pathKey(filename) ~= pathKey(sprite.filename),
         "Choose a destination different from the open source file")
  return filename
end

local function writePng(sprite, frame, bounds, filename, overwrite)
  local image = core.capture(sprite, frame, bounds)
  if not sprite.selection.isEmpty then
    image = sheet.mask(sprite, bounds, image)
  end
  exporter.write({ { path = filename, image = image } }, overwrite)
end

local unsaved = {}

local function targetsForSprite(prefs, sprite)
  local transient = unsaved[sprite.id]
  if not sprite.hasAssociatedFile then
    transient = transient or {}
    unsaved[sprite.id] = transient
    return transient
  end

  -- Extension preferences serialize table keys without escaping backslashes.
  local key = pathKey(sprite.filename):gsub(".", function(byte)
    return string.format("%02x", byte:byte())
  end)
  prefs.png_targets = prefs.png_targets or {}
  local saved = prefs.png_targets[key] or {}
  if transient then
    for region, filename in pairs(transient) do saved[region] = filename end
    unsaved[sprite.id] = nil
  end
  prefs.png_targets[key] = saved
  return saved
end

local function chooseFile(prefs, sprite, bounds, previous)
  local folder = prefs.png_folder or app.fs.filePath(sprite.filename)
  if not absolutePath(folder) or not app.fs.isDirectory(folder) then folder = app.fs.userDocsPath end
  local name = app.fs.fileTitle(sprite.filename)
  if not sprite.selection.isEmpty then
    name = core.assetName(sprite, bounds)
  end
  local dialog = Dialog("Export Sprite as PNG")
  dialog:file {
    id = "file", label = "PNG file:", title = "Choose PNG Destination",
    filename = previous or app.fs.joinPath(folder, name .. ".png"),
    save = true, entry = true, filetypes = { "png" }
  }
  dialog:label { text = sprite.selection.isEmpty and "No selection: export the entire canvas."
                  or "Aseprite remembers this path for the selected region." }
  dialog:button { id = "export", text = "Export", focus = true }
  dialog:button { text = "Cancel" }
  dialog:show { autoscrollbars = true }
  if dialog.data.export then return dialog.data.file end
end

function M.run(prefs, chooseDestination)
  local sprite = assert(app.sprite, "Open or create a sprite first")
  local frame = app.frame
  local bounds = exportRegion(sprite)
  local key = regionKey(bounds)
  local targets = targetsForSprite(prefs, sprite)
  local previous = targets[key]
  local filename = previous
  local destinationAvailable = previous and app.fs.isDirectory(app.fs.filePath(previous))
  if chooseDestination or not destinationAvailable then
    filename = chooseFile(prefs, sprite, bounds, destinationAvailable and previous or nil)
    if not filename then return end
  end
  filename = outputPath(sprite, filename)
  local overwrite = previous ~= nil and pathKey(filename) == pathKey(previous)
  if app.fs.isFile(filename) and not overwrite then
    if alert { title = "Replace PNG?", text = filename, buttons = { "Replace", "Cancel" } } ~= 1 then
      return
    end
    overwrite = true
  end
  writePng(sprite, frame, bounds, filename, overwrite)
  -- A newly assigned destination takes over from any older binding to that file.
  local destination = pathKey(filename)
  local function unbindOtherRegions(regions)
    for region, path in pairs(regions) do
      if (regions ~= targets or region ~= key) and pathKey(path) == destination then
        regions[region] = nil
      end
    end
  end
  for _, regions in pairs(prefs.png_targets or {}) do unbindOtherRegions(regions) end
  for _, regions in pairs(unsaved) do unbindOtherRegions(regions) end
  targets[key] = filename
  prefs.png_folder = app.fs.filePath(filename)
  app.tip(tr("PNG exported: %s", app.fs.fileName(filename)))
end

return M
