#include "SArc.hpp"
#include "SArc/BlockEncoder.hpp"
#include "SArc/Helpers.hpp"
#include "SArc/TermColour.hpp"

#include <CLI/CLI.hpp>

#include <filesystem>
#include <ranges>

using namespace SArc;

SArchiveMemory create_archive_signed(const std::filesystem::path &in_folder,
									 const std::filesystem::directory_options dir_options,
									 const std::string &pgp_sign_fingerprint, const std::filesystem::path &pgp_sign_key,
									 const std::string &pgp_sign_passphrase) {
	SArchiveMemory archive;

	SARC_RUNTIME_ASSERT(pgp_sign_fingerprint.length() == 40, std::invalid_argument,
						"PGP fingerprint must be 40 characters long (20 hex bytes).");

	bytes_t key_data = helpers::read_file(pgp_sign_key);

	pgp_fingerprint_t fingerprint{};
	for (size_t i = 0; i < 20; ++i) {
		std::string byte_str = pgp_sign_fingerprint.substr(i * 2, 2);
		fingerprint[i] = static_cast<uint8_t>(std::stoi(byte_str, nullptr, 16));
	}

	archive.sign(key_data, pgp_sign_passphrase, fingerprint);

	std::cout << STC_GREEN << "Loaded PGP signing key." << STC_RESET << std::endl;

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

	return archive;
}

file_block_map_t map_files_for_block_encoder(const uint32_t file_count, const uint32_t target_block_size,
											 const std::filesystem::path &in_folder,
											 const std::filesystem::directory_options dir_options) {
	std::vector<std::string> file_paths;
	std::vector<uint32_t> file_sizes;

	file_paths.reserve(file_count);
	file_sizes.reserve(file_count);

	for (const auto &entry : std::filesystem::recursive_directory_iterator(in_folder, dir_options)) {
		if (!entry.is_regular_file()) continue;
		std::string entry_path = std::filesystem::relative(entry.path(), in_folder).generic_string();
		std::ranges::replace(entry_path, '\\', '/');

		file_paths.push_back(entry_path);
		file_sizes.push_back(std::filesystem::file_size(entry));
	}

	return helpers::auto_mappings(file_paths, file_sizes, target_block_size, {});
}

struct CompressionAlogrithmAndLevel {
	std::string algorithm;
	uint8_t level;
};

CompressionAlogrithmAndLevel
get_compression_algorithm_and_level_from_combined_string(const std::string &combined_string) {
	const size_t colon_pos = combined_string.find(':');
	SARC_RUNTIME_ASSERT(colon_pos != std::string::npos, std::invalid_argument,
						"Colon ':' missing from algorithm string");

	return {combined_string.substr(0, colon_pos),
			static_cast<uint8_t>(std::stoi(combined_string.substr(colon_pos + 1)))};
}

int main(const int argc, char *argv[]) {
	CLI::App app;

	std::filesystem::path in_folder = ".";
	app.add_option("input", in_folder, "Input Folder")->check(CLI::ExistingDirectory);

	std::filesystem::path out_file = "out.sarc";
	app.add_option("output", out_file, "Output File");

	uint32_t target_block_size = 128 << 20;
	app.add_option("-b", target_block_size, "Target block size")
		->transform(CLI::AsSizeValue(true))
		->default_str("128MiB");

	std::string compression_algorithm = "lzma:5";
	app.add_option("-c", compression_algorithm, "Compression algorithm (lzma|lz4) and level\nalg:level");

	bool follow_symlinks = false;
	app.add_flag("--symlinks", follow_symlinks, "Follow symlinks")->default_str("false");

	std::filesystem::path pgp_sign_key;
	app.add_option("--pgp-sign", pgp_sign_key, "PGP signing key")->check(CLI::ExistingFile);

	std::string pgp_sign_fingerprint{};
	app.add_option("--pgp-sign-fp", pgp_sign_fingerprint, "PGP signing key fingerprint");

	std::string pgp_sign_passphrase{};
	app.add_option("--pgp-sign-ps", pgp_sign_passphrase, "PGP signing key passphrase");

	CLI11_PARSE(app, argc, argv);

	const auto [algorithm, level] = get_compression_algorithm_and_level_from_combined_string(compression_algorithm);
	const uint8_t compression_level = level;
	compression_algorithm = algorithm;

	std::filesystem::directory_options dir_options{};

	if (follow_symlinks) dir_options |= std::filesystem::directory_options::follow_directory_symlink;

	try {
		if (!pgp_sign_fingerprint.empty()) {
			SArchiveMemory archive =
				create_archive_signed(in_folder, dir_options, pgp_sign_fingerprint, pgp_sign_key, pgp_sign_passphrase);

			std::ofstream out{out_file, std::ios::binary};
			if (!out) throw io_error("Failed to open output file.");

			archive.serialise_to_stream(LZMA, compression_level, out, target_block_size, {}, print_progress_callback);

			if (!out) throw io_error("Failed to write to output file.");

			std::cout << STC_GREEN << "Written archive to " << STC_BOLDGREEN << out_file << STC_RESET << std::endl;

			return 0;
		}

		size_t file_count = 0;
		for (const auto &entry : std::filesystem::recursive_directory_iterator(in_folder, dir_options))
			if (entry.is_regular_file()) file_count++;

		if (file_count > UINT32_MAX) throw std::runtime_error("File count above UINT32_MAX.");

		const file_block_map_t block_map =
			map_files_for_block_encoder(file_count, target_block_size, in_folder, dir_options);

		uint32_t block_count = 0;
		for (const auto block : block_map | std::views::values)
			if (block >= block_count) block_count = block + 1;

		std::ofstream out{out_file, std::ios::binary};
		if (!out) throw io_error("Failed to open output file.");

		BlockEncoder block_encoder{out};

		uint32_t i = 0;
		for (uint32_t block = 0; block < block_count; block++) {
			block_encoder.start_block();

			for (const auto &[file, file_block] : block_map)
				if (file_block == block) block_encoder.block_add_file_path(file);

			for (const auto &[file, file_block] : block_map) {
				if (file_block != block) continue;
				block_encoder.block_add_file_data(helpers::read_file(in_folder / file));
				std::cout << std::format("[" STC_BLUE "{}/{}" STC_RESET "] ", ++i, file_count) << file << std::endl;
			}

			std::printf("[%.2f%%] Compressing block %u/%u\n",
						static_cast<float>(block) / static_cast<float>(block_count + 1), block + 1, block_count);
			block_encoder.end_block();
		}

		std::cout << "[100.00%] Serialised all blocks\n"
				  << STC_GREEN << "Written archive to " << STC_BOLDGREEN << out_file << STC_RESET << std::endl;
	} catch (std::exception &e) {
		std::cerr << STC_RED << e.what() << STC_RESET << std::endl;
		return 1;
	}

	return 0;
}
