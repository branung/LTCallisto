-- clientscript_mod (Blood II): a mod's Mod\ClientScript.lua.
-- It draws a panel and a line of text over the HUD every frame, and says what
-- it was given the first time, so a console log and a screenshot both show it.
-- The panel is half way down the left edge, clear of the console's lines.
frames = 0

function OnClientStart() print('start', 'api', mod.api) end

function OnWorldStart(world) print('world', world) end

function OnDraw(w, h)
  frames = frames + 1
  if frames == 1 then print('first draw', w, h) end
  mod.hud.rect(16, h / 2, 300, 36, 0, 0, 128)
  mod.hud.text(28, h / 2 + 10, 'CLIENT SCRIPT ' .. frames, 255, 255, 0)
end

function OnCommand(name, ...) print('command', name, ...) end
