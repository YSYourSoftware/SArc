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
> Any archives still using it will need to be repacked. See [Repacking & Updating](https://github.com/YSYourSoftware/SArc?tab=readme-ov-file#repacking--updating-archives) for more details.

## Format

> [!NOTE]
> SArc is a **Big Endian** format, that is to say all multibyte integer and float values are stored in the big-endian byte order.

The format of SArc v1 goes as follows: 

- Magic value - ASCII `SArc`
- Version - `0x01`
- File count - `UInt64`
- *Per file:*
- - File path - Null-terminated UTF-8 string (use forward-slashes `/` to seperate folders)
- - Compression type - `SArcCompression`
- - Compressed data length - `UInt64`
- - Compressed data
- - CRC32 checksum - `UInt32`

## Creating & Unpacking Archives

It would not be user-friendly if, every time you wanted to create or unpack an archive, you had to write a python script, so, `SArc` and `UnSArc` are provided for you to download.

These are simple packer and unpacker command-line executables.

```bash
# Packs the current directory into 'out.sarc'
SArc

# Provide input and output paths:
SArc   -i <input folder>  -o <output archive>
UnSArc -i <input archive> -o <output folder>

# Provide a compression scheme:
SArc -c DEFLATE

# Use the best compression scheme (can take a while as we have to try every scheme for every file)
SArc --best-comp
```

> [!NOTE]
> `SArc` does not currently support assigning different compression schemes to each file

## Repacking & Updating Archives

Different versions of SArc will introduce breaking changes, and therefore prevent older archives being loaded in newer applications and vice versa.
To combat this, you will need to unpack an archive using the `UnSArc` executable from the version it was created with, and repack it with the `SArc` executable of the new version.

> [!TIP]
> You can also do this to switch compression types.

There is also a [web tool](https://YourSoftware.org/projects/SArc/archive-updater) which supports every version of SArc (excluding v0).