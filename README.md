# BFTools
A GUI and CLI utility for reading/writing Watch_Dogs bigfile archives. Does not support Far Cry.


## Compiling
## Windows
I have no idea, sorry. I haven't compiled it under Windows, you'll need to download Qt which is a nightmare on Windows, good luck.

## Linux
Ensure you have installed Clang, you can also compile it with GCC if you wish.

### Install
You will need:
- CMake 3.25+
- Git
- Qt6 (6.12 tested)
- GCC 13+ / Clang 16+
- Zlib
- LZMA
- LZO2
- MINGW toolchain if crosscompiling to Windows

### Arch Linux
```
sudo pacman -S --needed base-devel cmake git \
    zlib xz lzo qt6-base
```
### Debian/Ubuntu
```
sudo apt update && sudo apt install -y build-essential cmake git \
    zlib1g-dev liblzma-dev liblzo2-dev \
    qt6-base-dev qt6-base-dev-tools
```

### RHEL/Fedora
```
sudo dnf install -y gcc-c++ cmake git \
    zlib-devel xz-devel lzo-devel \
    qt6-qtbase-devel
```

## Compiling
```
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

or run the build script
```
./scripts/build-linux-bundle.sh
```
to generate a tar.gz in the `dist` folder with all the required Qt runtime libraries.

### Cross-compiling for Windows on Linux
You'll need the mingw toolchain.

### Debian/Ubuntu
```
sudo apt install -y mingw-w64 clang lld cmake ninja-build
```

### Arch Linux
```
sudo pacman -S mingw-w64-gcc mingw-w64-binutils clang
```

Then run the `scripts/build-windows.sh` script to crosscompile.

# Credits
- Gibbed <https://github.com/gibbed/Gibbed.Disrupt>
- miru