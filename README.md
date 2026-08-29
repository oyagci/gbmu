# gbmu
gameboy / gameboy color emulator in C++

This project is for educational purpose only.

# Info
The Emulator can be build on either OSX or Linux.
It is possible that some games do not work properly.

# Dependencies
* boost-devel
* portaudio-devel
* qt5-qmake
* clang++

# OSX
```brew install qt@5 boost portaudio```

# Fedora
```# dnf install portaudio-devel boost-devel clang qt5 qt5-qtbase-devel```

# Debian
```# apt-get install qt5-qmake portaudio19-dev clang libboost-all-dev```

# Build
```./configure && make -j```

# Docker
Builds on Linux without installing anything locally.
```
docker build -t gbmu .
```
The binary is `/gbmu/gbmu` in the image. Tests:
```
docker run --rm gbmu make -f RawProject.mk test_sample test_Operations_utils test_Interrupt
```

# Headless debugger
`gbmu-dbg` links the emulator without Qt, for scripting and CI.
```
make -f RawProject.mk dbg
./gbmu-dbg <rom.gb> [options]
```
| option | effect |
| --- | --- |
| `-b, --break ADDR` | break at ADDR (hex), repeatable |
| `-n, --steps N` | stop after N instructions |
| `-t, --trace` | disassemble every instruction |
| `-d, --dump ADDR` | dump 160 bytes at ADDR when stopped |
| `--dmg` / `--cgb` | force a model (default: auto-detect) |

Exit codes: `0` stopped or test passed, `1` test failed or bad ROM, `2` step
limit, `3` bad usage. Link-port output goes to stderr as the ROM writes it, so
Blargg's test ROMs report their own verdict:
```
./gbmu-dbg gb-test-roms/cpu_instrs/cpu_instrs.gb -n 200000000
```
Smoke test: `make -f RawProject.mk test_dbg`
