#include "pch.h"
#include "FApplication.h"

#include "Resource.h"
#include "Render/FLoadingScreen.h"
#include "Core/Console/Console.h"
#include "Core/Base/TypeRegistry.h"
#include "Core/Channel/Messages/FMousePickRequestMessage.h"
#include "Asset/UTexture.h"
#include "Asset/UFont.h"
#include "Asset/UFreeTypeFont.h"
#include "World/Component/UBoxColliderComponent.h"
#include "World/Component/UDirectionalLightComponent.h"
#include "World/Component/UPointLightComponent.h"
#include "World/Component/USpotLightComponent.h"
#include "World/Component/UBillboardComponent.h"
#include "World/Component/UBillboardTextComponent.h"
#include "World/Component/UNameTagComponent.h"
#include "World/Component/UScrollUVComponent.h"
#include "World/Component/USubUVComponent.h"
#include "ImGui/imgui.h"
#include "ImGui/imgui_impl_dx11.h"
#include "ImGui/imgui_impl_win32.h"

FApplication::FApplication() = default;

FApplication::~FApplication() {
    Shutdown();
}

int FApplication::Run(HINSTANCE Instance, int ShowCommand) {
    if (!RegisterWindowClass(Instance) || !CreateApplicationWindow(Instance, ShowCommand)) {
        return FALSE;
    }

    mContext.mRenderer.Create(mWindowState.mWindowHandle, mLoadingWindowWidth, mLoadingWindowHeight);
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui_ImplWin32_Init(static_cast<void*>(mWindowState.mWindowHandle));
    ImGui_ImplDX11_Init(mContext.mRenderer.GetDevice(), mContext.mRenderer.GetDeviceContext());
    mWindowState.mImGuiInitialized = true;
    ImGui::StyleColorsDark();

    ImGuiIO& Io{ImGui::GetIO()};
    Io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    Io.Fonts->AddFontFromFileTTF("./Content/Font/NotoSansKR-Medium.ttf", 16.0f, nullptr, Io.Fonts->GetGlyphRangesKorean());

    const HACCEL AcceleratorTable{LoadAccelerators(Instance, MAKEINTRESOURCE(IDC_MACAW))};
    FLoadingScreen LoadingScreen{};
    const HWND WindowHandle{mWindowState.mWindowHandle};
    const bool Loaded{LoadingScreen.Run(mContext.mRenderer, AcceleratorTable, [this, WindowHandle](FLoadingProgress& Progress) {
        return InitializeApplication(Progress, WindowHandle);
    })};
    mEditorLogo = LoadingScreen.TakeLogoShaderResourceView();
    if (!Loaded) {
        Shutdown();
        return FALSE;
    }

    RestoreGameWindow();
    mAcceptGameInput.store(true, std::memory_order_release);
    Console::AddLog(Console::STDOutHandle, ELogLevel::Log, ELogCategory::Etc, "Macaw Engine Initialized.");
    mLastTickTime = std::chrono::steady_clock::now();
    mWindowState.mFrameEnabled = true;
    const int ExitCode{RunMessageLoop(AcceleratorTable)};
    mWindowState.mFrameEnabled = false;
    SaveState();
    Shutdown();
    mContext.mRenderer.ReportLiveObjects();
    return ExitCode;
}

void FApplication::RegisterObjectTypes() {
    TypeRegistry::Register(UObject::StaticTypeInfo());
    TypeRegistry::Register(UAsset::StaticTypeInfo());
    TypeRegistry::Register(UMesh::StaticTypeInfo());
    TypeRegistry::Register(UPipeline::StaticTypeInfo());
    TypeRegistry::Register(UTexture::StaticTypeInfo());
    TypeRegistry::Register(AActor::StaticTypeInfo());
    TypeRegistry::Register(UFont::StaticTypeInfo());
    TypeRegistry::Register(UFreeTypeFont::StaticTypeInfo());
    TypeRegistry::Register(UWorld::StaticTypeInfo());
    TypeRegistry::Register(UCameraComponent::StaticTypeInfo());
    TypeRegistry::Register(UStaticMeshComponent::StaticTypeInfo());
    TypeRegistry::Register(UCollisionComponent::StaticTypeInfo());
    TypeRegistry::Register(UBoxColliderComponent::StaticTypeInfo());
    TypeRegistry::Register(UDirectionalLightComponent::StaticTypeInfo());
    TypeRegistry::Register(UPointLightComponent::StaticTypeInfo());
    TypeRegistry::Register(USpotLightComponent::StaticTypeInfo());
    TypeRegistry::Register(UActorComponent::StaticTypeInfo());
    TypeRegistry::Register(USceneComponent::StaticTypeInfo());
    TypeRegistry::Register(UBillboardTextComponent::StaticTypeInfo());
    TypeRegistry::Register(UNameTagComponent::StaticTypeInfo());
    TypeRegistry::Register(UBillboardComponent::StaticTypeInfo());
    TypeRegistry::Register(USubUVComponent::StaticTypeInfo());
    TypeRegistry::Register(UScrollUVComponent::StaticTypeInfo());
}

