project(Test_ButeMerge)

# CButeMgr::Merge, a mod's bute overlay: a self-test on text, and every retail
# bute file in a folder merged onto itself with nothing changed
add_executable(${PROJECT_NAME} main.cpp)
target_compile_definitions(${PROJECT_NAME} PRIVATE _NOMFC)
if(MSVC)
	target_compile_definitions(${PROJECT_NAME} PRIVATE _SILENCE_STDEXT_HASH_DEPRECATION_WARNINGS)
endif(MSVC)
target_include_directories(${PROJECT_NAME} PRIVATE
	../../libs/ButeMgr
	../../libs/MFCStub
	../../libs/CryptMgr
	../../libs/stdlith
	../../libs/lith)
target_link_libraries(${PROJECT_NAME}
	LIB_ButeMgr
	LIB_CryptMgr
	LIB_MFCStub
	LIB_StdLith
	LIB_Lith)
set_target_properties(${PROJECT_NAME}
	PROPERTIES OUTPUT_NAME testButeMerge)
