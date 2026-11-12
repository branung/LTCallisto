-- message_mod: the server's half of a script message round trip.
-- Each client message is printed with the player
-- it came from; a hello is answered to that player alone and to every
-- player, and a hud puts a line on that player's screen.
received = 0

function OnClientMessage(player, name, payload)
  received = received + 1
  if name == 'spam' then
    if payload == '1' or payload == '20' or payload == '30' then print('spam', payload, 'received', received) end
    return
  end
  print('from', player:name(), name, payload)
  if name == 'hello' then
    print('answered', mod.client(player, 'welcome', player:name()), mod.client(nil, 'all', 'everyone'))
  end
  if name == 'hud' then
    mod.hud(player, 'SCRIPT HUD LINE FOR ONE PLAYER')
    print('hud sent')
  end
end
