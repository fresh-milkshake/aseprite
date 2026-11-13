-- This file is released under the terms of the MIT license.
-- Read LICENSE.txt for more information.

local previousSetting = app.preferences.undo.allow_nonlinear_history
for _, legacySetting in ipairs{ false, true } do
  app.preferences.undo.allow_nonlinear_history = legacySetting
  local sprite = Sprite(1, 1)
  local function expectSize(size)
    assert(sprite.width == size and sprite.height == size)
  end

  for _, size in ipairs{ 2, 3, 4 } do sprite:resize(size, size) end
  expectSize(4)
  app.command.Undo()
  expectSize(3)
  app.command.Undo()
  expectSize(2)
  app.command.Redo()
  expectSize(3)
  app.command.Undo()
  expectSize(2)

  -- A new edit discards sizes 3 and 4 from the redo branch.
  sprite:resize(5, 5)
  assert(sprite.undoHistory.undoSteps == 2)
  assert(sprite.undoHistory.redoSteps == 0)
  app.command.Redo()
  expectSize(5)
  app.command.Undo()
  expectSize(2)
  app.command.Undo()
  expectSize(1)
  app.command.Undo()
  expectSize(1)
  app.command.Redo()
  expectSize(2)
  app.command.Redo()
  expectSize(5)
  app.command.Redo()
  expectSize(5)
  sprite:close()
end
app.preferences.undo.allow_nonlinear_history = previousSetting
