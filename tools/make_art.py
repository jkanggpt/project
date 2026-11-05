#!/usr/bin/env python3
"""
make_art.py — 지형 아틀라스와 성채 임시 그림을 만듭니다.

Kenney Tiny Town(CC0)에 없는 타일(물, 바위 지대, 마정석 광맥)은 같은 팔레트로 절차적으로 그리고,
있는 타일(풀밭, 나무, 성채 벽)은 Kenney 타일을 그대로 합성합니다.

결과 파일 (커밋합니다):
  game/assets/tiles/terrain.png    16×16 타일, 4열(변형) × 5행(지형: 풀밭, 숲, 바위, 물, 마정석)
  game/assets/buildings/keep.png   48×48 성채 (3×3 타일)

진짜 그림으로 바꾸고 싶으면 같은 배치의 PNG로 위 파일만 바꾸면 됩니다 (코드는 그대로).
다시 만들기: python3 tools/make_art.py   (Pillow 필요: pip install pillow)
"""
import os
import random

from PIL import Image

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
KENNEY_TOWN = os.path.join(ROOT, "game/assets/third_party/kenney/tiny-town/Tilemap/tilemap_packed.png")
OUT_TERRAIN = os.path.join(ROOT, "game/assets/tiles/terrain.png")
OUT_KEEP = os.path.join(ROOT, "game/assets/buildings/keep.png")

T = 16  # 타일 크기
VARIANTS = 4

# Kenney Tiny Town 팔레트에서 뽑은 색
OUTLINE = (0x3F, 0x26, 0x31, 255)
GRASS = (0x84, 0xC6, 0x69, 255)
GRASS_DARK = (0x65, 0xA5, 0x56, 255)
STONE_LIGHT = (0xC0, 0xCB, 0xDC, 255)
STONE_MID = (0x8B, 0x9B, 0xB4, 255)
STONE_DARK = (0x5A, 0x69, 0x88, 255)
# 물과 마정석은 Kenney 우물(파랑)과 어울리게 고른 색
WATER = (0x3B, 0x8F, 0xD9, 255)
WATER_DARK = (0x2E, 0x74, 0xC0, 255)
WATER_LIGHT = (0x7C, 0xC6, 0xF2, 255)
WATER_FOAM = (0xCF, 0xEE, 0xFF, 255)
MANA = (0x9B, 0x5D, 0xE5, 255)
MANA_LIGHT = (0xD6, 0xA6, 0xFF, 255)
MANA_GLOW = (0xF4, 0xE4, 0xFF, 255)
MANA_DARK = (0x5E, 0x3A, 0xA8, 255)


def kenney_tile(sheet, index):
    col, row = index % 12, index // 12
    return sheet.crop((col * T, row * T, col * T + T, row * T + T))


def solid(color):
    return Image.new("RGBA", (T, T), color)


def over(base, top):
    out = base.copy()
    out.alpha_composite(top)
    return out


def grass_variants(sheet):
    plain = kenney_tile(sheet, 0)
    tufts = kenney_tile(sheet, 1)
    flowers = kenney_tile(sheet, 2)
    return [plain, tufts, flowers, tufts.transpose(Image.FLIP_LEFT_RIGHT)]


def forest_variants(sheet):
    # 타일 4, 5 의 나무는 속이 풀밭과 같은 색이라 풀 위에서는 테두리만 보이므로 16, 28 만 씁니다
    base = kenney_tile(sheet, 0)
    round_tree = kenney_tile(sheet, 16)
    pine = kenney_tile(sheet, 28)
    return [
        over(base, round_tree),
        over(base, pine),
        over(base, round_tree.transpose(Image.FLIP_LEFT_RIGHT)),
        over(base, pine.transpose(Image.FLIP_LEFT_RIGHT)),
    ]


def draw_boulder(img, cx, cy, rx, ry):
    """테두리, 밝은 면, 그림자가 있는 픽셀 바위 하나"""
    px = img.load()
    inside = set()
    for y in range(T):
        for x in range(T):
            dx = (x + 0.5 - cx) / rx
            dy = (y + 0.5 - cy) / ry
            if dx * dx + dy * dy <= 1.0:
                inside.add((x, y))
    for (x, y) in inside:
        dx = (x + 0.5 - cx) / rx
        dy = (y + 0.5 - cy) / ry
        if dx + dy < -0.7:
            px[x, y] = STONE_LIGHT
        elif dx + dy > 0.6:
            px[x, y] = STONE_DARK
        else:
            px[x, y] = STONE_MID
    for (x, y) in inside:
        for nx, ny in ((x + 1, y), (x - 1, y), (x, y + 1), (x, y - 1)):
            if 0 <= nx < T and 0 <= ny < T and (nx, ny) not in inside:
                px[nx, ny] = OUTLINE


