#!/usr/bin/env python3
"""
KANJO RACER: BEAR PRODUCTS - asset generator.
Draws ALL art procedurally (original work) and writes C source into ../source/.
Run:  python3 tools/make_assets.py        (needs Pillow + numpy)
The generated .c/.h files are committed, so the GitHub build does NOT need Python.
"""
import os, math, random
import numpy as np
from PIL import Image, ImageDraw, ImageFont, ImageFilter

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
SRC = os.path.join(ROOT, "source")
PREV = os.path.join(ROOT, "tools", "preview")
os.makedirs(SRC, exist_ok=True)
os.makedirs(PREV, exist_ok=True)

FD = "/usr/share/fonts/truetype/dejavu/"
F_MONO_B = FD + "DejaVuSansMono-Bold.ttf"
F_SANS_BO = FD + "DejaVuSans-BoldOblique.ttf"
F_SANS_B = FD + "DejaVuSans-Bold.ttf"
F_CJK_BOLD = "/usr/share/fonts/opentype/noto/NotoSansCJK-Bold.ttc"


# ----------------------------------------------------------------------------- helpers
def b555(c):
    r, g, b = c
    return (r >> 3) | ((g >> 3) << 5) | ((b >> 3) << 10)


def scale(c, f):
    return tuple(max(0, min(255, int(v * f))) for v in c)


def lerp(a, b, t):
    return tuple(int(a[i] + (b[i] - a[i]) * t) for i in range(3))


def grad(stops, y):
    for (y0, c0), (y1, c1) in zip(stops, stops[1:]):
        if y <= y1:
            t = 0 if y1 == y0 else max(0.0, min(1.0, (y - y0) / (y1 - y0)))
            return lerp(c0, c1, t)
    return stops[-1][1]


def emit_array(f, ctype, name, data, per_line=16, fmt="%d"):
    f.write("__attribute__((aligned(4))) const %s %s[%d] = {\n" % (ctype, name, len(data)))
    for i in range(0, len(data), per_line):
        f.write("  " + ",".join(fmt % v for v in data[i:i + per_line]) + ",\n")
    f.write("};\n\n")


BAYER = [[0, 8, 2, 10], [12, 4, 14, 6], [3, 11, 1, 9], [15, 7, 13, 5]]


def index_image(rgb, max_colors):
    arr = np.array(rgb.convert("RGB"))
    flat = arr.reshape(-1, 3)
    uniq, inv = np.unique(flat, axis=0, return_inverse=True)
    if len(uniq) <= max_colors:
        return inv.reshape(arr.shape[:2]).astype(np.uint8), [tuple(int(v) for v in u) for u in uniq]
    q = rgb.convert("RGB").quantize(colors=max_colors, method=Image.Quantize.MEDIANCUT,
                                    dither=Image.Dither.FLOYDSTEINBERG)
    pal = q.getpalette()[:max_colors * 3]
    pal = [tuple(pal[i:i + 3]) for i in range(0, len(pal), 3)]
    return np.array(q).astype(np.uint8), pal


# ----------------------------------------------------------------------------- car drawing
def car_pal(body, tail=(190, 22, 32), tail_hi=(255, 120, 120)):
    return {0: (255, 0, 255), 1: body, 2: scale(body, 0.70), 3: scale(body, 0.42),
            4: (22, 26, 42), 5: (80, 102, 134), 6: (12, 12, 16), 7: tail, 8: tail_hi,
            9: (48, 50, 60), 10: (236, 236, 226), 11: (190, 196, 208), 12: (255, 70, 150),
            13: (84, 10, 18), 14: (14, 14, 22), 15: (130, 136, 150)}


PLAYER_PAL = car_pal((238, 240, 246))
PLAYER_BRAKE_PAL = car_pal((238, 240, 246), tail=(255, 40, 40), tail_hi=(255, 196, 176))


