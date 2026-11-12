-- Scripting step 4: version 4 in Blood II, on retail 01_SubwayA with
-- ../archetype_mod/Mod, patched by archetype_mod/scripttrigger_map.json (its
-- map.json and a ScriptTrigger named st1 whose Function is poke):
--     MAP=scripttrigger_map.json MOD_EXTRA=script4_mod archetype_mod/build.sh out.rez
-- Commands.txt's start command triggers st1; OnWorldStart spawns a RedLeader
-- beside herm.
function poke(msg, from)
    print("poked", msg, from)
end

function OnWorldStart(world)
    local herm = mod.find("herm")
    local x, y, z = herm:pos()
    local s = mod.spawn("RedLeader", x + 48, y, z, "Name leader2")
    print("spawned", s:name(), s:archetype(), s:class(), s == mod.find("leader2"), s:health())
end
