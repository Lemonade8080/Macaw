#include "pch.h"

#include "FMouseInput.h"
#include "Core/Channel/Messages/FMousePickRequestMessage.h"
#ifdef OBJ_VIEWER
#include "Core/Channel/Messages/FMouseCameraRotateRequestMessage.h"
#include "Core/Channel/Messages/FKeyboardCameraMoveRequestMessage.h"
#include "Core/Channel/Messages/FMouseCameraMoveRequestMessage.h"
#include "Core/Channel/Messages/FMouseCameraDollyRequestMessage.h"
#endif

namespace {
    std::int32_t GetMouseX(LPARAM LParam) {
        return static_cast<std::int32_t>(static_cast<std::int16_t>(LOWORD(LParam)));
    }

    std::int32_t GetMouseY(LPARAM LParam) {
        return static_cast<std::int32_t>(static_cast<std::int16_t>(HIWORD(LParam)));
    }
}

void FMouseInput::InitializeWorldCommandSender(FMessageChannel::FSender&& InSender) {
    mWorldCommandSender.emplace(std::move(InSender));
}

EKeyState FMouseInput::GetKeyState(EMouseSide Side) const {
    return mKeyStates[Side];
}

bool FMouseInput::IsWorldDragActive(EMouseSide Side) const {
    const EKeyState State{mKeyStates[Side]};
    return mDragOwners[Side] == EDragOwner::World && (State == EKeyState::Pressed || State == EKeyState::Down);
}

const FMouseInput::DragCapture& FMouseInput::GetDragCapture(EMouseSide Side) const {
    return mClickCaptures[Side];
}

void FMouseInput::ProcessWindowMessage(UINT Message, WPARAM WParam, LPARAM LParam) {
    UNREFERENCED_PARAMETER(WParam);

    switch (Message) {
        case WM_LBUTTONDOWN:
        case WM_RBUTTONDOWN: {
            const EMouseSide Side{Message == WM_LBUTTONDOWN ? Left : Right};
            const EMouseSide Other{(Side == Left) ? Right : Left};

            if (mKeyStates[Other] == EKeyState::Pressed ||
                mKeyStates[Other] == EKeyState::Down) {
                break;
            }

            EKeyState& State{mKeyStates[Side]};

            if (State == EKeyState::None ||
                State == EKeyState::Released) {
                State = EKeyState::Pressed;
                mDragOwners[Side] = EDragOwner::None;

                DragCapture& Capture{mClickCaptures[Side]};

                Capture.mStart = { GetMouseX(LParam), GetMouseY(LParam)};

                Capture.mCurrent = Capture.mStart;

                //if (Side == Right)
                {
                    mPendingDeltaX = 0.0f;
                    mPendingDeltaY = 0.0f;
                }
            }

            break;
        }

        case WM_MOUSEMOVE: {
            const POINT Position{ GetMouseX(LParam), GetMouseY(LParam)};

            for (std::size_t Side{0}; Side < MAX; ++Side) {
                if (mKeyStates[Side] != EKeyState::Pressed &&
                    mKeyStates[Side] != EKeyState::Down) {
                    continue;
                }

                DragCapture& Capture{mClickCaptures[Side]};

                //if (Side == Right)
                {
                    mPendingDeltaX += static_cast<float>(Position.x - Capture.mCurrent.x);

                    mPendingDeltaY -= static_cast<float>(Position.y - Capture.mCurrent.y);
                }

                Capture.mCurrent = Position;
            }

            break;
        }

        case WM_LBUTTONUP:
        case WM_RBUTTONUP: {
            const EMouseSide Side{Message == WM_LBUTTONUP ? Left : Right};

            //좌,우를 같이 눌렀을때 PendingRotateDelta가 중복해서 누적되는걸 방지하기위해
            //한쪽을 누른상태인지 체크
            const EMouseSide Other{(Side == Left) ? Right : Left};
            if (mKeyStates[Other] == EKeyState::Pressed ||
                mKeyStates[Other] == EKeyState::Down) {
                break;
            }

            EKeyState& State{mKeyStates[Side]};

            if (State == EKeyState::Pressed ||
                State == EKeyState::Down) {
                DragCapture& Capture{mClickCaptures[Side]};

                const POINT Position{ GetMouseX(LParam), GetMouseY(LParam)};

                //if (Side == Right)
                {
                    mPendingDeltaX += static_cast<float>(Position.x - Capture.mCurrent.x);

                    mPendingDeltaY -= static_cast<float>(Position.y - Capture.mCurrent.y);
                }

                Capture.mCurrent = Position;
                State = EKeyState::Released;
            }

            break;
        }

        case WM_KILLFOCUS:
        case WM_CANCELMODE:
        case WM_CAPTURECHANGED:
            ResetKeyStates();
            break;

        case WM_MOUSEWHEEL:
            mPendingWheelSteps += GET_WHEEL_DELTA_WPARAM(WParam) / static_cast<float>(WHEEL_DELTA);
            break;
    }
}

