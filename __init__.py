# This is using an older, edited version of the v0 decoder.
# It will not function on any SArc archives at this time.
# TODO: Rewrite for SArc v1

from __future__ import annotations

from io import BufferedReader as _bufReader

import lzma as _lzma

from crc import Crc32 as _crc32
from crc import Calculator as _crcCalc

class SArchiveFile:
	file_path: str
	data: bytes
 
	def __init__(self) -> None:
		self.data = b""
	
	def serialize(self) -> bytes:
		out = b""
		
		out += self.file_path.encode("utf-8")
		out += b"\x00"
		comp = _lzma.compress(self.data, _lzma.FORMAT_RAW)
		out += len(comp).to_bytes(4, "big")
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
		decomp = _lzma.decompress(file.read(int.from_bytes(file.read(4), "big")), _lzma.FORMAT_RAW)
		self.data = decomp
		assert _crcCalc(_crc32.CRC32, optimized=True).verify(self.data, int.from_bytes(file.read(4), "big")) # pyright: ignore[reportArgumentType]
		
		return self

class SArchive:
	files: list[SArchiveFile]
 
	def __init__(self) -> None:
		self.files = []
	
	def serialize(self) -> bytes:
		out = b"SArc\x01"

		out += len(self.files).to_bytes(4, "big")
		for file in self.files:
			out += file.serialize()
     
		return out

	@staticmethod
	def load(file: _bufReader) -> SArchive:
		self = SArchive()
  
		assert file.read(5) == b"SArc\x01"

		for i in range(int.from_bytes(file.read(4), "big")):
			self.files.append(SArchiveFile.load(file))
  
		return self