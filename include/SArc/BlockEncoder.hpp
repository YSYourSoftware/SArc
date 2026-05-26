#pragma once

#include "SArc.hpp"

namespace SArc {
	SARC_ADD_RUNTIME_ERROR(state_error);

	class BlockEncoder {
	public:
		explicit BlockEncoder(std::ostream &stream);

		void start_block();

		void block_set_file_count(uint8_t count);
		void block_add_file_path(const std::string &path);

		void block_add_file_data(const byte_span_const_t &data);

		void end_block();
	private:
		enum State : uint8_t {
			OUT_OF_BLOCK = 0,
			IN_BLOCK_HEADERS = 1,
			IN_BLOCK_FILE_DATA = 2
		};

		std::ostream &m_stream;

		State m_state = OUT_OF_BLOCK;
		uint8_t m_block_target_file_count;
		uint8_t m_block_written_file_count;

		const std::istream::pos_type m_block_count_offset;
		std::istream::pos_type m_block_size_headers_offset;
	};
}