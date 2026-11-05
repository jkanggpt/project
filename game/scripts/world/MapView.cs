// MapView.cs — C 의 지형 격자로 TileMapLayer 를 채우고 성채를 그립니다.
//
// 지형 그림은 assets/tiles/terrain.png 한 장(4열 변형 × 5행 지형)에서 가져옵니다.
// 그림 파일이 없으면 단색 임시 타일을 만들어 씁니다 (그림만 바꾸면 코드는 그대로).
using System;
using Godot;
using MagicCastle.Core;

namespace MagicCastle.World;

public partial class MapView : Node2D
{
    public const int TileSize = 16;
    private const int Variants = 4;

    // 지형 행 번호 = CoreTerrain 값. 풀밭은 꽃 타일이 너무 자주 나오지 않게 가중치를 둡니다.
    private static readonly int[][] VariantWeights =
    {
        new[] { 60, 30, 4, 6 }, // 풀밭: 그냥, 풀포기, 꽃, 풀포기(뒤집음)
        new[] { 1, 1, 1, 1 },   // 숲
        new[] { 1, 1, 1, 1 },   // 바위
        new[] { 3, 3, 3, 1 },   // 물
        new[] { 1, 1, 1, 1 },   // 마정석 광맥
    };

    // 그림이 없을 때 쓸 단색 (지형 순서)
    private static readonly Color[] PlaceholderColors =
    {
        new(0.52f, 0.78f, 0.41f), new(0.28f, 0.62f, 0.29f), new(0.55f, 0.61f, 0.71f),
        new(0.23f, 0.56f, 0.85f), new(0.61f, 0.36f, 0.90f),
    };

    private TileMapLayer _ground = null!;
    private Sprite2D _keep = null!;
    private int _sourceId = -1;

    [Export]
    public Texture2D? TerrainAtlas { get; set; }

    [Export]
    public Texture2D? KeepTexture { get; set; }

    /// <summary>지도 전체의 픽셀 영역 (카메라 경계에 씀)</summary>
    public Rect2 PixelBounds { get; private set; }

    public override void _Ready()
    {
        _ground = GetNode<TileMapLayer>("Ground");
        _keep = GetNode<Sprite2D>("Keep");
    }

    /// <summary>지형을 처음부터 다시 그립니다 (새 게임, 불러오기 후).</summary>
    public void Build(byte[] terrain, int width, int height, in CoreSummaryView summary)
    {
        EnsureTileSet();
        _ground.Clear();
        for (int y = 0; y < height; y++)
        {
            for (int x = 0; x < width; x++)
            {
                int type = terrain[y * width + x];
                if (type < 0 || type >= VariantWeights.Length)
                {
                    continue;
                }
                _ground.SetCell(new Vector2I(x, y), _sourceId, new Vector2I(PickVariant(type, x, y), type));
            }
        }
        PixelBounds = new Rect2(0, 0, width * TileSize, height * TileSize);

        // 성채(임시 그림): keep_size × keep_size 칸, 중심이 keep_x, keep_y
        _keep.Texture = KeepTexture ?? MakePlaceholderKeep(summary.KeepSize);
        _keep.Centered = true;
        _keep.Position = new Vector2((summary.KeepX + 0.5f) * TileSize, (summary.KeepY + 0.5f) * TileSize);
    }

    public Vector2 TileCenter(int x, int y) => new((x + 0.5f) * TileSize, (y + 0.5f) * TileSize);

    private void EnsureTileSet()
    {
        if (_sourceId >= 0)
        {
            return;
        }
        Texture2D atlas = TerrainAtlas ?? MakePlaceholderAtlas();
        var tileSet = new TileSet { TileSize = new Vector2I(TileSize, TileSize) };
        var source = new TileSetAtlasSource
        {
            Texture = atlas,
            TextureRegionSize = new Vector2I(TileSize, TileSize),
        };
        int columns = atlas.GetWidth() / TileSize;
        int rows = atlas.GetHeight() / TileSize;
        for (int row = 0; row < rows; row++)
        {
            for (int column = 0; column < columns; column++)
            {
                source.CreateTile(new Vector2I(column, row));
            }
        }
        _sourceId = tileSet.AddSource(source);
        _ground.TileSet = tileSet;
    }

    /// <summary>위치로 정해지는 변형 번호. 같은 칸은 언제나 같은 그림이 됩니다.</summary>
    private static int PickVariant(int type, int x, int y)
    {
        uint hash = (uint)(x * 73856093) ^ (uint)(y * 19349663);
        hash ^= hash >> 13;
        hash *= 0x5bd1e995;
        hash ^= hash >> 15;
        int[] weights = VariantWeights[type];
        int total = 0;
        foreach (int weight in weights)
        {
            total += weight;
        }
        int roll = (int)(hash % (uint)total);
        for (int i = 0; i < weights.Length; i++)
        {
            roll -= weights[i];
            if (roll < 0)
            {
                return i;
            }
        }
        return 0;
    }

    private static ImageTexture MakePlaceholderAtlas()
    {
        GD.PushWarning("terrain.png 가 없어 임시 단색 타일을 씁니다 (game/assets/tiles/terrain.png).");
        var image = Image.CreateEmpty(TileSize * Variants, TileSize * PlaceholderColors.Length, false, Image.Format.Rgba8);
        for (int row = 0; row < PlaceholderColors.Length; row++)
        {
            for (int column = 0; column < Variants; column++)
            {
                Color color = PlaceholderColors[row].Darkened(column * 0.04f);
                image.FillRect(new Rect2I(column * TileSize, row * TileSize, TileSize, TileSize), color);
            }
        }
        return ImageTexture.CreateFromImage(image);
    }

    private static ImageTexture MakePlaceholderKeep(int keepSize)
    {
        GD.PushWarning("keep.png 가 없어 임시 성채 그림을 씁니다 (game/assets/buildings/keep.png).");
        int size = Math.Max(1, keepSize) * TileSize;
        var image = Image.CreateEmpty(size, size, false, Image.Format.Rgba8);
        image.Fill(new Color(0.55f, 0.61f, 0.71f));
        image.FillRect(new Rect2I(2, 2, size - 4, size - 4), new Color(0.75f, 0.8f, 0.86f));
        return ImageTexture.CreateFromImage(image);
    }
}
