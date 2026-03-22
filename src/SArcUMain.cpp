#include "SArc.hpp"

#include "SArc/TermColour.hpp"

#include <CLI/CLI.hpp>

#include <string>

using namespace SArc;

int main(const int argc, char *argv[]) {
	CLI::App app;

	std::filesystem::path in_file;
	app.add_option("input", in_file, "Input File")->required();

	std::filesystem::path out_folder = ".";
	app.add_option("output", out_folder, "Output Folder");

	CLI11_PARSE(app, argc, argv);

	try {
		SArchive archive(in_file);

		size_t file_count = 0;
		for (const auto &filepath : archive.get_all_paths()) file_count++;

		uint32_t i = 0;
		for (const auto &filepath : archive.get_all_paths()) {
			SArchiveFile &file = archive.get_file_by_path(filepath);

			std::ofstream out(out_folder / filepath, std::ios::binary);
			if (!out) throw io_error("Failed to open output file");

			out.write(reinterpret_cast<const char*>(file.data.data()), file.data.size());
			if (!out) throw io_error("Failed to write to output file");

			std::cout << std::format("[" STC_BLUE "{}/{}" STC_RESET "] ", ++i, file_count) << filepath << std::endl;
		}

		std::cout << STC_GREEN << "Extracted archive to " << STC_BOLDGREEN << out_folder << STC_RESET << std::endl;
	} catch (std::exception &e) {
		std::cerr << STC_RED << e.what() << STC_RESET << std::endl;
		return 1;
	}

	return 0;
}