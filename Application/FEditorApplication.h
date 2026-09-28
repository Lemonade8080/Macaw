#pragma once

#include "FApplication.h"

class FEditorApplication final : public FApplication {
public:
    FEditorApplication();
    ~FEditorApplication() override;

private:
    void InitializeMode(FApplicationContext& Context, HWND WindowHandle) override;
    void TickMode(FApplicationContext& Context, float DeltaTime) override;
};
