-- Scripting step 3: what lasts through a save, in Blood II.  Mounted
-- with ../archetype_mod/Mod on retail 01_SubwayA (MOD_EXTRA=script3_mod
-- archetype_mod/build.sh out.rez).  New Game, quicksave, then load it: the
-- restored game raises OnRestore with mod.save as it was, its reference to
-- herm found again, and the named timer back with the time it had left.
mod.require_api(3)

function ring()
    print("ring", mod.save.visits)
end

function OnWorldStart(world)
    mod.save.visits = (mod.save.visits or 0) + 1
    mod.save.herm = mod.find("herm")
    mod.save.note = { "saved", world }
    mod.after(60, ring)
    print("start", world, "visits", mod.save.visits, "herm", mod.save.herm ~= nil)
end

function OnRestore(world)
    print("restore", world, "visits", mod.save.visits, "herm", mod.save.herm == mod.find("herm"),
          "note", mod.save.note[1], mod.save.note[2])
end
