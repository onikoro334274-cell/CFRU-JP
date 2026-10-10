import struct

def read_indexed_png(path):
    """パレット PNG を**色番号のまま**読む -> (幅, 高さ, [[番号]], [RGB])

    ★ `compare_screenshot.read_png` は RGB に展開してしまい、
      しかも 8 ビットの PNG しか扱えない。
      エメラルドの絵は **4 ビットのパレット PNG** で、
      こちらは色番号そのものが要るので別に読む。
    """
    import zlib
    d = open(path, 'rb').read()
    if d[:8] != b'\x89PNG\r\n\x1a\n':
        raise SystemExit('PNG ではない: %s' % path)
    o, idat, plte = 8, b'', None
    w = h = bd = ct = 0
    while o < len(d):
        ln = struct.unpack('>I', d[o:o + 4])[0]
        typ = d[o + 4:o + 8]
        body = d[o + 8:o + 8 + ln]
        o += 12 + ln
        if typ == b'IHDR':
            w, h, bd, ct = struct.unpack('>IIBB', body[:10])
        elif typ == b'PLTE':
            plte = body
        elif typ == b'IDAT':
            idat += body
        elif typ == b'IEND':
            break
    if ct != 3 or bd not in (1, 2, 4, 8):
        raise SystemExit('パレットの PNG だけ扱える: %s（色種別 %d / %d ビット）'
                         % (path, ct, bd))
    raw = zlib.decompress(idat)
    stride = (w * bd + 7) // 8
    if w <= 0 or h <= 0 or len(raw) != h * (stride + 1):
        raise ValueError('Invalid indexed PNG dimensions or decompressed length')
    rows, prev, p = [], bytearray(stride), 0
    for _ in range(h):
        f = raw[p]
        if f > 4:
            raise ValueError('Unsupported PNG filter')
        p += 1
        line = bytearray(raw[p:p + stride])
        p += stride
        for i in range(stride):
            a = line[i - 1] if i >= 1 else 0
            b = prev[i]
            c = prev[i - 1] if i >= 1 else 0
            if f == 1:
                line[i] = (line[i] + a) & 0xFF
            elif f == 2:
                line[i] = (line[i] + b) & 0xFF
            elif f == 3:
                line[i] = (line[i] + (a + b) // 2) & 0xFF
            elif f == 4:
                pp = a + b - c
                pa, pb, pc = abs(pp - a), abs(pp - b), abs(pp - c)
                pr = a if (pa <= pb and pa <= pc) else (b if pb <= pc else c)
                line[i] = (line[i] + pr) & 0xFF
        prev = line
        per = 8 // bd
        mask = (1 << bd) - 1
        rows.append([(line[x // per] >> (8 - bd - (x % per) * bd)) & mask
                     for x in range(w)])
    pal = [tuple(plte[i * 3:i * 3 + 3]) for i in range(len(plte) // 3)]
    return w, h, rows, pal
