#include "SArc/Streaming.hpp"

using namespace SArc;

SArchiveStream::SArchiveStream(std::istream &stream, const size_t size) : m_stream(stream), m_size(size) {}