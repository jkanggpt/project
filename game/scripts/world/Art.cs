// Art.cs — 그림 파일을 찾아 읽고 기억해 두는 곳
//
// 건물 그림은 C 정의의 키(예: "farm")로 찾습니다: res://assets/buildings/farm.png
// 그림 파일이 없으면 단색 임시 그림을 만들어 씁니다 (그림만 바꾸면 코드는 그대로).
using System.Collections.Generic;
using Godot;
using MagicCastle.Core;

namespace MagicCastle.World;

public static class Art
{
    public const int Tile = 16;
    public const int VillagerVariants = 8;

    private static readonly Dictionary<string, Texture2D?> Cache = new();
    private static readonly Dictionary<int, Texture2D> VillagerCache = new();
    private static readonly string[] ResourceKeys = { "gold", "mana", "food", "wood", "stone", "herbs" };

    private static Texture2D? Load(string path)
    {
        if (Cache.TryGetValue(path, out Texture2D? texture))
        {
            return texture;
        }
        texture = ResourceLoader.Exists(path) ? GD.Load<Texture2D>(path) : null;
        Cache[path] = texture;
        return texture;
    }

    public static Texture2D Building(string key, int w, int h)
    {
        return Load($"res://assets/buildings/{key}.png") ?? Placeholder($"building:{key}", w, h, new Color(0.6f, 0.5f, 0.4f));
    }

    public static Texture2D Construction(int w, int h)
    {
        return Load($"res://assets/buildings/construction_{w}x{h}.png")
               ?? Load("res://assets/buildings/construction_2x2.png")
               ?? Placeholder($"construction:{w}x{h}", w, h, new Color(0.75f, 0.55f, 0.35f));
    }

    public static Texture2D Villager(int id)
    {
        int variant = ((id % VillagerVariants) + VillagerVariants) % VillagerVariants;
        if (VillagerCache.TryGetValue(variant, out Texture2D? cached))
        {
            return cached;
        }
        Texture2D? strip = Load("res://assets/units/villagers.png");
        Texture2D result;
        if (strip != null)
        {
            result = new AtlasTexture { Atlas = strip, Region = new Rect2(variant * Tile, 0, Tile, Tile) };
        }
        else
        {
            result = Placeholder("villager", 1, 1, new Color(0.9f, 0.75f, 0.6f));
        }
        VillagerCache[variant] = result;
        return result;
    }

    public static Texture2D? Icon(string name) => Load($"res://assets/ui/{name}.png");

    public static Texture2D? ResourceIcon(ResourceKind resource)
    {
        int index = (int)resource;
        return index >= 0 && index < ResourceKeys.Length ? Icon("res_" + ResourceKeys[index]) : null;
    }

    public static Texture2D? JobIcon(string buildingKey) => Icon("job_" + buildingKey);

    private static Texture2D Placeholder(string key, int w, int h, Color color)
    {
        string cacheKey = "placeholder:" + key;
        if (Cache.TryGetValue(cacheKey, out Texture2D? cached) && cached != null)
        {
            return cached;
        }
        var image = Image.CreateEmpty(System.Math.Max(1, w) * Tile, System.Math.Max(1, h) * Tile, false, Image.Format.Rgba8);
        image.Fill(color.Darkened(0.4f));
        image.FillRect(new Rect2I(1, 1, image.GetWidth() - 2, image.GetHeight() - 2), color);
        ImageTexture texture = ImageTexture.CreateFromImage(image);
        Cache[cacheKey] = texture;
        return texture;
    }
}
