# 투명도 보정

2026-11-17 회전 비교 촬영에서 병영의 옆면에 앞면 일부가 섞인 것을 확인했습니다. `buildings_08.png`를 내장 image_gen으로 다시 보정하고, C 패커가 행별 투명한 세로 골을 찾아 열을 자르도록 바꿨습니다. 그림을 다시 그리는 대신 셀 경계를 여백에 맞추어 전체 지붕과 소품을 보존합니다.

```text
Use case: sprite-atlas repair. Edit the provided medieval fantasy building atlas, keeping the same architecture, colors and 45-degree overhead orthogonal camera. Output a genuinely transparent PNG, 1024x1536, EXACT 4 columns x4 rows of equal256x384 cells. Each row: crossbow tower, cannon tower, crystal arcane tower, blue-roof barracks. Columns: front/south entrance, east/right side, north/back NO front door, west/left side. IMPORTANT FIX: all 16 COMPLETE sprites MUST fit entirely INSIDE their own cell with at least24 pixels transparent padding on left and right; shrink each sprite uniformly inside its cell as needed and center it. The bottom-left barracks currently extends into the second column: fix this by fitting it inside column1, and remove all stray fragments of that barracks from the bottom-row second/fourth column views. Do not crop any roof, banner, target, spear, stone or railing at any cell boundary. All gutters must be fully alpha0. Remove the entire brown/gray background glow, no opaque background, no drawn checkerboard. Keep pixel-art appearance and consistent detail, no labels. Preserve all four actual rotations per row, no pasted duplicate front view.
```

buildings_03.png의 조림소 두 셀에 배경이 남아 내장 image_gen 편집으로 보정했습니다. 원본의 구도·그림·배치·색은 유지합니다.

```text
Use case: background-extraction. Edit the attached building atlas: remove ALL white/gray background and ALL diffuse background glow behind every sprite, replacing it with REAL alpha=0 transparency throughout the entire atlas. Preserve the 4 columns and 4 rows, the exact dimensions, all 16 building sprites, their colors, positions, outlines, rotations and detail. Do not repaint buildings or crop them. Preserve grassy floor inside fenced pasture plots. Outside object boundaries and in every gutter must be actual fully transparent alpha, especially the two forestry huts in bottom row columns 1 and 2. No white or opaque backdrop, no checkerboard drawn into pixels, no labels.
```
