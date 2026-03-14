# SArc

SArc is a simple archive format used primarily in [YourSoftware](https://github.com/YSYourSoftware) applications.
It officially supports 6 compression formats:
- Deflate
- PPM
- LZMA
- Zstandard
- LZ4
- BZIP2

## Format

> [!NOTE]
> SArc is a **Big Endian** format, that is to say all multibyte integer and float values are stored in the big-endian byte order.

The format of SArc V0 goes as follows: 

- Magic value - ASCII "SArc"
- Version - 0x00
- File count - UInt64
- *Per file:*
- - File path - Null-terminated UTF-8 string (use forward-slashes [/] to seperate folders)
- - Compression type - SArcCompression
- - Compressed data length - UInt64
- - Compressed data
- - CRC32 checksum - UInt32