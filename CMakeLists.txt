cmake_minimum_required(VERSION 3.15)

project(SArc
	VERSION 2.0.0
	DESCRIPTION "Sam's Archive Format (SArc) support in C/C++"
	LANGUAGES CXX C
)

option(SARC_BUILD_EXEC "Build SArcTool executables" ON)

add_library(SArc STATIC)
target_include_directories(SArc PUBLIC include)
target_compile_features(SArc PUBLIC cxx_std_23)
target_compile_definitions(SArc PUBLIC OPENSSL_NO_DEPRECATED)

if(MSVC)
	target_compile_options(SArc PRIVATE
		$<$<CONFIG:Debug>:/Z7>
	)
endif()

add_executable(SArcDemo src/SArcDemoMain.cpp)
target_link_libraries(SArcDemo PRIVATE SArc)
target_compile_features(SArcDemo PUBLIC cxx_std_23)

if(SARC_BUILD_EXEC)
	add_executable(SArcC src/SArcCMain.cpp)
	add_executable(SArcU src/SArcUMain.cpp)
	add_executable(SArcInfo src/SArcInfoMain.cpp)

	set_target_properties(SArcC PROPERTIES RUNTIME_OUTPUT_NAME "SArc")
	set_target_properties(SArcU PROPERTIES RUNTIME_OUTPUT_NAME "UnSArc")

	target_link_libraries(SArcC PRIVATE SArc)
	target_link_libraries(SArcU PRIVATE SArc)
	target_link_libraries(SArcInfo PRIVATE SArc)
endif()

add_subdirectory(lib)
add_subdirectory(src)