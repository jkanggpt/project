"""C 굽기 결과의 크기·해시·출처를 두 원장에 등록합니다. 이미지 연산은 하지 않습니다."""
import hashlib
import json
from pathlib import Path
import struct
import zlib


def publish(root):
    recipe = json.loads((root / "art/recipes/sprite_materials.json").read_text())
    runtime_path = root / "game/assets/assets_manifest.json"
    art_path = root / "art/art_manifest.json"
    runtime = json.loads(runtime_path.read_text())
    art = json.loads(art_path.read_text())
    runtime["assets"] = [a for a in runtime["assets"] if not a["id"].startswith("materials.")]
    art["resources"] = [a for a in art["resources"] if not a["id"].startswith("materials.")]
    originals = {a["id"]: a for a in runtime["assets"]}
    for entry in recipe["records"]:
        original = originals[entry["id"]]
        if original["sha256"] != entry["source_sha256"]:
            raise ValueError(f'원화 해시 변경: {entry["id"]}')
        original["lighting"] = {
            "mode": entry["mode"], "albedo": "", "normal": "", "surface": "",
            "divisor": entry["normal_divisor"], "recipe": "art/recipes/sprite_materials.json",
        }
        if entry["mode"] != "baked":
            continue
        for kind in ("albedo", "normals", "surface"):
            asset_id = f'materials.{kind}.{entry["id"]}'
            relative = f'materials/{kind}/{entry["path"]}'
            data = (root / "game/assets" / relative).read_bytes()
            width, height = struct.unpack(">II", data[16:24])
            divisor = 1 if kind == "albedo" else entry["normal_divisor"]
            if (width, height) != (entry["width"] // divisor, entry["height"] // divisor):
                raise ValueError(f"크기 불일치: {relative}")
            digest = hashlib.sha256(data).hexdigest()
            runtime["assets"].append({
                "id": asset_id, "path": relative, "kind": "texture", "required": True,
                "width": width, "height": height, "columns": entry["columns"], "rows": entry["rows"],
                "frames": entry["animation_span"], "content_keys": [entry["id"]],
                "source": f"art/art_manifest.json#{asset_id}", "generated_by": "castle_art_pipeline",
                "license": "licenses/generated-art.txt", "runtime_owner": "GPU Bridge / game_gpu_shutdown",
                "validation": "exact-size;point-filter;frame-isolated;source-alpha;world-normal",
                "sha256": digest, "stage_file": relative, "crc32": zlib.crc32(data),
            })
            art["resources"].append({
                "id": asset_id, "output": f"game/assets/{relative}", "stage_file": relative,
                "sha256": digest, "source_group": "art/sources/sprite_materials",
                "generation": "원화 단계 출력과 프레임별 정수 표면 프로필로 자동 굽기; 수작업 법선 아님",
                "recipe": "art/recipes/sprite_materials.json", "original": entry["id"],
            })
            original["lighting"]["normal" if kind == "normals" else kind] = relative
    for relative in ("art/sources/sprite_materials/catalog.txt", "art/sources/sprite_materials/profiles.txt", "art/sources/sprite_materials/guides.txt"):
        art["source_files"] = [s for s in art["source_files"] if s["path"] != relative]
        art["source_files"].append({
            "path": relative, "sha256": hashlib.sha256((root / relative).read_bytes()).hexdigest(),
            "producer": "Codex가 작성한 프레임 목록과 표면 추정 규약; 외부 그림 추가 없음",
            "license": "game/assets/licenses/generated-art.txt", "consumer": "castle_art_pipeline",
            "verification": "알파·법선·프레임 독립 검사 및 전체 재생성 SHA256 일치",
        })
    for path, value in ((runtime_path, runtime), (art_path, art)):
        path.write_text(json.dumps(value, ensure_ascii=False, indent=2) + "\n")
    print(f'원장 등록: 재료 쌍 354개, 전체 런타임 자료 {len(runtime["assets"])}개')


if __name__ == "__main__":
    publish(Path(__file__).resolve().parents[1])
