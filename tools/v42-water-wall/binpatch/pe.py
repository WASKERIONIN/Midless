import struct
BASE = 0x140000000
class PE:
    def __init__(self, path):
        self.d = bytearray(open(path, 'rb').read())
        d = self.d
        self.pe = struct.unpack_from('<I', d, 0x3c)[0]
        self.nsec = struct.unpack_from('<H', d, self.pe + 6)[0]
        self.optsz = struct.unpack_from('<H', d, self.pe + 20)[0]
        self.opt = self.pe + 24
        self.sec = self.opt + self.optsz
        self.secs = []
        for i in range(self.nsec):
            name, vs, va, rs, ro = struct.unpack_from('<8sIIII', d, self.sec + 40 * i)
            ch = struct.unpack_from('<I', d, self.sec + 40 * i + 36)[0]
            self.secs.append(dict(i=i, name=name.rstrip(b'\0').decode(), vs=vs, va=va, rs=rs, ro=ro, ch=ch))
    def off(self, vma):
        rva = vma - BASE
        for s in self.secs:
            if s['va'] <= rva < s['va'] + max(s['vs'], s['rs']):
                return s['ro'] + (rva - s['va'])
        raise ValueError(hex(vma))
    def read(self, vma, n):
        o = self.off(vma); return bytes(self.d[o:o + n])
    def write(self, vma, b):
        o = self.off(vma); self.d[o:o + len(b)] = b
    def section(self, name):
        return next(s for s in self.secs if s['name'] == name)
    def set_vs(self, name, vs):
        s = self.section(name); struct.pack_into('<I', self.d, self.sec + 40 * s['i'] + 8, vs); s['vs'] = vs
    def checksum_fix(self):
        d = self.d; cs_off = self.opt + 64
        struct.pack_into('<I', d, cs_off, 0)
        total = len(d); s = 0
        data = bytes(d) + (b'\0' if total % 2 else b'')
        for i in range(0, len(data), 2):
            s += data[i] | (data[i + 1] << 8)
            s = (s & 0xffff) + (s >> 16)
        s = (s & 0xffff) + (s >> 16)
        s = (s & 0xffff) + (s >> 16)
        s += total
        struct.pack_into('<I', d, cs_off, s & 0xffffffff)
        return s & 0xffffffff
    def save(self, path):
        open(path, 'wb').write(self.d)
