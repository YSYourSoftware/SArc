cmake_minimum_required(VERSION 3.15)

project(SArc
	VERSION 2.0.0
	DESCRIPTION "Sam's Archive Format (SArc) support in C/C++"
	LANGUAGES CXX C
)

option(SARC_BUILD_EXEC "Build SArcTool executables" ON)
option(SARC_SHARED "Build SArc as a shared library rather than a static library" OFF)

add_library(SArc STATIC)
target_include_directories(SArc PUBLIC include)
target_compile_features(SArc PUBLIC cxx_std_23)
target_compile_definitions(SArc PRIVATE OPENSSL_NO_DEPRECATED SARC_IN_SARC_BUILD)

if(SARC_SHARED)
	target_compile_definitions(SArc PUBLIC SARC_SHARED)
endif()

if(MSVC)
	target_compile_options(SArc PRIVATE
		$<$<CONFIG:Debug>:/Z7 /Od>
		$<$<CONFIG:Release>:/O2>
		/permissive- /volatile:iso /EHsc /Zc:__cplusplus /Zc:__STDC__ /Zc:throwingNew
	)
endif()

option(SARC_LINK_PERF_EXTENSIONS "Link SArc to IPP or Apple Accelerate to speed up compression." ON)

if(SARC_LINK_PERF_EXTENSIONS)
	set(IPP_SHARED ON)
	find_package(IPP QUIET)

	if(IPP_FOUND)
		message(STATUS "Linking to IPP")
		target_link_libraries(SArc PRIVATE "${IPP_LIBRARIES}")
		target_compile_definitions(SArc PRIVATE SARC_USE_IPP "${IPP_DEFINITIONS}")
		target_include_directories(SArc PRIVATE "${IPP_INCLUDE_DIRS}")
	else()
		message(STATUS "IPP not found")
		if(WIN32 OR UNIX)
			message(NOTICE "IPP was not found, but likely can be installed.
Installing IPP would significantly speed up compression.
You can get it from https://www.intel.com/content/www/us/en/developer/tools/oneapi/ipp.html")
		endif()
	endif()

	if(APPLE)
		message(STATUS "Linking to Apple Accelerate")
		target_link_libraries(SArc PUBLIC "-framework Accelerate")
		target_compile_definitions(SArc PUBLIC SARC_USE_AAC)
	endif()
endif()

add_executable(SArcDemo src/SArcDemoMain.cpp)
target_link_libraries(SArcDemo PRIVATE SArc)

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

target_sources(SArc PRIVATE
	src/BlockEncoder.cpp
	src/Helpers.cpp
	src/SArchive.cpp
	src/SArchiveFile.cpp
	src/SArchiveCrypto.cpp
	src/SArchiveStream.cpp
)