#pragma once

#include "../Common.h"
#include "Math/FMath.h"
#include "Core/Archive/FArchive.h"

struct FEditorSettings {
    Float32 mMoveSensitivity{5.0f};
    Float32 mRotationSensitivity{1.0f};
    FVector mCameraStartPosition{0.0f, 0.0f, 0.0f};
    Float32 mGridSize{1.0f};
    bool mGridVisible{true};
    bool mGridSnapEnabled{true};
    bool mAxisVisible{true};
    FString mLastLoadedScenePath{};
    Uint8 mViewportLayoutPreset{7U};
    Uint32 mViewportSplitterCount{3U};
    Float32 mViewportSplitterRatio0{0.5f};
    Float32 mViewportSplitterRatio1{0.5f};
    Float32 mViewportSplitterRatio2{0.5f};
    bool mControlPanelEnabled{true};
    bool mViewportPanelEnabled{true};
    bool mPropertyPanelEnabled{true};
    bool mConsolePanelEnabled{true};
    bool mStatPanelEnabled{true};
    bool mAssetBrowserPanelEnabled{true};
    bool mMaterialEditorPanelEnabled{true};
    bool mOutlinerPanelEnabled{true};
    bool mViewerPanelEnabled{true};

    void Serialize(FArchive& Ar);
};
