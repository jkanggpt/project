#!/usr/bin/env python3
"""
make_art.py — 지형 아틀라스와 성채 임시 그림을 만듭니다.

Kenney Tiny Town(CC0)에 없는 타일(물, 바위 지대, 마정석 광맥)은 같은 팔레트로 절차적으로 그리고,
있는 타일(풀밭, 나무, 성채 벽)은 Kenney 타일을 그대로 합성합니다.

결과 파일 (커밋합니다):
  game/assets/tiles/terrain.png    16×16 타일, 4열(변형) × 5행(지형: 풀밭, 숲, 바위, 물, 마정석)
  game/assets/tiles/roads.png      도로 타일 2가지
  game/assets/tiles/snow.png       겨울 눈 덮개 4가지 (반투명, 땅 위에 겹쳐 그림)
  game/assets/buildings/<키>.png    건물 그림 (키는 data/buildings.def 의 [building 키])
  game/assets/buildings/construction_WxH.png  짓는 중 그림 (발자국 크기별)
  game/assets/units/villagers.png  주민 그림 8가지 (16×16 가로 띠)
  game/assets/units/enemies.png    적 그림 6가지 (data/enemies.def 순서: 도적, 두목, 고블린, 오우거, 병사, 궁수)
  game/assets/units/arrow.png      감시탑 화살 (오른쪽을 향함, Godot 이 돌려서 씀)
  game/assets/ui/res_*.png, job_*.png, bubble_*.png  자원·직업 아이콘, 상태 말풍선

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
KENNEY_DUNGEON = os.path.join(ROOT, "game/assets/third_party/kenney/tiny-dungeon/Tilemap/tilemap_packed.png")
OUT_BUILDINGS = os.path.join(ROOT, "game/assets/buildings")
OUT_UNITS = os.path.join(ROOT, "game/assets/units")
OUT_UI = os.path.join(ROOT, "game/assets/ui")
OUT_ROADS = os.path.join(ROOT, "game/assets/tiles/roads.png")
OUT_SNOW = os.path.join(ROOT, "game/assets/tiles/snow.png")

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


# ---- 건물 (마일스톤 2) -------------------------------------------------------

def tint(img, rgb, strength=0.65):
    """색이 있는 픽셀을 rgb 쪽으로 물들입니다 (테두리 색은 그대로)"""
    out = img.copy()
    px = out.load()
    for y in range(out.height):
        for x in range(out.width):
            r, g, b, a = px[x, y]
            if a == 0 or (r, g, b) == OUTLINE[:3]:
                continue
            light = (r + g + b) / (3 * 255)
            px[x, y] = (
                int(r * (1 - strength) + rgb[0] * light * 1.3 * strength) if True else r,
                int(g * (1 - strength) + rgb[1] * light * 1.3 * strength),
                int(b * (1 - strength) + rgb[2] * light * 1.3 * strength),
                a,
            )
    return out


def compose(sheet, grid, base=None):
    """타일 번호 격자를 한 장으로 합칩니다. None 은 비움. base 가 있으면 그 타일을 바닥에 깝니다"""
    h, w = len(grid), len(grid[0])
    img = Image.new("RGBA", (w * T, h * T), (0, 0, 0, 0))
    for r, row in enumerate(grid):
        for c, index in enumerate(row):
            if base is not None:
                img.alpha_composite(kenney_tile(sheet, base), (c * T, r * T))
            if index is not None:
                img.alpha_composite(kenney_tile(sheet, index), (c * T, r * T))
    return img


def paste_icon(img, icon, x, y):
    img.alpha_composite(icon, (x, y))
    return img


def draw_crops(img, x0, y0, w, h, rng):
    """밭 이랑과 싹"""
    px = img.load()
    for y in range(y0 + 3, y0 + h - 2, 4):
        for x in range(x0 + 2, x0 + w - 2):
            if (x + y) % 3 != 0:
                px[x, y] = GRASS_DARK
            if (x * 5 + y) % 7 == 0:
                px[x, y - 1] = (0x47, 0x9F, 0x4A, 255)
                px[x, y - 2] = (0xD8, 0xC0, 0x4A, 255) if rng.random() < 0.5 else (0x47, 0x9F, 0x4A, 255)


def crystal_icon(color=MANA, light=MANA_LIGHT):
    img = Image.new("RGBA", (T, T), (0, 0, 0, 0))
    draw_crystal(img, 5, 13, 9)
    if color != MANA:
        img = tint(img, color[:3], 0.8)
    return img


def make_buildings(town, dungeon, rng):
    b = {}
    red_roof_2 = compose(town, [[52, 54], [84, 87]])
    b["house"] = red_roof_2
    farm = compose(town, [[12, 13, 14], [24, 25, 26], [36, 37, 38]])
    draw_crops(farm, 0, 0, 48, 48, rng)
    farm.alpha_composite(kenney_tile(town, 81), (16, 32))  # 울타리
    b["farm"] = farm
    gatherer = tint(compose(town, [[52, 54], [72, 86]]), (0x58, 0xA0, 0x48))
    gatherer.alpha_composite(kenney_tile(town, 29), (16, 16))  # 버섯
    b["gatherer"] = gatherer
    lumber = tint(compose(town, [[52, 54], [72, 75]]), (0x8A, 0x5A, 0x34))
    lumber.alpha_composite(kenney_tile(town, 106), (0, 16))  # 통나무
    b["lumber"] = lumber
    quarry = compose(town, [[96, 98], [120, 122]])
    draw_boulder(quarry, 9, 10, 5, 4)
    quarry.alpha_composite(kenney_tile(town, 115), (16, 16))  # 곡괭이
    b["quarry"] = quarry
    mine = compose(town, [[99, 101], [112, 113]])
    mine.alpha_composite(crystal_icon(), (8, 6))
    b["mana_mine"] = mine
    b["warehouse"] = compose(town, [[48, 49, 50], [72, 74, 75]])
    market = tint(compose(town, [[52, 53, 54], [84, 107, 84]]), (0xE8, 0xB0, 0x40), 0.5)
    market.alpha_composite(kenney_tile(town, 93), (32, 16))
    b["market"] = market
    b["barracks"] = compose(town, [[48, 49, 50], [126, 125, 126]])
    b["tower"] = compose(town, [[103]])
    b["wall"] = compose(town, [[100]])
    b["gate"] = compose(town, [[112]])
    healer = compose(town, [[48, 50], [88, 91]])
    hp = healer.load()
    for i in range(-2, 3):  # 빨간 십자
        hp[16 + i, 7] = (0xD9, 0x3A, 0x3A, 255)
        hp[16, 7 + i] = (0xD9, 0x3A, 0x3A, 255)
    b["healer"] = healer
    mage = tint(compose(town, [[48, 50], [123, 124]]), (0x7A, 0x52, 0xC8), 0.6)
    mage.alpha_composite(crystal_icon(), (8, 10))
    b["mage_tower"] = mage
    return b


def make_construction(town, w, h):
    """짓는 중: 흙바닥 + 나무 비계(울타리) 테두리"""
    img = Image.new("RGBA", (w * T, h * T), (0, 0, 0, 0))
    for r in range(h):
        for c in range(w):
            img.alpha_composite(kenney_tile(town, 25), (c * T, r * T))
    for c in range(w):
        img.alpha_composite(kenney_tile(town, 81), (c * T, 0))
        img.alpha_composite(kenney_tile(town, 81), (c * T, (h - 1) * T))
    for r in range(h):
        img.alpha_composite(kenney_tile(town, 47 if r == 0 else 59), (0, r * T))
        img.alpha_composite(kenney_tile(town, 47 if r == 0 else 59), ((w - 1) * T, r * T))
    return img


def make_enemies(dungeon):
    """적 그림 6가지 (Tiny Dungeon). 순서는 core/src/defs.h 의 ENEMY_* 와 같음"""
    indices = [87, 109, 108, 110, 96, 97]
    strip = Image.new("RGBA", (T * len(indices), T), (0, 0, 0, 0))
    for i, index in enumerate(indices):
        strip.alpha_composite(kenney_tile(dungeon, index), (i * T, 0))
    return strip


def make_arrow():
    """화살: 8×8, 오른쪽을 향함 (나무 자루 + 쇠 촉 + 깃)"""
    img = Image.new("RGBA", (8, 8), (0, 0, 0, 0))
    px = img.load()
    shaft = (0x8B, 0x5A, 0x2B, 255)
    tip = (0xD8, 0xDC, 0xE8, 255)
    feather = (0xF0, 0xF0, 0xF0, 255)
    for x in range(1, 7):
        px[x, 4] = shaft
    px[7, 4] = tip
    px[6, 3] = tip
    px[6, 5] = tip
    px[0, 3] = feather
    px[0, 5] = feather
    px[1, 3] = feather
    px[1, 5] = feather
    return img


def make_villagers(dungeon):
    """주민 그림 8가지 (Tiny Dungeon 사람들)"""
    indices = [85, 86, 88, 98, 99, 100, 111, 112]
    strip = Image.new("RGBA", (T * len(indices), T), (0, 0, 0, 0))
    for i, index in enumerate(indices):
        strip.alpha_composite(kenney_tile(dungeon, index), (i * T, 0))
    return strip


def small_icon(draw):
    img = Image.new("RGBA", (T, T), (0, 0, 0, 0))
    draw(img.load())
    return img


def icon_food(px):
    # 빵 한 덩이
    for y in range(6, 12):
        for x in range(3, 13):
            if (x - 8) ** 2 / 25 + (y - 9) ** 2 / 9 <= 1:
                px[x, y] = (0xE3, 0x9B, 0x4A, 255) if y > 7 else (0xF2, 0xC2, 0x7A, 255)
    for x in (6, 8, 10):
        px[x, 8] = (0xB8, 0x6A, 0x2C, 255)


def icon_stone(px):
    for y in range(5, 13):
        for x in range(3, 13):
            if (x - 8) ** 2 / 25 + (y - 9) ** 2 / 16 <= 1:
                px[x, y] = STONE_LIGHT if x + y < 14 else (STONE_MID if x + y < 19 else STONE_DARK)


def icon_herbs(px):
    for y in range(3, 14):
        px[8, y] = (0x47, 0x9F, 0x4A, 255)
    for (x, y) in [(6, 5), (5, 6), (10, 7), (11, 8), (6, 9), (5, 10), (10, 11)]:
        px[x, y] = (0x65, 0xC0, 0x56, 255)
        px[x + (1 if x < 8 else -1), y] = (0x65, 0xC0, 0x56, 255)


def bubble(symbol_pixels, color):
    img = Image.new("RGBA", (10, 10), (0, 0, 0, 0))
    px = img.load()
    for y in range(10):
        for x in range(10):
            if (x - 4.5) ** 2 + (y - 4.5) ** 2 <= 20:
                px[x, y] = (255, 255, 255, 235)
    for (x, y) in symbol_pixels:
        px[x, y] = color
    return img


def make_ui(town, dungeon):
    ui = {}
    ui["res_gold"] = kenney_tile(town, 93)
    ui["res_mana"] = crystal_icon()
    ui["res_food"] = small_icon(icon_food)
    ui["res_wood"] = kenney_tile(town, 106)
    ui["res_stone"] = small_icon(icon_stone)
    ui["res_herbs"] = small_icon(icon_herbs)
    jobs = {
        "farm": kenney_tile(town, 116), "gatherer": kenney_tile(town, 29), "lumber": kenney_tile(town, 127),
        "quarry": kenney_tile(town, 115), "mana_mine": crystal_icon(), "market": kenney_tile(town, 93),
        "barracks": kenney_tile(dungeon, 104), "healer": kenney_tile(dungeon, 115),
        "mage_tower": kenney_tile(dungeon, 129),
    }
    for key, icon in jobs.items():
        ui["job_" + key] = icon
    z = [(3, 3), (4, 3), (5, 3), (6, 3), (5, 4), (4, 5), (3, 6), (4, 6), (5, 6), (6, 6)]
    ui["bubble_sleep"] = bubble(z, (0x3B, 0x5B, 0xD9, 255))
    food = [(4, 3), (5, 3), (3, 4), (4, 4), (5, 4), (6, 4), (3, 5), (4, 5), (5, 5), (6, 5), (4, 6), (5, 6)]
    ui["bubble_hungry"] = bubble(food, (0xE3, 0x9B, 0x4A, 255))
    angry = [(3, 3), (6, 3), (4, 4), (5, 4), (4, 5), (5, 5), (3, 6), (6, 6)]
    ui["bubble_angry"] = bubble(angry, (0xD9, 0x3A, 0x3A, 255))
    sick = [(4, 2), (5, 2), (4, 3), (5, 3), (2, 4), (3, 4), (4, 4), (5, 4), (6, 4), (7, 4), (4, 5), (5, 5),
            (4, 6), (5, 6)]
    ui["bubble_sick"] = bubble(sick, (0x4C, 0xAF, 0x50, 255))
    blocked = [(4, 2), (5, 2), (4, 3), (5, 3), (4, 4), (5, 4), (4, 5), (5, 5), (4, 7), (5, 7)]
    ui["bubble_blocked"] = bubble(blocked, (0xE0, 0x8A, 0x1E, 255))
    alarm = [(3, 2), (3, 3), (3, 4), (3, 5), (3, 7), (6, 2), (6, 3), (6, 4), (6, 5), (6, 7)]
    ui["bubble_alarm"] = bubble(alarm, (0xC6, 0x28, 0x28, 255))  # 습격: 피신 중
    return ui


def make_roads(town):
    atlas = Image.new("RGBA", (T * 2, T), (0, 0, 0, 0))
    atlas.alpha_composite(kenney_tile(town, 41), (0, 0))
    atlas.alpha_composite(kenney_tile(town, 40), (T, 0))
    return atlas


def make_snow(rng):
    """눈 덮개 (가로 띠, 변형 4가지 × 2줄).
    1행: 풀밭·바위용 두꺼운 눈 — 거의 하얗고 군데군데 맨땅이 비침. 가장자리까지 고르게 칠해 이웃 타일과 이어짐
    2행: 숲용 얇은 눈 — 나무가 보이도록 듬성듬성"""
    snow = (0xF2, 0xF6, 0xFC)
    shade = (0xD6, 0xE2, 0xF2)
    atlas = Image.new("RGBA", (T * VARIANTS, T * 2), (0, 0, 0, 0))
    px = atlas.load()
    for v in range(VARIANTS):
        ox = v * T
        # 두꺼운 눈: 전부 칠하고, 그늘 몇 점과 맨땅 구멍 몇 개
        for y in range(T):
            for x in range(T):
                px[ox + x, y] = snow + (225,)
        for _ in range(rng.randint(2, 4)):
            cx, cy = rng.randint(1, T - 3), rng.randint(1, T - 3)
            for dx, dy in [(0, 0), (1, 0), (0, 1), (1, 1)][: rng.randint(2, 4)]:
                px[ox + cx + dx, cy + dy] = (0, 0, 0, 0)
        for _ in range(rng.randint(4, 7)):
            cx, cy = rng.randint(0, T - 2), rng.randint(0, T - 1)
            px[ox + cx, cy] = shade + (225,)
            px[ox + cx + 1, cy] = shade + (225,)
        # 얇은 눈: 위쪽(나무 꼭대기 쪽)에 몰린 하얀 점들
        for y in range(T):
            for x in range(T):
                chance = 0.32 if y < T // 2 else 0.12
                if rng.random() < chance:
                    px[ox + x, T + y] = snow + (200,)
    return atlas


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

    dungeon = Image.open(KENNEY_DUNGEON).convert("RGBA")
    os.makedirs(OUT_UNITS, exist_ok=True)
    os.makedirs(OUT_UI, exist_ok=True)
    for key, img in make_buildings(sheet, dungeon, rng).items():
        img.save(os.path.join(OUT_BUILDINGS, key + ".png"))
    for (w, h) in [(1, 1), (2, 2), (3, 2), (3, 3)]:
        make_construction(sheet, w, h).save(os.path.join(OUT_BUILDINGS, "construction_%dx%d.png" % (w, h)))
    make_villagers(dungeon).save(os.path.join(OUT_UNITS, "villagers.png"))
    make_enemies(dungeon).save(os.path.join(OUT_UNITS, "enemies.png"))
    make_arrow().save(os.path.join(OUT_UNITS, "arrow.png"))
    for key, img in make_ui(sheet, dungeon).items():
        img.save(os.path.join(OUT_UI, key + ".png"))
    make_roads(sheet).save(OUT_ROADS)
    make_snow(random.Random(1225)).save(OUT_SNOW)
    print("wrote terrain, keep, buildings, construction, villagers, ui icons, roads, snow")


if __name__ == "__main__":
    main()
