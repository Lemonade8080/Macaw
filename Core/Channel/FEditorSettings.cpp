#include "pch.h"
#include "FEditorSettings.h"
#include "Core/Archive/FArchive.h"

void FEditorSettings::Serialize(FArchive& Ar) {
    Ar.BeginObjectScope("Camera");
    Ar.Serialize("MoveSensitivity", mMoveSensitivity);
    Ar.Serialize("RotationSensitivity", mRotationSensitivity);
    Ar.Serialize("CameraStartPosition", mCameraStartPosition);
    Ar.EndObjectScope();

    Ar.BeginObjectScope("Grid");
    Ar.Serialize("GridSize", mGridSize);
    Ar.Serialize("GridVisible", mGridVisible);
    Ar.Serialize("GridSnapEnabled", mGridSnapEnabled);
    Ar.Serialize("AxisVisible", mAxisVisible);
    Ar.EndObjectScope();

    Ar.BeginObjectScope("Splitter");
    Ar.Serialize("ViewportLayoutPreset", mViewportLayoutPreset);
    Ar.Serialize("ViewportSplitterCount", mViewportSplitterCount);
    Ar.Serialize("ViewportSplitterRatio0", mViewportSplitterRatio0);
    Ar.Serialize("ViewportSplitterRatio1", mViewportSplitterRatio1);
    Ar.Serialize("ViewportSplitterRatio2", mViewportSplitterRatio2);
    Ar.EndObjectScope();

    Ar.BeginObjectScope("Pannel");
    Ar.Serialize("ControlPanelEnabled", mControlPanelEnabled);
    Ar.Serialize("ViewportPanelEnabled", mViewportPanelEnabled);
    Ar.Serialize("PropertyPanelEnabled", mPropertyPanelEnabled);
    Ar.Serialize("ConsolePanelEnabled", mConsolePanelEnabled);
    Ar.Serialize("StatPanelEnabled", mStatPanelEnabled);
    Ar.Serialize("AssetBrowserPanelEnabled", mAssetBrowserPanelEnabled);
    Ar.Serialize("MaterialEditorPanelEnabled", mMaterialEditorPanelEnabled);
    Ar.Serialize("OutlinerPanelEnabled", mOutlinerPanelEnabled);
    Ar.Serialize("ViewerPanelEnabled", mViewerPanelEnabled);
    Ar.EndObjectScope();

    Ar.Serialize("LastLoadedScenePath", mLastLoadedScenePath);
}
