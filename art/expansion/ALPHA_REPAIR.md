# 투명도 보정

buildings_03.png의 조림소 두 셀에 배경이 남아 내장 image_gen 편집으로 보정했습니다. 원본의 구도·그림·배치·색은 유지합니다.

```text
Use case: background-extraction. Edit the attached building atlas: remove ALL white/gray background and ALL diffuse background glow behind every sprite, replacing it with REAL alpha=0 transparency throughout the entire atlas. Preserve the 4 columns and 4 rows, the exact dimensions, all 16 building sprites, their colors, positions, outlines, rotations and detail. Do not repaint buildings or crop them. Preserve grassy floor inside fenced pasture plots. Outside object boundaries and in every gutter must be actual fully transparent alpha, especially the two forestry huts in bottom row columns 1 and 2. No white or opaque backdrop, no checkerboard drawn into pixels, no labels.
```
