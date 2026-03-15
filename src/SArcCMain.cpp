#include "SArc.hpp"

#include <CLI/CLI.hpp>

#include <string>

using namespace SArc;

int main(int argc, char* argv[]) {
	CLI::App app;

	std::string in_folder = ".";
	app.add_option("-i", in_folder, "Input Folder")->default_str(in_folder);

	std::string out_file = "out.sarc";
	app.add_option("-o", out_file, "Output File")->default_str(out_file);

	CLI11_PARSE(app, argc, argv);

	return 0;
}