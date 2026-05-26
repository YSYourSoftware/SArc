#include "SArc.hpp"
#include "SArc/Streaming.hpp"
#include "SArc/TermColour.hpp"

#include <CLI/CLI.hpp>

#include <fstream>
#include <string>

#include "SArc/Helpers.hpp"

using namespace SArc;

int main(const int argc, char *argv[]) {
	CLI::App app;
	app.require_subcommand(0, 1);

	std::filesystem::path in_file;
	app.add_option("input", "Input File")->check(CLI::ExistingFile)->required();

	CLI::App *script_verify_signature = app.add_subcommand(
		"script-verify-signature",
		"Verify a signature and return the result as the exit code.\n0: Valid, -1: Invalid, 1: Other Error");

	std::filesystem::path public_key;
	std::string public_key_fingerprint;

	script_verify_signature->add_option("public-key", public_key, "PGP public key")
		->check(CLI::ExistingFile)
		->required();
	script_verify_signature->add_option("public-key-fp", public_key_fingerprint, "PGP public key fingerprint")
		->required();

	CLI11_PARSE(app, argc, argv);

	//try {
		std::fstream file{in_file, std::ios::binary};

		if (script_verify_signature->parsed()) {
			SARC_RUNTIME_ASSERT(public_key_fingerprint.length() == 40, std::invalid_argument,
								"PGP fingerprint must be 40 characters long (20 hex bytes).");

			bytes_t public_key_bytes = helpers::read_file(public_key);

			try {
				SArchiveStream archive{file, public_key_bytes, helpers::hex_str_to_fingerprint(public_key_fingerprint)};
			} catch (invalid_signature &e) {
				std::cerr << STC_RED << e.what() << STC_RESET << std::endl;
				return 2;
			}

			return 0;
		}

		file.clear();
		SArchiveStream archive{file};

		std::cout << STC_BOLDBLUE << archive.get_all_paths().size() << STC_BLUE << " files" << STC_RESET << std::endl;
	/*} catch (std::exception &e) {
		std::cerr << STC_RED << e.what() << STC_RESET << std::endl;
		return 1;
	}*/

	return 0;
}