bool FApplication::InitializeApplication(FLoadingProgress& Progress, HWND WindowHandle) {
    Progress.SetProgress(0.02f, "Registering object types");
    RegisterObjectTypes();

    Progress.SetProgress(0.06f, "Initializing renderer resources");
    if (!mContext.mRenderer.Initialize()) {
        return false;
    }

    Progress.SetProgress(0.10f, "Creating world services");
    mContext.mWorld = std::make_unique<UWorld>();
    mContext.mEditorContext = std::make_unique<FWorldEditorContext>();
    mContext.mAssetRegistry = std::make_unique<FAssetRegistry>();
    mContext.mThumbnailRenderer = std::make_unique<FAssetThumbnailRenderer>();
    mContext.mWorldCommandChannel = std::make_unique<FMessageChannel>(64);
    mContext.mEditorView = std::make_unique<EditorViewport>();
    mContext.mEditorUIManager = std::make_unique<FEditorUIManager>();

    Progress.SetProgress(0.13f, "Loading editor settings");
    if (!FEditorConfigManager::Load(mContext.mEditorSettings)) {
        FEditorConfigManager::Save(mContext.mEditorSettings);
    }

    mContext.mEditorContext->SetEditorSettings(mContext.mEditorSettings);
    mContext.mWorld->SetEditorContext(mContext.mEditorContext.get());
    mContext.mEditorContext->SetWorld(mContext.mWorld.get());

    const FAssetRegistry::FProgressCallback AssetProgressCallback{[&Progress](float AssetProgress, const std::string& Status) {
        Progress.SetProgress(0.15f + AssetProgress * 0.55f, Status);
    }};

    const bool AssetsInitialized{mContext.mAssetRegistry->Initialize(mContext.mRenderer.GetDevice(), 128, AssetProgressCallback)};
    if (!AssetsInitialized) {
        Console::AddLog(Console::STDOutHandle, ELogLevel::Warning, ELogCategory::Etc, "Some optional assets failed to load. Initialization will continue.");
    }

    mContext.mRenderer.BindAssetRegistry(mContext.mAssetRegistry.get());
    mContext.mWorld->SetAssetRegistry(mContext.mAssetRegistry.get(), mContext.mAssetRegistry.get());

    Progress.SetProgress(0.73f, "Initializing editor channels");
    mContext.mEditorContext->InitializeChannels(*mContext.mAssetRegistry);
    mContext.mMouseInput.InitializeWorldCommandSender(mContext.mWorldCommandChannel->GetSender());
    mContext.mKeyboardInput.InitializeWorldCommandSender(mContext.mWorldCommandChannel->GetSender());
    mContext.mWorldCommandChannel->TryBind<FMousePickRequestMessage>([this](const FMousePickRequestMessage& Message) {
        mContext.mWorld->HandleMousePickRequest(Message);
    });

    Progress.SetProgress(0.78f, "Initializing editor view");
    mContext.mEditorView->Initialize(mContext.mRenderer.GetDevice(), *mContext.mAssetRegistry, *mContext.mEditorContext);

    InitializeMode(mContext, WindowHandle);

    Progress.SetProgress(0.86f, "Loading scene");
    const bool SceneLoaded{mContext.mWorld->LoadScene("./scenes/MainScene1.json")};
    if (!SceneLoaded) {
        Console::AddLog(Console::STDOutHandle, ELogLevel::Warning, ELogCategory::Etc, "The startup scene failed to load. Initialization will continue with an empty world.");
    }

    Progress.SetProgress(0.96f, "Finalizing assets");
    mContext.mAssetRegistry->Finalize();
    mContext.mThumbnailRenderer->Create(&mContext.mRenderer, mContext.mAssetRegistry.get());
    Progress.SetProgress(1.0f, "Ready");
    return true;
}

