# stopwatch_mod: an example of a mod's two scripts

The example to start a scripted mod from: `Mod/Script.lua` and
`Mod/ClientScript.lua`, with every line commented. The two scripts are the
same in Shogo and in Blood 2; only `Mod/Mod.txt` names the game.

- The **server** counts each player's deaths, and tells that player's client
  when they join and each time they die.
- The **client** draws a panel above the HUD with the time in this level and
  the deaths. The console command `script reset`, which you can bind to a
  key, asks the server to start again; the server answers on your screen
  alone with `Stopwatch: started again`.

It changes nothing else and needs no patched level. `tools/modtool.py check`
passes it with no warning.
