"""실제 실험실의 고정 장면과 조명 셰이더를 대화 안의 비교 화면으로 묶습니다."""
import argparse
import base64
import json
from pathlib import Path


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("scene", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    source = Path(__file__).resolve().parent
    scene = json.loads((args.scene / "scene.json").read_text(encoding="utf-8"))
    scene["images"] = {
        name: "data:image/png;base64," + base64.b64encode(
            (args.scene / (name + ".png")).read_bytes()).decode("ascii")
        for name in ("albedo", "normals", "surface", "roughness")
    }
    shader = (source / "shaders/lighting.fs").read_text(encoding="utf-8")
    shader = shader.replace("#version 120", "precision highp float;\nprecision highp int;")
    shader = shader.replace("uniform sampler2D surfaceBuffer;",
                            "uniform sampler2D surfaceBuffer;\nuniform sampler2D roughnessBuffer;")
    shader = shader.replace("vec4 data = texture2D(surfaceBuffer, fragTexCoord);",
                            "vec4 data = texture2D(surfaceBuffer, fragTexCoord);\n"
                            "    data.a = texture2D(roughnessBuffer, fragTexCoord).r;")
    fragment = (source / "preview.html").read_text(encoding="utf-8")
    fragment = fragment.replace("__LIGHTING_DATA__", json.dumps(scene, separators=(",", ":")))
    fragment = fragment.replace("__LIGHTING_SHADER__", shader)
    fragment = fragment.replace("__LIGHTING_WORKER__", (source / "preview-worker.js").read_text(encoding="utf-8"))
    if len(fragment.encode("utf-8")) >= 1000000:
        raise ValueError("대화 비교 화면은 1MB보다 작아야 합니다.")
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(fragment, encoding="utf-8")
    print(args.output, len(fragment.encode("utf-8")), "bytes")


if __name__ == "__main__":
    main()
