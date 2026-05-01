cmake_minimum_required(VERSION 3.15)

project(SArc
	VERSION 1.0.1
	DESCRIPTION "Sam's Archive Format (SArc) support in C/C++"
	LANGUAGES CXX C
)

option(SARC_BUILD_EXEC "Build SArc and UnSArc executables" ON)

add_library(SArc STATIC)

target_include_directories(SArc PUBLIC include)

if(SARC_BUILD_EXEC)
	add_executable(SArcC src/SArcCMain.cpp)
	add_executable(SArcU src/SArcUMain.cpp)

	set_target_properties(SArcC PROPERTIES RUNTIME_OUTPUT_NAME "SArc")
	set_target_properties(SArcU PROPERTIES RUNTIME_OUTPUT_NAME "UnSArc")

	target_link_libraries(SArcC PRIVATE SArc)
	target_link_libraries(SArcU PRIVATE SArc)

	target_compile_features(SArcC PUBLIC cxx_std_23)
	target_compile_features(SArcU PUBLIC cxx_std_23)
endif()

add_subdirectory(lib)
add_subdirectory(src)

target_compile_features(SArc PUBLIC cxx_std_23)

if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
	target_compile_options(SArc PRIVATE
		/permissive-
		/W4
		/w14640
	)
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" || CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
	target_compile_options(SArc PRIVATE
		-Wall
		-Wextra
		-Wshadow
		-Wnon-virtual-dtor
		-pedantic
	)
endif()