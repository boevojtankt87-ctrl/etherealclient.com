# -*- coding: utf-8 -*-
"""Premium dark wallpapers via numpy: smooth aurora, circular bokeh, stars, mist."""
import math, random
import numpy as np
from PIL import Image, ImageDraw, ImageFilter

OUT = r"C:\Users\artemka1234\Desktop\brokeSOURSE\website\assets"

def base_gradient(W, H, top, bottom):
    t = np.linspace(0, 1, H, dtype=np.float32)[:, None, None]
    arr = (1 - t) * np.array(top, dtype=np.float32) + t * np.array(bottom, dtype=np.float32)
    return np.repeat(arr, W, axis=1)

def radial_blob(W, H, cx, cy, rx, ry, color, strength=1.0, power=2.0):
    ys, xs = np.mgrid[0:H, 0:W].astype(np.float32)
    d = ((xs - cx) / rx) ** 2 + ((ys - cy) / ry) ** 2
    mask = np.clip(1.0 - d, 0, 1) ** power * strength
    return mask[:, :, None] * np.array(color, dtype=np.float32)

def circle_bokeh(W, H, cx, cy, r, color, alpha=0.5):
    ys, xs = np.mgrid[0:H, 0:W].astype(np.float32)
    d = np.sqrt((xs - cx) ** 2 + (ys - cy) ** 2)
    mask = np.clip(1.0 - d / r, 0, 1) ** 1.8 * alpha
    return mask[:, :, None] * np.array(color, dtype=np.float32)

def aurora_curtains(W, H, colors, seed=0, n=18, y_peak=0.12):
    random.seed(seed)
    acc = np.zeros((H, W, 3), dtype=np.float32)
    c_top, c_bot = np.array(colors[0], dtype=np.float32), np.array(colors[1], dtype=np.float32)
    xs_row = np.arange(W, dtype=np.float32)
    ys = np.arange(H, dtype=np.float32)
    for i in range(n):
        x0 = random.uniform(0.02, 0.98) * W
        width = random.uniform(W * 0.025, W * 0.08)
        y_top = random.uniform(0, y_peak) * H
        y_bot = random.uniform(0.35, 0.85) * H
        amp = random.uniform(0.3, 1.0)
        sway = random.uniform(-W * 0.04, W * 0.04)
        freq = random.uniform(0.8, 1.6)
        phase = random.uniform(0, math.pi * 2)
        t = np.clip((ys - y_top) / max(1.0, (y_bot - y_top)), 0, 1)
        xc = x0 + sway * np.sin(t * math.pi * freq + phase)
        vfade = (1 - t) ** 0.85 * amp
        tt = np.clip(t * 1.25, 0, 1)[:, None]
        col = (1 - tt) * c_top + tt * c_bot
        # horizontal gaussian around xc, per-row (vectorized via broadcasting)
        dx = (xs_row[None, :] - xc[:, None]) / width
        hmask = np.exp(-dx * dx)
        layer = (hmask * vfade[:, None])[:, :, None] * col[:, None, :]
        acc += layer
    return acc

def add_stars(img_arr, n=180, region=(0.0, 0.0, 1.0, 0.55), seed=42, bright=1.0):
    H, W = img_arr.shape[:2]
    random.seed(seed)
    for _ in range(n):
        x = random.randint(int(region[0] * W), int(region[2] * W) - 1)
        y = random.randint(int(region[1] * H), int(region[3] * H) - 1)
        b = random.randint(80, 230) * bright
        r = random.choice([0, 0, 0, 1])
        img_arr[y, x, 0] = min(255, img_arr[y, x, 0] + b)
        img_arr[y, x, 1] = min(255, img_arr[y, x, 1] + b * 1.05)
        img_arr[y, x, 2] = min(255, img_arr[y, x, 2] + b * 0.95)
        if r and y + 1 < H and x + 1 < W:
            img_arr[y:y+2, x:x+2] += b * 0.35
    return img_arr

