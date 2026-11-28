# 별도 코딩 세션 전달 요청

아래 블록은 그대로 복사하는 영어 요청 원문입니다. 이미지 생성은 앞선 세션에서 수행했습니다. 실제 산출물과 한계는 `GRAPHICS_FINISH_IMAGE_HANDOFF.md`를 먼저 확인하세요.

```text
Continue the graphics finishing work for GitHub repository jkanggpt/project.

This is the IMPLEMENTATION continuation of an image-only pass. Do not invoke an AI image-generation tool or commission/redraw new source illustrations. Use the delivered image pack, the existing original artwork, and the supplied technical guides. Deterministic atlas packing, pixel cleanup, palette conversion, mask extraction, and normal/height/material-map baking are allowed and expected.

READ FIRST
1. AGENTS.md if present.
2. docs/GRAPHICS_FINISH_IMAGE_HANDOFF.md: authoritative delivered-image inventory, selection/rejection status, packing limitations, and code-checkpoint import instructions.
3. docs/PLAN_MAIN_SCREEN_TUTORIAL.md, docs/SPEC.md, PROGRESS.md.
4. docs/SPRITE_MATERIALS.md, docs/PLAN_SPRITE_MATERIALS.md, docs/GPU_SCENERY.md, docs/NATURAL_SCENERY.md, docs/SCENERY_ART_DIRECTION.md.
5. The actual git diff and source code. Some earlier documentation predates later fixes; confirm implementation state.

REPOSITORY AND AUTHORIZATION
- Original cloud checkout: /workspace/project. Continue on codex/main-screen.
- Commit/push ONLY codex/main-screen. Target base: codex/structure-cleanup.
- Existing PR #6: https://github.com/jkanggpt/project/pull/6 . Update it; do not create a duplicate or merge any PR.
- Do not push to codex/structure-cleanup. Before marking ready, fetch/merge its latest head, then run the full MSVC build/tests with -DCASTLE_WERROR=ON.
- Last verified committed baseline before finishing work: 53bf361837b34c7c98195d172d77662521483f69.
- There is substantial UNCOMMITTED finishing work. Preserve it. It is a checkpoint, not a passing release. If using a fresh clone, import the supplied code checkpoint BEFORE overlaying this image pack, as the handoff explains.
- Earlier protected-file restrictions were relaxed by the user. Keep changes graphics-focused; avoid unnecessary core/save/CLI/tutorial/CI changes. Preserve game_tutorial_start(app) and the menu button calling it.
- Korean UI/documentation/code comments; English identifiers. Follow the repository audit rules, including its ban on retired version-based names.
- No additional permission question is needed for ordinary reversible graphics implementation and tests already authorized here.

TARGET LOOK AND ENGINE
- Windows 11, mobile RTX 4060; macOS is not required.
- Preserve existing 2D pixel-art identities, silhouettes, outlines, and palette. Magical, slightly gritty but light.
- Shared logical world-pixel resolution; nearest-neighbor final enlargement for sprites, shadows, vegetation, water, smoke, fog, and reflections.
- OpenGL 4.3 compute renderer. No visible 3D models or paper-cutout shadows. Analytical volumes are internal lighting aids.
- The baseline has automatic albedo/normal pairs for 354 world sheets / 7,304 cells including empty slots; seven terrain sheets use procedural materials and twelve glow/wake sheets have effect treatment. Automatic coverage is not equivalent to full art approval.

DELIVERED ART
- Read art/recipes/graphics_finish_image_generation.json and honor each image's status.
- Use carried_lights_padded.png: 8 columns x 4 rows, torch / lantern front / lantern side / lantern back, 8 flame phases per row.
- Use stationary_fires_padded.png: 8 columns x 4 rows, wall torch / brazier / campfire / furnace flame-and-ember insert.
- The first unpadded versions are superseded source drafts.
- Cloud and water sheets are art-direction references for procedural GPU effects, not images to paste over the map.
- Architecture and nature sheets are painting/shape references, not pixel-exact replacements or validated normal/height maps.
- The merchant repaint is REJECTED: retain the original merchant atlas and derive its technical maps from existing source pixels/guides.
- Generated dimensions differ from requested dimensions. Use recorded actual dimensions, normalized grid boundaries, safe cropping, stable solid-body anchors, alpha cleanup, palette limits, and deterministic packing. Preserve exact original alpha/silhouettes when applying material corrections.
- Never feed the architecture sheet's displayed RGB normals or grayscale row directly to the shader as validated physical data. Use authored source guides and normalize/validate baked results.

PAUSED IMPLEMENTATION TO INSPECT AND FINISH
- tools/lighting_lab/gpu/: dynamic tiled lights, fog.cs, water/smoke lighting, and GPU fixtures.
- game/src/render/carried_lights.{c,h}, scene_profiles.{c,h}, and gpu_bridge.{c,h}.
- game/src/render/world.c: fog ordering, visibility fringe, and character attachments.
- art/sources/scene_profiles/buildings.inc: 73 building types x four rotations of authored technical profiles.
- tools/art/carried_lights.{c,h}, its mask source, CLI and fixtures.
- tools/art/sprite_materials.c and art/sources/sprite_materials/guides.txt: work on per-asset guides and packed surface maps.
- CMake, asset bindings, publishers and manifests are partially integrated.
- Last known integrated build failed on a CoreBoatView field named id that does not exist. Check the actual diff; this may still need fixing. Surface maps were not yet fully baked/published at the checkpoint. Do not assume --lighting-review was completed.

REQUIRED WORK

1. Ground footprints
Remove hard brown rectangles. The original defect overwrote occupied terrain cells with 16 and painted them uniformly as earth. Separate occupancy/grass exclusion/visible dirt from physical terrain and visibility. Preserve road/water/snow data, follow actual foundations, and use irregular pixel transitions. Preserve orderly farm rows and paving while softening their outer ground edges.

2. Shadowless art and material finish
Use existing pixels plus supplied guides to remove baked directional lighting while retaining material texture, colors, outlines and silhouettes. Correct normals with roof/wall/rock/canopy/limb/clothing/animal/boat guides. Keep poses, rotations, seasons and outfit variants coherent. Reconcile the six old calibrated samples with the complete material catalog to prevent switching brightness/shape conventions between frames.
Proposed packed surface RGBA8 at normal-map resolution: R=relative height, G=emission, B=roughness, A=crevice AO/open visibility. Verify actual code before changing that contract. Avoid treating painted color boundaries as physical relief.

3. Grounded shadows and building profiles
Calibrate contacts, wall/roof/chimney volumes and window/door/torch/furnace/magic/lighthouse anchors for each rotation. Normals shade surfaces; height/occluder data produces cast shadows. Keep openings aligned to art and prevent light leaking through solid walls. Gate production flames/smoke by actual operation and domestic light by appropriate occupancy/night state. Use supplied stationary-fire art for sparse selected camp/checkpoint/gathering locations, preserving gameplay rules.

4. Scalable local lights
Replace global nearest-four/fixed-sixteen bottlenecks with GPU tile-based emitter lists. Every visible source contributes light; use a separate quality budget for detailed dynamic shadows. Preserve stable identities, cache correctness, and overflow behavior without silent light drops. Balance radii/exposure for crowds. Existing first-four point-cache channels and moving-NPC cache invalidation require careful review.

5. All friendly NPCs carry a light at night
Include residents, workers, guards, merchants and visible boat crews. Torches for guards, lanterns for civilians/merchants, with a consistent assignment. Attach to the appropriate hand/belt position through walking, direction, mirroring, action poses and front/back draw order. Match the light to the flame position. Handle dusk/dawn and indoor fading/occlusion. Preserve inventory/fuel rules. Verify hundreds of NPCs have both an attachment and an actual light contribution.

6. Dense procedural cloud visibility boundary
Replace black rectangular cutoffs with naturally varying rolling pixel cloud banks. Render exactly ONE additional terrain-tile ring outside the visible boundary beneath the clouds. Opaque outer coverage must conceal that extra ring and everything beyond; soften the inner edge over rendered ground. Use world-seeded multiscale warped noise and slow independent movement with no tiled repetition or camera crawling. Composite after ALL world visuals, including smoke, shadows and effects, before UI. Keep exploration/selection/actor visibility logically separate from the render fringe. Hidden content must never leak through animation, labels, or error fallback.

7. Water sun reflections
Use sun direction, the game's fixed perspective and animated water normals for restrained pixel glints and shimmering streaks. Keep reflections world-anchored, continuous across tiles, reduced by shadow/weather and absent appropriately at night. Preserve water color/depth/foam/caustics and shore readability. Use the delivered water sheet only as a style reference.

8. Permanent actual-game review mode
Finish or add an easy local --lighting-review scene using the REAL game renderer and material pipeline. Provide day/night, movable sun, temporary test torch, and albedo/normal/shadow/material diagnostics with Korean help. Inspect possible F7/F8/arrow/right-mouse work in the checkpoint. Include building variants, rocks, trees, animated NPCs, crops, roads, water, dense night crowds and cloud boundaries. Provide a Windows launcher.

PIPELINE, TESTS AND DELIVERY
- Sources/guides under art/. Update both manifests, recipes, provenance and CREDITS.md as assets become runtime inputs. The image-only pass has not integrated or validated them as runtime atlases.
- Keep outputs reproducible. Never make generated PNGs the sole untracked source of an art change.
- Review all 354 sheets and every occupied animation/direction group, using contact sheets and animated GPU views; record visual approval separately from automated validation.
- Run assets_regenerate/assets_reproducible and check source hashes, alpha, frame isolation, normal axes, mirrors, rotations, heights/material decoding, temporal consistency, missing-map cleanup, and stable anchors.
- Test hundreds of lights, offscreen culling, overflow, moving-light caches, 500-NPC attachments, dense cloud coverage including the fringe, night/shadow reflection suppression, and shutdown resource counts.
- Capture title/tutorial/game/review at 800x600, 1024x768, 1440x900 and 1920x1080.
- Repeat menu -> new game/tutorial/continue/load -> menu and verify memory/resource stability.
- Run Linux/GPU checks and full MSVC WERROR build/tests after latest-base integration.
- Prepare a repeatable actual-game benchmark for the RTX 4060. Cloud llvmpipe is a functional check, not a new RTX measurement. Record actual nonzero GPU timing samples and frame/memory statistics.
- Document final scope, visual review, tests and limitations, commit/push only the authorized branch, and update PR #6. Do not merge it.

Finish with working local game/review launchers, integrated source art and reproducible assets, a clean committed branch and honest validation evidence. Do not request or perform more AI image generation. If the pack has an actual unusable item, use original project art or deterministic processing, and identify any genuinely blocking gap precisely rather than inventing a completed asset.
```
