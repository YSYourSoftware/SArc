#include "SArc.hpp"

#include <CLI/CLI.hpp>

#include <string>

using namespace SArc;

int main(int argc, char* argv[]) {
	CLI::App app;

	std::string in_file;
	app.add_option("-i", in_file, "Input File")->required();

	std::string out_folder = ".";
	app.add_option("-o", out_folder, "Output Folder")->default_str(".");

	CLI11_PARSE(app, argc, argv);

	return 0;
}