void FMouseInput::Consume(EMouseSide Side) {
    if (mKeyStates[Side] == EKeyState::Pressed) {
        mDragOwners[Side] = EDragOwner::Gizmo;
    }
}

FViewportMouseNavigationInput FMouseInput::DispatchPendingViewportCommands(std::int32_t ViewportLeft, std::int32_t ViewportTop, std::uint32_t ViewportWidth, std::uint32_t ViewportHeight, const FMatrix& ViewProjection, const FMatrix& View, bool BMouseCapturedByUi) {
    FViewportMouseNavigationInput NavigationInput{};

    // 누르기 시작한 시점에 입력 소유권을 결정한다.
    // 기즈모가 Consume한 버튼은 그대로 유지한다.
    for (std::size_t Side{0}; Side < MAX; ++Side) {
        if (mKeyStates[Side] == EKeyState::Pressed && mDragOwners[Side] == EDragOwner::None) {
            mDragOwners[Side] = BMouseCapturedByUi ? EDragOwner::UI : EDragOwner::World;
        }
    }

#ifndef OBJ_VIEWER
    if (mWorldCommandSender.has_value() && mDragOwners[Left] == EDragOwner::World && mKeyStates[Left] == EKeyState::Pressed) {
        const DragCapture& Capture{mClickCaptures[Left]};

        mWorldCommandSender->TryEmplace<FMousePickRequestMessage>(Capture.mStart.x, Capture.mStart.y, ViewportLeft, ViewportTop, ViewportWidth, ViewportHeight, ViewProjection, View);
    }

    if (mDragOwners[Right] == EDragOwner::World && (mPendingDeltaX != 0.0f || mPendingDeltaY != 0.0f)) {
        NavigationInput.mDragDeltaX = mPendingDeltaX;
        NavigationInput.mDragDeltaY = mPendingDeltaY;
    }
    if (!BMouseCapturedByUi) {
        NavigationInput.mWheelSteps = mPendingWheelSteps;
    }
#endif

#ifdef OBJ_VIEWER
    //TODO : 회전방식 바꿔야함
    const bool BHasDelta{mPendingDeltaX != 0.0f || mPendingDeltaY != 0.0f};

    if (mWorldCommandSender.has_value() && mDragOwners[Left] == EDragOwner::World && BHasDelta) {
        mWorldCommandSender->TryEmplace<FMouseCameraRotateRequestMessage>(mPendingDeltaX, mPendingDeltaY);
    }

    if (mWorldCommandSender.has_value() && mDragOwners[Right] == EDragOwner::World && BHasDelta) {
        mWorldCommandSender->TryEmplace<FMouseCameraMoveRequestMessage>(mPendingDeltaX, mPendingDeltaY);
    }

    if (mWorldCommandSender.has_value() && !BMouseCapturedByUi && mPendingWheelSteps != 0.0f) {
        mWorldCommandSender->TryEmplace<FMouseCameraDollyRequestMessage>(mPendingWheelSteps);
    }
#endif

    mPendingDeltaX = 0.0f;
    mPendingDeltaY = 0.0f;
    mPendingWheelSteps = 0.0f;
    return NavigationInput;
}

void FMouseInput::EndFrame() {
    for (std::size_t Side{0}; Side < MAX; ++Side) {
        EKeyState& State{mKeyStates[Side]};

        if (State == EKeyState::Pressed) {
            State = EKeyState::Down;
        } else if (State == EKeyState::Released) {
            State = EKeyState::None;
            mDragOwners[Side] = EDragOwner::None;
        }
    }

    mPendingDeltaX = 0.0f;
    mPendingDeltaY = 0.0f;
}

void FMouseInput::ResetKeyStates() {
    for (EKeyState& State : mKeyStates) {
        // 드래그를 종료할 수 있도록 Released를 한 프레임 유지한다.
        if (State == EKeyState::Pressed ||
            State == EKeyState::Down) {
            State = EKeyState::Released;
        }
    }

    mPendingDeltaX = 0.0f;
    mPendingDeltaY = 0.0f;
}
