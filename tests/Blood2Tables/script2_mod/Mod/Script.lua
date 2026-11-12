-- Scripting step 2: version 2 in Blood II, on retail 01_SubwayA with
-- ../archetype_mod/Mod (RedCultist, RedLeader); build it with
--     MOD_EXTRA=script2_mod archetype_mod/build.sh out.rez
-- The player's events, a reference kept from OnCreate and found again by
-- name, and a timer two seconds in that kills every RedCultist through the
-- game's own damage path -- OnDamage and OnDeath in every run.
cultists = {}

function OnCreate(obj)
    cultists[obj] = true
end

function OnPlayerJoin(p)  print("joined", p:name(), p:is_player()) end
function OnPlayerSpawn(p) print("spawned", p:name(), #mod.players()) end

function OnWorldStart(world)
    local herm = mod.find("herm")
    print("v2", world, "api", mod.api, "herm kept", herm ~= nil and cultists[herm] == true,
          "objects", #mod.objects("RedCultist"))
    mod.after(2.0, function()
        for _, c in ipairs(mod.objects("RedCultist")) do
            c:damage(500)
        end
    end)
end

function OnDamage(obj, amount) print("damaged", obj:name(), amount) end
function OnDeath(obj) print("killed", obj:name(), obj:archetype()) end
