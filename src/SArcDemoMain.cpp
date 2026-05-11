#include "SArc.hpp"
#include "Sarc/Helpers.hpp"

#include <fstream>

using namespace SArc;

int main() {
	SArchiveMemory archive;

	{
		const auto data = "test data blah blah blah blah 1234567890 what a wonderful day it is on the 9th may 2026 when sam sits inside writing this code...";
		SArchiveFile &file = archive.create_file("test.txt");
		file.data.resize(std::strlen(data));
		std::memcpy(file.data.data(), data, std::strlen(data));
	}

	bytes_t secret_key = helpers::read_file("secret.asc");

	archive.sign(std::span(secret_key), "password", {0x40, 0x3B, 0xE7, 0x73, 0x73, 0x48, 0xB7, 0x60, 0x01, 0xEC,
													 0xCF, 0xCC, 0x3D, 0x5F, 0x1B, 0x95, 0xEC, 0xB4, 0xBE, 0x90});


	std::ofstream out("test.sarc");
	out << archive;

	return 0;
}
