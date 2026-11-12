# Kenney 에셋

모두 Kenney(kenney.nl)의 CC0 에셋입니다. 각 폴더의 `License.txt`에 라이선스가 들어 있습니다. kenney.nl은 클라우드 세션에서 접속이 막혀 있어서, 팩은 직접 내려받아 이 폴더에 커밋합니다.

## 들어 있는 팩

| 폴더 | 팩 | 내용 |
| --- | --- | --- |
| `tiny-town/` | [Tiny Town](https://kenney.nl/assets/tiny-town) | 16×16 지형 타일, 건물, 나무, 소품 (`Tiles/`, `Tilemap/`) |
| `tiny-dungeon/` | [Tiny Dungeon](https://kenney.nl/assets/tiny-dungeon) | 16×16 캐릭터, 몬스터, 무기, 던전 타일 (`Tiles/`, `Tilemap/`, `Tiled/`) |
| `audio-ui/` | [UI Audio](https://kenney.nl/assets/ui-audio) | 클릭, 스위치 등 UI 효과음 (`Audio/`, OGG) |
| `audio-interface/` | [Interface Sounds](https://kenney.nl/assets/interface-sounds) | 확인, 오류, 알림 등 인터페이스 효과음 (`Audio/`, OGG) |
| `audio-rpg/` | [RPG Audio](https://kenney.nl/assets/rpg-audio) | 발소리, 금속, 문, 책 등 RPG 효과음 (`Audio/`, OGG) |
| `audio-impact/` | [Impact Sounds](https://kenney.nl/assets/impact-sounds) | 타격, 충돌 효과음 (`Audio/`, OGG) |

팩의 `.url` 바로가기 파일은 뺐습니다. 나머지는 원본 그대로입니다.

## 새 팩을 추가할 때

1. zip을 내려받아 압축을 풀고, 이 폴더 안에 새 폴더로 넣습니다. `License.txt`도 함께 넣습니다.
2. 위 표와 저장소 루트의 `CREDITS.md`에 한 줄씩 추가합니다.
3. 커밋하고 푸시합니다.

## Godot 가져오기에서 뺀 폴더

`tiny-town/Tiles/`, `tiny-dungeon/Tiles/`, `tiny-dungeon/Tiled/` 에는 `.gdignore` 가 있어서 Godot 이 가져오지 않습니다.
같은 그림이 `Tilemap/` 의 한 장짜리 시트에 있고, 게임은 그 시트만 씁니다. 낱장 타일이 필요해지면 `.gdignore` 를 지우면 됩니다.
