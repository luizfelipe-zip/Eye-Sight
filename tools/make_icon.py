"""Gera src/app.ico (ícone do .exe) com os mesmos pontinhos do ícone da bandeja.

Uso: python tools/make_icon.py src/app.ico docs/icon.png   (requer Pillow)
Os pontos são lidos de DOTS_SMALL e DOTS_LARGE em src/main.c.
"""
import re
import sys
from pathlib import Path

from PIL import Image, ImageDraw

main_c = (Path(__file__).resolve().parent.parent / "src" / "main.c").read_text(encoding="utf-8")


def dots(name):
    body = re.search(name + r"\[\]\[3\] = \{(.*?)\};", main_c).group(1)
    return [tuple(map(float, m)) for m in re.findall(r"\{(-?[\d.]+)f, (-?[\d.]+)f, (-?[\d.]+)f\}", body)]


SMALL, LARGE = dots("DOTS_SMALL"), dots("DOTS_LARGE")


def icon(s):
    S = s * 8
    im = Image.new("RGBA", (S, S), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    d.ellipse([S * 0.03, S * 0.03, S * 0.97, S * 0.97], fill=(32, 32, 32, 255))
    th, r, c = S * 0.06, S * 0.37, S / 2
    d.ellipse([c - r, c - r, c + r, c + r], outline=(255, 255, 255, 255), width=int(th))
    small = s < 32
    inner = r - th / 2
    base = (0.11 if small else 0.08) * inner
    for x, y, k in SMALL if small else LARGE:
        rr = base * k
        d.ellipse([c + x * inner - rr, c + y * inner - rr, c + x * inner + rr, c + y * inner + rr], fill=(255, 255, 255, 255))
    return im.resize((s, s), Image.LANCZOS)


sizes = [16, 20, 24, 32, 48, 64, 128, 256]
imgs = [icon(s) for s in sizes]
imgs[-1].save(sys.argv[1], sizes=[(s, s) for s in sizes], append_images=imgs[:-1])
if len(sys.argv) > 2:
    imgs[-1].save(sys.argv[2])
