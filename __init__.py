from __future__ import annotations

from enum import IntEnum as _ienum
from io import BufferedReader as _bufReader

import deflate as _deflate
import pyppmd as _ppm
import lzma as _lzma
import zstandard as _zstd
import lz4.frame as _lz4
import bz2 as _bzip2

from crc import Crc32 as _crc32
from crc import Calculator as _crcCalc

class SArcCompression(_ienum):
	NONE = 0
	DEFLATE = 1
	PPM = 2
	LZMA = 3
	ZSTD = 4
	LZ4 = 5
	BZIP2 = 6

def _compress_data(data: bytes, compression_type: SArcCompression) -> bytes:
	match compression_type:
		case SArcCompression.NONE:
			return data
		case SArcCompression.DEFLATE:
			osz = len(data).to_bytes(4, "big")
			return osz + _deflate.deflate_compress(data)
		case SArcCompression.PPM:
			return _ppm.compress(data)
		case SArcCompression.LZMA:
			return _lzma.compress(data)
		case SArcCompression.ZSTD:
			return _zstd.compress(data)
		case SArcCompression.LZ4:
			return _lz4.compress(data)
		case SArcCompression.BZIP2:
			return _bzip2.compress(data)
		case _:
			print("Compression type not understood: defaulting to no compression.")
			return data

def _decompress_data(data: bytes, compression_type: SArcCompression) -> bytes:
	match compression_type:
		case SArcCompression.NONE:
			return data
		case SArcCompression.DEFLATE:
			return _deflate.deflate_decompress(data[4:], int.from_bytes(data[0:4], "big"))
		case SArcCompression.PPM:
			return _ppm.decompress(data) # pyright: ignore[reportReturnType]
		case SArcCompression.LZMA:
			return _lzma.decompress(data)
		case SArcCompression.ZSTD:
			return _zstd.decompress(data)
		case SArcCompression.LZ4:
			return _lz4.decompress(data)
		case SArcCompression.BZIP2:
			return _bzip2.decompress(data)
		case _:
			print("Compression type not understood: defaulting to no compression.")
			return data

class SArchiveFile:
	file_path: str
	compression_type: SArcCompression = SArcCompression.NONE
	data: bytes
 
	def __init__(self) -> None:
		self.data = b""
	
	def serialize(self) -> bytes:
		out = b""
		
		out += self.file_path.encode("utf-8")
		out += b"\x00"
		out += self.compression_type.to_bytes(1, "big")
		comp = _compress_data(self.data, self.compression_type)
		out += len(comp).to_bytes(8, "big")
		out += comp
		out += _crcCalc(_crc32.CRC32, optimized=True).checksum(self.data).to_bytes(4, "big") # pyright: ignore[reportArgumentType]
				
		return out
	
	@staticmethod
	def load(file: _bufReader) -> SArchiveFile:
		self = SArchiveFile()
		
		buf = b""
		while True:
			b = file.read(1)
			if b == b"\x00":
				break
			buf += b
		
		self.file_path = buf.decode("utf-8")
		self.compression_type = SArcCompression.from_bytes(file.read(1), "big")
		decomp = _decompress_data(file.read(int.from_bytes(file.read(8), "big")), self.compression_type)
		self.data = decomp
		assert _crcCalc(_crc32.CRC32, optimized=True).verify(self.data, int.from_bytes(file.read(4), "big")) # pyright: ignore[reportArgumentType]
		
		return self

class SArchive:
	files: list[SArchiveFile]
 
	def __init__(self) -> None:
		self.files = []
	
	def serialize(self) -> bytes:
		out = b"SArc\x00"

		out += len(self.files).to_bytes(8, "big")
		for file in self.files:
			out += file.serialize()
     
		return out

	@staticmethod
	def load(file: _bufReader) -> SArchive:
		self = SArchive()
  
		assert file.read(5) == b"SArc\x00"

		for i in range(int.from_bytes(file.read(8), "big")):
			self.files.append(SArchiveFile.load(file))
  
		return self