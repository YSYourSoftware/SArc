#include <CLI/CLI.hpp>

#include <string>

int main(int argc, char* argv[]) {
	CLI::App app;

	std::string inFolder;
	app.add_option("-if", inFolder, "Input Folder")->required();

	CLI11_PARSE(app, argc, argv);

	return 0;
}