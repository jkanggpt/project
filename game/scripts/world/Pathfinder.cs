// Pathfinder.cs — 주민 길찾기 (AStarGrid2D)
//
// C 가 준 이동 비용 격자로 만들고, 비용 격자의 수정 번호(cost_grid_rev)가 바뀔 때만 다시 만듭니다.
// 길찾기는 Godot 의 몫이고(SPEC §3), C 는 "어디로 가야 하는지"만 정합니다.
using System.Collections.Generic;
using Godot;
using MagicCastle.Core;

namespace MagicCastle.World;

public sealed class Pathfinder
{
    private readonly AStarGrid2D _grid = new();
    private byte[] _cost = System.Array.Empty<byte>();
    private int _width;
    private int _height;

    public int Revision { get; private set; } = -1;

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
                bool solid = !IsWalkable(c);
                _grid.SetPointSolid(cell, solid);
                if (!solid)
                {
                    _grid.SetPointWeightScale(cell, c == CostCodes.Gate ? 1f : c / 10f);
                }
            }
        }
    }

    /// <summary>주민에게 지나갈 수 있는 칸인가 (성벽은 막힘, 성문은 지나감)</summary>
    public static bool IsWalkable(byte cost) => cost != CostCodes.Blocked && cost != CostCodes.Wall;

    public bool InBounds(Vector2I cell) => cell.X >= 0 && cell.Y >= 0 && cell.X < _width && cell.Y < _height;

    public bool Walkable(Vector2I cell) => InBounds(cell) && IsWalkable(_cost[cell.Y * _width + cell.X]);

    /// <summary>이 칸을 지날 때의 속도 배율 (도로 2배, 숲·바위는 느림)</summary>
    public float SpeedFactor(Vector2I cell)
    {
        if (!InBounds(cell))
        {
            return 1f;
        }
        byte c = _cost[cell.Y * _width + cell.X];
        if (c == CostCodes.Gate || !IsWalkable(c))
        {
            return 1f;
        }
        return CostCodes.Normal / (float)c;
    }

    /// <summary>from 에서 to 까지의 칸 목록. 길이 없으면 false</summary>
    public bool FindPath(Vector2I from, Vector2I to, List<Vector2I> into)
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

    private bool TryNearestWalkable(Vector2I from, out Vector2I found)
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
