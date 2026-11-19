-- Scripting step 1: Blood II's server runs a mod's script.  Mounted
-- with ../archetype_mod/Mod (RedCultist, RedLeader) on retail 01_SubwayA as
-- archetype_mod/map.json patches it; build it with
--     MOD_EXTRA=script_mod archetype_mod/build.sh out.rez
-- Each archetype AI is handed to OnCreate, which prints it and gives it 50
-- hit points; Commands.txt's start command reaches OnCommand.
count = 0

function OnCreate(obj)
    count = count + 1
    obj:set_health(50)
    print("created", obj:name(), obj:archetype(), obj:class(), obj:health())
end

function OnWorldStart(world)
    print("world", world, "archetypes", count, "api", mod.api)
end

function OnCommand(name, ...)
    print("command", name, ...)
end

function OnDeath(obj)
    print("died", obj:name(), obj:archetype())
end
