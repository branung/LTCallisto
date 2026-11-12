# clientscript_mod: a mod's `Mod\ClientScript.lua`

A client script alone: no table, no server script. It draws a dark-blue panel
half way down the left edge, with `CLIENT SCRIPT <frames>` in yellow, over
the HUD every frame, and prints what each event was given. What it should
print, in a level:

```
Mod\ClientScript.lua: handles OnClientStart, OnWorldStart, OnDraw, OnCommand
ClientScript: start	api	4
ClientScript: world	<the world's file name>
ClientScript: first draw	<width>	<height>
ClientScript: command	hello	world          (after the console's `script hello world`)
```

Mount this folder last (`-rez tests/Blood2Tables/clientscript_mod`) on retail and type
`script hello world` at the console; it needs no patched level.
