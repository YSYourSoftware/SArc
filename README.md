# SArc

SArc is a simple archive format used primarily in [YourSoftware](https://YourSoftware.org) applications.
It currently officially supports 1 compression format:

- LZMA

> [!NOTE]
> All other compression schemes were dropped in v1.

## Format

> [!NOTE]
> SArc is a **big-endian** format, that is to say all multibyte integer and float values are stored in the big-endian
> byte order.

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

`SArc`, `UnSArc` and `SArcInfo` are provided for you to download over on
the [releases page](https://github.com/YSYourSoftware/SArc/releases).

These are simple packer, unpacker and signature verification command-line executables.

```bash
# Packs the current directory into 'out.sarc'
SArc

# Provide input and output paths:
SArc    <input folder>   <output archive>
UnSArc  <input archive>  <output folder>

# Provide a compression level
SArc -c <compression level>

# Provide a target block size
SArc -b 8MiB   # Faster streaming
SArc -b 128MiB # Balanced (default)
SArc -b 512MiB # Better compression

# Sign the archive using a PGP key
SArc --pgp-sign <private key> --pgp-sign-fp <key fingerprint>
SArc --pgp-sign <private key> --pgp-sign-fp <key fingerprint> --pgp-sign-ps <key passphrase>

# Print useful information about an archive
SArcInfo <input archive>

# Verify a signature (can be used in scripts: returns 0 if valid, -1 if invalid, and 1 for any other error)
SArcInfo <input archive> script-verify-signature <public key> <public key fingerprint>
```

For more information, use `--help`.

## Repacking & Updating Archives

Different versions of SArc will introduce breaking changes, and therefore prevent older archives being loaded in newer
applications and vice versa.
To combat this, you will need to unpack an archive using the `UnSArc` executable from the version it was created with,
and repack it with the `SArc` executable of the new version.

> [!TIP]
> You can also do this to switch compression schemes.

## Extensions

Extensions are a way to increase the functionality of SArc. They are provided as extra headers / files, usually in a
subfolder called `SArc`.

### Streaming

Allows streaming of archives from the disk or the network. Streaming large archives can help reduce memory usage.

> [!NOTE]
> Signing archives is not possible using streamed archives.
> Use `memory_archive.sign(...);` on a memory-loaded archive to achieve this.

> [!CAUTION]
> Streamed archives allocate new `SArchiveFile` objects on calls to `get_file_by_path_const`, remember to `delete` them,
> or you'll leak memory.
> Freeing `SArchiveFile`s obtained from a memory archive will cause dangling pointer issues, to differentiate between
> the two, use `archive.is_streamed()`.

```cpp
// C++ Demo for SArc streaming extension
#include <SArc/Streaming.hpp>

using namespace SArc;

SArchiveStream streamed_archive("archive.sarc"); // Stream an archive from a file
const SArchiveFile *my_file = streamed_archive["hello_world.txt"];
delete my_file; // Make sure to free files created by streamed archives

void archive_magic(SArchive &archive);
archive_magic(streamed_archive); // SArchiveStream inherits SArchive

// Certain actions require an archive loaded in memory, like signing
SArchiveMemory memory_archive = streamed_archive.load_into_memory(); // Load the archive into memory
memory_archive.sign(key, "my_passphrase", {0x01, 0x02, ...});
```

See the [benchmarking page](https://yoursoftware.org/projects/SArc/benchmark?highlight=stream) for more details on benefits from
SArc streaming.

### Block Encoding

Allows for memory-efficient writing of archives without using a `SArchive` object. If you only need to pack archives
using streamed or very large data, this will drastically reduce memory usage.

> [!NOTE]
> Signing archives is not possible using a `BlockEncoder`.
> Create a `SArchive` object if you need to sign archives.

```cpp
// C++ Demo for SArc BlockEncoder extension
#include <SArc/BlockEncoder.hpp>

using namespace SArc;

BlockEncoder sarc_encoder("archive.sarc"); // Write an archive to a file

sarc_encoder.start_block();

sarc_encoder.block_set_file_count(1);
sarc_encoder.block_add_file_path("hello_world.txt");

// File data must be added in the same order as file paths were added
bytes_t data;
data.resize();
std::memcpy(data.data(), "Hello, World!", 13);
sarc_encoder.block_add_file_data(data);

sarc_encoder.end_block();
```