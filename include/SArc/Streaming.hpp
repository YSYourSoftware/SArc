#pragma once

#include "SArc.hpp"

namespace SArc {
	SARC_ADD_RUNTIME_ERROR(not_supported);

	class SArchiveStream : public SArchive {
		public:
			/**
			 * <summary>
			 * Initliase from serialised data in a stream
			 * </summary>
			 *
			 * @param stream Input stream of serialised data
			 * @param size Size of archive data in stream
			 */
			SArchiveStream(std::istream &stream, size_t size);

			bytes_t serialise(uint8_t compression_level, CompressStats *compression_stats=nullptr) {throw not_supported("SArchiveStream objects cannot be serialised\nUse SArchiveStream.copy_into_memory to get a normal SArchive object from a stream");}
			void serialise_to_stream(uint8_t compression_level, std::ostream &stream, CompressStats *compression_stats=nullptr) {throw not_supported("SArchiveStream object cannot be serialised\nUse SArchiveStream.copy_into_memory to get a normal SArchive object from a stream");};

			bool is_stream() {return true;}
		private:
			std::istream &m_stream;
			size_t m_size;
	};
}