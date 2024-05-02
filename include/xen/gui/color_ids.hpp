#pragma once

namespace xen::gui
{

enum class AccordionColorIDs
{
    Background = 0xA000000,
    Text = 0xA000001,
    Triangle = 0xA000003,
    TitleUnderline = 0xA000004,
};

enum class DirectoryViewColorIDs
{
    TitleText = 0xA000010,
    TitleBackground = 0xA000011,
    ItemBackground = 0xA000012,
    ItemText = 0xA000013,
    SelectedItemBackground = 0xA000014,
    SelectedItemText = 0xA00001,
};

enum class ActiveSessionsColorIDs
{
    TitleText = 0xA000020,
    TitleBackground = 0xA000021,
    ItemBackground = 0xA000022,
    ItemText = 0xA000023,
    SelectedItemBackground = 0xA000024,
    SelectedItemText = 0xA000025,
    CurrentItemBackground = 0xA000026,
    CurrentItemText = 0xA000027,
    BackgroundWhenEditing = 0xA000028,
    TextWhenEditing = 0xA000029,
    OutlineWhenEditing = 0xA00002A,
};

enum class TimelineColorIDs
{
    Background = 0xA000030,
    SelectionHighlight = 0xA000031,
    VerticalSeparator = 0xA000032,
    Note = 0xA000033,
    Rest = 0xA000034,
};

enum class TimeSignatureColorIDs
{
    Background = 0xA000040,
    Text = 0xA000041,
};

enum class MeasureColorIDs
{
    Background = 0xA000050,
    Outline = 0xA000051,
    SelectionHighlight = 0xA000052,
};

enum class RestColorIDs
{
    Foreground = 0xA000060,
    Text = 0xA000061,
    Outline = 0xA000062,
};

enum class NoteColorIDs
{
    Foreground = 0xA000070,
    IntervalLow = 0xA000071,
    IntervalMid = 0xA000072,
    IntervalHigh = 0xA000073,
    IntervalText = 0xA000074,
    OctaveText = 0xA000075,
};

enum class StatusBarColorIDs
{
    Background = 0xA000080,
    DebugText = 0xA000081,
    InfoText = 0xA000082,
    WarningText = 0xA000083,
    ErrorText = 0xA000084,
    ModeLetter = 0xA000085,
    Outline = 0xA000086,
};

enum class CommandBarColorIDs
{
    Background = 0xA000090,
    Text = 0xA000091,
    GhostText = 0xA000092,
    Outline = 0xA000093,
};

} // namespace xen::gui