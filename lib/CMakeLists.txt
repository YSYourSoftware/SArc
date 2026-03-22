if(SARC_BUILD_EXEC)
	set(CLI11_TESTING OFF)
	add_subdirectory(CLI11)

	target_link_libraries(SArcC PRIVATE CLI11::CLI11)
	target_include_directories(SArcC PUBLIC CLI11/include)
	
	target_link_libraries(SArcU PRIVATE CLI11::CLI11)
	target_include_directories(SArcU PUBLIC CLI11/include)
endif()

target_include_directories(SArc PUBLIC utfcpp/source)

target_sources(SArc PUBLIC
	ccrc/arccrc16.c
	ccrc/checkexe.c
	ccrc/checksum.c
	ccrc/crc_16.c
	ccrc/crc_16f.c
	ccrc/crc_32.c
	lzma/Alloc.c
	lzma/CpuArch.c
	lzma/LzFind.c
	lzma/LzFindMt.c
	lzma/LzFindOpt.c
	lzma/LzmaDec.c
	lzma/LzmaEnc.c
	lzma/LzmaLib.c
	lzma/Threads.c
)

target_include_directories(SArc PUBLIC
	ccrc
	lzma
)