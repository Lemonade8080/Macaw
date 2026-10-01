#include "pch.h"
#include "Editor/Panel/Stats/StatWindow.h"

#include "Core/Stat/Stat.h"
#include "ImGui/imgui.h"
#include <array>
#include <cfloat>
#include <cmath>
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

    void DrawScrollableStatOverlay(const ImVec2& Min, const ImVec2& Max, const FStatOverlayRows& Rows, std::size_t RowCount, float FontSize) {
        const float Padding{ 14.0f };
        const float Margin{ 12.0f };
        const float ColumnGap{ 16.0f };
        const float LineHeight{ FontSize + 6.0f };
        float LabelWidth{};
        float ValueWidth{};
        for (std::size_t Index{}; Index < RowCount; ++Index) {
            LabelWidth = std::max(LabelWidth, ImGui::GetFont()->CalcTextSizeA(FontSize, FLT_MAX, 0.0f, Rows[Index].mLabel).x);
            ValueWidth = std::max(ValueWidth, ImGui::GetFont()->CalcTextSizeA(FontSize, FLT_MAX, 0.0f, Rows[Index].mValue.data()).x);
        }
        const float Width{ std::min(LabelWidth + ColumnGap + ValueWidth + Padding * 2.0f + ImGui::GetStyle().ScrollbarSize, Max.x - Min.x - Margin * 2.0f) };
        const float Height{ std::min(LineHeight * static_cast<float>(RowCount) + Padding * 2.0f + ImGui::GetStyle().ScrollbarSize, Max.y - Min.y - Margin * 2.0f) };
        ImGui::SetNextWindowPos(ImVec2{ Max.x - Margin - Width, Min.y + Margin });
        ImGui::SetNextWindowSize(ImVec2{ Width, Height });
        ImGui::SetNextWindowViewport(ImGui::GetWindowViewport()->ID);
        ImGui::PushStyleColor(ImGuiCol_WindowBg, IM_COL32(12, 17, 23, 220));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{ Padding, Padding });
        if (ImGui::Begin("##StatRenderOverlay", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_HorizontalScrollbar)) {
            ImDrawList& DrawList{ *ImGui::GetWindowDrawList() };
            for (std::size_t Index{}; Index < RowCount; ++Index) {
                const FStatOverlayRow& Row{ Rows[Index] };
                const ImVec2 Position{ ImGui::GetCursorScreenPos() };
                const float RowValueWidth{ ImGui::GetFont()->CalcTextSizeA(FontSize, FLT_MAX, 0.0f, Row.mValue.data()).x };
                DrawShadowedText(DrawList, Position, Row.mColor, Row.mLabel, FontSize);
                DrawShadowedText(DrawList, ImVec2{ Position.x + LabelWidth + ColumnGap + ValueWidth - RowValueWidth, Position.y }, Row.mColor, Row.mValue.data(), FontSize);
                ImGui::Dummy(ImVec2{ LabelWidth + ColumnGap + ValueWidth, LineHeight - ImGui::GetStyle().ItemSpacing.y });
            }
        }
        ImGui::End();
        ImGui::PopStyleVar();
        ImGui::PopStyleColor();
    }
}

