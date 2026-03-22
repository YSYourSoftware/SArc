#include "SArc.hpp"

#include "SArc/TermColour.hpp"

#include <CLI/CLI.hpp>

#include <filesystem>

using namespace SArc;

int main(const int argc, char *argv[]) {
	CLI::App app;

	std::filesystem::path in_folder = ".";
	app.add_option("input", in_folder, "Input Folder")->check(CLI::ExistingDirectory);

	std::filesystem::path out_file = "out.sarc";
	app.add_option("output", out_file, "Output File");

	uint8_t compression_level = 5;
	app.add_option("-c", compression_level, "LZMA compression level (0-9)")->default_val(5);

	bool follow_symlinks = false;
	app.add_flag("--symlinks", follow_symlinks, "Follow symlinks")->default_str("false");

	CLI11_PARSE(app, argc, argv);

	std::filesystem::directory_options dir_options{};

	if (follow_symlinks) dir_options |= std::filesystem::directory_options::follow_directory_symlink;

	try {
		SArchive archive;

		size_t file_count = 0;
		for (const auto &entry : std::filesystem::recursive_directory_iterator(in_folder, dir_options)) if (entry.is_regular_file()) file_count++;

		if (file_count > UINT32_MAX) throw std::runtime_error("File count above UINT32_MAX");

		uint32_t i = 0;
		for (const auto &entry : std::filesystem::recursive_directory_iterator(in_folder, dir_options)) {
			if (!entry.is_regular_file()) continue;
			std::string entry_path = std::filesystem::relative(entry.path(), in_folder).string();
			std::ranges::replace(entry_path, '\\', '/');
			std::cout << std::format("[" STC_BLUE "{}/{}" STC_RESET "] ", ++i, file_count) << entry_path << std::endl;
			archive.add_file(SArchiveFile{entry.path()}, entry_path);
		}

		CompressStats compress_stats{};
		bytes_t data = archive.serialise(compression_level, &compress_stats);

		std::cout << STC_MAGENTA << std::format("Compression (LZMA level {}) saved {} bytes", compression_level, compress_stats.decompressed_size - compress_stats.compressed_size) << STC_RESET << std::endl;

		std::ofstream out(out_file, std::ios::binary);
		if (!out) throw io_error("Failed to open output file");

		out.write(reinterpret_cast<const char*>(data.data()), data.size());
		if (!out) throw io_error("Failed to write to output file");

		std::cout << STC_GREEN << "Written archive to " << STC_BOLDGREEN << out_file << STC_RESET << std::endl;
	} catch (std::exception &e) {
		std::cerr << STC_RED << e.what() << STC_RESET << std::endl;
		return 1;
	}

	return 0;
}