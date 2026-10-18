find_package(OpenSSL CONFIG REQUIRED)
target_link_libraries(SArcHelpers PRIVATE
	OpenSSL::Crypto
)

target_sources(SArcHelpers PRIVATE
	ccrc/crc_32.c

	lz4/lib/lz4.c
	lz4/lib/lz4hc.c

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

target_include_directories(SArcHelpers PRIVATE
	ccrc
	lz4/lib
	lzma
)