def rear_car(d, T, S, pal, yaw=0, style="ef"):
    """Rear view of a late-80s hatch/coupe/truck. Unit space is 64x32; T maps units to canvas px."""
    k = 1 - 0.06 * abs(yaw)

    def X(x):
        return 32 + (x - 32) * k

    def P(x, y):
        return T(X(x), y)

    def poly(pts, c):
        d.polygon([P(x, y) for x, y in pts], fill=pal[c])

    def rect(x0, y0, x1, y1, c):
        d.rectangle([P(x0, y0), P(x1, y1)], fill=pal[c])

    def rr(x0, y0, x1, y1, r, c):
        d.rounded_rectangle([P(x0, y0), P(x1, y1)], radius=max(1, r * S), fill=pal[c])

    d.ellipse([T(5, 26), T(59, 31.8)], fill=pal[14])
    if yaw:
        sw = 4.6 * abs(yaw)
        if yaw > 0:
            xl = X(6)
            pts = [(xl, 11), (xl - sw, 12.5), (xl - sw, 21.5), (xl, 22)]
        else:
            xl = X(58)
            pts = [(xl, 11), (xl + sw, 12.5), (xl + sw, 21.5), (xl, 22)]
        d.polygon([T(x, y) for x, y in pts], fill=pal[3])
        gx = xl - sw * 0.9 if yaw > 0 else xl + sw * 0.9
        d.polygon([T(xl, 13), T(gx, 14), T(gx, 15.5), T(xl, 15.5)], fill=pal[4])
    if style == "truck":
        rr(8, 25, 17, 31, 1.5, 6)
        rr(47, 25, 56, 31, 1.5, 6)
        rect(9, 0, 55, 23, 1)
        rect(9, 0, 14, 23, 2)
        rect(50, 0, 55, 23, 2)
        rect(9, 0, 55, 1.6, 2)
        rect(31.4, 1.5, 32.6, 23, 3)
        rect(9, 21.5, 55, 23, 3)
        rect(34.5, 10, 36.5, 13, 11)
        rr(10, 16, 15, 21, 0.8, 7)
        rr(49, 16, 54, 21, 0.8, 7)
        rr(8, 23, 56, 27, 1.2, 9)
        rr(27, 23.2, 37, 26.6, 0.6, 10)
        return
    rr(7, 21, 15, 29, 1.5, 6)
    rr(49, 21, 57, 29, 1.5, 6)
    rr(8, 22, 56, 28, 2, 9)
    rr(6, 10, 58, 23, 3, 1)
    poly([(6, 12), (10, 10), (10, 22), (6, 22)], 2)
    poly([(54, 10), (58, 12), (58, 22), (54, 22)], 2)
    rect(6, 21, 58, 23, 3)
    if style == "ae86":
        poly([(17, 4), (47, 4), (51, 10), (13, 10)], 4)
        poly([(20, 5), (26, 5), (22, 9.5), (16, 9.5)], 5)
        poly([(19, 1), (45, 1), (47, 4), (17, 4)], 1)
        poly([(17, 4), (13, 10), (9, 10.5), (14.5, 4.5)], 2)
        poly([(47, 4), (51, 10), (55, 10.5), (49.5, 4.5)], 2)
        rect(8, 10, 56, 11.3, 3)
        rr(7, 12.5, 23, 19.5, 1, 13)
        rr(8, 13.2, 22, 18.8, 1, 7)
        rect(8.5, 13.6, 14, 15, 8)
        rr(41, 12.5, 57, 19.5, 1, 13)
        rr(42, 13.2, 56, 18.8, 1, 7)
        rect(50, 13.6, 55.5, 15, 8)
        rect(23, 12.5, 41, 19.5, 6)
        rect(24, 16, 40, 17.2, 7)
    else:
        poly([(15, 3), (49, 3), (54, 11), (10, 11)], 4)
        poly([(18, 4), (25, 4), (20, 10), (14, 10)], 5)
        d.line([P(33, 10), P(43, 5)], fill=pal[6], width=max(1, int(0.7 * S)))
        rect(38, 7.4, 49, 9.6, 12)
        poly([(17, 0.5), (47, 0.5), (49, 3), (15, 3)], 1)
        poly([(15, 3), (10, 11), (7, 11.5), (12.5, 3.5)], 2)
        poly([(49, 3), (54, 11), (57, 11.5), (51.5, 3.5)], 2)
        rr(8, 12, 23, 18, 1.2, 13)
        rr(9, 12.8, 22, 17.2, 1, 7)
        rect(9.5, 13.2, 15, 14.6, 8)
        rr(41, 12, 56, 18, 1.2, 13)
        rr(42, 12.8, 55, 17.2, 1, 7)
        rect(49, 13.2, 54.5, 14.6, 8)
        rect(23, 13, 41, 17, 4)
        rect(30.5, 14, 33.5, 16, 11)
        rect(4, 6, 7.2, 8.6, 2)
        rect(56.8, 6, 60, 8.6, 2)
    rr(26, 18.6, 38, 22.6, 0.8, 10)
    rect(28, 20.2, 36, 21, 6)
    rect(42, 27, 47, 28.6, 15)


def snap(img, pal):
    a = np.array(img).astype(np.int32)
    cols = np.array([pal[i] for i in range(1, 16)], np.int32)
    rgb = a[..., :3].reshape(-1, 3)
    dist = ((rgb[:, None, :] - cols[None, :, :]) ** 2).sum(-1)
    best = dist.argmin(1).reshape(a.shape[:2]) + 1
    return np.where(a[..., 3] >= 128, best, 0).astype(np.uint8)