int FApplication::RunMessageLoop(HACCEL AcceleratorTable) {
    MSG Message{};
    while (true) {
        while (PeekMessage(&Message, nullptr, 0, 0, PM_REMOVE)) {
            if (Message.message == WM_QUIT) {
                return static_cast<int>(Message.wParam);
            }
            if (!TranslateAccelerator(Message.hwnd, AcceleratorTable, &Message)) {
                TranslateMessage(&Message);
                DispatchMessage(&Message);
            }
        }
        RenderFrame();
    }
}

void FApplication::RenderFrame() {
    if (!mWindowState.mFrameEnabled || mWindowState.mRenderingFrame) {
        return;
    }
    mWindowState.mRenderingFrame = true;
    const auto CurrentTickTime{std::chrono::steady_clock::now()};
    const float DeltaTime{std::chrono::duration<float>(CurrentTickTime - mLastTickTime).count()};
    mLastTickTime = CurrentTickTime;

    mContext.mThumbnailRenderer->Tick();
    mContext.mEditorUIManager->RenderOffscreen(mContext.mRenderer, *mContext.mAssetRegistry);

    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
    DrawTitleBar();
    mContext.mEditorUIManager->Tick();

    for (const FPendingExternalFileDrop& Drop : mPendingExternalFileDrops) {
        mContext.mEditorUIManager->HandleExternalFileDrop(Drop.mFilePath, ImVec2{static_cast<float>(Drop.mScreenPosition.x), static_cast<float>(Drop.mScreenPosition.y)});
    }

    mPendingExternalFileDrops.clear();

    TickMode(mContext, DeltaTime);

    ImGui::Render();
    mContext.mRenderer.BeginUiRender();
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

    if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
        ImGui::UpdatePlatformWindows();
        EnableExternalDropsForImGuiViewports();
        ImGui::RenderPlatformWindowsDefault();
    }

    mContext.mRenderer.EndFrame();
    mContext.mMouseInput.EndFrame();
    mWindowState.mRenderingFrame = false;
}

void FApplication::SaveState() {
    mContext.mEditorSettings = mContext.mEditorContext->GetEditorSettings();
    if (FViewportHostWindow* Host{mContext.mEditorUIManager->GetViewportHostWindow()}) {
        Host->CaptureLayoutSettings(mContext.mEditorSettings);
    }
    FEditorConfigManager::Save(mContext.mEditorSettings);
    mContext.mWorld->SaveScene("test", mContext.mAssetRegistry.get());
}

void FApplication::Shutdown() {
    mWindowState.mFrameEnabled = false;
    mAcceptGameInput.store(false, std::memory_order_release);
    if (mContext.mThumbnailRenderer != nullptr) {
        mContext.mThumbnailRenderer->Terminate();
    }
    if (mContext.mEditorUIManager != nullptr) {
        mContext.mEditorUIManager->ReleaseRenderResources();
    }
    if (mWindowState.mImGuiInitialized) {
        ImGui_ImplDX11_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();
        mWindowState.mImGuiInitialized = false;
    }
    mContext.mRenderer.BindAssetRegistry(nullptr);
    mContext.mMenuPanel.reset();
    mContext.mEditorUIManager.reset();
    mContext.mEditorView.reset();
    mContext.mThumbnailRenderer.reset();
    mContext.mWorldCommandChannel.reset();
    mContext.mWorld.reset();
    mContext.mAssetRegistry.reset();
    mContext.mEditorContext.reset();
    mEditorLogo.Reset();
    mPendingExternalFileDrops.clear();
    if (mWindowState.mWindowHandle != nullptr) {
        DestroyWindow(mWindowState.mWindowHandle);
        mWindowState.mWindowHandle = nullptr;
    }
    if (mContext.mRenderer.GetDeviceContext() != nullptr) {
        mContext.mRenderer.Terminate();
    }
}
