-- Mod\ClientScript.lua: each player's own script, an outline of every event it can answer.
--
-- Copy this file to your mod's Mod\ClientScript.lua and fill in the events you need. Delete
-- the ones you don't: each one left defined is still called, OnDraw on every frame. Copied as
-- it is, the file changes nothing in the game.
--
-- The client's script is for what a player sees and presses: it draws over the HUD, plays
-- sounds, and answers the player's own commands. It decides nothing in the game and sees no
-- objects. What happens in the game belongs in Mod\ServerScript.lua (an outline of it is
-- beside this file), and the two talk only through messages.
--
-- WHAT RUNS WHEN
--   The top level of this file runs once, when the game loads the mod, and its globals last
--   for the whole session, across every level. There is no mod.save here.
--   An error in an event prints its line and turns that event off until the next world.
--
-- THE SANDBOX
--   The same as the server's: Lua 5.4's base, string, table, math, utf8 and coroutine
--   libraries, with no file access. OnDraw may run 1,000,000 instructions a frame.
--
-- THE API, version 1: what this file can call.
--   mod.print(...)              a line on the console
--   mod.time()                  the game's clock, in seconds
--   mod.world  mod.api          this world's name, and the API version the game provides
--   mod.require_api(n)          refuse to run on a game older than version n
--   mod.random(lo, hi)          a random integer in lo..hi; mod.random() is one in [0, 1)
--   mod.after(s, fn)  mod.every(s, fn) -> timer; timer:cancel()
--   mod.set_update_rate(hz)     calls OnUpdate hz times a second, 0 (off) to 10
--   mod.hud.text(x, y, text [, r, g, b])   text on the screen, inside OnDraw only
--   mod.hud.rect(x, y, w, h, r, g, b)      a filled rectangle, inside OnDraw only
--   mod.sound(path)             a sound for this player alone
--   mod.server(name [, payload])   a message to the server's script (OnClientMessage there).
--                               A name is 1 to 64 characters and a payload at most 1024;
--                               past 20 a second, messages are dropped and this answers false.
--   require(name)               a module from Mod\Scripts, as on the server; each side runs
--                               its own copy
--
-- CHECK IT before playing: tools/modtool.py check <your mod's folder> runs this file the way
-- the game does and names any call this side does not have.

-- This is version 1 of the scripting API, the first. A later game adds to it and still runs
-- this script; asking for a version stops the script from running on a game older than it needs.
mod.require_api(1)


-- The game has loaded the mod. Runs once, before the first world.
function OnClientStart()
end

-- This player has entered a world. `world` is the level's name ("03_museum").
function OnWorldStart(world)
end

-- Called hz times a second once mod.set_update_rate(hz) has been called, and never before.
-- `dt` is the time since the last call.
function OnUpdate(dt)
end

-- Every frame, after the game's HUD is drawn, with the screen's width and height. The only
-- place mod.hud.text and mod.hud.rect work. For example:
--   mod.hud.text(w // 2 - 40, h - 60, "Hello", 255, 255, 255)
function OnDraw(w, h)
end

-- The player typed `script <name> [args...]` at the console, or pressed a key bound to that
-- command. The arguments arrive as strings.
function OnCommand(name, ...)
end

-- The server's script sent mod.client(player, name, payload) to this player, or to everyone.
function OnServerMessage(name, payload)
end
