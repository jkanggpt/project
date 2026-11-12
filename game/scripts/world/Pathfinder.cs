// Pathfinder.cs — 주민 길찾기 (AStarGrid2D)
//
// C 가 준 이동 비용 격자로 만들고, 비용 격자의 수정 번호(cost_grid_rev)가 바뀔 때만 다시 만듭니다.
// 길찾기는 Godot 의 몫이고(SPEC §3), C 는 "어디로 가야 하는지"만 정합니다.
//
// 적용 길찾기(ForEnemies)는 같은 격자를 다르게 읽습니다 (SPEC §3.3 규칙 6):
//   성벽과 성문은 "비싸지만 지나갈 수 있는" 칸입니다. 적은 빈틈이 가까우면 돌아가고, 없으면 성벽을 부수고 지나갑니다.
//   (부수는 것은 EnemyAgent 가 다음 칸이 성벽·성문일 때 멈춰서 공격하는 것으로 합니다.)
using System.Collections.Generic;
using Godot;
using MagicCastle.Core;

namespace MagicCastle.World;

public sealed class Pathfinder
{
    /// <summary>적이 성벽·성문 한 칸을 지나는 비용 (보통 칸의 몇 배). 이만큼 돌아가는 길이 있으면 돌아감</summary>
    public const float EnemyWallWeight = 12f;

    /// <summary>
    /// 한 프레임에 주민 길찾기에 쓸 시간 (ms). 아침 일곱 시에 주민 150명이 한꺼번에 일터로 갈 때
    /// 길찾기 150번이 한 프레임에 몰려 30 ms 씩 끊기던 것을, 남은 주민은 다음 프레임으로 넘겨 나눕니다 (마일스톤 7).
    /// </summary>
    private const double FrameBudgetMs = 4.0;
    private static readonly long FrameBudgetTicks = (long)(System.Diagnostics.Stopwatch.Frequency * FrameBudgetMs / 1000.0);
    private ulong _budgetFrame = ulong.MaxValue;
    private long _spentTicks;

    private readonly AStarGrid2D _grid = new();
    private readonly bool _forEnemies;
    private byte[] _cost = System.Array.Empty<byte>();
    private int _width;
    private int _height;

    public int Revision { get; private set; } = -1;

    public Pathfinder(bool forEnemies = false)
    {
        _forEnemies = forEnemies;
    }

    public int Width => _width;
    public int Height => _height;

    public void Rebuild(byte[] cost, int width, int height, int revision)
    {
        _cost = cost;
        _width = width;
        _height = height;
        Revision = revision;
        _grid.Region = new Rect2I(0, 0, width, height);
        _grid.CellSize = new Vector2(Art.Tile, Art.Tile);
        _grid.Offset = new Vector2(Art.Tile / 2f, Art.Tile / 2f);
        // 대각선은 양옆이 모두 비었을 때만 (건물 모서리를 비스듬히 뚫고 지나가지 않게)
        _grid.DiagonalMode = AStarGrid2D.DiagonalModeEnum.OnlyIfNoObstacles;
        _grid.DefaultComputeHeuristic = AStarGrid2D.Heuristic.Octile;
        _grid.DefaultEstimateHeuristic = AStarGrid2D.Heuristic.Octile;
        _grid.Update();
        for (int y = 0; y < height; y++)
        {
            for (int x = 0; x < width; x++)
            {
                byte c = cost[y * width + x];
                var cell = new Vector2I(x, y);
                bool solid = !Passable(c);
                _grid.SetPointSolid(cell, solid);
                if (!solid)
                {
                    _grid.SetPointWeightScale(cell, Weight(c));
                }
            }
        }
    }

    /// <summary>주민에게 지나갈 수 있는 칸인가 (성벽은 막힘, 성문은 지나감)</summary>
    public static bool IsWalkable(byte cost) => cost != CostCodes.Blocked && cost != CostCodes.Wall;

    /// <summary>성벽이나 성문 칸인가 (적은 여기서 멈춰서 부숨)</summary>
    public static bool IsBarrier(byte cost) => cost == CostCodes.Wall || cost == CostCodes.Gate;

    private bool Passable(byte cost) => _forEnemies ? cost != CostCodes.Blocked : IsWalkable(cost);

    private float Weight(byte cost)
    {
        if (IsBarrier(cost))
        {
            return _forEnemies ? EnemyWallWeight : 1f;
        }
        return cost / 10f;
    }

    public bool InBounds(Vector2I cell) => cell.X >= 0 && cell.Y >= 0 && cell.X < _width && cell.Y < _height;

    public bool Walkable(Vector2I cell) => InBounds(cell) && Passable(_cost[cell.Y * _width + cell.X]);

    /// <summary>그 칸의 이동 비용 코드 (CostCodes). 지도 밖이면 Blocked</summary>
    public byte CostAt(Vector2I cell) => InBounds(cell) ? _cost[cell.Y * _width + cell.X] : CostCodes.Blocked;

    /// <summary>이 칸을 지날 때의 속도 배율 (도로 2배, 숲·바위는 느림)</summary>
    public float SpeedFactor(Vector2I cell)
    {
        if (!InBounds(cell))
        {
            return 1f;
        }
        byte c = _cost[cell.Y * _width + cell.X];
        if (IsBarrier(c) || c == CostCodes.Blocked)
        {
            return 1f;
        }
        return CostCodes.Normal / (float)c;
    }

    /// <summary>이번 프레임에 길찾기를 더 해도 되는가 (아니면 다음 프레임에)</summary>
    public bool HasBudget
    {
        get
        {
            ResetBudgetIfNewFrame();
            return _spentTicks < FrameBudgetTicks;
        }
    }

    private void ResetBudgetIfNewFrame()
    {
        ulong frame = Engine.GetProcessFrames();
        if (frame != _budgetFrame)
        {
            _budgetFrame = frame;
            _spentTicks = 0;
        }
    }

    /// <summary>from 에서 to 까지의 칸 목록. 길이 없으면 false</summary>
    public bool FindPath(Vector2I from, Vector2I to, List<Vector2I> into)
    {
        ResetBudgetIfNewFrame();
        long start = System.Diagnostics.Stopwatch.GetTimestamp();
        bool found = FindPathNow(from, to, into);
        _spentTicks += System.Diagnostics.Stopwatch.GetTimestamp() - start;
        return found;
    }

    private bool FindPathNow(Vector2I from, Vector2I to, List<Vector2I> into)
    {
        into.Clear();
        if (Revision < 0 || !Walkable(to))
        {
            return false;
        }
        // 막힌 칸에 서 있으면(방금 그 자리에 건물이 지어짐) 가까운 빈칸에서 출발
        if (!Walkable(from) && !TryNearestWalkable(from, out from))
        {
            return false;
        }
        if (from == to)
        {
            into.Add(to);
            return true;
        }
        Godot.Collections.Array<Vector2I> path = _grid.GetIdPath(from, to);
        if (path.Count == 0)
        {
            return false;
        }
        foreach (Vector2I cell in path)
        {
            into.Add(cell);
        }
        return true;
    }

    public bool TryNearestWalkable(Vector2I from, out Vector2I found)
    {
        for (int radius = 1; radius <= 3; radius++)
        {
            for (int dy = -radius; dy <= radius; dy++)
            {
                for (int dx = -radius; dx <= radius; dx++)
                {
                    var cell = new Vector2I(from.X + dx, from.Y + dy);
                    if (Walkable(cell))
                    {
                        found = cell;
                        return true;
                    }
                }
            }
        }
        found = from;
        return false;
    }
}
