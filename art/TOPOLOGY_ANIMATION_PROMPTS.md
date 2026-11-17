# 성벽 연결·동물 걷기 원화 (2026-11-17)

내장 image_gen으로 새로 제작합니다. 원본은 art/expansion에, 런타임은 game/assets/expansion에 보관합니다. C 패커가 성벽의 연결 마스크와 동물의 4프레임을 만듭니다.

## animal_walk_0

```text
Original medieval farming RPG pixel art, matching the attached reference palette and texture, warm dark outlines, crisp pixel clusters, fixed 45-degree overhead orthogonal camera. Real alpha transparency, NO backdrop, NO ground patch, NO text or grid. A clean 1024x1024 atlas, EXACT 4 equal columns by4 equal rows, 256x256 cells. Each sprite fully inside its cell with 20px gutters. ANIMATION SHEET: each row is ONE animal, top to bottom: rabbit, deer, mountain goat, duck. Every animal faces RIGHT in SIDE/three-quarter PROFILE, not front or back. Four columns are four sequential WALK animation frames: contact A, passing A, opposite contact B, passing B. Clearly change all leg/wing/body poses between frames while keeping the same individual, markings, size and color; small natural body sway. Seal uses flipper crawl, snake undulates in four distinct poses. Feet contact baseline at y220 within each cell, body centers aligned. Consistent scale across four frames in each row. No people or scenery.
```

## animal_walk_1

```text
Original medieval farming RPG pixel art, matching the attached reference palette and texture, warm dark outlines, crisp pixel clusters, fixed 45-degree overhead orthogonal camera. Real alpha transparency, NO backdrop, NO ground patch, NO text or grid. A clean 1024x1024 atlas, EXACT 4 equal columns by4 equal rows, 256x256 cells. Each sprite fully inside its cell with 20px gutters. ANIMATION SHEET: each row is ONE animal, top to bottom: fox, alpaca, seal, cow. Every animal faces RIGHT in SIDE/three-quarter PROFILE, not front or back. Four columns are four sequential WALK animation frames: contact A, passing A, opposite contact B, passing B. Clearly change all leg/wing/body poses between frames while keeping the same individual, markings, size and color; small natural body sway. Seal uses flipper crawl, snake undulates in four distinct poses. Feet contact baseline at y220 within each cell, body centers aligned. Consistent scale across four frames in each row. No people or scenery.
```

## animal_walk_2

```text
Original medieval farming RPG pixel art, matching the attached reference palette and texture, warm dark outlines, crisp pixel clusters, fixed 45-degree overhead orthogonal camera. Real alpha transparency, NO backdrop, NO ground patch, NO text or grid. A clean 1024x1024 atlas, EXACT 4 equal columns by4 equal rows, 256x256 cells. Each sprite fully inside its cell with 20px gutters. ANIMATION SHEET: each row is ONE animal, top to bottom: sheep, pig, chicken, wolf. Every animal faces RIGHT in SIDE/three-quarter PROFILE, not front or back. Four columns are four sequential WALK animation frames: contact A, passing A, opposite contact B, passing B. Clearly change all leg/wing/body poses between frames while keeping the same individual, markings, size and color; small natural body sway. Seal uses flipper crawl, snake undulates in four distinct poses. Feet contact baseline at y220 within each cell, body centers aligned. Consistent scale across four frames in each row. No people or scenery.
```

## animal_walk_3

```text
Original medieval farming RPG pixel art, matching the attached reference palette and texture, warm dark outlines, crisp pixel clusters, fixed 45-degree overhead orthogonal camera. Real alpha transparency, NO backdrop, NO ground patch, NO text or grid. A clean 1024x1024 atlas, EXACT 4 equal columns by4 equal rows, 256x256 cells. Each sprite fully inside its cell with 20px gutters. ANIMATION SHEET: each row is ONE animal, top to bottom: brown bear, crocodile, buffalo, snake. Every animal faces RIGHT in SIDE/three-quarter PROFILE, not front or back. Four columns are four sequential WALK animation frames: contact A, passing A, opposite contact B, passing B. Clearly change all leg/wing/body poses between frames while keeping the same individual, markings, size and color; small natural body sway. Seal uses flipper crawl, snake undulates in four distinct poses. Feet contact baseline at y220 within each cell, body centers aligned. Consistent scale across four frames in each row. No people or scenery.
```

## wall_modules

```text
Original medieval farming RPG pixel art, matching the attached reference palette and texture, warm dark outlines, crisp pixel clusters, fixed 45-degree overhead orthogonal camera. Real alpha transparency, NO backdrop, NO ground patch, NO text or grid. A clean 1024x1024 atlas, EXACT 4 equal columns by4 equal rows, 256x256 cells. Each sprite fully inside its cell with 20px gutters. MODULAR WALL PARTS for a square tile grid. Each ROW is ONE wall material: row1 pointed wooden palisade logs; row2 rough timber and rubble barricade; row3 ivory stone crenellated castle wall; row4 stone wall reinforced with dark iron bands. Each COLUMN is a DIFFERENT CONNECTING MODULE: col1 compact square central post/block with flat top, col2 straight horizontal EAST-WEST wall beam with level continuous top face and NO posts at ends, col3 straight vertical NORTH-SOUTH wall beam with level continuous top face and NO end posts, col4 plus-shaped cross junction with four short arms. These are LOW COMPACT walls viewed mostly from above, almost square footprints, never tall broad facade panels or towers. Precisely centered alignment, horizontal module has flat cut ends at left/right, vertical module has flat cut ends at top/bottom. Same thickness of arms in each row. Ends will join other tiles to form unbroken horizontal and vertical walls, L corners, T junctions and crossroads. Preserve transparent padding outside each component, no shadows stretching outside cells.
```