def rock_variants(sheet, rng):
    layouts = [
        [(8, 8.5, 5.5, 4.5)],
        [(5, 6, 3.5, 3), (11, 11, 3.5, 3)],
        [(4.5, 11, 3, 2.5), (11, 5, 3.5, 3), (11.5, 12, 2.5, 2)],
        [(7, 7, 4.5, 4), (12.5, 12.5, 2.5, 2)],
    ]
    out = []
    for layout in layouts:
        img = kenney_tile(sheet, 0).copy()
        px = img.load()
        # 자갈 몇 개
        for _ in range(4):
            x, y = rng.randrange(1, T - 1), rng.randrange(1, T - 1)
            px[x, y] = STONE_MID
        for (cx, cy, rx, ry) in layout:
            draw_boulder(img, cx, cy, rx, ry)
        out.append(img)
    return out


def water_variants(rng):
    out = []
    for v in range(VARIANTS):
        img = solid(WATER)
        px = img.load()
        # 잔물결: 짧은 가로 줄. 타일 가장자리를 넘지 않게 두어 이웃 타일과 자연스럽게 이어지게 합니다
        for _ in range(2 + v % 2):
            length = rng.randrange(3, 6)
            x = rng.randrange(1, T - length - 1)
            y = rng.randrange(2, T - 2)
            for i in range(length):
                px[x + i, y] = WATER_LIGHT
            px[x + 1, y + 1] = WATER_DARK
            if length > 3:
                px[x + length - 2, y + 1] = WATER_DARK
        if v == 3:
            px[rng.randrange(2, T - 2), rng.randrange(2, T - 2)] = WATER_FOAM
        out.append(img)
    return out


def draw_crystal(img, x, base_y, height, width=3):
    """세로로 솟은 마정석 결정 하나 (테두리 포함)"""
    px = img.load()
    top = base_y - height
    cells = set()
    for y in range(top, base_y + 1):
        # 꼭대기는 뾰족하게
        w = width if y > top + 1 else 1
        start = x + (width - w) // 2
        for xx in range(start, start + w):
            cells.add((xx, y))
    for (cx, cy) in cells:
        if 0 <= cx < T and 0 <= cy < T:
            px[cx, cy] = MANA_LIGHT if cx == x else (MANA_DARK if cx == x + width - 1 else MANA)
    if 0 <= top < T:
        px[x + width // 2, top] = MANA_GLOW
    for (cx, cy) in cells:
        for nx, ny in ((cx + 1, cy), (cx - 1, cy), (cx, cy - 1), (cx, cy + 1)):
            if 0 <= nx < T and 0 <= ny < T and (nx, ny) not in cells:
                px[nx, ny] = OUTLINE


def mana_variants(sheet):
    layouts = [
        [(6, 12, 7)],
        [(3, 13, 5), (9, 11, 6)],
        [(2, 12, 4), (6, 13, 8), (11, 12, 5)],
        [(8, 10, 5), (4, 14, 4)],
    ]
    out = []
    for layout in layouts:
        img = kenney_tile(sheet, 0).copy()
        px = img.load()
        # 광맥 둘레의 땅을 살짝 물들입니다
        for y in range(8, T):
            for x in range(T):
                if (x * 7 + y * 3) % 5 == 0:
                    px[x, y] = GRASS_DARK
        for (x, base_y, height) in layout:
            draw_crystal(img, x, base_y, height)
        out.append(img)
    return out


def make_keep(sheet):
    layout = [[99, 100, 101], [126, 126, 126], [126, 125, 126]]
    img = Image.new("RGBA", (T * 3, T * 3), (0, 0, 0, 0))
    for r, row in enumerate(layout):
        for c, index in enumerate(row):
            tile = kenney_tile(sheet, index)
            img.alpha_composite(tile, (c * T, r * T))
    return img


def main():
    rng = random.Random(20261105)  # 다시 만들어도 같은 그림이 나오게 고정 시드
    sheet = Image.open(KENNEY_TOWN).convert("RGBA")
    rows = [
        grass_variants(sheet),
        forest_variants(sheet),
        rock_variants(sheet, rng),
        water_variants(rng),
        mana_variants(sheet),
    ]
    atlas = Image.new("RGBA", (T * VARIANTS, T * len(rows)), (0, 0, 0, 0))
    for r, variants in enumerate(rows):
        assert len(variants) == VARIANTS
        for c, tile in enumerate(variants):
            atlas.alpha_composite(tile, (c * T, r * T))
    os.makedirs(os.path.dirname(OUT_TERRAIN), exist_ok=True)
    os.makedirs(os.path.dirname(OUT_KEEP), exist_ok=True)
    atlas.save(OUT_TERRAIN)
    make_keep(sheet).save(OUT_KEEP)
    print("wrote", os.path.relpath(OUT_TERRAIN, ROOT), "and", os.path.relpath(OUT_KEEP, ROOT))


if __name__ == "__main__":
    main()
