project(Test_Commands)

# LT1Mod's CommandMgr port against NOLF1's original NOLF/ObjectDLL/CommandMgr.cpp and the
# engine's own tokenizer (runtime/shared/src/conparse.cpp), with stub/ standing in for the
# engine under both. The originals are compiled from copies in the build tree, because a quoted
# include looks beside the including file first, where the real headers stub/ replaces sit.
#
# NOLF1's copy gets two mechanical edits: the stub's header case, and const on the pointers it
# takes from the SDK's const ConParse::m_Args. The logic is untouched.
set(nolf1_dir ${CMAKE_CURRENT_SOURCE_DIR}/../../NOLF/ObjectDLL)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
	${nolf1_dir}/CommandMgr.cpp
	${nolf1_dir}/CommandMgr.h)
file(READ ${nolf1_dir}/CommandMgr.cpp nolf1_cpp)
string(REPLACE "#include \"stdafx.h\"" "#include \"Stdafx.h\"" nolf1_cpp "${nolf1_cpp}")
string(REGEX REPLACE "([\t ])char\\* (p(ObjectNames|Msg|Cmd1|Cmd2|Id)[\t ]*=)" "\\1const char* \\2" nolf1_cpp "${nolf1_cpp}")
file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/nolf1/CommandMgr.cpp "${nolf1_cpp}")
file(READ ${nolf1_dir}/CommandMgr.h nolf1_h)
string(REGEX REPLACE "\tchar\\*\t(p(Cmd|Id);)" "\tconst char*\t\\1" nolf1_h "${nolf1_h}")
file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/nolf1/CommandMgr.h "${nolf1_h}")
configure_file(../../runtime/shared/src/conparse.cpp ${CMAKE_CURRENT_BINARY_DIR}/engine/conparse.cpp COPYONLY)
configure_file(../../runtime/shared/src/conparse.h ${CMAKE_CURRENT_BINARY_DIR}/engine/conparse.h COPYONLY)

add_executable(${PROJECT_NAME}
	main.cpp
	${CMAKE_CURRENT_BINARY_DIR}/nolf1/CommandMgr.cpp
	${CMAKE_CURRENT_BINARY_DIR}/engine/conparse.cpp)
target_include_directories(${PROJECT_NAME} PRIVATE
	stub
	${CMAKE_CURRENT_BINARY_DIR}/nolf1
	${CMAKE_CURRENT_BINARY_DIR}/engine
	../../libs/LT1Mod)
target_compile_definitions(${PROJECT_NAME} PRIVATE _CRT_SECURE_NO_WARNINGS)
if(NOT MSVC)
	# NOLF1's CommandMgr binds string literals to char *, compiled as shipped
	target_compile_options(${PROJECT_NAME} PRIVATE -Wno-write-strings)
endif()
target_link_libraries(${PROJECT_NAME} LIB_LT1Mod)
set_target_properties(${PROJECT_NAME} PROPERTIES OUTPUT_NAME testCommands)
