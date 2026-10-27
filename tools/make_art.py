"""Generate Nexus release art for Boss Sense (1920x1080 PNG)."""

import math
import os
import random

from PIL import Image, ImageDraw, ImageFilter, ImageFont

W, H = 1920, 1080
OUT = r"D:\OpenCode666\BossSense\art"

GOLD = (212, 175, 55)
GOLD_LIGHT = (242, 219, 150)
GOLD_DARK = (150, 108, 34)
TEXT = (234, 226, 210)
MUTED = (166, 156, 140)
DARK_RED = (96, 24, 18)
BLACK = (8, 7, 6)

F_TITLE = r"C:\Windows\Fonts\georgiab.ttf"
F_SERIF = r"C:\Windows\Fonts\georgia.ttf"
F_CN = r"C:\Windows\Fonts\msyh.ttc"
F_CN_B = r"C:\Windows\Fonts\msyhbd.ttc"


def font(path, size):
    return ImageFont.truetype(path, size)


def background(seed=1):
    random.seed(seed)
    img = Image.new("RGB", (W, H), BLACK)
    d = ImageDraw.Draw(img)

    for y in range(H):
        t = y / H
        r = int(10 + 26 * t)
        g = int(9 + 9 * t)
        b = int(8 + 7 * t)
        d.line([(0, y), (W, y)], fill=(r, g, b))

    # warm radial glow, upper centre
    rad = Image.radial_gradient("L").resize((W, H))
    red = Image.new("RGB", (W, H), (74, 20, 14))
    img = Image.composite(red, img, rad.point(lambda v: int(v * 0.30)))

    # vignette
    vig = Image.radial_gradient("L").resize((W, H)).point(lambda v: int(200 - v * 0.55))
    dark = Image.new("RGB", (W, H), (0, 0, 0))
    img = Image.composite(dark, img, vig)

    # fine noise
    px = img.load()
    for _ in range(9000):
        x = random.randrange(W)
        y = random.randrange(H)
        r, g, b = px[x, y]
        n = random.randint(-6, 8)
        px[x, y] = (max(0, min(255, r + n)), max(0, min(255, g + n)), max(0, min(255, b + n)))

    # border
    d = ImageDraw.Draw(img)
    d.rectangle([38, 38, W - 39, H - 39], outline=GOLD_DARK, width=2)
    d.rectangle([52, 52, W - 53, H - 53], outline=(110, 84, 34), width=1)

    # corner diamonds
    for cx, cy in ((38, 38), (W - 39, 38), (38, H - 39), (W - 39, H - 39)):
        d.polygon([(cx, cy - 9), (cx + 9, cy), (cx, cy + 9), (cx - 9, cy)], fill=GOLD)

    return img


def gradient_text(img, text, fnt, x, y, top=GOLD_LIGHT, bottom=GOLD_DARK, shadow=True):
    bbox = fnt.getbbox(text)
    tw, th = bbox[2] - bbox[0], bbox[3] - bbox[1]
    pad = 12
    mask = Image.new("L", (tw + pad * 2, th + pad * 2), 0)
    md = ImageDraw.Draw(mask)
    md.text((pad - bbox[0], pad - bbox[1]), text, font=fnt, fill=255)

    grad = Image.new("RGB", mask.size)
    gd = ImageDraw.Draw(grad)
    for yy in range(mask.size[1]):
        t = yy / max(1, mask.size[1] - 1)
        col = tuple(int(top[i] + (bottom[i] - top[i]) * t) for i in range(3))
        gd.line([(0, yy), (mask.size[0], yy)], fill=col)

    if shadow:
        sh = Image.new("RGBA", mask.size, (0, 0, 0, 0))
        sh.paste((0, 0, 0, 170), (6, 8), mask)
        img.paste(sh, (x - pad + 4, y - pad + 4), sh)
    img.paste(grad, (x - pad, y - pad), mask)


def center(draw, text, fnt, y, fill=TEXT, w=W, shadow=True):
    bbox = draw.textbbox((0, 0), text, font=fnt)
    x = (w - (bbox[2] - bbox[0])) // 2
    if shadow:
        draw.text((x + 2, y + 2), text, font=fnt, fill=(0, 0, 0))
    draw.text((x, y), text, font=fnt, fill=fill)
    return bbox[2] - bbox[0]


def line(draw, y, width=520, color=GOLD_DARK):
    x = (W - width) // 2
    draw.line([(x, y), (x + width, y)], fill=color, width=2)
    draw.polygon([(x - 10, y), (x, y - 6), (x + 10, y), (x, y + 6)], fill=GOLD)


