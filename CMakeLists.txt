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

if(MSVC)
	target_compile_options(SArcHelpers PRIVATE
		$<$<CONFIG:Debug>:/Z7 /Od>
		$<$<CONFIG:Release>:/O2>
		/permissive- /volatile:iso /EHsc /Zc:__cplusplus /Zc:__STDC__ /Zc:throwingNew
	)
endif()

add_subdirectory(lib)

target_sources(SArcHelpers PRIVATE
	src/Helpers.cpp
)