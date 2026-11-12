#!/usr/bin/env python3
"""Write a Blood II .b2c character file.

    b2c.py OUT.b2c CHARACTER [COLOR]

A .b2c is SharedDefs.h's B2C struct as the character menu saves it: fifteen
little endian DWORDs: dwCharacter, dwColor, the four attributes, then the
nine weapon slots.  CHARACTER is an id: 0-3 retail's, 4 (8 with the
Nightmare Levels) the first a mod's Characters.txt adds.  The attributes and
weapons are the client's own defaults when it finds no file
(BloodClientShell.cpp): strength 5, speed 3, resistance 3, focus 1,
weapons 1-9.
"""
import struct
import sys

if len(sys.argv) not in (3, 4):
    sys.exit(__doc__)
character = int(sys.argv[2])
color = int(sys.argv[3]) if len(sys.argv) == 4 else 1
fields = [character, color, 5, 3, 3, 1] + list(range(1, 10))
with open(sys.argv[1], "wb") as f:
    f.write(struct.pack("<15I", *fields))
