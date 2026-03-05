if(SARC_BUILD_EXEC)
	set(CLI11_TESTING OFF)
	add_subdirectory(CLI11)

	target_link_libraries(SArcC PRIVATE CLI11::CLI11)
	target_include_directories(SArcC PRIVATE CLI11/include)
	
	target_link_libraries(SArcU PRIVATE CLI11::CLI11)
	target_include_directories(SArcU PRIVATE CLI11/include)
endif()