def bullet(img, x, y, text, fnt, sub=None, sub_fnt=None):
    d = ImageDraw.Draw(img)
    d.polygon([(x, y + 16), (x + 9, y + 25), (x, y + 34), (x - 9, y + 25)], fill=GOLD)
    d.text((x + 26, y), text, font=fnt, fill=TEXT)
    if sub:
        d.text((x + 26, y + 38), sub, font=sub_fnt, fill=MUTED)


def save(img, name):
    os.makedirs(OUT, exist_ok=True)
    path = os.path.join(OUT, name)
    img.save(path, "PNG", optimize=True)
    print(f"{path}  {os.path.getsize(path) / 1024:.0f} KB")


def boss_bar(img, x, y, w, h, pct, name, level):
    d = ImageDraw.Draw(img)
    glow = Image.new("RGBA", (w + 80, h + 80), (0, 0, 0, 0))
    gd = ImageDraw.Draw(glow)
    gd.rounded_rectangle([40, 40, 40 + w, 40 + h], radius=h // 2, fill=(190, 25, 18, 120))
    glow = glow.filter(ImageFilter.GaussianBlur(16))
    img.paste(glow, (x - 40, y - 40), glow)

    d.rounded_rectangle([x - 3, y - 3, x + w + 3, y + h + 3], radius=h // 2 + 3, outline=GOLD_DARK, width=2)
    d.rounded_rectangle([x, y, x + w, y + h], radius=h // 2, fill=(28, 6, 6))

    # fill, clipped to the rounded bar shape
    fw = int(w * pct)
    layer = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    ld = ImageDraw.Draw(layer)
    for i in range(fw):
        t = i / max(1, fw - 1)
        col = (int(115 + 95 * t), int(12 + 14 * t), int(10 + 10 * t), 255)
        ld.line([(i, 0), (i, h)], fill=col)
    mask = Image.new("L", (w, h), 0)
    ImageDraw.Draw(mask).rounded_rectangle([0, 0, w - 1, h - 1], radius=h // 2, fill=255)
    img.paste(layer, (x, y), mask)

    f_name = font(F_TITLE, 34)
    f_lvl = font(F_SERIF, 26)
    d.text((x + 6, y - 52), name, font=f_name, fill=TEXT)
    lvl_w = d.textbbox((0, 0), level, font=f_lvl)[2]
    d.text((x + w - lvl_w - 6, y - 46), level, font=f_lvl, fill=GOLD_LIGHT)


def header():
    img = background(1)
    d = ImageDraw.Draw(img)

    aura = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    ad = ImageDraw.Draw(aura)
    ad.ellipse([W // 2 - 700, 30, W // 2 + 700, 640], fill=(150, 20, 12, 90))
    aura = aura.filter(ImageFilter.GaussianBlur(100))
    img.paste(aura, (0, 0), aura)

    f_top = font(F_SERIF, 26)
    f_foot = font(F_SERIF, 26)

    center(d, "S K S E   P L U G I N   ·   T R U E H U D", f_top, 80, GOLD)

    title_font = font(F_TITLE, 176)
    bbox = title_font.getbbox("BOSS SENSE")
    gradient_text(img, "BOSS SENSE", title_font, (W - (bbox[2] - bbox[0])) // 2, 160)

    center(d, "Every boss. No INI lists.", font(F_SERIF, 54), 420, TEXT)
    line(d, 512, 640)

    boss_bar(img, 400, 600, 1120, 34, 0.72, "MOLAG BAL", "LV 85")

    tags = ["NO INI LISTS", "NO ESP", "8 LANGUAGES"]
    tag_font = font(F_SERIF, 30)
    widths = [d.textbbox((0, 0), t, font=tag_font)[2] for t in tags]
    gap = 44
    total = sum(widths) + gap * 2 * (len(tags) - 1)
    x = (W - total) // 2
    for i, t in enumerate(tags):
        d.text((x, 724), t, font=tag_font, fill=GOLD_LIGHT)
        x += widths[i] + gap
        if i < len(tags) - 1:
            d.polygon([(x, 739), (x + 8, 747), (x, 755), (x - 8, 747)], fill=GOLD)
            x += gap

    center(d, "DELETE EVERY BOSS INI   ·   KEEP ONLY THE REAL BOSSES", f_foot, 928, MUTED)
    center(d, "VIGILANT  ·  LEGACY OF THE DRAGONBORN  ·  WYRMFSTOOTH  ·  ANY FUTURE QUEST MOD", font(F_SERIF, 24), 820, (150, 118, 78))
    save(img, "BossSense_01_Header.png")


def rules():
    img = background(2)
    d = ImageDraw.Draw(img)
    f_title = font(F_TITLE, 78)
    f_head = font(F_SERIF, 34)
    f_row = font(F_SERIF, 32)
    f_num = font(F_TITLE, 34)
    f_foot = font(F_SERIF, 28)

    center(d, "HOW A BOSS IS DECIDED", f_title, 96, GOLD_LIGHT)
    line(d, 210, 620)

    # left column
    lx = 150
    d.text((lx, 250), "STRONG EVIDENCE", font=f_head, fill=GOLD)
    d.text((lx, 292), "one is enough", font=font(F_SERIF, 24), fill=MUTED)
    d.line([(lx, 336), (lx + 620, 336)], fill=GOLD_DARK, width=1)
    rows_l = [("Boss race (dragons, priests)", "+100"),
              ("Location boss marker", "+100"),
              ("EditorID contains \"boss\"", "+90")]
    y = 362
    for label, score in rows_l:
        d.text((lx, y), label, font=f_row, fill=TEXT)
        tw = d.textbbox((0, 0), score, font=f_num)[2]
        d.text((lx + 620 - tw, y - 2), score, font=f_num, fill=GOLD_LIGHT)
        if label != rows_l[-1][0]:
            d.line([(lx, y + 50), (lx + 620, y + 50)], fill=(48, 38, 26), width=1)
        y += 62

    # right column
    rx = 1000
    d.text((rx, 250), "SMALL SIGNALS", font=f_head, fill=GOLD)
    d.text((rx, 292), "they add up", font=font(F_SERIF, 24), fill=MUTED)
    d.line([(rx, 336), (rx + 620, 336)], fill=GOLD_DARK, width=1)
    rows_r = [("Hand-authored NPC (not Lvl/Enc)", "+25"),
              ("Not from a leveled list", "+20"),
              ("Unique flag", "+20"),
              ("Level above the player", "+20"),
              ("Max health threshold", "+20"),
              ("Very aggressive + foolhardy", "+10"),
              ("Has a name", "+10")]
    y = 362
    for label, score in rows_r:
        d.text((rx, y), label, font=f_row, fill=TEXT)
        tw = d.textbbox((0, 0), score, font=f_num)[2]
        d.text((rx + 620 - tw, y - 2), score, font=f_num, fill=GOLD_LIGHT)
        if label != rows_r[-1][0]:
            d.line([(rx, y + 50), (rx + 620, y + 50)], fill=(48, 38, 26), width=1)
        y += 56

    # threshold band
    d.rectangle([150, 860, W - 150, 950], outline=GOLD_DARK, width=1)
    center(d, "THRESHOLD 70", font(F_TITLE, 46), 872, GOLD_LIGHT)
    center(d, "a generic leveled bandit stays at 50-60 and gets no bar", f_foot, 962, MUTED)
    save(img, "BossSense_02_Rules.png")


def features():
    img = background(3)
    d = ImageDraw.Draw(img)
    f_title = font(F_TITLE, 74)
    f_head = font(F_SERIF, 30)
    f_row = font(F_SERIF, 27)
    f_foot = font(F_SERIF, 27)

    center(d, "BUILT FOR MODDED LOAD ORDERS", f_title, 96, GOLD_LIGHT)
    line(d, 206, 780)

    cols = [
        (120, "8 INTERFACE LANGUAGES", [
            "English",
            "Chinese",
            "Russian",
            "Japanese",
            "Korean",
            "French",
            "German",
            "Spanish",
        ]),
        (720, "IN-GAME MENU", [
            "Threshold and weights",
            "Crosshair evaluation",
            "Never / Always boss",
            "Live re-evaluation",
            "Optional SKSE Menu Framework",
        ]),
        (1320, "MEASURED", [
            "16,145 NPC_ records",
            "0.40 s load-time scan",
            "0 hitches in HitchHunter",
            "One DLL, SE + AE",
            "GPL-3.0-or-later",
        ]),
    ]

    for x, head, rows in cols:
        d.text((x, 256), head, font=f_head, fill=GOLD)
        d.line([(x, 300), (x + 460, 300)], fill=GOLD_DARK, width=1)
        y = 330
        for row in rows:
            d.polygon([(x, y + 13), (x + 6, y + 19), (x, y + 25), (x - 6, y + 19)], fill=GOLD)
            d.text((x + 20, y), row, font=f_row, fill=TEXT)
            y += 58

    d.rectangle([120, 850, W - 120, 940], outline=GOLD_DARK, width=1)
    center(d, "SKSE64   ·   ADDRESS LIBRARY   ·   TRUEHUD 1.1.10+", font(F_SERIF, 32), 872, TEXT)
    center(d, "No ESP  ·  no scripts  ·  no save data  ·  delete the folder to uninstall", f_foot, 952, MUTED)
    save(img, "BossSense_03_Features.png")


if __name__ == "__main__":
    header()
    rules()
    features()
