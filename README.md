# SArc

SArc is a simple archive format used primarily in [YourSoftware](https://YourSoftware.org) applications.
It currently officially supports 1 compression format:
- LZMA

> [!NOTE]
> All other compression schemes were dropped in v1.

## Format

> [!NOTE]
> SArc is a **big-endian** format, that is to say all multibyte integer and float values are stored in the big-endian byte order.

The format of SArc v2 goes as follows: 

```text
Magic value                 0x53417263 (SArc)
Version                     0x02
Block count                 UInt32
PGP signed flag             UInt8
If PGP:
  PGP signature size        UInt16
  PGP signature data
Per block:
  File count                UInt8
  Per file:
    File path - Null-terminated UTF-8 string (use forward-slashes `/` to seperate folders)
                (the order of paths denotes the order files are stored in)
  Decompressed block CRC32  UInt32
  Decompressed block size   UInt32
  Compressed block size     UInt32
  Compressed:
   Per file:
     Data length            UInt32
     Data
```

## Creating, Unpacking & Verifying Archives

`SArc`, `UnSArc` and `SArcSiVe` are provided for you to download over on the [releases page](https://github.com/YSYourSoftware/SArc/releases).

These are simple packer, unpacker and signiture verification command-line executables.

```bash
# Packs the current directory into 'out.sarc'
SArc

# Provide input and output paths:
SArc    <input folder>   <output archive>
UnSArc  <input archive>  <output folder>

# Provide a compression level
SArc -c <compression level>

# Sign the archive using a PGP key
SArc --pgp-sign <private key> --pgp-sign-fp <key fingerprint>
SArc --pgp-sign <private key> --pgp-sign-fp <key fingerprint> --pgp-sign-ps <key passphrase>

# Verify a signiture
SArcSiVe <input archive> <public key>
```

For more information, use `--help`.

## Repacking & Updating Archives

Different versions of SArc will introduce breaking changes, and therefore prevent older archives being loaded in newer applications and vice versa.
To combat this, you will need to unpack an archive using the `UnSArc` executable from the version it was created with, and repack it with the `SArc` executable of the new version.

> [!TIP]
> You can also do this to switch compression schemes.

## Extensions

Extensions are a way to increase the functionality of SArc. They are provided as extra headers / files, usually in a subfolder called `SArc`.

### Streaming

Allows streaming of archives from the disk or the network. Streaming large archives can help reduce memory usage.

> [!NOTE]
> Signing archives is not possible using streamed archives.
> Use `memory_archive.sign(/* TODO: Add API */);` on a memory-loaded archive to achieve this.

```cpp
// C++ Demo for SArc streaming extension
#include <SArc/Streaming.hpp>

using namespace SArc;

SArchiveStream streamed_archive("archive.sarc"); // Stream an archive from a file
SArchiveFile &my_file = streamed_archive.get_file_by_path("hello_world.txt");

void archive_magic(SArchive &archive);
archive_magic(streamed_archive); // SArchiveStream inherits SArchive

// Certain actions require an archive loaded in memory, like signing
SArchiveMemory memory_archive = streamed_archive.load_into_memory(); // Load the archive into memory
memory_archive.sign(/* TODO: Add API */);
```