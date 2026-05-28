#include "SArc.hpp"
#include "SArc/BlockEncoder.hpp"
#include "SArc/Helpers.hpp"

#include <fstream>
#include <iostream>

using namespace SArc;

int main() {
	SArchiveMemory archive;

	const auto data = "test data blah blah blah blah 1234567890 what a wonderful day it is on the 9th may 2026 "
					  "when sam sits inside writing this code...\nUpdate on the 13th may!\nUpdate again on the "
					  "fourteenth wow i love debugging!!!";

	const auto data1 = "another test this is. another test for another file. let's see if this one reads "
					   "properly\nloooooooooooooaaaaaaaaaaaaaaaaaaadddddddddddddddddsssssssssssssssss "
					   "oooooooooooooooooooooofffffffffffffffffffffff "
					   "rrrrrrrrrrrrrreeeeeeeeeeeeeeppppppppppppppppppppeeeeeeeeeeeeeeeeaaaaaaaaaaaaaaaaaattttttttt"
					   "ttttteeeeeeeeeeeeeeddddddddddd dddddddddddddaaaaaaaaaaaatttttttttttttttttaaaaaaaaaaa";

	{
		SArchiveFile &file = archive.create_file("test.txt");
		file.data.resize(std::strlen(data));
		std::memcpy(file.data.data(), data, std::strlen(data));

		SArchiveFile &file1 = archive.create_file("test1.txt");
		file1.data.resize(std::strlen(data1));
		std::memcpy(file1.data.data(), data1, std::strlen(data1));
	}

	constexpr pgp_fingerprint_t pgp_fp{0x40, 0x3B, 0xE7, 0x73, 0x73, 0x48, 0xB7, 0x60, 0x01, 0xEC,
									   0xCF, 0xCC, 0x3D, 0x5F, 0x1B, 0x95, 0xEC, 0xB4, 0xBE, 0x90};

	{
		bytes_t secret_key = helpers::read_file("secret.asc");
		archive.sign(std::span(secret_key), "password", pgp_fp);

		std::ofstream out("test.sarc", std::ios::binary);
		archive.serialise_to_stream(9, out, UINT32_MAX, {}, print_progress_callback);
	}

	{
		std::ifstream reopen("test.sarc", std::ios::binary);
		SArchiveMemory archive_reopen{reopen, helpers::read_file("public.asc"), pgp_fp};

		SARC_RUNTIME_ASSERT(
			std::memcmp(data, archive_reopen["test.txt"]->data.data(), archive_reopen["test.txt"]->data.size()) == 0,
			std::runtime_error, "File content not equal to written");

		SARC_RUNTIME_ASSERT(
			std::memcmp(data1, archive_reopen["test1.txt"]->data.data(), archive_reopen["test1.txt"]->data.size()) == 0,
			std::runtime_error, "File content not equal to written");
	}

	{
		std::ofstream out("blockencode.sarc", std::ios::binary);
		BlockEncoder block_encoder{out};

		block_encoder.start_block();
		block_encoder.block_add_file_path("test.txt");
		block_encoder.block_add_file_path("test1.txt");

		bytes_t file_data;

		file_data.resize(std::strlen(data));
		std::memcpy(file_data.data(), data, std::strlen(data));

		block_encoder.block_add_file_data(file_data);

		file_data.resize(std::strlen(data1));
		std::memcpy(file_data.data(), data1, std::strlen(data1));

		block_encoder.block_add_file_data(file_data);

		block_encoder.end_block();
	}

	return 0;
}
