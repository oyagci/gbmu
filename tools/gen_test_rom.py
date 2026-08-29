#!/usr/bin/env python3
"""Emit a minimal 32K DMG ROM that prints a word over the link port.

Used to smoke-test the headless debugger (tools/../gbmu-dbg).
    python3 tools/gen_test_rom.py out.gb [Passed]
"""
import sys

LOGO = bytes.fromhex(
    "CEED6666CC0D000B03730083000C000D0008111F8889000E"
    "DCCC6EE6DDDDD999BBBB67636E0EECCCDDDC999FBBB9333E")


def build(msg):
    rom = bytearray(0x8000)
    rom[0x100:0x104] = bytes([0x00, 0xC3, 0x50, 0x01])  # NOP ; JP 0x0150
    rom[0x104:0x134] = LOGO
    rom[0x134:0x13F] = b"SERIALTEST"[:11].ljust(11, b"\0")
    rom[0x143] = 0x00  # DMG
    rom[0x147] = 0x00  # ROM only
    rom[0x148] = 0x00  # 32K
    rom[0x149] = 0x00  # no RAM

    text = msg.encode() + b"\n\0"
    rom[0x150:0x161] = bytes([
        0x21, 0x61, 0x01,  # LD HL, 0x0161  (message)
        0x2A,              # LD A, (HL+)
        0xB7,              # OR A
        0x28, 0x08,        # JR Z, done
        0xE0, 0x01,        # LDH (0xFF01), A
        0x3E, 0x81,        # LD A, 0x81
        0xE0, 0x02,        # LDH (0xFF02), A
        0x18, 0xF4,        # JR loop
        0x18, 0xFE,        # done: JR done
    ])
    rom[0x161:0x161 + len(text)] = text

    checksum = 0
    for i in range(0x134, 0x14D):
        checksum = (checksum - rom[i] - 1) & 0xFF
    rom[0x14D] = checksum
    return bytes(rom)


if __name__ == "__main__":
    if len(sys.argv) < 2:
        sys.exit(__doc__)
    open(sys.argv[1], "wb").write(build(sys.argv[2] if len(sys.argv) > 2 else "Passed"))
