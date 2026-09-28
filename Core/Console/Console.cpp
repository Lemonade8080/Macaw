#include "pch.h"

#include "Console.h"

#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <locale>
#include <mutex>
#include <cstdarg>
#include "Core/Base/ErrorHandler.h"

namespace {
    class FConsoleMessageStorage {
    public:
        FConsoleMessageStorage(std::size_t InCapacity)
            : mCapacity(InCapacity) {
            mMessages.resize(mCapacity);
        }

        void PushHistory(FConsoleMessage Message) {
            ErrorHandler::Report(Message.mLevel == ELogLevel::Error, "Console Log", std::format("[{}] [{}] {}", Message.mTime, static_cast<int>(Message.mLevel), Message.mText), ErrorHandler::EErrorLevel::Warning);
            if (mCount < mCapacity) {
                std::size_t Index{(mFront + mCount) % mCapacity};
                mMessages[Index] = std::move(Message);
                ++mCount;
            } else {
                mMessages[mFront] = std::move(Message);
                mFront = (mFront + 1) % mCapacity;
            }
        }

        void PushPending(FConsoleMessage Message) {
            mPendingBuffers[mWriteBufferIndex].push_back(std::move(Message));
        }

        std::size_t GetMessageCount() const {
            // return Messages.size();
            return mCount;
        }

        const FConsoleMessage& GetMessage(std::size_t Index) const {
            std::size_t CircleIndex{(mFront + Index) % mCapacity};
            return mMessages[CircleIndex];
        }

        void Clear() {
            mFront = 0;
            mCount = 0;
        }

        void SwapPendingBuffers() {
            std::swap(mWriteBufferIndex, mReadBufferIndex);
        }

        void FlushReadBuffer() {
            // 원본 직접 참조
            auto& ReadBuffer{mPendingBuffers[mReadBufferIndex]};

            for (FConsoleMessage& Message : ReadBuffer) {
                PushHistory(std::move(Message));
            }

            ReadBuffer.clear();
        }

    private:
        std::size_t mCapacity{};
        std::size_t mFront{0};
        std::size_t mCount{0};
        TArray<FConsoleMessage> mMessages{};

        TArray<FConsoleMessage> mPendingBuffers[2]{};

        std::size_t mWriteBufferIndex{0};
        std::size_t mReadBufferIndex{1};
    };

    struct FConsoleState {
        std::vector<FConsoleMessageStorage> mOutputStorages{};
        std::mutex mPendingMutex{};

        FConsoleState() {
            mOutputStorages.emplace_back(1000); // STDOut
            mOutputStorages.emplace_back(1000); // STDError
        }
    };

    FConsoleState& GetConsoleState() {
        static FConsoleState State{};
        return State;
    }

    FConsoleMessageStorage* ResolveStorage(FConsoleOutputHandle Handle) {
        FConsoleState& State{GetConsoleState()};

        if (!Handle.IsValid()) {
            return nullptr;
        }

        if (Handle.mIndex >= State.mOutputStorages.size()) {
            return nullptr;
        }

        return &State.mOutputStorages[Handle.mIndex];
    }

    FString GetCurrentTimeString() {
        auto Now{std::chrono::system_clock::now()};
        std::time_t NowTime{std::chrono::system_clock::to_time_t(Now)};

        std::tm LocalTime{};
        localtime_s(&LocalTime, &NowTime);

        std::ostringstream Stream{};
        Stream.imbue(std::locale(""));
        Stream << std::put_time(&LocalTime, "%H:%M:%S");

        return FString{Stream.str()};
    }
}

namespace Console {
    void Print(FConsoleOutputHandle Handle, FConsoleMessage Message) {
        Message.mTime = GetCurrentTimeString();

        FConsoleMessageStorage* Storage{ResolveStorage(Handle)};

        if (Storage == nullptr) {
            return;
        }

        Storage->PushHistory(std::move(Message));
    }

    void Clear(FConsoleOutputHandle Handle) {
        FConsoleMessageStorage* Storage{ResolveStorage(Handle)};

        if (Storage == nullptr)
            return;

        Storage->Clear();
    }

    void AddLog(FConsoleOutputHandle Handle, ELogLevel Level, ELogCategory Category, const char* Format, ...) {
        va_list Args{};
        va_start(Args, Format);

        va_list ArgsCopy{};
        va_copy(ArgsCopy, Args);

        int Length{vsnprintf(nullptr, 0, Format, ArgsCopy)};

        va_end(ArgsCopy);

        if (Length < 0) {
            va_end(Args);
            return;
        }

        FString Text{};
        Text.resize(static_cast<std::size_t>(Length + 1), '\0');

        vsnprintf(Text.data(), Text.size(), Format, Args);

        va_end(Args);

        Text.resize(Length);

        FConsoleMessage Message{};
        Message.mCategory = Category;
        Message.mLevel = Level;
        Message.mText = std::move(Text);

        Message.mTime = GetCurrentTimeString();

        // Print(Handle, std::move(Message));

        FConsoleState& State{GetConsoleState()};

        {
            std::lock_guard<std::mutex> Lock{State.mPendingMutex};

            FConsoleMessageStorage* Storage{ResolveStorage(Handle)};

            if (Storage == nullptr)
                return;

            Storage->PushPending(std::move(Message));
        }
    }

    void Flush(FConsoleOutputHandle Handle) {
        FConsoleState& State{GetConsoleState()};
        FConsoleMessageStorage* Storage{ResolveStorage(Handle)};

        if (Storage == nullptr)
            return;
        // lock 수명 짧게
        {
            std::lock_guard<std::mutex> Lock{State.mPendingMutex};
            Storage->SwapPendingBuffers();
        }

        Storage->FlushReadBuffer();
    }

    std::size_t GetMessageCount(FConsoleOutputHandle Handle) {
        const FConsoleMessageStorage* Storage{ResolveStorage(Handle)};

        return Storage ? Storage->GetMessageCount() : 0;
    }

    const FConsoleMessage& GetMessageAt(FConsoleOutputHandle Handle, std::size_t Index) {
        return ResolveStorage(Handle)->GetMessage(Index);
    }
}

bool FConsoleOutputHandle::IsValid() const {
    return mIndex != UINT32_MAX;
}
