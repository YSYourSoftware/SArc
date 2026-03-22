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
	} catch (std::exception &e) {
		std::cerr << STC_RED << e.what() << STC_RESET << std::endl;
		return 1;
	}

	return 0;
}