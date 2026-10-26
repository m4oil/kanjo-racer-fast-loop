#!/usr/bin/env python3
"""Compose a frame dumped by the host harness (Mode 4 page + OBJ layer) into a PNG."""
import sys, struct
import numpy as np
from PIL import Image

SHAPES = {  # (shape,size) -> (w,h)
    (0, 0): (8, 8), (0, 1): (16, 16), (0, 2): (32, 32), (0, 3): (64, 64),
    (1, 0): (16, 8), (1, 1): (32, 8), (1, 2): (32, 16), (1, 3): (64, 32),
    (2, 0): (8, 16), (2, 1): (8, 32), (2, 2): (16, 32), (2, 3): (32, 64)}

def rgb(c):
    return ((c & 31) << 3, ((c >> 5) & 31) << 3, ((c >> 10) & 31) << 3)

def compose(path, out, scale=3):
    d = open(path, "rb").read()
    page = np.frombuffer(d[:38400], np.uint8).reshape(160, 240)
    pal = struct.unpack("<512H", d[38400:38400 + 1024])
    oam = struct.unpack("<512H", d[38400 + 1024:38400 + 2048])
    tiles = d[38400 + 2048:]
    bg = np.array([rgb(c) for c in pal[:256]], np.uint8)
    img = bg[page].copy()
    def tile_px(tn, tx, ty):
        off = (tn - 512) * 32
        t = np.zeros((8, 8), np.uint8)
        for r in range(8):
            w = struct.unpack_from("<I", tiles, off + r * 4)[0]
            for c in range(8):
                t[r, c] = (w >> (4 * c)) & 15
        return t
    cache = {}
    def sprite_pixels(tile, w, h):
        key = (tile, w, h)
        if key in cache: return cache[key]
        a = np.zeros((h, w), np.uint8)
        for ty in range(h // 8):
            for tx in range(w // 8):
                a[ty * 8:ty * 8 + 8, tx * 8:tx * 8 + 8] = tile_px(tile + ty * (w // 8) + tx, tx, ty)
        cache[key] = a
        return a
    for i in range(127, -1, -1):
        a0, a1, a2 = oam[i * 4], oam[i * 4 + 1], oam[i * 4 + 2]
        affine = (a0 >> 8) & 1
        if not affine and (a0 >> 9) & 1: continue
        shape, size = (a0 >> 14) & 3, (a1 >> 14) & 3
        if (shape, size) not in SHAPES: continue
        w, h = SHAPES[(shape, size)]
        y = a0 & 255
        x = a1 & 511
        if x >= 256: x -= 512
        if y >= 160: y -= 256
        tile = a2 & 1023
        bank = (a2 >> 12) & 15
        px = sprite_pixels(tile, w, h)
        colors = [rgb(pal[256 + bank * 16 + k]) for k in range(16)]
        if not affine:
            for yy in range(h):
                for xx in range(w):
                    v = px[yy, xx]
                    X, Y = x + xx, y + yy
                    if v and 0 <= X < 240 and 0 <= Y < 160: img[Y, X] = colors[v]
        else:
            g = (a1 >> 9) & 31
            pa, pb, pc, pd = [((oam[g * 16 + k] ^ 0x8000) - 0x8000) / 256.0 for k in (3, 7, 11, 15)]
            dbl = (a0 >> 9) & 1
            bw, bh = (w * 2, h * 2) if dbl else (w, h)
            for yy in range(bh):
                for xx in range(bw):
                    dx, dy = xx - bw / 2, yy - bh / 2
                    u = pa * dx + pb * dy + w / 2
                    v2 = pc * dx + pd * dy + h / 2
                    ui, vi = int(np.floor(u)), int(np.floor(v2))
                    if 0 <= ui < w and 0 <= vi < h:
                        v = px[vi, ui]
                        X, Y = x + xx, y + yy
                        if v and 0 <= X < 240 and 0 <= Y < 160: img[Y, X] = colors[v]
    Image.fromarray(img).resize((240 * scale, 160 * scale), Image.NEAREST).save(out)

if __name__ == "__main__":
    compose(sys.argv[1], sys.argv[2], int(sys.argv[3]) if len(sys.argv) > 3 else 2)
