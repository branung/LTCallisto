-- Mod\ServerScript.lua: the server's script, an outline of every event it can answer.
--
-- Copy this file to your mod's Mod\ServerScript.lua and fill in the events you need. Delete
-- the ones you don't: each one left defined is still called, and an empty OnCommand quietly
-- answers every SCRIPT command, so a misspelled one is no longer reported. Copied as it is,
-- the file changes nothing in the game.
--
-- The server's script decides what happens: it sees the mod's objects and the players, and it
-- is the only side that can change them. What a player sees and presses belongs in
-- Mod\ClientScript.lua (an outline of it is beside this file).
--
-- WHAT RUNS WHEN
--   The top level of this file runs again for every world, in a fresh state, before the
--   world's objects are created. So every global here lasts one level. To keep something
--   across levels and in the player's saved games, put it in mod.save.
--   An error in an event prints its line and turns that event off until the next world.
--
-- WHICH OBJECTS THE EVENTS SEE
--   The players, and the AIs the mod names: an archetype of its own in Mod\AI.txt, or a retail
--   class there with Script TRUE. A retail object the mod did not name raises no event.
--
-- THE SANDBOX
--   Lua 5.4's base, string, table, math, utf8 and coroutine libraries, with no file access
--   (no io, os, package, debug, dofile, loadfile or load). math.random is the game's random.
--   Each call may run 10,000,000 instructions, and the script has 16 MB of memory.
--
-- THE API, version 1: what this file can call.
--   mod.print(...)              a line on the server console
--   mod.command(cmd)            runs a command string: "MSG door1 TRIGGER", "DELAY 2 ..."
--   mod.time()                  the game's clock, in seconds
--   mod.world  mod.api          this world's name, and the API version the game provides
--   mod.require_api(n)          refuse to run on a game older than version n
--   mod.find(name)              a live object by its level name, or nil
--   mod.objects([archetype])    the live mod objects (of that archetype), a list
--   mod.players()               the live players, a list
--   mod.after(s, fn)  mod.every(s, fn) -> timer; timer:cancel()
--   mod.random(lo, hi)          a random integer in lo..hi; mod.random() is one in [0, 1)
--   mod.hud(player | nil, text) a line on one player's screen, or on everyone's
--   mod.sound(path, x, y, z)    a one shot sound at a point
--   mod.set_update_rate(hz)     calls OnUpdate hz times a second, 0 (off) to 10
--   mod.save                    a table that lasts across worlds and into saved games:
--                               strings, numbers, booleans, tables and object references
--   mod.spawn(name, x, y, z [, props [, yaw]])   one of the mod's archetypes, or any class
--                               the game has, at a point; props is the engine's property
--                               string ("Name boss1;HitPoints 400"), yaw in degrees
--   mod.explode(x, y, z, radius, min, max [, effect [, source]])   each object in radius
--                               takes min..max damage, source left out; effect names the
--                               game's explosion ("DIVINE_SHOCKBALL")
--   mod.client(player | nil, name [, payload])   a message to that player's client script,
--                               or to every client (OnServerMessage there)
--   require(name)               a module: require("ec.revenants") runs
--                               Mod\Scripts\ec\revenants.lua once and returns its result
--   obj:valid()  obj:name()  obj:archetype()  obj:class()  obj:is_player()
--   obj:health()  obj:max_health()  obj:set_health(n)  obj:damage(n)  obj:pos() -> x, y, z
--   obj:send(msg)  obj:teleport(x, y, z)  obj:remove()
--
-- An object is a reference: the same object is the same value in every event, so it can be
-- kept in a variable or used as a table key. Once the game removes it, obj:valid() is false
-- and any other call on it is an error.
--
-- CHECK IT before playing: tools/modtool.py check <your mod's folder> runs this file the way
-- the game does and names any call this side does not have.

-- This is version 1 of the scripting API, the first. A later game adds to it and still runs
-- this script; asking for a version stops the script from running on a game older than it needs.
mod.require_api(1)

-- A module, from Mod\Scripts\ec\revenants.lua. Uncomment to use one:
-- local revenants = require("ec.revenants")


-- THE WORLD

-- A world starts: a new game, or a change of level. `world` is the level's name ("03_museum").
function OnWorldStart(world)
end

-- A saved game was restored, instead of OnWorldStart. mod.save holds what was saved, and each
-- object reference in it is found again, or is nil.
function OnRestore(world)
end

-- The world is about to end. The objects are still there to read.
function OnWorldEnd(world)
end

-- Called hz times a second once mod.set_update_rate(hz) has been called, and never before.
-- `dt` is the time since the last call.
function OnUpdate(dt)
end


-- THE MOD'S OBJECTS

-- One of the mod's objects was created. `placed` is true when the level placed it, false when
-- it was spawned while playing (by mod.spawn, for example).
function OnCreate(obj, placed)
end

-- An object lost `amount` hit points. `from` is the attacker if the script can see it, else
-- nil; `kind` is the damage type ("NORMAL", "FIRE", "MELEE", "EXPLODE", "ELECTRIC", ...) or
-- nil.
function OnDamage(obj, amount, from, kind)
end

-- An object died. `killer` and `kind` are as in OnDamage; `damage` is the killing blow's own
-- amount.
function OnDeath(obj, killer, kind, damage)
end

-- A trigger message reached one of the mod's objects. `from` is the sender's name, or "".
function OnTrigger(obj, msg, from)
end

-- An object is being removed. Only obj:name(), obj:archetype() and obj:class() still answer.
function OnRemove(obj)
end


-- THE PLAYERS

-- A player joined the game.
function OnPlayerJoin(player)
end

-- A player was put in the world: at each level's start, and on a respawn.
function OnPlayerSpawn(player)
end

-- A player died.
function OnPlayerDeath(player)
end

-- A player left the game.
function OnPlayerLeave(player)
end


-- COMMANDS AND MESSAGES

-- A command string ran SCRIPT <name> [args...]: from a Command object in a level, the mod's
-- Mod\Commands.txt, or mod.command. The arguments arrive as strings.
function OnCommand(name, ...)
end

-- A player's client script sent mod.server(name, payload). It is a request, never an order:
-- check it before acting on it, because a modified client can send anything.
function OnClientMessage(player, name, payload)
end


-- A ScriptTrigger placed in a level calls the global function its Function property names,
-- with the trigger message and the sender's name. Any name will do, for example:
--
-- function OpenVault(msg, from)
--   mod.command("MSG vault_door TRIGGER")
-- end
