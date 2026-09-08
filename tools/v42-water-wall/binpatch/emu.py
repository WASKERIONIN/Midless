#!/usr/bin/env python3
"""Run the world generator *of a Windows exe* (original or binary-patched v42 game.exe /
server.exe) under Unicorn (x86-64 emulator) on Linux and count water walls.

ServerWorldGenerator_Init / _Generate / _GenerateStructures are executed from the PE
image itself; the ten CRT imports they reach (rand, srand, malloc, free, realloc,
memcmp, memmove, strcmp, strlen, _errno) are emulated in Python (rand/srand via the
host libc so RandomFromPosition() matches a native build of the same source).

usage: emu.py <exe> <seed> [cx0 cx1 cz0 cz1]    (chunk range, default -2..2)
needs: pip install unicorn ; `nm` (binutils) for the symbol table.
"""
import ctypes, os, struct, subprocess, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from unicorn import Uc, UC_ARCH_X86, UC_MODE_64, UC_HOOK_CODE
from unicorn.x86_const import *
from pe import PE, BASE

def syms(path):
    out = subprocess.run(['nm', path], capture_output=True, text=True, check=True).stdout
    m = {}
    for l in out.splitlines():
        p = l.split()
        if len(p) == 3:
            m[p[2]] = int(p[0], 16)
    return m

class Machine:
    def __init__(self, path):
        self.pe = PE(path); self.S = syms(path)
        mu = self.mu = Uc(UC_ARCH_X86, UC_MODE_64)
        img_size = struct.unpack_from('<I', self.pe.d, self.pe.opt + 56)[0]
        mu.mem_map(BASE, (img_size + 0xfff) & ~0xfff)
        for s in self.pe.secs:
            if s['rs']:
                mu.mem_write(BASE + s['va'], bytes(self.pe.d[s['ro']:s['ro'] + s['rs']]))
        self.STACK = 0x7fff0000000; mu.mem_map(self.STACK - 0x100000, 0x200000)
        self.HEAP, self.HEAP_SZ = 0x10000000, 0x4000000; mu.mem_map(self.HEAP, self.HEAP_SZ)
        self.brk = self.HEAP + 0x1000; self.blocks = {}
        self.libc = ctypes.CDLL(None)
        self.MAGIC = 0x7ff00000000; mu.mem_map(self.MAGIC, 0x1000)
        self.handlers = {}
        for name in ('rand', 'srand', 'malloc', 'free', 'realloc', 'memcmp', 'memmove', 'strcmp', 'strlen', '_errno'):
            slot = self.S.get('__imp_' + name)
            if slot is None:
                continue
            addr = self.MAGIC + 16 * len(self.handlers)
            mu.mem_write(slot, struct.pack('<Q', addr)); mu.mem_write(addr, b'\xc3')
            self.handlers[addr] = getattr(self, 'h_' + name)
        mu.hook_add(UC_HOOK_CODE, self._hook, begin=self.MAGIC, end=self.MAGIC + 0x1000)
        self.STOP = self.MAGIC + 0xff0; mu.mem_write(self.STOP, b'\xf4')
        self.CH = 0x20000000; mu.mem_map(self.CH, 0x10000)

    # --- emulated imports (Win64 ABI: rcx, rdx, r8) ---
    def rd(self, a, n): return bytes(self.mu.mem_read(a, n))
    def cstr(self, a):
        out = b''
        while (c := self.rd(a, 1)) != b'\0':
            out += c; a += 1
        return out
    def h_rand(self): return self.libc.rand() & 0x7fffffff
    def h_srand(self): self.libc.srand(ctypes.c_uint(self.mu.reg_read(UC_X86_REG_ECX)).value); return 0
    def h_malloc(self):
        n = ((self.mu.reg_read(UC_X86_REG_RCX) + 15) & ~15) or 16
        p = self.brk; self.brk += n; self.blocks[p] = n
        assert self.brk < self.HEAP + self.HEAP_SZ, 'emulated heap exhausted'
        return p
    def h_free(self): return 0
    def h_realloc(self):
        p, n = self.mu.reg_read(UC_X86_REG_RCX), self.mu.reg_read(UC_X86_REG_RDX)
        q = self.brk; self.brk += ((n + 15) & ~15) or 16; self.blocks[q] = n
        if p:
            self.mu.mem_write(q, self.rd(p, min(self.blocks.get(p, n), n)))
        return q
    def h_memcmp(self):
        a, b, n = (self.mu.reg_read(r) for r in (UC_X86_REG_RCX, UC_X86_REG_RDX, UC_X86_REG_R8))
        x, y = self.rd(a, n), self.rd(b, n); return 0 if x == y else (1 if x > y else 0xffffffff)
    def h_memmove(self):
        a, b, n = (self.mu.reg_read(r) for r in (UC_X86_REG_RCX, UC_X86_REG_RDX, UC_X86_REG_R8))
        self.mu.mem_write(a, self.rd(b, n)); return a
    def h_strcmp(self):
        x, y = self.cstr(self.mu.reg_read(UC_X86_REG_RCX)), self.cstr(self.mu.reg_read(UC_X86_REG_RDX))
        return 0 if x == y else (1 if x > y else 0xffffffff)
    def h_strlen(self): return len(self.cstr(self.mu.reg_read(UC_X86_REG_RCX)))
    def h__errno(self): return self.HEAP + 0x100
    def _hook(self, uc, addr, size, ud):
        if addr in self.handlers:
            uc.reg_write(UC_X86_REG_RAX, self.handlers[addr]())

    def call(self, fn, rcx=0, rdx=0, r8=0, r9=0, xmm=None):
        mu = self.mu; sp = self.STACK - 0x8000
        mu.mem_write(sp, struct.pack('<Q', self.STOP))
        for r, v in ((UC_X86_REG_RSP, sp), (UC_X86_REG_RBP, sp), (UC_X86_REG_RCX, rcx), (UC_X86_REG_RDX, rdx), (UC_X86_REG_R8, r8), (UC_X86_REG_R9, r9)):
            mu.reg_write(r, v)
        for i, v in (xmm or {}).items():
            mu.reg_write((UC_X86_REG_XMM0, UC_X86_REG_XMM1, UC_X86_REG_XMM2, UC_X86_REG_XMM3)[i], v)
        mu.emu_start(fn, self.STOP)
        return mu.reg_read(UC_X86_REG_RAX)

    # Chunk layout (server/src/world/chunk/chunk.h): unsigned short data[4096]; unsigned char skyMask[32];
    # Vector3 position; Vector3 blockPosition; bool fromFile; bool modified; Player **players;
    OFF_POS = 8192 + 32; OFF_BPOS = OFF_POS + 12; CH_SIZE = OFF_BPOS + 12 + 16
    def init(self, seed): self.call(self.S['ServerWorldGenerator_Init'], rcx=seed & 0xffffffff)
    def chunk(self, cx, cy, cz, structures=True):
        mu = self.mu
        mu.mem_write(self.CH, b'\0' * self.CH_SIZE)
        mu.mem_write(self.CH + self.OFF_POS, struct.pack('<fff', cx, cy, cz))
        mu.mem_write(self.CH + self.OFF_BPOS, struct.pack('<fff', cx * 16, cy * 16, cz * 16))
        self.call(self.S['ServerWorldGenerator_Generate'], rcx=self.CH)
        if structures:
            self.call(self.S['ServerWorldGenerator_GenerateStructures'], rcx=self.CH)
        return self.rd(self.CH, 8192)

