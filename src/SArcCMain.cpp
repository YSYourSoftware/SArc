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

	uint32_t target_block_size = 128 << 20;
	app.add_option("-b", target_block_size, "Target block size")
		->transform(CLI::AsSizeValue(true))
		->default_str("128MiB");

	bool follow_symlinks = false;
	app.add_flag("--symlinks", follow_symlinks, "Follow symlinks")->default_str("false");

	std::filesystem::path pgp_sign_key;
	app.add_option("--pgp-sign", pgp_sign_key, "PGP signing key")->check(CLI::ExistingFile);

	std::string pgp_sign_fingerprint{};
	app.add_option("--pgp-sign-fp", pgp_sign_fingerprint, "PGP signing key fingerprint");

	std::string pgp_sign_passphrase{};
	app.add_option("--pgp-sign-ps", pgp_sign_passphrase, "PGP signing key passphrase");

	CLI11_PARSE(app, argc, argv);

	std::filesystem::directory_options dir_options{};

	if (follow_symlinks) dir_options |= std::filesystem::directory_options::follow_directory_symlink;

	try {
		SArchiveMemory archive;

		if (!pgp_sign_fingerprint.empty()) {
			SARC_RUNTIME_ASSERT(pgp_sign_fingerprint.length() == 40, std::invalid_argument,
								"PGP fingerprint must be 40 characters long (20 hex bytes).");

			std::ifstream key_file{pgp_sign_key, std::ios::binary};

			key_file.seekg(0, std::ios::end);
			const uint32_t key_size = key_file.tellg();
			key_file.seekg(0, std::ios::beg);

			bytes_t key_data{key_size};
			key_file.read(reinterpret_cast<char *>(key_data.data()), key_size);

			pgp_fingerprint_t fingerprint{};
			for (size_t i = 0; i < 20; ++i) {
				std::string byte_str = pgp_sign_fingerprint.substr(i * 2, 2);
				fingerprint[i] = static_cast<uint8_t>(std::stoi(byte_str, nullptr, 16));
			}

			archive.sign(key_data, pgp_sign_passphrase, fingerprint);

			std::cout << STC_GREEN << "Loaded PGP signing key." << STC_RESET << std::endl;
		}

		size_t file_count = 0;
		for (const auto &entry : std::filesystem::recursive_directory_iterator(in_folder, dir_options))
			if (entry.is_regular_file()) file_count++;

		if (file_count > UINT32_MAX) throw std::runtime_error("File count above UINT32_MAX.");

		uint32_t i = 0;
		for (const auto &entry : std::filesystem::recursive_directory_iterator(in_folder, dir_options)) {
			if (!entry.is_regular_file()) continue;
			std::string entry_path = std::filesystem::relative(entry.path(), in_folder).generic_string();
			std::ranges::replace(entry_path, '\\', '/');

			std::cout << std::format("[" STC_BLUE "{}/{}" STC_RESET "] ", ++i, file_count) << entry_path << std::endl;

			archive += SArchiveFile{entry.path()}.at(entry_path);
		}

		std::ofstream out{out_file, std::ios::binary};
		if (!out) throw io_error("Failed to open output file.");

		archive.serialise_to_stream(compression_level, out, target_block_size, {}, print_progress_callback);

		if (!out) throw io_error("Failed to write to output file.");

		std::cout << STC_GREEN << "Written archive to " << STC_BOLDGREEN << out_file << STC_RESET << std::endl;
	} catch (std::exception &e) {
		std::cerr << STC_RED << e.what() << STC_RESET << std::endl;
		return 1;
	}

	return 0;
}
