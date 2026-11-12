project(Test_Blood2Tables)

# Blood II's mod-overridable tables against retail. Compiles the game's own tables
# (Blood2/Shared/WeaponDefs.cpp, which both modules run) and the BloodMod*.cpp loaders. Needs no
# engine and no game data, and builds without the two modules.
set(BLOOD2TABLES_SRC
	main.cpp
	../../Blood2/Shared/WeaponDefs.cpp
	../../Blood2/Shared/BloodModTable.cpp
	../../Blood2/Shared/BloodModWeapons.cpp)
set(BLOOD2TABLES_INC
	# Before DECompat: the stub server_de.h, scalar types only
	stub
	../../Blood2/Shared
	../../sdk/inc
	../../libs/DECompat)
add_executable(${PROJECT_NAME} ${BLOOD2TABLES_SRC})

target_include_directories(${PROJECT_NAME} PRIVATE ${BLOOD2TABLES_INC})

if(LINUX)
	# The game modules' Linux spellings (Blood2/ObjectDLL/CMakeLists.txt)
	target_compile_options(${PROJECT_NAME} PRIVATE -fpermissive)
endif()
# The example mod, which the selftest loads as the shells would
target_compile_definitions(${PROJECT_NAME} PRIVATE
	BLOOD2_BALANCE_MOD="${CMAKE_CURRENT_SOURCE_DIR}/balance_mod/Mod/Weapons.txt")
target_link_libraries(${PROJECT_NAME} LIB_LT1Mod)
set_target_properties(${PROJECT_NAME} PROPERTIES OUTPUT_NAME testBlood2Tables)