def generate(path, seed, xr, zr, CY1=5, structures=True):
    """Returns (world bytes [y][z][x] u16, NX, NXx, NXz, NY) for chunk range xr/zr, chunk y 0..CY1."""
    m = Machine(path); m.init(seed)
    NXx, NXz = (xr[1] - xr[0] + 1) * 16, (zr[1] - zr[0] + 1) * 16; NX = max(NXx, NXz); NY = (CY1 + 1) * 16
    world = bytearray(NX * NX * NY * 2)
    for cy in range(CY1 + 1):
        for cz in range(zr[0], zr[1] + 1):
            for cx in range(xr[0], xr[1] + 1):
                data = m.chunk(cx, cy, cz, structures)
                for i in range(4096):
                    v = data[2 * i] | (data[2 * i + 1] << 8)
                    if v:  # ServerChunk_IndexToPos: x = i % 16, z = (i / 16) % 16, y = i / 256
                        gx, gy, gz = (cx - xr[0]) * 16 + i % 16, cy * 16 + i // 256, (cz - zr[0]) * 16 + (i // 16) % 16
                        o = ((gy * NX + gz) * NX + gx) * 2
                        world[o], world[o + 1] = v & 0xff, v >> 8
    return bytes(world), NX, NXx, NXz, NY

def count(world, NX, NXx, NXz, NY, sea=48):
    def at(x, y, z):
        o = ((y * NX + z) * NX + x) * 2; return world[o] | (world[o + 1] << 8)
    water = walls = floating = dry = 0
    for y in range(1, NY - 1):
        for z in range(1, NXz - 1):
            for x in range(1, NXx - 1):
                v = at(x, y, z)
                if v in (5, 20):
                    water += 1
                    if 0 in (at(x + 1, y, z), at(x - 1, y, z), at(x, y, z + 1), at(x, y, z - 1)): walls += 1
                    if at(x, y - 1, z) == 0: floating += 1
                elif v == 0 and y < sea:
                    dry += 1
    return water, walls, floating, dry

if __name__ == '__main__':
    if len(sys.argv) < 3:
        sys.exit(__doc__)
    path, seed = sys.argv[1], int(sys.argv[2])
    xr = (int(sys.argv[3]), int(sys.argv[4])) if len(sys.argv) > 4 else (-2, 2)
    zr = (int(sys.argv[5]), int(sys.argv[6])) if len(sys.argv) > 6 else (-2, 2)
    w = generate(path, seed, xr, zr)
    water, walls, floating, dry = count(*w)
    print(f'{path}: seed {seed} chunks x{xr} z{zr} y0..{w[4]-1}: water {water}, '
          f'water-next-to-dry-air {walls}, water-over-air {floating}, dry air below 48: {dry}')
