-- stopwatch_mod, the server's half: an example of a mod's two scripts
-- working together.
--
-- The server keeps the score, because only the server decides what
-- happened: it counts each player's deaths and tells that player's client.
-- The client (ClientScript.lua) only shows it.  Copy both files to start a
-- mod of your own.

-- Version 1 of the scripting API, the first.  A game older than the version
-- a script asks for refuses it and says so, instead of failing at the first
-- call it does not have.
mod.require_api(1)

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
