# SArc

SArc is a simple archive format used primarily in [YourSoftware](https://github.com/YSYourSoftware) applications.
It officially supports 5 compression formats:
- Deflate
- LZMA
- Zstandard
- LZ4
- BZIP2

> [!NOTE]
> PPM Compresion was dropped in v1.
> Any archives still using it will need to be repacked. See [Repacking & Updating](https://github.com/YSYourSoftware/SArc?tab=readme-ov-file#repacking-updating) for more details.

## Format

> [!NOTE]
> SArc is a **Big Endian** format, that is to say all multibyte integer and float values are stored in the big-endian byte order.

The format of SArc v1 goes as follows: 

- Magic value - ASCII "SArc"
- Version - 0x01
- File count - UInt64
- *Per file:*
- - File path - Null-terminated UTF-8 string (use forward-slashes [/] to seperate folders)
- - Compression type - SArcCompression
- - Compressed data length - UInt64
- - Compressed data
- - CRC32 checksum - UInt32

## Repacking & Updating Archives