def sprite_idx(drawfn, w, h, pal, ux=64.0, S=8):
    im = Image.new("RGBA", (w * S, h * S), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    f = w * S / ux
    drawfn(d, lambda x, y: (x * f, y * f), f)
    return snap(im.resize((w, h), Image.BOX), pal)


def tiles_from(arr):
    h, w = arr.shape
    words = []
    for ty in range(h // 8):
        for tx in range(w // 8):
            for r in range(8):
                v = 0
                for c in range(8):
                    v |= int(arr[ty * 8 + r, tx * 8 + c] & 15) << (4 * c)
                words.append(v)
    return words


# ----------------------------------------------------------------------------- scenery sprites
SCEN_PAL = {0: (255, 0, 255), 1: (88, 92, 106), 2: (44, 46, 58), 3: (255, 242, 176), 4: (255, 172, 72),
            5: (140, 76, 40), 6: (70, 72, 84), 7: (10, 10, 14), 8: (16, 14, 30), 9: (34, 28, 60),
            10: (255, 206, 112), 11: (124, 218, 255), 12: (62, 52, 92), 13: (255, 72, 172),
            14: (64, 240, 255), 15: (66, 60, 92)}


def lamp_sprite():
    a = np.zeros((64, 32), np.uint8)
    for y in range(8, 18):
        for x in range(16, 31):
            r = math.hypot((x - 24) / 11.0, (y - 9) / 8.0)
            if r < 0.34:
                a[y, x] = 3
            elif r < 0.62 and (x + y) % 2 == 0:
                a[y, x] = 4
            elif r < 1.0 and (x + y) % 3 == 0:
                a[y, x] = 5
    a[12:62, 3:5] = 1
    a[12:62, 5] = 2
    a[60:64, 1:8] = 6
    for (x, y) in [(4, 13), (4, 11), (5, 9), (6, 8), (8, 7), (11, 6), (15, 6), (19, 6), (22, 6)]:
        a[y:y + 2, x:x + 2] = 1
    a[6:8, 21:29] = 2
    a[8:9, 22:28] = 3
    return a


def building_sprite(kind, seed):
    rng = random.Random(seed)
    a = np.zeros((64, 32), np.uint8)
    top = 8 if kind == 0 else 20
    a[top:64, 2:30] = 8
    a[top:64, 26:30] = 9
    a[top - 2:top, 2:30] = 15
    if kind == 0:
        a[3:top - 2, 6:10] = 15
        a[2:top - 2, 22:24] = 15
        a[1:2, 22:24] = 13
    else:
        a[12:top - 2, 7:25] = 9
        a[10:12, 7:25] = 15
        a[8:10, 14:16] = 15
        a[7:8, 14:16] = 14
    for r in range(top + 4, 52 if kind else 60, 5):
        for c in range(5, 26, 5):
            v = rng.random()
            a[r:r + 3, c:c + 3] = 10 if v < 0.38 else 11 if v < 0.5 else 12
    if kind == 0:
        a[14:44, 0:2] = 13
        a[48:51, 8:24] = 14
    else:
        a[53:58, 2:30] = 14
        a[54:57, 4:28] = 8
        a[40:41, 2:30] = 13
    return a


BILL_PAL = {0: (255, 0, 255), 1: (22, 18, 44), 2: (255, 72, 172), 3: (250, 250, 250), 4: (158, 98, 52),
            5: (98, 60, 32), 6: (228, 188, 134), 7: (80, 84, 98), 8: (64, 240, 255), 9: (10, 10, 14),
            10: (255, 220, 40), 11: (220, 40, 50)}


def bear_head(d, cx, cy, r, c_fur, c_dark, c_snout, c_black):
    for ex in (-1, 1):
        d.ellipse([cx + ex * r * 0.75 - r * 0.38, cy - r * 1.05, cx + ex * r * 0.75 + r * 0.38, cy - r * 0.3], fill=c_black)
        d.ellipse([cx + ex * r * 0.75 - r * 0.24, cy - r * 0.92, cx + ex * r * 0.75 + r * 0.24, cy - r * 0.45], fill=c_dark)
    d.ellipse([cx - r, cy - r * 0.85, cx + r, cy + r * 0.95], fill=c_black)
    d.ellipse([cx - r * 0.88, cy - r * 0.73, cx + r * 0.88, cy + r * 0.83], fill=c_fur)
    d.ellipse([cx - r * 0.5, cy + r * 0.0, cx + r * 0.5, cy + r * 0.72], fill=c_snout)
    d.ellipse([cx - r * 0.2, cy + r * 0.08, cx + r * 0.2, cy + r * 0.3], fill=c_black)
    for ex in (-1, 1):
        d.ellipse([cx + ex * r * 0.42 - r * 0.12, cy - r * 0.28, cx + ex * r * 0.42 + r * 0.12, cy - r * 0.04], fill=c_black)


def text_mask(size, text, font, xy):
    m = Image.new("L", size, 0)
    md = ImageDraw.Draw(m)
    md.fontmode = "1"
    md.text(xy, text, font=font, fill=255)
    return np.array(m) > 0


def billboard_sprite():
    im = Image.new("P", (64, 32), 0)
    im.putpalette([v for i in range(16) for v in BILL_PAL.get(i, (0, 0, 0))])
    d = ImageDraw.Draw(im)
    d.rectangle([2, 0, 61, 22], fill=2)
    d.rectangle([3, 1, 60, 21], fill=1)
    d.rectangle([4, 2, 59, 20], outline=8)
    d.rectangle([12, 22, 15, 31], fill=7)
    d.rectangle([48, 22, 51, 31], fill=7)
    bear_head(d, 15, 11, 7.5, 4, 5, 6, 9)
    a = np.array(im)
    a[text_mask((64, 32), "BEAR", ImageFont.truetype(F_MONO_B, 10), (27, 1))] = 3
    a[text_mask((64, 32), "PRODUCTS", ImageFont.truetype(F_MONO_B, 6), (25, 11))] = 8
    return a


def banner_sprite():
    a = np.zeros((32, 64), np.uint8)
    a[0:16, 0:64] = 9
    for x in range(64):
        for y in range(0, 3):
            a[y, x] = 10 if ((x // 4) + y) % 2 == 0 else 9
            a[15 - y, x] = 10 if ((x // 4) + y) % 2 == 1 else 9
    a[3:13, 0:64] = 1
    a[text_mask((64, 32), "CHECKPOINT", ImageFont.truetype(F_MONO_B, 7), (7, 4))] = 3
    return a


def post_sprite():
    a = np.zeros((32, 16), np.uint8)
    a[0:3, 3:13] = 11
    a[3:32, 6:10] = 7
    a[3:32, 8:10] = 9
    for y in range(3, 32, 6):
        a[y:y + 3, 6:10] = 3
    a[29:32, 3:13] = 7
    return a


# ----------------------------------------------------------------------------- HUD font
def font_tiles():
    f = ImageFont.truetype(F_MONO_B, 8)
    glyphs = []
    for code in range(32, 96):
        ch = chr(code)
        m = Image.new("L", (8, 8), 0)
        d = ImageDraw.Draw(m)
        d.fontmode = "1"
        d.text((1, 0), ch, font=f, fill=255)
        g = np.array(m) > 0
        if ch == "#":
            g = np.zeros((8, 8), bool)
            g[1:7, 1:7] = True
        a = np.zeros((8, 8), np.uint8)
        if ch == "_":
            a[1:7, 1:7] = 3
        else:
            outline = np.zeros_like(g)
            for dx in (-1, 0, 1):
                for dy in (-1, 0, 1):
                    outline |= np.roll(np.roll(g, dx, 1), dy, 0)
            a[outline & ~g] = 2
            a[g] = 1
        glyphs.append(a)
    return np.vstack(glyphs)


# ----------------------------------------------------------------------------- themed sky (role-based indices)
# index roles:
K = 26                       # gradient steps 1..26
R_HAZE0 = 27                 # haze steps 27..32
R_STAR, R_STAR2, R_MOON, R_MOON2 = 33, 34, 35, 36
R_FAR, R_MID, R_NEAR = 37, 38, 39
R_WARM, R_CY, R_PINK = 40, 41, 42
R_DIM_FAR, R_DIM_MID, R_DIM_NEAR = 43, 44, 45
R_NEON = [46, 47, 48, 49]
R_RIM, R_SPOKE, R_CAB, R_LEG = 50, 51, 52, 53
R_TWR, R_TWR2, R_TWRLIT, R_TWRRED = 54, 55, 56, 57
R_GL1, R_GL2, R_GL3 = 58, 59, 60
SKY_NROLES = 61

THEMES = {
    "DUSK": dict(
        stops=[(0, (10, 6, 40)), (24, (46, 16, 84)), (42, (130, 36, 110)), (58, (240, 96, 96)), (70, (255, 160, 100)), (79, (255, 206, 130))],
        haze=[(46, 20, 52), (36, 16, 46), (28, 12, 40), (22, 10, 34), (18, 8, 30), (14, 8, 26)],
        far=(84, 36, 96), mid=(36, 18, 60), near=(14, 8, 28), dim=((84, 36, 96), (58, 34, 90), (40, 28, 70)),
        win=1.0, star=1.0, moon=((240, 235, 210), (214, 208, 184))),
    "NIGHT": dict(
        stops=[(0, (2, 2, 12)), (30, (6, 6, 34)), (52, (20, 14, 66)), (66, (52, 22, 92)), (74, (96, 36, 112)), (79, (140, 56, 120))],
        haze=[(40, 18, 56), (30, 14, 46), (22, 10, 38), (16, 8, 30), (12, 6, 24), (8, 4, 18)],
        far=(44, 24, 88), mid=(22, 14, 54), near=(8, 6, 22), dim=((44, 24, 88), (34, 22, 74), (30, 22, 60)),
        win=1.0, star=1.0, moon=((255, 252, 230), (225, 222, 200))),
    "DAWN": dict(
        stops=[(0, (24, 52, 118)), (26, (70, 96, 160)), (48, (140, 128, 184)), (62, (240, 150, 160)), (72, (255, 196, 130)), (79, (255, 232, 160))],
        haze=[(120, 90, 120), (96, 74, 110), (76, 62, 100), (60, 52, 90), (48, 44, 80), (38, 38, 70)],
        far=(110, 100, 160), mid=(70, 70, 126), near=(34, 34, 80), dim=((110, 100, 160), (84, 84, 140), (60, 64, 110)),
        win=0.35, star=0.0, moon=((255, 244, 190), (255, 224, 140))),
}


def theme_palette(name):
    t = THEMES[name]
    p = [(0, 0, 0)] * 96
    for i in range(K):
        p[1 + i] = grad(t["stops"], i * 79 / (K - 1))
    for i in range(6):
        p[R_HAZE0 + i] = t["haze"][i]
    star = t["star"]
    p[R_STAR] = lerp(t["stops"][0][1], (255, 255, 255), star)
    p[R_STAR2] = lerp(t["stops"][0][1], (200, 200, 255), star)
    p[R_MOON], p[R_MOON2] = t["moon"]
    p[R_FAR], p[R_MID], p[R_NEAR] = t["far"], t["mid"], t["near"]
    w = t["win"]
    bright = lambda c: lerp(t["mid"], c, w)
    p[R_WARM] = bright((255, 205, 110))
    p[R_CY] = bright((120, 215, 255))
    p[R_PINK] = bright((255, 110, 190))
    p[R_DIM_FAR], p[R_DIM_MID], p[R_DIM_NEAR] = t["dim"]
    neons = [(255, 70, 170), (60, 240, 255), (255, 60, 70), (90, 255, 160)]
    for i in range(4):
        p[R_NEON[i]] = lerp(t["mid"], neons[i], 0.4 + 0.6 * w)
    p[R_RIM] = lerp(t["mid"], (255, 70, 100), 0.5 + 0.5 * w)
    p[R_SPOKE] = lerp(t["mid"], (150, 44, 90), 0.6 + 0.4 * w)
    p[R_CAB] = (255, 235, 200)
    p[R_LEG] = lerp(t["near"], (40, 20, 60), 0.7)
    p[R_TWR] = lerp(t["near"], (70, 36, 100), 0.8)
    p[R_TWR2] = t["near"]
    p[R_TWRLIT] = bright((255, 214, 120))
    p[R_TWRRED] = (255, 60, 70)
    p[R_GL1] = bright((255, 205, 110))
    p[R_GL2] = bright((120, 215, 255))
    p[R_GL3] = bright((255, 150, 60))
    return p


def sky_layer(d, rng, W, base_y, hmin, hmax, wmin, wmax, wall, dim, litp, neon_p):
    x = 0
    while x < W:
        w = rng.randint(wmin, wmax)
        h = rng.randint(hmin, hmax)
        ops = [(x, base_y - h, x + w - 2, base_y, wall)]
        if h > 22 and rng.random() < 0.5:
            ops.append((x + w // 2 - 1, base_y - h - 4, x + w // 2, base_y - h, wall))
        for wy in range(base_y - h + 3, base_y - 2, 5):
            for wx in range(x + 2, x + w - 3, 4):
                v = rng.random()
                if v < litp:
                    r = rng.random()
                    c = R_WARM if r < 0.7 else (R_CY if rng.random() < 0.6 else R_PINK)
                    ops.append((wx, wy, wx + 1, wy + 1, c))
                elif v < litp + 0.18:
                    ops.append((wx, wy, wx + 1, wy + 1, dim))
        if neon_p and rng.random() < neon_p and h > 18:
            c = rng.choice(R_NEON)
            if rng.random() < 0.5:
                ops.append((x + w - 3, base_y - h + 4, x + w - 2, base_y - h + 4 + rng.randint(8, 18), c))
            else:
                ops.append((x + 1, base_y - h // 2, x + w - 4, base_y - h // 2 + 2, c))
        for off in (0, -W, W):
            for a, b, c2, e, col in ops:
                d.rectangle([a + off, b, c2 + off, e], fill=col)
        x += w


def ferris_idx(d, cx, cy, r):
    d.polygon([(cx, cy), (cx - r * 0.55, cy + r * 1.25), (cx - r * 0.38, cy + r * 1.25)], fill=R_LEG)
    d.polygon([(cx, cy), (cx + r * 0.55, cy + r * 1.25), (cx + r * 0.38, cy + r * 1.25)], fill=R_LEG)
    d.ellipse([cx - r, cy - r, cx + r, cy + r], outline=R_RIM, width=2)
    d.ellipse([cx - r * 0.55, cy - r * 0.55, cx + r * 0.55, cy + r * 0.55], outline=R_SPOKE, width=1)
    for i in range(12):
        a = i * math.pi / 6
        d.line([cx, cy, cx + math.cos(a) * r, cy + math.sin(a) * r], fill=R_SPOKE, width=1)
        d.rectangle([cx + math.cos(a) * r - 1, cy + math.sin(a) * r - 1, cx + math.cos(a) * r + 1, cy + math.sin(a) * r + 1], fill=R_CAB)


def tower_idx(d, cx, base_y, h):
    d.polygon([(cx - 9, base_y), (cx - 1, base_y - h), (cx + 1, base_y - h), (cx + 9, base_y)], fill=R_TWR)
    d.polygon([(cx - 4, base_y), (cx - 1, base_y - h * 0.5), (cx + 1, base_y - h * 0.5), (cx + 4, base_y)], fill=R_TWR2)
    for yy in range(int(h * 0.15), int(h), 7):
        wdt = 9 * (1 - yy / h)
        d.rectangle([cx - wdt, base_y - yy, cx - wdt + 1, base_y - yy + 1], fill=R_TWRLIT)
        d.rectangle([cx + wdt - 1, base_y - yy, cx + wdt, base_y - yy + 1], fill=R_TWRLIT)
    d.rectangle([cx - 5, base_y - h * 0.62, cx + 5, base_y - h * 0.62 + 4], fill=R_TWRRED)
    d.rectangle([cx - 1, base_y - h - 5, cx, base_y - h], fill=R_TWRRED)


def make_sky():
    W, H, HOR = 480, 112, 79
    rng = random.Random(486)
    img = Image.new("P", (W, H), 0)
    px = img.load()
    for y in range(H):
        for x in range(W):
            if y < HOR:
                pos = y * (K - 1) / (HOR - 1)
                i = min(K - 2, int(pos))
                px[x, y] = 1 + (i + 1 if (pos - i) * 16 > BAYER[y % 4][x % 4] else i)
            else:
                pos = (y - HOR) * 5 / (H - 1 - HOR)
                i = min(4, int(pos))
                px[x, y] = R_HAZE0 + (i + 1 if (pos - i) * 16 > BAYER[y % 4][x % 4] else i)
    d = ImageDraw.Draw(img)
    for _ in range(70):
        d.point((rng.randrange(W), rng.randrange(2, 44)), fill=R_STAR if rng.random() < 0.6 else R_STAR2)
    d.ellipse([340, 14, 360, 34], fill=R_MOON)
    d.ellipse([348, 18, 354, 24], fill=R_MOON2)
    sky_layer(d, rng, W, HOR, 12, 30, 7, 16, R_FAR, R_DIM_FAR, 0.0, 0)
    ferris_idx(d, 120, 50, 24)
    tower_idx(d, 300, HOR, 54)
    sky_layer(d, rng, W, HOR, 18, 44, 7, 17, R_MID, R_DIM_MID, 0.30, 0.10)
    sky_layer(d, rng, W, HOR + 2, 24, 58, 8, 20, R_NEAR, R_DIM_NEAR, 0.34, 0.14)
    d.rectangle([0, HOR + 1, W, HOR + 8], fill=R_NEAR)
    for _ in range(120):
        d.point((rng.randrange(W), rng.randrange(HOR + 2, HOR + 9)), fill=rng.choice([R_GL1, R_GL2, R_GL3]))
    return W, H, np.array(img).astype(np.uint8)


# ----------------------------------------------------------------------------- title / hub art
def big_car_layer(S, pal, yaw=0, style="ef"):
    SS = 4
    w, h = int(64 * S) + 8, int(34 * S) + 8
    im = Image.new("RGBA", (w * SS, h * SS), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    f = S * SS
    rear_car(d, lambda x, y: (x * f + 4 * SS, y * f + 4 * SS), f, pal, yaw, style)
    return im.resize((w, h), Image.LANCZOS)


def draw_text_grad(img, text, font, xy, top, bot, outline, glow, stroke=2, spacing=0):
    W, H = img.size
    m = Image.new("L", (W, H), 0)
    md = ImageDraw.Draw(m)
    full = Image.new("L", (W, H), 0)
    fd = ImageDraw.Draw(full)
    x, y = xy
    for ch in text:
        md.text((x, y), ch, font=font, fill=255)
        fd.text((x, y), ch, font=font, fill=255, stroke_width=stroke)
        x += font.getlength(ch) + spacing
    g = Image.new("RGB", (W, H))
    gp = g.load()
    bbox = m.getbbox() or (0, 0, W, H)
    for yy in range(H):
        t = max(0.0, min(1.0, (yy - bbox[1]) / max(1, bbox[3] - bbox[1])))
        c = lerp(top, bot, t)
        for xx in range(W):
            gp[xx, yy] = c
    halo = full.filter(ImageFilter.GaussianBlur(3))
    img.paste(Image.new("RGB", (W, H), glow), (0, 0), halo.point(lambda v: int(v * 0.8)))
    img.paste(Image.new("RGB", (W, H), outline), (0, 0), full)
    img.paste(g, (0, 0), m)


def text_width(text, font, spacing=0):
    return sum(font.getlength(c) + spacing for c in text) - spacing


def rgb_layer(name):
    """Render the DUSK sky + skyline as RGB for the title (same art, same themed palette)."""
    W, H, idx = make_sky()
    pal = theme_palette(name)
    return W, H, idx, pal


def make_title():
    W, H = 240, 160
    HOR = 99
    img = Image.new("RGB", (W, H))
    px = img.load()
    stops = [(0, (6, 4, 28)), (36, (42, 14, 76)), (62, (150, 36, 112)), (82, (255, 96, 92)), (HOR, (255, 170, 94))]
    for y in range(HOR + 1):
        for x in range(W):
            px[x, y] = grad(stops, y)
    d = ImageDraw.Draw(img)
    rng = random.Random(7)
    for _ in range(45):
        d.point((rng.randrange(W), rng.randrange(0, 40)), fill=(255, 255, 255))
    cx, cy, R = 120, 92, 36
    for y in range(cy - R, HOR + 1):
        for x in range(cx - R, cx + R + 1):
            if (x - cx) ** 2 + (y - cy) ** 2 <= R * R:
                px[x, y] = lerp((255, 232, 120), (255, 64, 150), (y - (cy - R)) / (2 * R))
    for i, y in enumerate(range(cy - 10, HOR + 1, 4)):
        for yy in range(y, min(HOR + 1, y + 1 + i // 2)):
            for x in range(cx - R, cx + R + 1):
                if (x - cx) ** 2 + (yy - cy) ** 2 <= R * R:
                    px[x, yy] = grad(stops, yy)
    # skyline drawn in RGB with the DUSK theme colours
    pal = theme_palette("DUSK")
    img2 = Image.new("P", (W, H), 0)
    mask_layer = Image.new("P", (W, H), 0)
    md = ImageDraw.Draw(mask_layer)
    r2 = random.Random(21)
    sky_layer(md, r2, W, HOR, 10, 26, 6, 13, R_FAR, R_DIM_FAR, 0.0, 0)
    ferris_idx(md, 52, 74, 19)
    tower_idx(md, 196, HOR, 44)
    sky_layer(md, r2, W, HOR, 16, 36, 6, 14, R_MID, R_DIM_MID, 0.30, 0.12)
    sky_layer(md, r2, W, HOR + 1, 18, 40, 7, 15, R_NEAR, R_DIM_NEAR, 0.34, 0.14)
    ma = np.array(mask_layer)
    arr = np.array(img)
    for y in range(H):
        for x in range(W):
            if ma[y, x]:
                arr[y, x] = pal[ma[y, x]]
    img = Image.fromarray(arr)
    d = ImageDraw.Draw(img)
    d.rectangle([0, HOR + 1, W, H], fill=(12, 8, 26))
    d.polygon([(118, HOR + 1), (122, HOR + 1), (240, H), (0, H)], fill=(40, 40, 58))
    d.line([(118, HOR + 1), (0, H)], fill=(255, 70, 170), width=2)
    d.line([(122, HOR + 1), (240, H)], fill=(255, 70, 170), width=2)
    for i in range(1, 11):
        t0, t1 = (i / 11.0) ** 1.7, ((i + 0.45) / 11.0) ** 1.7
        y0, y1 = HOR + 1 + (H - HOR) * t0, HOR + 1 + (H - HOR) * t1
        w0, w1 = 0.6 + 3.0 * t0, 0.6 + 3.0 * t1
        d.polygon([(120 - w0, y0), (120 + w0, y0), (120 + w1, y1), (120 - w1, y1)], fill=(250, 200, 80))
    for i in range(2, 9):
        t = (i / 9.0) ** 1.8
        y = HOR + 1 + (H - HOR) * t
        for sgn in (-1, 1):
            x = 120 + sgn * (6 + 118 * t + 5 * t)
            ph = 4 + 16 * t
            d.line([(x, y), (x, y - ph)], fill=(70, 72, 90), width=1)
            r = 1 + 2.4 * t
            d.ellipse([x - r, y - ph - r, x + r, y - ph + r], fill=(255, 190, 90))
            d.ellipse([x - r * 2, y - ph - r * 2, x + r * 2, y - ph + r * 2], outline=(150, 86, 46))
    glow = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    ImageDraw.Draw(glow).ellipse([66, 118, 174, 150], fill=(255, 30, 50, 120))
    glow = glow.filter(ImageFilter.GaussianBlur(7))
    img.paste(glow, (0, 0), glow)
    car = big_car_layer(1.78, PLAYER_BRAKE_PAL)
    img.paste(car, (120 - car.width // 2, 142 - car.height + 4), car)
    fL = ImageFont.truetype(F_SANS_BO, 27)
    tw = text_width("KANJO RACER:", fL, 0)
    draw_text_grad(img, "KANJO RACER:", fL, ((W - tw) / 2, 6), (255, 110, 200), (255, 214, 96), (14, 4, 36), (70, 220, 255))
    # second title line in Japanese: 高速ループ ("Expressway Loop")
    fJ = ImageFont.truetype(F_CJK_BOLD, 21, index=0)
    jt = "\u9ad8\u901f\u30eb\u30fc\u30d7"
    jw = text_width(jt, fJ, 3)
    draw_text_grad(img, jt, fJ, ((W - jw) / 2, 34), (120, 235, 255), (255, 120, 210), (14, 4, 36), (255, 60, 150), stroke=2, spacing=3)
    # quiet signature: tiny bear + dim "for Collin" in the bottom corner
    d2 = ImageDraw.Draw(img)
    fm = ImageFont.truetype(F_MONO_B, 7)
    d2.text((19, 151), "for Collin", font=fm, fill=(112, 86, 140))
    bear_head(d2, 11, 156, 3.4, (120, 84, 70), (80, 56, 50), (150, 120, 110), (30, 20, 40))
    img.save(os.path.join(PREV, "title.png"))
    return img


def make_hub():
    W, H = 240, 160
    img = Image.new("RGB", (W, H))
    px = img.load()
    for y in range(H):
        c = lerp((16, 12, 34), (34, 24, 58), y / 112) if y < 112 else lerp((30, 26, 46), (12, 10, 22), (y - 112) / 48)
        for x in range(W):
            px[x, y] = c
    d = ImageDraw.Draw(img)
    for y in range(0, 40, 4):
        d.rectangle([0, y, W, y + 1], fill=(26, 20, 48))
    d.rectangle([0, 40, W, 42], fill=(255, 70, 170))
    d.rectangle([0, 112, W, 113], fill=(70, 220, 255))
    spot = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    ImageDraw.Draw(spot).polygon([(52, 43), (76, 43), (112, 138), (14, 138)], fill=(255, 230, 190, 52))
    spot = spot.filter(ImageFilter.GaussianBlur(3))
    img.paste(spot, (0, 0), spot)
    car = big_car_layer(1.7, PLAYER_PAL)
    img.paste(car, (62 - car.width // 2, 140 - car.height + 4), car)
    d.ellipse([14, 134, 110, 146], outline=(255, 70, 170))
    fs = ImageFont.truetype(F_SANS_B, 10)
    draw_text_grad(img, "BEAR PRODUCTS", fs, (26, 14), (255, 120, 210), (255, 214, 100), (14, 4, 36), (255, 60, 150), stroke=1, spacing=1)
    bear_head(d, 15, 22, 8, (158, 98, 52), (98, 60, 32), (228, 188, 134), (14, 6, 20))
    d.rounded_rectangle([120, 30, 234, 148], radius=5, fill=(8, 6, 24), outline=(70, 220, 255), width=2)
    d.rounded_rectangle([122, 32, 232, 146], radius=4, outline=(255, 70, 170), width=1)
    img.save(os.path.join(PREV, "hub.png"))
    return img


# ----------------------------------------------------------------------------- main
def pal256(pal_rgb):
    p = list(pal_rgb) + [(0, 0, 0)] * (256 - len(pal_rgb))
    return [b555(c) for c in p[:256]]


def main():
    blob, offs, sheet = [], {}, []

    def add(name, arr):
        offs[name] = len(blob) // 8
        blob.extend(tiles_from(arr))
        sheet.append((name, arr))

    offs["FONT"] = 0
    blob.extend(tiles_from(font_tiles()))
    assert len(blob) // 8 == 64
    for i, y in enumerate((0, 1, -1)):
        add("PLAYER%d" % i, sprite_idx(lambda d, T, S, y=y: rear_car(d, T, S, PLAYER_PAL, y), 64, 32, PLAYER_PAL))
    for nm, st in (("EF", "ef"), ("AE86", "ae86"), ("TRUCK", "truck")):
        add(nm, sprite_idx(lambda d, T, S, st=st: rear_car(d, T, S, PLAYER_PAL, 0, st), 64, 32, PLAYER_PAL))
    for nm, st in (("EF_S", "ef"), ("AE86_S", "ae86"), ("TRUCK_S", "truck")):
        add(nm, sprite_idx(lambda d, T, S, st=st: rear_car(d, T, S, PLAYER_PAL, 0, st), 32, 16, PLAYER_PAL))
    add("LAMP", lamp_sprite())
    add("BLDG_A", building_sprite(0, 11))
    add("BLDG_B", building_sprite(1, 29))
    add("BOARD", billboard_sprite())
    add("BANNER", banner_sprite())
    add("POST", post_sprite())
    total_tiles = len(blob) // 8
    assert total_tiles <= 512, total_tiles

    banks = {i: {0: (255, 0, 255)} for i in range(16)}
    banks[0] = {0: (255, 0, 255), 1: (255, 255, 255), 2: (18, 14, 40), 3: (70, 70, 98)}
    banks[15] = {0: (255, 0, 255), 1: (255, 224, 70), 2: (40, 20, 10), 3: (70, 70, 98)}
    banks[10] = {0: (255, 0, 255), 1: (70, 255, 110), 2: (10, 40, 20), 3: (70, 70, 98)}
    banks[11] = {0: (255, 0, 255), 1: (255, 224, 70), 2: (40, 30, 8), 3: (70, 70, 98)}
    banks[12] = {0: (255, 0, 255), 1: (255, 70, 70), 2: (40, 8, 8), 3: (70, 70, 98)}
    banks[1], banks[2] = PLAYER_PAL, PLAYER_BRAKE_PAL
    for bank, col in ((3, (40, 96, 230)), (4, (214, 40, 48)), (5, (240, 190, 40)), (6, (40, 168, 90)),
                      (7, (190, 194, 204)), (8, (44, 46, 58)), (9, (226, 228, 236))):
        banks[bank] = car_pal(col)
    banks[13], banks[14] = BILL_PAL, SCEN_PAL
    objpal = [b555(banks[b].get(i, (0, 0, 0))) for b in range(16) for i in range(16)]

    W, H, sky_idx = make_sky()
    title = make_title()
    hub = make_hub()
    t_idx, t_pal = index_image(title, 256)
    h_idx, h_pal = index_image(hub, 256)

    with open(os.path.join(SRC, "gfx_obj.c"), "w") as f:
        f.write("// GENERATED by tools/make_assets.py - do not edit\n#include \"gfx.h\"\n")
        emit_array(f, "unsigned int", "obj_tiles", blob, 8, "0x%08xu")
        emit_array(f, "unsigned short", "obj_pal", objpal, 16, "0x%04x")
    with open(os.path.join(SRC, "gfx_sky.c"), "w") as f:
        f.write("// GENERATED by tools/make_assets.py - do not edit\n#include \"gfx.h\"\n")
        for i, name in enumerate(("DUSK", "NIGHT", "DAWN")):
            emit_array(f, "unsigned short", "sky_pal_%d" % i, [b555(c) for c in theme_palette(name)], 16, "0x%04x")
        emit_array(f, "unsigned char", "sky_img", [int(v) for v in sky_idx.flatten()], 32)
    with open(os.path.join(SRC, "gfx_screens.c"), "w") as f:
        f.write("// GENERATED by tools/make_assets.py - do not edit\n#include \"gfx.h\"\n")
        emit_array(f, "unsigned short", "title_pal", pal256(t_pal), 16, "0x%04x")
        emit_array(f, "unsigned char", "title_img", [int(v) for v in t_idx.flatten()], 32)
        emit_array(f, "unsigned short", "hub_pal", pal256(h_pal), 16, "0x%04x")
        emit_array(f, "unsigned char", "hub_img", [int(v) for v in h_idx.flatten()], 32)
    with open(os.path.join(SRC, "gfx.h"), "w") as f:
        f.write("// GENERATED by tools/make_assets.py - do not edit\n#ifndef GFX_H\n#define GFX_H\n")
        f.write("#define SKY_W %d\n#define SKY_H %d\n#define SKY_NCOL 96\n" % (W, H))
        f.write("#define OBJ_TILE_COUNT %d\n" % total_tiles)
        for k, v in offs.items():
            f.write("#define T_%s %d\n" % (k, v))
        f.write("extern const unsigned int obj_tiles[];\nextern const unsigned short obj_pal[];\n")
        f.write("extern const unsigned short sky_pal_0[];\nextern const unsigned short sky_pal_1[];\n")
        f.write("extern const unsigned short sky_pal_2[];\nextern const unsigned char sky_img[];\n")
        f.write("extern const unsigned short title_pal[];\nextern const unsigned char title_img[];\n")
        f.write("extern const unsigned short hub_pal[];\nextern const unsigned char hub_img[];\n#endif\n")

    # ---- previews
    for name in ("DUSK", "NIGHT", "DAWN"):
        p = theme_palette(name)
        im = Image.new("RGB", (W, H))
        px = im.load()
        for y in range(H):
            for x in range(W):
                px[x, y] = p[sky_idx[y, x]]
        im.save(os.path.join(PREV, "sky_%s.png" % name.lower()))
    sh = Image.new("RGB", (900, 400), (60, 62, 74))
    x = y = 4
    rowh = 0
    bankfor = {"PLAYER0": 1, "PLAYER1": 1, "PLAYER2": 1, "EF": 3, "AE86": 4, "TRUCK": 9, "EF_S": 5, "AE86_S": 6,
               "TRUCK_S": 7, "LAMP": 14, "BLDG_A": 14, "BLDG_B": 14, "BOARD": 13, "BANNER": 13, "POST": 13}
    for name, arr in sheet:
        pal = banks[bankfor[name]]
        im = Image.new("RGBA", (arr.shape[1], arr.shape[0]))
        ip = im.load()
        for yy in range(arr.shape[0]):
            for xx in range(arr.shape[1]):
                v = int(arr[yy, xx])
                ip[xx, yy] = (0, 0, 0, 0) if v == 0 else pal.get(v, (255, 0, 255)) + (255,)
        im = im.resize((im.width * 2, im.height * 2), Image.NEAREST)
        if x + im.width > 896:
            x = 4
            y += rowh + 4
            rowh = 0
        sh.paste(im, (x, y), im)
        x += im.width + 4
        rowh = max(rowh, im.height)
    sh.save(os.path.join(PREV, "sprites.png"))
    print("tiles used:", total_tiles, "/ 512   title colors:", len(t_pal), " hub colors:", len(h_pal))


if __name__ == "__main__":
    main()
