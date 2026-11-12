-- stopwatch_mod, the server's half: an example of a mod's two scripts
-- working together.
--
-- The server keeps the score, because only the server decides what
-- happened: it counts each player's deaths and tells that player's client.
-- The client (ClientScript.lua) only shows it.  Copy both files to start a
-- mod of your own.

-- Messages came in version 5: an older game refuses this file and says so,
-- instead of failing at the first mod.client.
mod.require_api(5)

-- A player's deaths, kept by the player object.  Like every global here it
-- lasts one level: the script starts fresh in each.  (mod.save lasts longer.)
deaths = {}

-- Tells one player its count, as text: a message carries strings only.
local function tell(player)
  mod.client(player, 'deaths', tostring(deaths[player] or 0))
end

function OnPlayerJoin(player)
  tell(player)
end

function OnPlayerDeath(player)
  deaths[player] = (deaths[player] or 0) + 1
  tell(player)
end

-- The client asks; the server decides.  A message is a request, never an
-- order, so a player with a modified game can only ask for its own reset.
function OnClientMessage(player, name, payload)
  if name == 'reset' then
    deaths[player] = 0
    tell(player)
    mod.hud(player, 'Stopwatch: started again')
  end
end