void DrawStatOverlay(const ImVec2& Min, const ImVec2& Max, FStatDisplayFlags StatFlags) {
    if (!StatFlags.mBShowFps && !StatFlags.mBShowMemory && !StatFlags.mBObjectSystem && !StatFlags.mBShowPicking && !StatFlags.mBShowRender && !StatFlags.mBShowLOD) {
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
        const double TotalMilliseconds{ Snapshot.mFrame.mAverageFrameMilliseconds };
        AddRow(Rows, RowCount, "STAT RENDER (CPU)", 0, HeadingColor, "ms / frame %%");
        AddRow(Rows, RowCount, "0.5 s average / all views", 0, MutedColor, "");
        AddRow(Rows, RowCount, "Viewport 0 size", 0, TextColor, "%d x %d", static_cast<int>(Max.x - Min.x), static_cast<int>(Max.y - Min.y));
        AddRow(Rows, RowCount, "Frame time", 0, HeadingColor, "%.3f / %.1f%%", TotalMilliseconds, TotalMilliseconds > 0.0 ? 100.0 : 0.0);
        double CumulativeMilliseconds{};
        double PreviousMicroseconds{};
        double PreviousShareTenths{};
        for (std::size_t Index{}; Index < Snapshot.mSystemSamples.size(); ++Index) {
            CumulativeMilliseconds += Snapshot.mSystemSamples[Index].mExclusiveMilliseconds;
            const double RoundedMicroseconds{ std::round(CumulativeMilliseconds * 1000.0) };
            const double RoundedShareTenths{ TotalMilliseconds > 0.0 ? std::round(CumulativeMilliseconds * 1000.0 / TotalMilliseconds) : 0.0 };
            AddRow(Rows, RowCount, Stat::GetSystemStageName(static_cast<Stat::ESystemStatStage>(Index)), 0, TextColor, "%.3f / %.1f%%", (RoundedMicroseconds - PreviousMicroseconds) / 1000.0, (RoundedShareTenths - PreviousShareTenths) / 10.0);
            PreviousMicroseconds = RoundedMicroseconds;
            PreviousShareTenths = RoundedShareTenths;
            if (Index == static_cast<std::size_t>(Stat::ESystemStatStage::WorldUpdate)) {
                const Stat::FWorldTickStatAverage& Tick{Snapshot.mWorldTick};
                AddRow(Rows, RowCount, "  Actor tick loop", 1, MutedColor, "%.3f / %.1f%%", Tick.mTotalMilliseconds, TotalMilliseconds > 0.0 ? Tick.mTotalMilliseconds * 100.0 / TotalMilliseconds : 0.0);
                AddRow(Rows, RowCount, "    Actor ticks / frame", 1, MutedColor, "%.0f", Tick.mActorTickCount);
                AddRow(Rows, RowCount, "    Component visits / frame", 1, MutedColor, "%.0f", Tick.mComponentVisitCount);
                AddRow(Rows, RowCount, "    Component ticks / frame", 1, MutedColor, "%.0f", Tick.mComponentTickCount);
                AddRow(Rows, RowCount, "    Inactive components / frame", 1, MutedColor, "%.0f", Tick.mComponentVisitCount - Tick.mComponentTickCount);
            }
            if (Index == static_cast<std::size_t>(Stat::ESystemStatStage::RenderPreparation)) {
                double DetailedMilliseconds{};
                for (std::size_t DetailIndex{}; DetailIndex < Snapshot.mRenderPreparationSamples.size(); ++DetailIndex) {
                    const double DetailMilliseconds{Snapshot.mRenderPreparationSamples[DetailIndex].mTotalMilliseconds};
                    DetailedMilliseconds += DetailMilliseconds;
                    AddRow(Rows, RowCount, Stat::GetRenderPreparationStageName(static_cast<Stat::ERenderPreparationStage>(DetailIndex)), 1, MutedColor, "%.3f / %.1f%%", DetailMilliseconds, TotalMilliseconds > 0.0 ? DetailMilliseconds * 100.0 / TotalMilliseconds : 0.0);
                }
                const double RemainingMilliseconds{Snapshot.mSystemSamples[Index].mExclusiveMilliseconds > DetailedMilliseconds ? Snapshot.mSystemSamples[Index].mExclusiveMilliseconds - DetailedMilliseconds : 0.0};
                AddRow(Rows, RowCount, "  Other preparation", 1, MutedColor, "%.3f / %.1f%%", RemainingMilliseconds, TotalMilliseconds > 0.0 ? RemainingMilliseconds * 100.0 / TotalMilliseconds : 0.0);
            }
        }
    }

    if (StatFlags.mBShowFps) {
        const double FPS{Snapshot.mFrame.mFramesPerSecond};
        const ImU32 FpsColor{FPS >= 60.0 ? IM_COL32(123, 235, 133, 255) : FPS >= 30.0 ? HeadingColor : IM_COL32(255, 112, 103, 255)};
        AddRow(Rows, RowCount, "STAT FPS", 0, FpsColor, "%.1f FPS", FPS);
        AddRow(Rows, RowCount, "Frame time", 0, TextColor, "%.3f ms", Snapshot.mFrame.mAverageFrameMilliseconds);
    }

    if (StatFlags.mBShowLOD) {
        const Stat::FLODStatAverage& LOD{Snapshot.mLOD};
        const double Reduction{LOD.mOriginalTriangleCount > 0.0 ? (1.0 - LOD.mRenderedTriangleCount / LOD.mOriginalTriangleCount) * 100.0 : 0.0};
        AddRow(Rows, RowCount, "STAT LOD", 0, HeadingColor, "0.5 s average");
        AddRow(Rows, RowCount, "LOD Level", 0, TextColor, "%u", LOD.mLevel);
        AddRow(Rows, RowCount, "LOD Triangles", 0, TextColor, "%.0f", LOD.mRenderedTriangleCount);
        AddRow(Rows, RowCount, "LOD Reduction", 0, TextColor, "%.1f%%", Reduction);
        AddRow(Rows, RowCount, "LOD Draw Calls", 0, TextColor, "%.1f", LOD.mDrawCallCount);
    }

    if (StatFlags.mBShowPicking) {
        const Stat::FPickingStatAverage& Picking{Snapshot.mPicking};
        const Stat::FPickingStats PickingStats{Stat::GetPickingStats()};
        AddRow(Rows, RowCount, "STAT PICKING", 0, HeadingColor, "");
        AddRow(Rows, RowCount, "Average picking (0.5 s)", 0, TextColor, "%.5f ms", Picking.mAverageMilliseconds);
        AddRow(Rows, RowCount, "Picking attempts / frame", 0, TextColor, "%.2f", Picking.mAttemptsPerFrame);
        AddRow(Rows, RowCount, "Picking time / frame", 0, TextColor, "%.5f ms", Picking.mMillisecondsPerFrame);
        AddRow(Rows, RowCount, "Last picking", 0, TextColor, "%.5f ms", PickingStats.mLastMilliseconds);
        AddRow(Rows, RowCount, "Picking attempts", 0, TextColor, "%llu", static_cast<unsigned long long>(PickingStats.mAttemptCount));
        AddRow(Rows, RowCount, "Picking total", 0, TextColor, "%.5f ms", PickingStats.mTotalMilliseconds);
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

    if (StatFlags.mBShowRender) {
        DrawScrollableStatOverlay(Min, Max, Rows, RowCount, FontSize);
        return;
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
