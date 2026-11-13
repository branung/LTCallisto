project(Test_Blood2Tables)

# Blood II's mod-overridable tables against retail. Compiles the game's own tables
# (Blood2/Shared/WeaponDefs.cpp and CharacterValues.cpp, which the modules run) and the
# BloodMod*.cpp loaders. Needs no engine and no game data, and builds without the two modules.
set(BLOOD2TABLES_SRC
	main.cpp
	characters.cpp
	ai.cpp
	ammo.cpp
	projectiles.cpp
	weaponclasses.cpp
	rules.cpp
	explosions.cpp
	lasers.cpp
	objectfx.cpp
	pickups.cpp
	strings.cpp
	../../Blood2/Shared/WeaponDefs.cpp
	../../Blood2/Shared/BloodModTable.cpp
	../../Blood2/Shared/BloodModWeapons.cpp
	../../Blood2/Shared/CharacterValues.cpp
	../../Blood2/Shared/BloodModCharacters.cpp
	../../Blood2/Shared/AmmoValues.cpp
	../../Blood2/Shared/BloodModAmmo.cpp
	../../Blood2/Shared/BloodModLasers.cpp
	../../Blood2/Shared/BloodModObjectFX.cpp
	../../Blood2/Shared/BloodModRules.cpp
	../../Blood2/Shared/BloodModExplosions.cpp
	../../Blood2/ObjectDLL/BloodModAI.cpp
	../../Blood2/ObjectDLL/BloodModProjectiles.cpp
	../../Blood2/ObjectDLL/BloodModWeaponClasses.cpp
	../../Blood2/ObjectDLL/BloodModPickups.cpp
	../../Blood2/Shared/BloodModStrings.cpp)
set(BLOOD2TABLES_INC
	# Before DECompat: the stub server_de.h, scalar types only
	stub
	../../Blood2/Shared
	../../Blood2/ObjectDLL
	../../sdk/inc
	../../libs/DECompat)
# The example mods, which the selftest loads as the shells would, and where the string module is
# relative to this binary (OUT/<config>/Games/<game>)
set(BLOOD2TABLES_DEFS
	BLOOD2_BALANCE_MOD="${CMAKE_CURRENT_SOURCE_DIR}/balance_mod/Mod/Weapons.txt"
	BLOOD2_CHARACTERS_MOD="${CMAKE_CURRENT_SOURCE_DIR}/characters_mod/Mod/Characters.txt"
	BLOOD2_AI_MOD="${CMAKE_CURRENT_SOURCE_DIR}/ai_mod/Mod/AI.txt"
	BLOOD2_OBJECTDLL_DIR="${CMAKE_CURRENT_SOURCE_DIR}/../../Blood2/ObjectDLL"
	BLOOD2_STRINGS_MOD="${CMAKE_CURRENT_SOURCE_DIR}/strings_mod/Mod/Strings.txt"
	BLOOD2_CLIENTRES_H="${CMAKE_CURRENT_SOURCE_DIR}/../../Blood2/Shared/ClientRes.h")

add_executable(${PROJECT_NAME} ${BLOOD2TABLES_SRC})
target_include_directories(${PROJECT_NAME} PRIVATE ${BLOOD2TABLES_INC})
if(LINUX)
	# The game modules' Linux spellings (Blood2/ObjectDLL/CMakeLists.txt)
	target_compile_options(${PROJECT_NAME} PRIVATE -fpermissive)
endif()
target_compile_definitions(${PROJECT_NAME} PRIVATE ${BLOOD2TABLES_DEFS} BLOOD2_GAME_DIR="Games/Blood2")
target_link_libraries(${PROJECT_NAME} LIB_LT1Mod ${CMAKE_DL_LIBS})
set_target_properties(${PROJECT_NAME} PROPERTIES OUTPUT_NAME testBlood2Tables)

# The Nightmare Levels' tables, when that build is on: _ADD_ON renumbers the weapons and adds four,
# and four characters, so both tables and both name lists take another shape. The same selftest,
# both spellings as Blood2/ObjectDLL/CMakeLists.txt explains. No reference file: the mod format is
# the base game's.
if(BUILD_BLOOD2_ADDON)
	add_executable(Test_Blood2TablesNM ${BLOOD2TABLES_SRC})
	target_include_directories(Test_Blood2TablesNM PRIVATE ${BLOOD2TABLES_INC})
	if(LINUX)
		target_compile_options(Test_Blood2TablesNM PRIVATE -fpermissive)
	endif()
	target_compile_definitions(Test_Blood2TablesNM PRIVATE _ADDON _ADD_ON ${BLOOD2TABLES_DEFS}
		BLOOD2_GAME_DIR="Games/Blood2NM")
	target_link_libraries(Test_Blood2TablesNM LIB_LT1Mod ${CMAKE_DL_LIBS})
	set_target_properties(Test_Blood2TablesNM PROPERTIES OUTPUT_NAME testBlood2TablesNM)
endif()
