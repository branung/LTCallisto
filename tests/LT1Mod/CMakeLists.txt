project(Test_LT1Mod)

# libs/LT1Mod's self-test, and --sig for checking the fixture's signature against
# tools/lt1mod_sig.py. Needs no engine and no game data.
add_executable(${PROJECT_NAME} main.cpp)
target_link_libraries(${PROJECT_NAME} LIB_LT1Mod)
set_target_properties(${PROJECT_NAME}
	PROPERTIES OUTPUT_NAME testLT1Mod)
