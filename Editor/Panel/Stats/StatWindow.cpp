#include "pch.h"
#include "Editor/Panel/Stats/StatWindow.h"

#include "Core/Stat/Stat.h"
#include "ImGui/imgui.h"
#include <array>
#include <cfloat>
#include <cstdarg>
#include <cstdio>

namespace {
    constexpr ImU32 HeadingColor{IM_COL32(255, 216, 92, 255)};
    constexpr ImU32 TextColor{IM_COL32(224, 231, 238, 255)};
    constexpr ImU32 MutedColor{IM_COL32(157, 171, 185, 255)};

    struct FStatOverlayRow {
        const char* mLabel{};
        std::array<char, 96> mValue{};
        ImU32 mColor{TextColor};
        int mDetailLevel{};
    };

    using FStatOverlayRows = std::array<FStatOverlayRow, 64>;

    void AddRow(FStatOverlayRows& Rows, std::size_t& Count, const char* Label, int DetailLevel, ImU32 Color, const char* Format, ...) {
        if (Count >= Rows.size()) {
            return;
        }
        FStatOverlayRow& Row{Rows[Count++]};
        Row.mLabel = Label;
        Row.mDetailLevel = DetailLevel;
        Row.mColor = Color;
        va_list Arguments{};
        va_start(Arguments, Format);
        std::vsnprintf(Row.mValue.data(), Row.mValue.size(), Format, Arguments);
        va_end(Arguments);
    }

    void DrawShadowedText(ImDrawList& DrawList, const ImVec2& Position, ImU32 Color, const char* Text, float FontSize) {
        DrawList.AddText(ImGui::GetFont(), FontSize, ImVec2{Position.x + 1.0f, Position.y + 1.0f}, IM_COL32(0, 0, 0, 220), Text);
        DrawList.AddText(ImGui::GetFont(), FontSize, Position, Color, Text);
    }
}

