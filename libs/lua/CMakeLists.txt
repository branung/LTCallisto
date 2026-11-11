project(LIB_Lua C)

# Lua 5.4.6, vendored unmodified, for a mod's Mod\Script.lua. The library only: lua.c and luac.c
# are left out, and the game never opens the io, os, package or debug libraries
file(GLOB LUA_SRC ${CMAKE_CURRENT_SOURCE_DIR}/src/*.c)
add_library(${PROJECT_NAME} STATIC ${LUA_SRC})
target_include_directories(${PROJECT_NAME} PUBLIC src)
# Linked into the games' shared modules
set_target_properties(${PROJECT_NAME} PROPERTIES POSITION_INDEPENDENT_CODE ON)
if(MSVC)
	target_compile_definitions(${PROJECT_NAME} PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()
