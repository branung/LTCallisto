project(LIB_LT1Mod)

# Data driven mods for the LithTech 1.0 games: the Mod\Mod.txt manifest and its hash, the active
# mod, save paths, attribute files on ButeMgr, and the registry template the games' tables use.
# Game modules include only lt1mod*.h; ButeMgr and MFCStub stay inside lt1mod_attrib.cpp.
add_library(${PROJECT_NAME} STATIC
	lt1mod.cpp
	lt1mod_attrib.cpp
	lt1mod_command.cpp
	lt1mod_engine.cpp
	lt1mod_startcmds.cpp)

# ButeMgr's headers want _NOMFC and see MFCStub, CryptMgr, stdlith and lith
target_compile_definitions(${PROJECT_NAME} PRIVATE _NOMFC)
if(MSVC)
	target_compile_definitions(${PROJECT_NAME} PRIVATE _SILENCE_STDEXT_HASH_DEPRECATION_WARNINGS)
endif(MSVC)
target_include_directories(${PROJECT_NAME} PRIVATE
	../ButeMgr
	../MFCStub
	../CryptMgr
	../stdlith
	../lith)
target_include_directories(${PROJECT_NAME} PUBLIC
	.
	../../sdk/inc)

# stdlith's allocator hooks in their own archive, linked after LIB_StdLith: GNU ld takes a member
# only for symbols already unresolved, and nothing asks for them until stdlith is pulled in
add_library(LIB_LT1ModAlloc STATIC lt1mod_alloc.cpp)
target_include_directories(LIB_LT1ModAlloc PRIVATE ../stdlith ../lith)

target_link_libraries(${PROJECT_NAME}
	LIB_ButeMgr
	LIB_CryptMgr
	LIB_MFCStub
	LIB_StdLith
	LIB_LT1ModAlloc
	LIB_Lith)

if(LINUX)
	set_target_properties(${PROJECT_NAME} PROPERTIES COMPILE_FLAGS "-fPIC")
	set_target_properties(LIB_LT1ModAlloc PROPERTIES COMPILE_FLAGS "-fPIC")
endif(LINUX)