def soft_mist(W, H, y0, y1, color, alpha=0.4, seed=3, n=50):
    random.seed(seed)
    acc = np.zeros((H, W, 3), dtype=np.float32)
    col = np.array(color, dtype=np.float32)
    for _ in range(n):
        cx = random.uniform(-0.1, 1.1) * W
        cy = random.uniform(y0, y1) * H
        rx = random.uniform(W * 0.12, W * 0.35)
        ry = random.uniform(H * 0.04, H * 0.12)
        a = alpha * random.uniform(0.3, 1.0)
        acc += radial_blob(W, H, cx, cy, rx, ry, col, a, 1.6)
    return acc

def vignette_mask(W, H, strength=0.85):
    ys, xs = np.mgrid[0:H, 0:W].astype(np.float32)
    d = ((xs - W/2) / (W * 0.72)) ** 2 + ((ys - H/2) / (H * 0.72)) ** 2
    return np.clip(1.0 - d, 0, 1) ** 1.2 * strength + (1 - strength)

def to_image(arr):
    return Image.fromarray(np.clip(arr, 0, 255).astype(np.uint8))

def save_blur(arr, path, radius=0):
    img = to_image(arr)
    if radius > 0:
        img = img.filter(ImageFilter.GaussianBlur(radius))
    img.save(path, "PNG")
    print("saved", path, img.size)

def generate_hero(path, W=1920, H=1080, accent=(55, 185, 135), deep=(12, 45, 36), seed=0):
    arr = base_gradient(W, H, (3, 6, 5), (6, 11, 9))
    # horizon glow
    arr += radial_blob(W, H, W*0.5, H*0.58, W*0.6, H*0.28, deep, 0.95, 2.3)
    arr += radial_blob(W, H, W*0.22, H*0.42, W*0.28, H*0.2, deep, 0.5, 2.0)
    arr += radial_blob(W, H, W*0.78, H*0.38, W*0.25, H*0.18, deep, 0.45, 2.0)
    bright = tuple(min(255, c + 45) for c in accent)
    arr += aurora_curtains(W, H, (bright, deep), seed=seed, n=22, y_peak=0.1)
    arr += aurora_curtains(W, H, (accent, (8, 28, 22)), seed=seed + 11, n=12, y_peak=0.18)
    # circular bokeh (real circles)
    random.seed(seed + 7)
    for _ in range(22):
        cx = random.uniform(0, W)
        cy = random.uniform(H * 0.3, H * 0.95)
        r = random.uniform(W * 0.006, W * 0.028)
        a = random.uniform(0.04, 0.14)
        arr += circle_bokeh(W, H, cx, cy, r, np.array(accent, dtype=np.float32), a)
    arr = add_stars(arr, 200, (0, 0, 1, 0.5), seed=42 + seed)
    arr += soft_mist(W, H, 0.5, 0.9, (10, 18, 15), 0.5, seed=seed, n=40)
    arr += soft_mist(W, H, 0.7, 1.05, (5, 9, 8), 0.65, seed=seed + 1, n=35)
    # vignette multiply
    arr *= vignette_mask(W, H, 0.85)[:, :, None]
    save_blur(arr, path, 0)

def generate_card(path, W=640, H=400, accent=(55, 185, 135), seed=1):
    arr = base_gradient(W, H, (5, 9, 8), (9, 15, 13))
    arr += radial_blob(W, H, W*0.5, H*0.72, W*0.55, H*0.4, (16, 55, 44), 0.9, 2.2)
    bright = tuple(min(255, c + 50) for c in accent)
    arr += aurora_curtains(W, H, (bright, (10, 35, 28)), seed=seed, n=9, y_peak=0.05)
    random.seed(seed + 4)
    for _ in range(10):
        cx = random.uniform(0, W)
        cy = random.uniform(H * 0.35, H * 0.9)
        r = random.uniform(W * 0.01, W * 0.04)
        a = random.uniform(0.05, 0.16)
        arr += circle_bokeh(W, H, cx, cy, r, np.array(accent, dtype=np.float32), a)
    arr = add_stars(arr, 60, (0, 0, 1, 0.5), seed=seed + 2)
    arr *= vignette_mask(W, H, 0.65)[:, :, None]
    save_blur(arr, path, 0)

if __name__ == "__main__":
    generate_hero(OUT + r"\wp_ethereal.png", seed=0)
    generate_hero(OUT + r"\wp_ethereal_alt.png", accent=(100, 145, 255), deep=(18, 30, 62), seed=9)
    generate_card(OUT + r"\card_ethereal.png", seed=1)
