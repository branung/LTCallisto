-- message_mod: the client's half.  It says hello
-- a second after it enters a world, prints what the server sends, asks for
-- a HUD line on `script hud`, and on `script spam` sends 30 messages at once,
-- of which the rate lets 20 go.

-- A second in, not at once: a multiplayer Blood 2 server learns the player's
-- name from the client's own init, which follows the client into the world.
-- (Its single player sends none, so there the name is "".)
function OnWorldStart(world)
  mod.after(1, function() print('hello sent', mod.server('hello', world)) end)
end

function OnServerMessage(name, payload) print('got', name, payload) end

function OnCommand(name)
  if name == 'hud' then print('hud asked', mod.server('hud')) end
  if name == 'spam' then
    local sent = 0
    for i = 1, 30 do if mod.server('spam', tostring(i)) then sent = sent + 1 end end
    print('spam sent', sent)
  end
end
