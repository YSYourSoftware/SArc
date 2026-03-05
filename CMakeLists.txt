cmake_minimum_required(VERSION 3.23)

project(SArc
	VERSION 1.0.0
	DESCRIPTION "SArc Compression in C++"
	LANGUAGES CXX C
)

add_library(SArc STATIC)

option(SARC_BUILD_EXEC "Build SArcC and SArcU executables" ON)

if(SARC_BUILD_EXEC)
	add_executable(SArcC src/SArcCMain.cpp)
	add_executable(SArcU src/SArcUMain.cpp)

	target_link_libraries(SArcC PRIVATE SArc)
	target_link_libraries(SArcU PRIVATE SArc)
endif()

add_subdirectory(lib)
add_subdirectory(src)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

target_compile_features(SArc PRIVATE cxx_std_23)
target_compile_features(SArc PRIVATE c_std_17)

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