void DrawStatOverlay(const ImVec2& Min, const ImVec2& Max, FStatDisplayFlags StatFlags) {
    if (!StatFlags.mBShowFps && !StatFlags.mBShowMemory && !StatFlags.mBObjectSystem && !StatFlags.mBShowPicking && !StatFlags.mBShowRender) {
        return;
    }

    const float FontSize{ImGui::GetFontSize() * 1.25f};
    const float LineHeight{FontSize + 6.0f};
    const float Margin{12.0f};
    const float Padding{14.0f};
    const float AvailableWidth{Max.x - Min.x - Margin * 2.0f};
    const float AvailableHeight{Max.y - Min.y - Margin * 2.0f - Padding * 2.0f};
    if (AvailableWidth < FontSize * 8.0f || AvailableHeight < LineHeight) {
        return;
    }

    const Stat::FStatAverages Snapshot{Stat::GetStatAverages()};
    FStatOverlayRows Rows{};
    std::size_t RowCount{};

    if (StatFlags.mBShowRender) {
        const auto& Samples{Snapshot.mSystemSamples};
        const double TotalMilliseconds{Samples[static_cast<std::size_t>(Stat::ESystemStatStage::RenderBeginFrame)].mTotalMilliseconds + Samples[static_cast<std::size_t>(Stat::ESystemStatStage::RenderView)].mTotalMilliseconds + Samples[static_cast<std::size_t>(Stat::ESystemStatStage::UiRender)].mTotalMilliseconds + Samples[static_cast<std::size_t>(Stat::ESystemStatStage::Present)].mTotalMilliseconds};
        AddRow(Rows, RowCount, "STAT RENDER (CPU)", 0, HeadingColor, "ms / total %%");
        AddRow(Rows, RowCount, "0.5 s average / all views", 0, MutedColor, "");
        AddRow(Rows, RowCount, "Total render", 0, HeadingColor, "%.3f / %.1f%%", TotalMilliseconds, TotalMilliseconds > 0.0 ? 100.0 : 0.0);
        constexpr std::array Stages{Stat::ESystemStatStage::RenderFenceWait, Stat::ESystemStatStage::RenderBeginFrame, Stat::ESystemStatStage::RenderTarget, Stat::ESystemStatStage::RenderMaterials, Stat::ESystemStatStage::RenderQueue, Stat::ESystemStatStage::RenderViewUpload, Stat::ESystemStatStage::RenderGeometry, Stat::ESystemStatStage::RenderSelectionOutline, Stat::ESystemStatStage::RenderSceneGuides, Stat::ESystemStatStage::RenderGizmo, Stat::ESystemStatStage::RenderText, Stat::ESystemStatStage::RenderBillboard, Stat::ESystemStatStage::RenderOrientationAxis, Stat::ESystemStatStage::UiRender, Stat::ESystemStatStage::Present};
        double AccountedMilliseconds{};
        for (const Stat::ESystemStatStage Stage : Stages) {
            const double ExcludedMilliseconds{Stage == Stat::ESystemStatStage::RenderBeginFrame ? Samples[static_cast<std::size_t>(Stat::ESystemStatStage::RenderFenceWait)].mTotalMilliseconds : 0.0};
            const double Milliseconds{std::max(Samples[static_cast<std::size_t>(Stage)].mTotalMilliseconds - ExcludedMilliseconds, 0.0)};
            const double Share{TotalMilliseconds > 0.0 ? Milliseconds * 100.0 / TotalMilliseconds : 0.0};
            AddRow(Rows, RowCount, Stat::GetSystemStageName(Stage), 0, TextColor, "%.3f / %.1f%%", Milliseconds, Share);
            AccountedMilliseconds += Milliseconds;
        }
        const double OtherMilliseconds{std::max(TotalMilliseconds - AccountedMilliseconds, 0.0)};
        const double OtherShare{TotalMilliseconds > 0.0 ? OtherMilliseconds * 100.0 / TotalMilliseconds : 0.0};
        AddRow(Rows, RowCount, "Other render overhead", 0, MutedColor, "%.3f / %.1f%%", OtherMilliseconds, OtherShare);
    }

    //if(Reader.Peek() == EStatDisplayMode::Fps)
    if (StatFlags.mBShowFps) {
        const double FPS{Snapshot.mFrame.mFramesPerSecond};
        const ImU32 FpsColor{FPS >= 60.0 ? IM_COL32(123, 235, 133, 255) : FPS >= 30.0 ? HeadingColor : IM_COL32(255, 112, 103, 255)};
        AddRow(Rows, RowCount, "STAT FPS", 0, FpsColor, "%.1f FPS", FPS);
        AddRow(Rows, RowCount, "Frame time", 0, TextColor, "%.2f ms", Snapshot.mFrame.mAverageFrameMilliseconds);
    }

    if (StatFlags.mBShowFps || StatFlags.mBShowPicking) {
        const Stat::FPickingStatAverage& Picking{Snapshot.mPicking};
        AddRow(Rows, RowCount, "STAT PICKING", 0, HeadingColor, "");
        AddRow(Rows, RowCount, "Average picking (0.5 s)", 0, TextColor, "%.3f ms", Picking.mAverageMilliseconds);
        AddRow(Rows, RowCount, "Picking attempts / frame", 0, TextColor, "%.2f", Picking.mAttemptsPerFrame);
        AddRow(Rows, RowCount, "Picking time / frame", 0, TextColor, "%.3f ms", Picking.mMillisecondsPerFrame);
    }

    if (StatFlags.mBShowFps) {
        if (Snapshot.mFrameCount > 0) {
            AddRow(Rows, RowCount, "CPU / 0.5 s average", 1, HeadingColor, "ms / calls");
            for (std::size_t Index{}; Index <= static_cast<std::size_t>(Stat::ESystemStatStage::Present); ++Index) {
                const Stat::ESystemStatStage Stage{static_cast<Stat::ESystemStatStage>(Index)};
                const Stat::FSystemStatAverage& Sample{Snapshot.mSystemSamples[Index]};
                const bool BSummary{Stage == Stat::ESystemStatStage::Frame || Stage == Stat::ESystemStatStage::WorldTick || Stage == Stat::ESystemStatStage::SceneRender || Stage == Stat::ESystemStatStage::EditorUi || Stage == Stat::ESystemStatStage::Present};
                AddRow(Rows, RowCount, Stat::GetSystemStageName(Stage), BSummary ? 1 : 2, TextColor, "%.3f / %.2f", Sample.mTotalMilliseconds, Sample.mCallCount);
            }
        }
    }

    if (StatFlags.mBObjectSystem) {
        AddRow(Rows, RowCount, "STAT OBJECT SYSTEM", 0, HeadingColor, "0.5 s average");
        AddRow(Rows, RowCount, "UObjects", 0, TextColor, "%.1f", Snapshot.mObjects.mObjectCount);
        AddRow(Rows, RowCount, "Actors", 0, TextColor, "%.1f", Snapshot.mObjects.mActorCount);
    }

    if (StatFlags.mBShowMemory) {
        const Stat::FMemoryStatAverage& Memory{Snapshot.mMemory};
        AddRow(Rows, RowCount, "STAT MEMORY", 0, HeadingColor, "0.5 s average");
        AddRow(Rows, RowCount, "Allocated", 0, TextColor, "%.2f MiB", static_cast<double>(Memory.mAllocatedBytes) / (1024.0 * 1024.0));
        AddRow(Rows, RowCount, "Peak", 0, TextColor, "%.2f MiB", static_cast<double>(Memory.mPeakAllocatedBytes) / (1024.0 * 1024.0));
        AddRow(Rows, RowCount, "Active allocations", 1, TextColor, "%.1f", Memory.mActiveAllocationCount);
        AddRow(Rows, RowCount, "Allocation calls", 2, TextColor, "%llu", static_cast<unsigned long long>(Memory.mTotalAllocationCount));
        AddRow(Rows, RowCount, "Deallocation calls", 2, TextColor, "%llu", static_cast<unsigned long long>(Memory.mTotalDeallocationCount));
        AddRow(Rows, RowCount, "Memory by tag", 2, HeadingColor, "KiB / count / share");
        for (std::size_t Index{}; Index < static_cast<std::size_t>(Stat::EMemoryTag::Count); ++Index) {
            const Stat::FTagStatAverage& Tag{Memory.mTagStats[Index]};
            const double Share{Memory.mAllocatedBytes > 0 ? static_cast<double>(Tag.mAllocatedBytes) * 100.0 / static_cast<double>(Memory.mAllocatedBytes) : 0.0};
            AddRow(Rows, RowCount, Stat::GetMemoryTagName(static_cast<Stat::EMemoryTag>(Index)), 2, TextColor, "%.1f / %.1f / %.0f%%", Tag.mAllocatedBytes / 1024.0, Tag.mActiveAllocationCount, Share);
        }
    }

    const int Capacity{static_cast<int>(AvailableHeight / LineHeight)};
    std::array<int, 3> LevelCounts{};
    for (std::size_t Index{}; Index < RowCount; ++Index) {
        for (int Level{Rows[Index].mDetailLevel}; Level < static_cast<int>(LevelCounts.size()); ++Level) {
            ++LevelCounts[Level];
        }
    }
    int DetailLevel{2};
    while (DetailLevel > 0 && LevelCounts[DetailLevel] + (DetailLevel < 2 ? 1 : 0) > Capacity) {
        --DetailLevel;
    }
    const bool BCompact{DetailLevel < 2 || LevelCounts[DetailLevel] > Capacity};
    const int VisibleRows{std::min(LevelCounts[DetailLevel], Capacity - (BCompact && Capacity > 1 ? 1 : 0))};
    const bool BFooter{BCompact && Capacity > 1};
    const float ColumnGap{16.0f};
    float LabelWidth{};
    float MaxValueWidth{};
    int MeasuredRows{};
    for (std::size_t Index{}; Index < RowCount && MeasuredRows < VisibleRows; ++Index) {
        const FStatOverlayRow& Row{Rows[Index]};
        if (Row.mDetailLevel > DetailLevel) {
            continue;
        }
        LabelWidth = std::max(LabelWidth, ImGui::GetFont()->CalcTextSizeA(FontSize, FLT_MAX, 0.0f, Row.mLabel).x);
        MaxValueWidth = std::max(MaxValueWidth, ImGui::GetFont()->CalcTextSizeA(FontSize, FLT_MAX, 0.0f, Row.mValue.data()).x);
        ++MeasuredRows;
    }
    const float FooterWidth{BFooter ? ImGui::GetFont()->CalcTextSizeA(FontSize, FLT_MAX, 0.0f, "Enlarge viewport for details").x : 0.0f};
    const float Width{std::min(std::max(LabelWidth + ColumnGap + MaxValueWidth, FooterWidth) + Padding * 2.0f, AvailableWidth)};
    const ImVec2 PanelMin{Max.x - Margin - Width, Min.y + Margin};
    const ImVec2 PanelMax{Max.x - Margin, PanelMin.y + Padding * 2.0f + LineHeight * static_cast<float>(VisibleRows + (BFooter ? 1 : 0))};
    ImDrawList& DrawList{*ImGui::GetWindowDrawList()};
    DrawList.PushClipRect(Min, Max, true);
    DrawList.AddRectFilled(PanelMin, PanelMax, IM_COL32(12, 17, 23, 174), 3.0f);
    DrawList.AddRectFilled(PanelMin, ImVec2{PanelMin.x + 2.0f, PanelMax.y}, HeadingColor);
    const float Left{PanelMin.x + Padding};
    const float Right{PanelMax.x - Padding};
    float Y{PanelMin.y + Padding};
    int DrawnRows{};
    for (std::size_t Index{}; Index < RowCount && DrawnRows < VisibleRows; ++Index) {
        const FStatOverlayRow& Row{Rows[Index]};
        if (Row.mDetailLevel > DetailLevel) {
            continue;
        }
        const float ValueWidth{ImGui::GetFont()->CalcTextSizeA(FontSize, FLT_MAX, 0.0f, Row.mValue.data()).x};
        const float ValueLeft{std::max(Left + (Right - Left) * 0.45f, Right - ValueWidth)};
        DrawList.PushClipRect(ImVec2{Left, Y}, ImVec2{Row.mValue[0] != '\0' ? ValueLeft - 8.0f : Right, Y + LineHeight}, true);
        DrawShadowedText(DrawList, ImVec2{Left, Y}, Row.mColor, Row.mLabel, FontSize);
        DrawList.PopClipRect();
        DrawList.PushClipRect(ImVec2{ValueLeft, Y}, ImVec2{Right, Y + LineHeight}, true);
        DrawShadowedText(DrawList, ImVec2{ValueLeft, Y}, Row.mColor, Row.mValue.data(), FontSize);
        DrawList.PopClipRect();
        Y += LineHeight;
        ++DrawnRows;
    }
    if (BFooter) {
        DrawList.PushClipRect(ImVec2{Left, Y}, ImVec2{Right, Y + LineHeight}, true);
        DrawShadowedText(DrawList, ImVec2{Left, Y}, MutedColor, "Enlarge viewport for details", FontSize);
        DrawList.PopClipRect();
    }
    DrawList.PopClipRect();
}
