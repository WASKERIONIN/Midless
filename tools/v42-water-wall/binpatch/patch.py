#!/usr/bin/env python3
"""Binary patch for the shipped Midless v42 game.exe / server.exe:
"no caves below sea level" (the same fix as ../worldgenerator-no-caves-below-sea.patch).

GetTerrainPoint() in v42 is:

    if (p.y >= 16) { ...; if (p.y + terrain >= 96*elevation) return 0; }   // surface air
    a = fnlGetNoise3D(&caveNoise, ...); b = ...;                            // <- +0xef
    return a*b > 0.6f ? 2 : 1;

At +0xef (first instruction of the cave-noise part, reached only for non-air cells)
we place a 5-byte jmp to a 31-byte code cave in the unused tail of .text:

    comiss xmm6, [48.0f]        ; xmm6 = p.y (still live here)
    jb     GetTerrainPoint+0x166 ; y < 48  -> "mov eax,1" -> return 1 (solid)
    movss  xmm8, [1.5f]         ; the instruction displaced by the jmp
    jmp    GetTerrainPoint+0xf8  ; continue with the cave noise

Only RIP-relative addressing is used (ASLR safe). .text VirtualSize is widened to
SizeOfRawData so the cave is formally inside the section, and the PE checksum is
recomputed. 36 bytes change per file. Every byte pattern we rely on is asserted
before writing, so the script refuses to touch a different build.

usage: patch.py <orig.exe> <patched.exe>
"""
import os, struct, subprocess, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from pe import PE, BASE

SEA_LEVEL = 48.0

def find_sym(path, name):
    out = subprocess.run(['nm', path], capture_output=True, text=True, check=True).stdout
    for line in out.splitlines():
        p = line.split()
        if len(p) == 3 and p[2] == name:
            return int(p[0], 16)
    raise KeyError(f'{name} not found in {path} (binary must keep its symbols, v42 does)')

def patch(src, dst):
    pe = PE(src)
    G = find_sym(src, 'GetTerrainPoint')
    fn = pe.read(G, 409)
    # -- the exact instruction pattern of the v42 build (game.exe and server.exe are identical here)
    assert fn[0x32:0x35] == bytes.fromhex('0f2f35'), 'comiss xmm6,[16.0f] expected at +0x32'
    assert fn[0x48:0x4e] == bytes.fromhex('0f82a1000000'), 'jb +0xef expected at +0x48'
    assert fn[0xed:0xef] == bytes.fromhex('737c'), 'jae +0x16b expected at +0xed'
    assert fn[0xef:0xf4] == bytes.fromhex('f3440f1005'), 'movss xmm8,[rip+X] expected at +0xef'
    assert fn[0xf8:0xfb] == bytes.fromhex('0f10d6'), 'movups xmm2,xmm6 expected at +0xf8'
    assert fn[0x166:0x16b] == bytes.fromhex('b801000000'), 'mov eax,1 expected at +0x166'
    disp = struct.unpack_from('<i', fn, 0xf4)[0]
    const15 = G + 0xf8 + disp
    assert struct.unpack('<f', pe.read(const15, 4))[0] == 1.5, 'displaced movss must load 1.5f'
    # -- code cave: zero-filled tail of .text (between VirtualSize and SizeOfRawData)
    t = pe.section('.text')
    cave = (BASE + t['va'] + t['vs'] + 15) & ~15
    slack_end = BASE + t['va'] + t['rs']
    assert pe.read(cave, slack_end - cave) == b'\0' * (slack_end - cave), '.text slack is not empty'
    ret1, back = G + 0x166, G + 0xf8
    K = cave + 27
    code = (bytes.fromhex('0f2f35') + struct.pack('<i', K - (cave + 7))             # comiss xmm6,[K]
            + bytes.fromhex('0f82') + struct.pack('<i', ret1 - (cave + 13))          # jb ret1
            + bytes.fromhex('f3440f1005') + struct.pack('<i', const15 - (cave + 22))  # movss xmm8,[1.5f]
            + b'\xe9' + struct.pack('<i', back - (cave + 27))                        # jmp back
            + struct.pack('<f', SEA_LEVEL))
    assert len(code) == 31 and cave + len(code) <= slack_end
    pe.write(cave, code)
    pe.write(G + 0xef, b'\xe9' + struct.pack('<i', cave - (G + 0xef + 5)) + b'\x0f\x1f\x40\x00')  # jmp cave; nop4
    nxt = min(s['va'] for s in pe.secs if s['va'] > t['va'])
    assert t['va'] + t['rs'] <= nxt
    pe.set_vs('.text', t['rs'])
    cs = pe.checksum_fix()
    pe.save(dst)
    print(f'{os.path.basename(src)}: GetTerrainPoint=0x{G:x} cave=0x{cave:x} checksum=0x{cs:x} -> {dst}')

if __name__ == '__main__':
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    patch(sys.argv[1], sys.argv[2])
