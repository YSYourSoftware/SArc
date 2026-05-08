#pragma once

#include "SArc.hpp"

namespace SArc {
	SARC_ADD_RUNTIME_ERROR(stream_not_supported);

	class SArchiveStream : public SArchive {
		public:
			SArchiveStream(std::istream &stream, std::streamsize size);
		private:
			std::istream &m_stream;
			size_t m_size;
	};
}