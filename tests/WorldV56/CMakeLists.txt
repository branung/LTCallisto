project(Test_WorldV56)

# Regression test for the world converter (v56 and v66)

set(exec_src
	main.cpp
	../../runtime/world/src/world_read_v56.cpp
	../../runtime/world/src/world_read_v66.cpp
	../../runtime/world/src/world_write_v85.cpp)

include_directories(
	${CMAKE_SOURCE_DIR}/runtime/world/src
	${CMAKE_SOURCE_DIR}/sdk/inc)

add_executable(${PROJECT_NAME} ${exec_src})

set_target_properties(${PROJECT_NAME}
	PROPERTIES OUTPUT_NAME testWorldV56)
