#!/usr/bin/env python3
"""Sanity-check a built ROM: size, LoROM header, title and checksum.
Usage: romcheck.py RiverCityQuest.sfc"""
import sys

rom = open(sys.argv[1], 'rb').read()
ok = True


def check(cond, msg):
    global ok
    print(('ok   ' if cond else 'FAIL ') + msg)
    ok &= bool(cond)


check(len(rom) == 32 * 0x8000, f'size is 1 MB ({len(rom)} bytes)')
h = rom[0x7FC0:0x7FE0]
title = h[:21].decode('ascii', 'replace').strip(' \x00')
check(title == 'RIVER CITY QUEST', f'title "{title}"')
check(h[0x15] & 0x0F == 0, f'LoROM map mode (${h[0x15]:02X})')
comp = h[0x1C] | h[0x1D] << 8
csum = h[0x1E] | h[0x1F] << 8
check(comp ^ csum == 0xFFFF, f'checksum ${csum:04X} / complement ${comp:04X}')
total = sum(rom) & 0xFFFF
check(total == csum, f'checksum matches contents (${total:04X})')
reset = rom[0x7FFC] | rom[0x7FFD] << 8
check(reset >= 0x8000, f'reset vector ${reset:04X}')
sys.exit(0 if ok else 1)
