#!/usr/bin/env python3
"""balance.py — 시뮬레이터 봇으로 밸런스를 잽니다 (마일스톤 7).

여러 시드 × 난이도로 `castle_cli --simulate` 를 돌리고, 등급마다 처음 오른 날의 중앙값,
이긴 판 수, 진 판 수를 보여 줍니다. 정의 파일(game/data/*.def)의 숫자를 바꾼 뒤 다시 돌려
SPEC 의 목표(보통: 마을 ~8일, 요새 ~20일, 왕성 ~50일)와 비교하세요.

사용 (저장소 맨 위에서):
    python3 tools/balance.py                    # 16시드 × 평화·보통·혹독, 100일
    python3 tools/balance.py --seeds 8 --days 80 --difficulty 1
    python3 tools/balance.py --cli build/windows-msvc/bin/Release/castle_cli.exe

CSV 는 artifacts/balance/ 에 남습니다 (커밋하지 않음).
"""

import argparse
import csv
import os
import statistics
import subprocess
import sys
from concurrent.futures import ThreadPoolExecutor

RANK_NAMES = {1: "마을", 2: "요새", 3: "성", 4: "왕성"}
DIFFICULTY_NAMES = {0: "평화", 1: "보통", 2: "혹독"}


def default_cli():
    """릴리스 빌드가 있으면 그것을 씁니다 (ASan 디버그 빌드보다 7배쯤 빠름)."""
    for path in ("build/linux-release/bin/castle_cli",
                 "build/windows-msvc/bin/Release/castle_cli.exe",
                 "build/linux-debug/bin/castle_cli"):
        if os.path.exists(path):
            return path
    return "build/linux-release/bin/castle_cli"


def run_one(cli, data, out_dir, days, seed, difficulty):
    out = os.path.join(out_dir, f"d{difficulty}_s{seed}.csv")
    cmd = [cli, "--simulate", str(days), "--seed", str(seed), "--difficulty", str(difficulty),
           "--bot", "reasonable", "--data", data, "--out", out]
    result = subprocess.run(cmd, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE, text=True)
    if result.returncode != 0:
        raise RuntimeError(f"{' '.join(cmd)} 실패: {result.stderr.strip()}")
    return out


def summarize(path):
    """한 판의 CSV → (등급별 처음 오른 날, 승패). 승패: 0 진행 중, 1 승리, 2 패배."""
    with open(path, newline="", encoding="utf-8") as f:
        rows = list(csv.DictReader(f))
    first_day = {}
    for row in rows:
        for rank in range(1, int(row["rank"]) + 1):
            first_day.setdefault(rank, int(row["day"]))
    return first_day, int(rows[-1]["outcome"]) if rows else 0


def main():
    parser = argparse.ArgumentParser(description="시뮬레이터 봇으로 밸런스 재기")
    parser.add_argument("--cli", default=default_cli(), help="castle_cli 경로")
    parser.add_argument("--data", default="game/data", help="정의 파일 폴더")
    parser.add_argument("--seeds", type=int, default=16, help="시드 수 (1, 3, 5, … 홀수 시드)")
    parser.add_argument("--days", type=int, default=100, help="한 판의 날 수")
    parser.add_argument("--difficulty", type=int, choices=(0, 1, 2), action="append",
                        help="난이도 (여러 번 줄 수 있음, 기본은 셋 다)")
    parser.add_argument("--out", default="artifacts/balance", help="CSV 를 둘 폴더")
    args = parser.parse_args()

    if not os.path.exists(args.cli):
        sys.exit(f"castle_cli 를 찾을 수 없습니다: {args.cli} (먼저 빌드하세요)")
    os.makedirs(args.out, exist_ok=True)
    difficulties = args.difficulty or [0, 1, 2]
    seeds = [1 + 2 * i for i in range(args.seeds)]
    jobs = [(s, d) for d in difficulties for s in seeds]

    with ThreadPoolExecutor(max_workers=os.cpu_count() or 4) as pool:
        paths = list(pool.map(lambda job: run_one(args.cli, args.data, args.out, args.days, *job), jobs))

    print(f"{args.seeds}시드 × {args.days}일, 봇 reasonable, 정의 {args.data}")
    for difficulty in difficulties:
        firsts = {rank: [] for rank in RANK_NAMES}
        wins = lost = 0
        for (seed, d), path in zip(jobs, paths):
            if d != difficulty:
                continue
            first_day, outcome = summarize(path)
            for rank, day in first_day.items():
                if rank in firsts:
                    firsts[rank].append(day)
            wins += outcome == 1
            lost += outcome == 2
        parts = []
        for rank, name in RANK_NAMES.items():
            days = firsts[rank]
            median = f"{statistics.median(days):g}일" if days else "-"
            parts.append(f"{name} {median} ({len(days)}판)")
        print(f"{DIFFICULTY_NAMES[difficulty]}: 승리 {wins}/{args.seeds}, 패배 {lost} | " + ", ".join(parts))


if __name__ == "__main__":
    main()
