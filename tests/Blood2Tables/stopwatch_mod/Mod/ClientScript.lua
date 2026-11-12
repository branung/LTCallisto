-- stopwatch_mod, the client's half (see Script.lua): a panel above the HUD
-- with the time in this level and the deaths the server counted.  The
-- console command `script reset`, which a player can bind to a key, starts
-- both again.

mod.require_api(5)

-- The client's own state lasts the whole session; OnWorldStart resets it.
local started = mod.time()
local deaths = '0'

function OnWorldStart(world)
  started = mod.time()
  deaths = '0'
end

-- What the server's script sent: only it knows how many times we died.
function OnServerMessage(name, payload)
  if name == 'deaths' then deaths = payload end
end

function OnCommand(name)
  if name == 'reset' then
    started = mod.time()
    mod.server('reset')
  end
end

-- Every frame, over the game's HUD.  Drawing works only in here.
function OnDraw(w, h)
  local t = math.floor(mod.time() - started)
  local text = string.format('%d:%02d    deaths %s', t // 60, t % 60, deaths)
  mod.hud.rect(w // 2 - 110, h - 150, 220, 30, 0, 0, 0)
  mod.hud.text(w // 2 - 98, h - 142, text, 255, 255, 255)
end
