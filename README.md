# Pagonia Land Paker
Packing Tool for [Pioneers of Pagonia](https://pioneersofpagonia.com/)

> This project is not affiliated in any way with Envision Entertainment GmbH

---

**PLPaker** is a console application that is best used with PowerShell.

Here is an example: `.\plpaker.exe unpack .\core.pak`

*Always use absolute or relative paths*

```bash
usage:  plpaker <command> <args> [<options>] [<parameters>]

commands:
  list <pak> [<dir>]           # Parse pak file and write pakinfo.json

  unpack <pak> [<dir>]         # Unpack pak file into the folder
  pack <pakinfo> [<pak>]       # Pack new pak file based on pakinfo.json

  compress <file> <out>        # Compress a file
  decompress <file> <out>      # Decompress a file

  patch <pak> <out> <files>    # Repack pak file with files to be replaced

options:
  -c | --compress       # Unpack/Pack compressed files
  -d | --decompress     # Unpack/Pack decompressed files
  -p | --png            # Export images as PNG (unpack and decompress only)

If no options are specified, all options are active, otherwise only the set ones.

parameters:
  -s | --start      # Start index:    -s=38
  -e | --end        # End index:      -e=1602
  -f | --filter     # Name filter:    -f=gold

All parameters work on unpack and pack commands.
```

## Download

- Latest version: https://dl.pagonia.land/plpaker.zip
- MD5: https://dl.pagonia.land/plpaker.md5
- Readme: https://dl.pagonia.land/plpaker

Need help? - Please feel free to ask us on [Discord](https://Pagonia.Land/)

## How to Make a Simple Mod

Please make a backup of the original pak file or simply copy it to the folder where this tool is located.

1. `.\plpaker.exe unpack .\core.pak .\mod -f=system.ini`
2. **Adjust game camera and speed:** `mod\system.ini`
3. `.\plpaker.exe patch .\core.pak .\mod.pak .\mod\system.ini`
4. Replace `core.pak` in game folder with `mod.pak`

## Build

```bash
mkdir build
cd build
```

```bash
# debug

conan install ../conan/file.txt -pr="../conan/win_debug"

cmake ..
cmake --build . --parallel
```

Do you prefer an optimized version:

```bash
# release

conan install ../conan/file.txt -pr="../conan/win_release"

cmake -D CMAKE_BUILD_TYPE=Release ..
cmake --build . --config Release --parallel
```

When a library is missing after all:

```bash
# append conan install

--build=missing
```

## Requirements

* **C++23** compatible compiler
* [CMake](https://cmake.org/) **3.26+**
* [Conan](https://www.conan.io/)

# Third-Party

* [argh](https://github.com/adishavit/argh) &nbsp; **Argh! A minimalist argument handler** &nbsp; *3-clause BSD*
* [zlib](https://github.com/madler/zlib) &nbsp; **A massively spiffy yet delicately unobtrusive compression library** &nbsp; *zlib*
* [stb](https://github.com/nothings/stb) &nbsp; **Single-file public domain libraries for C/C++** &nbsp; *MIT*
* [json](https://github.com/nlohmann/json) &nbsp; **JSON for Modern C++** &nbsp; *MIT*

## Collaborate

Use the [issue tracker](https://github.com/pagonia-land/plpaker/issues) to report any bug or compatibility issue.

If you want to **contribute**, we suggest the following:

1. Fork the [official repository](https://github.com/pagonia-land/plpaker/fork)
2. Apply your changes to your fork
3. Submit a [pull request](https://github.com/pagonia-land/plpaker/pulls) describing the changes you have made

## Disclaimer

YOU USE MODS AT YOUR OWN RISK!

By installing Mods for Pioneers of Pagonia, you are changing the official game files and modifying your game experience from the original intended product developed and published by Envision Entertainment GmbH. Keep in mind that they won't be able to provide any official technical support for your game. Changing the files of your game can create bugs, cause instability of the game client or even damage your save file. This means that if anything goes wrong after applying Mods (like black screens or game crashes) the EE support team will not be able to help you until you reset all the changes.

## Credits

[Pioneers of Pagonia](https://pioneersofpagonia.com/) is a trademark of [Envision Entertainment GmbH](https://www.envision-entertainment.de/)

Copyright (c) 2024 - [Pagonia Land](https://pagonia.land/) and [contributors](https://github.com/pagonia-land/plpaker/graphs/contributors)

*Modding community project by [Lava Block](https://lava-block.com/)*
