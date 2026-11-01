cmake_minimum_required(VERSION 3.15)

project(SArcHelpers
	VERSION 2.0.0
	DESCRIPTION "JUST SArc Helpers"
	LANGUAGES CXX C
)

add_library(SArcHelpers STATIC)
target_include_directories(SArcHelpers PUBLIC include)
target_compile_features(SArcHelpers PUBLIC cxx_std_23)
target_compile_definitions(SArcHelpers PRIVATE OPENSSL_NO_DEPRECATED SARC_IN_SARC_BUILD)

option(SARC_SHARED "Build SArc as a shared library rather than a static library" OFF)

if(SARC_SHARED)
	target_compile_definitions(SArcHelpers PUBLIC SARC_SHARED)
endif()

if(MSVC)
	target_compile_options(SArcHelpers PRIVATE
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
		target_link_libraries(SArcHelpers PRIVATE "${IPP_LIBRARIES}")
		target_compile_definitions(SArcHelpers PRIVATE SARC_USE_IPP "${IPP_DEFINITIONS}")
		target_include_directories(SArcHelpers PRIVATE "${IPP_INCLUDE_DIRS}")
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
		target_link_libraries(SArcHelpers PUBLIC "-framework Accelerate")
		target_compile_definitions(SArcHelpers PUBLIC SARC_USE_AAC)
	endif()
endif()

add_subdirectory(lib)

target_sources(SArcHelpers PRIVATE
	src/Helpers.cpp
)