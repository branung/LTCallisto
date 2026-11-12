# message_mod: messages between a mod's two scripts

`Mod/Script.lua` and `Mod/ClientScript.lua` together. A second after the
client enters a world it sends `hello`; the server answers that player with
`welcome <name>` and every player with `all everyone`. `script hud` asks the
server for a HUD line on this player's screen alone, and `script spam` sends 30
messages at once, of which the rate lets 20 through. What it should print:

```
ClientScript: hello sent	true
Script: from	<player>	hello	<world>
Script: answered	true	true
ClientScript: got	welcome	<player>
ClientScript: got	all	everyone
ClientScript: hud asked	true            (after `script hud`)
Script: hud sent
SCRIPT HUD LINE FOR ONE PLAYER           (this player's console only)
Mod\ClientScript.lua: this client sent more than 20 messages a second: 1 dropped so far
ClientScript: spam sent	20                (after `script spam`)
Script: spam	20	received	<n>
```

No `spam 30` line: the server never sees the ten the client held back.

Single player: mount this folder last (`-rez tests/Blood2Tables/message_mod`)
and type `script hud` at the console. Blood II's single player sends the
server no player name, so `<player>` is empty there. With two clients on a
dedicated server, both mounting it, each hello is answered by name and each
`all` reaches every client then in the game.
