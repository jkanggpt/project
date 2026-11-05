// VillagerList.cs — 주민 목록 (열 제목을 누르면 정렬, 줄을 누르면 그 주민에게 카메라가 감)
using System.Collections.Generic;
using Godot;
using MagicCastle.Core;
using MagicCastle.World;

namespace MagicCastle.UI;

public partial class VillagerList : PanelContainer
{
    private readonly List<CoreVillagerView> _rows = new();
    private Tree _tree = null!;
    private int _sortColumn;
    private bool _descending;

    [Signal]
    public delegate void VillagerChosenEventHandler(int villagerId);

    public override void _Ready()
    {
        ProcessMode = ProcessModeEnum.Always;
        Visible = false;
        var box = new VBoxContainer();
        AddChild(box);
        var header = new HBoxContainer();
        header.AddChild(new Label { Text = Strings.VillagerListButton, SizeFlagsHorizontal = SizeFlags.ExpandFill });
        var close = new Button { Text = "×", FocusMode = FocusModeEnum.None };
        close.Pressed += () => Visible = false;
        header.AddChild(close);
        box.AddChild(header);
        _tree = new Tree
        {
            Columns = 4,
            ColumnTitlesVisible = true,
            HideRoot = true,
            SelectMode = Tree.SelectModeEnum.Row,
            SizeFlagsVertical = SizeFlags.ExpandFill,
            FocusMode = FocusModeEnum.None,
        };
        _tree.SetColumnTitle(0, Strings.ColumnName);
        _tree.SetColumnTitle(1, Strings.ColumnJob);
        _tree.SetColumnTitle(2, Strings.ColumnMood);
        _tree.SetColumnTitle(3, Strings.ColumnState);
        _tree.ColumnTitleClicked += (column, _) => SortBy((int)column);
        _tree.ItemSelected += OnItemSelected;
        box.AddChild(_tree);
    }

    public void ApplyScale(int scale)
    {
        CustomMinimumSize = new Vector2(280 * scale, 180 * scale);
        _tree.SetColumnCustomMinimumWidth(0, 60 * scale);
        _tree.SetColumnCustomMinimumWidth(1, 75 * scale);
        _tree.SetColumnCustomMinimumWidth(2, 30 * scale);
        _tree.SetColumnCustomMinimumWidth(3, 85 * scale);
    }

    public void Toggle()
    {
        Visible = !Visible;
        Refresh();
    }

    private void SortBy(int column)
    {
        _descending = column == _sortColumn && !_descending;
        _sortColumn = column;
        Refresh();
    }

    public void Refresh()
    {
        WorldSync? sync = WorldSync.Instance;
        if (!Visible || sync == null)
        {
            return;
        }
        _rows.Clear();
        foreach (CoreVillagerView v in sync.Villagers)
        {
            _rows.Add(v);
        }
        _rows.Sort((a, b) =>
        {
            int order = _sortColumn switch
            {
                1 => a.Job.CompareTo(b.Job),
                2 => a.Mood.CompareTo(b.Mood),
                3 => a.State.CompareTo(b.State),
                _ => string.CompareOrdinal(a.GetName(), b.GetName()),
            };
            return _descending ? -order : order;
        });
        // 줄 수가 같으면 있던 줄의 글자만 바꿉니다 (스크롤 위치와 고른 줄이 유지되게)
        TreeItem? root = _tree.GetRoot();
        if (root == null || root.GetChildCount() != _rows.Count)
        {
            _tree.Clear();
            root = _tree.CreateItem();
            for (int i = 0; i < _rows.Count; i++)
            {
                _tree.CreateItem(root);
            }
        }
        for (int i = 0; i < _rows.Count; i++)
        {
            CoreVillagerView v = _rows[i];
            TreeItem item = root.GetChild(i);
            item.SetText(0, v.GetName());
            item.SetText(1, v.Job >= 0 ? Strings.BuildingName(v.Job) : Strings.Unemployed);
            item.SetText(2, v.Mood.ToString());
            item.SetText(3, Strings.VillagerStateText(v.State));
            item.SetMetadata(0, v.Id);
        }
    }

    private void OnItemSelected()
    {
        TreeItem? item = _tree.GetSelected();
        if (item != null)
        {
            EmitSignal(SignalName.VillagerChosen, item.GetMetadata(0).AsInt32());
        }
    }
}
