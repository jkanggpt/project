// MapView.cs — C 의 지형 격자로 TileMapLayer 를 채우고, 도로 층과 겨울 눈 덮개를 그립니다.
// 계절에 따라 땅 색을 바꾸고(가을은 따뜻하게, 겨울은 눈), 가뭄이면 누렇게 합니다. (규칙이 아니라 보이는 모습만)
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
    private TileMapLayer _roads = null!;
    private TileMapLayer _snow = null!;
    private int _sourceId = -1;
    private int _roadSourceId = -1;
    private int _snowSourceId = -1;
    private int _snowRows = 1;
    private Tween? _lookTween;
    private byte[] _terrain = System.Array.Empty<byte>();
    private int _width;
    private Color _tint = Colors.White;
    private float _snowAlpha;

    // 계절별 땅 색 (봄, 여름, 가을, 겨울)
    private static readonly Color[] SeasonTints =
    {
        new(1f, 1f, 1f), new(1.04f, 1.02f, 0.9f), new(1.12f, 0.94f, 0.74f), new(0.92f, 0.96f, 1.06f),
    };

    [Export]
    public Texture2D? TerrainAtlas { get; set; }

    [Export]
    public Texture2D? RoadAtlas { get; set; }

    [Export]
    public Texture2D? SnowAtlas { get; set; }

    /// <summary>지도 전체의 픽셀 영역 (카메라 경계에 씀)</summary>
    public Rect2 PixelBounds { get; private set; }

    public override void _Ready()
    {
        _ground = GetNode<TileMapLayer>("Ground");
        _roads = GetNode<TileMapLayer>("Roads");
        _snow = new TileMapLayer { Name = "Snow", Modulate = new Color(1, 1, 1, 0) };
        AddChild(_snow); // 도로 위에 덮음
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
        BuildSnow(terrain, width, height);
        _lookTween?.Kill();
        _tint = Colors.White;
        _snowAlpha = 0;
        SetLook(summary.Season, summary.Weather, summary.NatureEvent, instant: true);
    }

    /// <summary>물이 아닌 모든 칸에 눈 덮개를 깔아 둡니다 (보일지는 투명도로 정함)</summary>
    private void BuildSnow(byte[] terrain, int width, int height)
    {
        if (_snowSourceId < 0)
        {
            Texture2D atlas = SnowAtlas ?? MakePlaceholderSnow();
            var tileSet = new TileSet { TileSize = new Vector2I(TileSize, TileSize) };
            var source = new TileSetAtlasSource { Texture = atlas, TextureRegionSize = new Vector2I(TileSize, TileSize) };
            for (int row = 0; row < atlas.GetHeight() / TileSize; row++)
            {
                for (int column = 0; column < atlas.GetWidth() / TileSize; column++)
                {
                    source.CreateTile(new Vector2I(column, row));
                }
            }
            _snowRows = atlas.GetHeight() / TileSize;
            _snowSourceId = tileSet.AddSource(source);
            _snow.TileSet = tileSet;
        }
        _terrain = terrain;
        _width = width;
        _snow.Clear();
        for (int y = 0; y < height; y++)
        {
            for (int x = 0; x < width; x++)
            {
                PutSnow(x, y);
            }
        }
    }

    /// <summary>한 칸의 눈: 물에는 없음, 숲은 얇게(2행), 나머지는 두껍게(1행)</summary>
    private void PutSnow(int x, int y)
    {
        int index = y * _width + x;
        if (index < 0 || index >= _terrain.Length || _terrain[index] == (byte)Terrain.Water)
        {
            _snow.EraseCell(new Vector2I(x, y));
            return;
        }
        int row = _terrain[index] == (byte)Terrain.Forest && _snowRows > 1 ? 1 : 0;
        _snow.SetCell(new Vector2I(x, y), _snowSourceId, new Vector2I(PickVariant(0, x, y) % Variants, row));
    }

    /// <summary>계절·날씨·사건에 맞게 땅 색과 눈을 바꿉니다 (천천히 바뀜)</summary>
    public void SetLook(int season, int weather, int natureEvent, bool instant = false)
    {
        Color tint = season >= 0 && season < SeasonTints.Length ? SeasonTints[season] : Colors.White;
        if (natureEvent == (int)NatureEvent.Drought)
        {
            tint *= new Color(1.08f, 1.0f, 0.72f); // 가뭄: 누렇게
        }
        // 겨울이면 눈이 쌓여 있고, 겨울이 아니어도 눈이 오는 날은 조금 덮임
        float snow = season == 3 ? 0.9f : weather == (int)Weather.Snow ? 0.35f : 0f;
        if (tint.IsEqualApprox(_tint) && Mathf.IsEqualApprox(snow, _snowAlpha))
        {
            return;
        }
        _tint = tint;
        _snowAlpha = snow;
        _lookTween?.Kill();
        if (instant)
        {
            _ground.Modulate = tint;
            _snow.Modulate = new Color(1, 1, 1, snow);
            return;
        }
        _lookTween = CreateTween().SetParallel();
        _lookTween.TweenProperty(_ground, "modulate", tint, 2.0);
        _lookTween.TweenProperty(_snow, "modulate:a", snow, 2.0);
    }

    /// <summary>도로 층을 다시 그립니다 (C 의 비용 격자가 바뀔 때마다)</summary>
    public void SetRoads(byte[] roads, int width, int height)
    {
        if (_roadSourceId < 0)
        {
            Texture2D atlas = RoadAtlas ?? MakePlaceholderRoads();
            var tileSet = new TileSet { TileSize = new Vector2I(TileSize, TileSize) };
            var source = new TileSetAtlasSource { Texture = atlas, TextureRegionSize = new Vector2I(TileSize, TileSize) };
            for (int column = 0; column < atlas.GetWidth() / TileSize; column++)
            {
                source.CreateTile(new Vector2I(column, 0));
            }
            _roadSourceId = tileSet.AddSource(source);
            _roads.TileSet = tileSet;
        }
        _roads.Clear();
        bool snowReady = _snowSourceId >= 0 && _terrain.Length == width * height;
        for (int y = 0; y < height; y++)
        {
            for (int x = 0; x < width; x++)
            {
                if (roads[y * width + x] != 0)
                {
                    _roads.SetCell(new Vector2I(x, y), _roadSourceId, new Vector2I((x * 7 + y * 3) % 2, 0));
                    if (snowReady)
                    {
                        _snow.EraseCell(new Vector2I(x, y)); // 도로는 눈을 치워 둠
                    }
                }
                else if (snowReady && _snow.GetCellSourceId(new Vector2I(x, y)) < 0)
                {
                    PutSnow(x, y); // 도로를 없앤 칸에는 다시 눈
                }
            }
        }
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

    private static ImageTexture MakePlaceholderSnow()
    {
        GD.PushWarning("snow.png 가 없어 임시 눈 타일을 씁니다 (game/assets/tiles/snow.png).");
        var image = Image.CreateEmpty(TileSize * Variants, TileSize, false, Image.Format.Rgba8);
        image.Fill(new Color(0.96f, 0.98f, 1f, 0.85f));
        return ImageTexture.CreateFromImage(image);
    }

    private static ImageTexture MakePlaceholderRoads()
    {
        GD.PushWarning("roads.png 가 없어 임시 도로 타일을 씁니다 (game/assets/tiles/roads.png).");
        var image = Image.CreateEmpty(TileSize * 2, TileSize, false, Image.Format.Rgba8);
        image.Fill(new Color(0.85f, 0.65f, 0.42f));
        return ImageTexture.CreateFromImage(image);
    }
}
