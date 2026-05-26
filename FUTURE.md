# SArc Future Features

Below is a list of all features that will be added to SArc, and the versions they will be added in.
If you have a feature suggestion, feel free to [submit an issue](https://github.com/YSYourSoftware/SArc/issues/new).

## Archive Format Features

### Version 2

- [X] PGP signing of archives
- [X] [Block architecture](plans/blocks.txt) rather than one blob

### Version 3

- [ ] AES256 and PGP encryption of archives

## Library Features

- [X] `SArchiveStream` class to stream archives from the disk or network
- [X] `BlockEncoder` for less memory usage when compressing
- [ ] `SArchive::get_file_by_path` & `SArchive::get_file_by_path_const` should return a shared pointer rather than
  C-style pointer to avoid manual memory management
- [ ] Add efficient way to append blocks to an archive stream
- [ ] File type auto-mapping rather than filesystem order  

## Extra features

- [ ] 7-Zip plugin