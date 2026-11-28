"""정수 제작 결과와 조명 기준점을 원장에 등록합니다. 이미지 연산은 하지 않습니다."""
import hashlib
import json
from pathlib import Path
import struct
import zlib


def main():
    root = Path(__file__).resolve().parents[1]
    runtime_path = root / "game/assets/assets_manifest.json"
    art_path = root / "art/art_manifest.json"
    runtime = json.loads(runtime_path.read_text())
    art = json.loads(art_path.read_text())
    atlases = [
        ("lighting.carried", "lighting_lab/carried_lights.png", "settlement/carried_lights.png", 128, 64,
         "art/sources/graphics_finish_generated/carried_lights_padded.png",
         "인계 원본 8×4 시트에서 받침 고정·후광/불씨 정리·정수 표본 평균으로 16화소 칸 제작"),
        ("lighting.stationary", "lighting_lab/stationary_fires.png", "settlement/stationary_fires.png", 192, 96,
         "art/sources/graphics_finish_generated/stationary_fires_padded.png",
         "인계 원본 8×4 시트에서 받침 고정·후광/불씨 정리·정수 표본 평균으로 24화소 칸 제작"),
    ]
    for asset_id, path, stage, expect_w, expect_h, source, generation in atlases:
        data = (root / "game/assets" / path).read_bytes()
        width, height = struct.unpack(">II", data[16:24])
        assert (width, height) == (expect_w, expect_h), path
        digest = hashlib.sha256(data).hexdigest()
        runtime["assets"] = [a for a in runtime["assets"] if a["id"] != asset_id]
        runtime["assets"].append({
            "id": asset_id, "path": path, "kind": "texture", "required": True,
            "width": width, "height": height, "columns": 8, "rows": 4, "frames": 8,
            "content_keys": [asset_id], "source": "art/art_manifest.json#" + asset_id,
            "generated_by": "castle_art_pipeline", "license": "licenses/generated-art.txt",
            "runtime_owner": "GameAssets / game_assets_free",
            "validation": "point-filter;binary-alpha;stable-base;flame-center;animated",
            "sha256": digest, "stage_file": stage, "crc32": zlib.crc32(data),
        })
        art["resources"] = [a for a in art["resources"] if a["id"] != asset_id]
        art["resources"].append({"id": asset_id, "output": "game/assets/" + path, "stage_file": stage,
                                 "sha256": digest, "source_group": "art/sources/graphics_finish_generated",
                                 "source": source, "implementation": "tools/art/carried_lights.c",
                                 "layout": "game/src/render/fire_atlas.h", "generation": generation})
    sources = ["art/sources/scene_profiles/buildings.inc", "art/sources/sprite_materials/guides.txt"]
    art["source_files"] = [a for a in art["source_files"] if a["path"] != "art/sources/carried_lights/masks.txt"]
    for relative in sources:
        if not (root / relative).exists():
            continue
        art["source_files"] = [a for a in art["source_files"] if a["path"] != relative]
        art["source_files"].append({"path": relative,
                                   "sha256": hashlib.sha256((root / relative).read_bytes()).hexdigest(),
                                   "producer": "기존 픽셀 원화를 기준으로 작성한 기술 기준점·표면 지침",
                                   "license": "game/assets/licenses/generated-art.txt",
                                   "consumer": "castle_game / scene_profiles.c" if relative.endswith(".inc") else "castle_art_pipeline",
                                   "verification": "원장 SHA256·정수 재현성·기준점 검사"})
    for p, value in ((runtime_path, runtime), (art_path, art)):
        p.write_text(json.dumps(value, ensure_ascii=False, indent=2) + "\n")
    p = root / "art/recipes/provenance.json"
    evidence = json.loads(p.read_text())
    evidence["records"] = [r for r in evidence["records"] if r["id"] != "graphics.finish"]
    evidence["records"].append({
        "id": "graphics.finish", "format": "technical_bake",
        "request": "기존 2D 픽셀 원화를 유지하며 명암 완화·법선·높이·발광·재질, 친선 인물 야간 등불, 구름 시야 경계와 물의 태양 반사를 완성한다.",
        "sources": sources + [a[5] for a in atlases],
        "implementation": ["tools/art/carried_lights.c", "tools/art/sprite_materials.c", "game/src/render/scene_profiles.c"],
        "method": "휴대 등불·고정 불은 인계된 padded 원본 시트를 받침 기준으로 정렬해 정수 표본 평균으로 게임 화소에 굽는다. 기존 원화별 표면 지침으로 색·법선·높이·발광·거칠기·틈새 차폐를 굽는다. 개구부와 굴뚝은 원화 기준점을 사용한다. 구름·반사·다중 조명은 GPU에서 계산한다.",
        "rights": "새 외부 원화·코드를 복제하지 않았다. 파생 아트에 대한 프로젝트 보유 권리는 기존 generated-art.txt의 CC0 1.0 정책을 따른다.",
        "limitations": "표면·차폐 체적은 2D 원화로부터 작성한 조명용 근사이다. 시각 검수와 자동 검사를 별도 기록한다."})
    p.write_text(json.dumps(evidence, ensure_ascii=False, indent=2) + "\n")
    print("등불 원장과 조명 기준점·제작 근거 등록 완료")


if __name__ == "__main__":
    main()
