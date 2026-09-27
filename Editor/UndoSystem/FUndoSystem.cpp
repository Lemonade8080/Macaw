#include "pch.h"
#include "FUndoSystem.h"

#include "IUndoContext.h"
#include "FUndoTransaction.h"
#include "FUndoRecords.h"
#include "FUndoMessages.h"
#include "Serialization/FArchiveMemory.h"
#include "Core/Channel/FMessageChannel.h"
#include "Core/Channel/FMessage.h"

namespace {
    template <Uint32 Capacity = 64>
    class FUndoHistoryStack {
    public:
        FUndoHistoryStack()
            : mHead(0),
              mSize(0),
              mCurrent(0) {
        }

    public:
        void Push(FUndoTransaction&& NewTransaction) {
            mSize = mCurrent;

            if (mSize == Capacity) {
                mHead = (mHead + 1) % Capacity;
                --mSize;
            }

            Uint32 WriteIndex{(mHead + mSize) % Capacity};
            mBuffer[WriteIndex].emplace(std::move(NewTransaction));

            ++mSize;
            mCurrent = mSize;
        }

        bool CanUndo() const {
            return mCurrent > 0;
        }

        FUndoTransaction* Undo() {
            if (!CanUndo()) {
                return nullptr;
            }

            --mCurrent;

            Uint32 TargetIndex{(mHead + mCurrent) % Capacity};
            return &mBuffer[TargetIndex].value();
        }

        bool CanRedo() const {
            return mCurrent < mSize;
        }

        FUndoTransaction* Redo() {
            if (!CanRedo()) {
                return nullptr;
            }

            Uint32 TargetIndex{(mHead + mCurrent) % Capacity};
            ++mCurrent;

            return &mBuffer[TargetIndex].value();
        }

        void Clear() {
            mHead = 0;
            mSize = 0;
            mCurrent = 0;
        }

    private:
        Uint32 mHead{};
        Uint32 mSize{};
        Uint32 mCurrent{};
        TFixedArray<std::optional<FUndoTransaction>, Capacity> mBuffer{};
    };

    struct FUndoSystemState {
        FUndoHistoryStack<64> mHistory{};
        std::unique_ptr<FUndoTransaction> mCurrentTransaction{nullptr};
        TArray<std::function<void(FUndoTransaction&)>> mPendingFinalizers{};
        TSet<UObject*> mModifiedObjectsThisTransaction{};
        std::optional<FMessageChannel::FSender> mMessageSender{};
    };

    static FUndoSystemState& GetState() {
        static FUndoSystemState State{};
        return State;
    }

    template <typename TMessage> requires CMessageType<std::remove_cvref_t<TMessage>> bool SendMessageToWorldChannel(TMessage&& Message) {
        FUndoSystemState& State{GetState()};
        if (State.mMessageSender.has_value()) {
            return State.mMessageSender->TryPush(std::forward<TMessage>(Message));
        }
        return false;
    }

    template <CMessageType TMessage, typename... Args> requires CMessageConstructible<TMessage, Args...> bool EmplaceMessageToWorldChannel(Args&&... Arguments) {
        FUndoSystemState& State{GetState()};
        if (State.mMessageSender.has_value()) {
            return State.mMessageSender->TryEmplace<TMessage>(std::forward<Args>(Arguments)...);
        }
        return false;
    }

    class FUndoContextImpl : public IUndoContext {
    public:
        virtual void NotifyObjectChanged(const FGuid& Guid, const TArray<Uint8>& Data) override {
            EmplaceMessageToWorldChannel<FMessageUndoObjectStateChanged>(Guid, TArray<Uint8>(Data));
        }

        virtual void NotifyObjectSpawned(const FGuid& Guid, const TArray<Uint8>& Data, FString&& TypeName) override {
            EmplaceMessageToWorldChannel<FMessageUndoObjectSpawned>(Guid, TArray<Uint8>(Data), std::move(TypeName));
        }

        virtual void NotifyObjectDeleted(const FGuid& Guid) override {
            EmplaceMessageToWorldChannel<FMessageUndoObjectDestroyed>(FMessageUndoObjectDestroyed(Guid));
        }
    };
}

namespace FUndoSystem {

    // =================================================================
    // Message Sender 관리 API
    // =================================================================
    void InitializeSenderToWorldChannel(FMessageChannel::FSender&& SenderToWorldChannel) {
        FUndoSystemState& State{GetState()};
        State.mMessageSender.emplace(std::move(SenderToWorldChannel));
    }

    // =================================================================
    // Undo/Redo API
    // =================================================================
    void BeginTransaction(const FString& TransactionName) {
        FUndoSystemState& State{GetState()};
        if (State.mCurrentTransaction != nullptr)
            return;

        State.mCurrentTransaction = std::make_unique<FUndoTransaction>(TransactionName);
    }

    void RecordObject(UObject* TargetObject, EUndoType UndoType, const IAssetRegistry* AssetRegistry) {
        FUndoSystemState& State{GetState()};
        if (!State.mCurrentTransaction || !TargetObject)
            return;

        if (UndoType == EUndoType::StateChange) {
            if (State.mModifiedObjectsThisTransaction.contains(TargetObject))
                return;

            State.mModifiedObjectsThisTransaction.insert(TargetObject);
        }

        TArray<Uint8> CurrentData{};
        FArchiveMemory MemoryArchive{CurrentData};
        MemoryArchive.SetAssetRegistry(AssetRegistry);
        TargetObject->Save(MemoryArchive);

        if (UndoType == EUndoType::StateChange) {
            State.mPendingFinalizers.push_back(
                [TargetObject, CurrentData = std::move(CurrentData)](FUndoTransaction& Transaction) {
                    TArray<Uint8> AfterData{};
                    FArchiveMemory MemoryArchiveAfter{AfterData};
                    TargetObject->Save(MemoryArchiveAfter);

                    auto Record{std::make_unique<FRecordObjectState>(TargetObject->GetGuid(), CurrentData, AfterData)};

                    Transaction.AddRecord(std::move(Record));
                });
        } else {
            std::unique_ptr<IUndoRecord> Record{nullptr};
            switch (UndoType) {
                case EUndoType::Spawn:
                    Record = std::make_unique<FRecordObjectSpawned>(TargetObject->GetGuid(), std::move(CurrentData), TargetObject->GetTypeInfo()->mTypeName);
                    break;
                case EUndoType::Destroy:
                    Record = std::make_unique<FRecordObjectDestroyed>(TargetObject->GetGuid(), std::move(CurrentData), TargetObject->GetTypeInfo()->mTypeName);
                    break;
            }

            if (Record)
                State.mCurrentTransaction->AddRecord(std::move(Record));
        }
    }

    void EndTransaction() {
        FUndoSystemState& State{GetState()};
        if (!State.mCurrentTransaction)
            return;

        for (const auto& Finalizer : State.mPendingFinalizers) {
            Finalizer(*State.mCurrentTransaction);
        }

        State.mHistory.Push(std::move(*State.mCurrentTransaction));

        State.mCurrentTransaction.reset();
        State.mPendingFinalizers.clear();
        State.mModifiedObjectsThisTransaction.clear();
    }

    void Undo() {
        if (FUndoTransaction * Transactions{GetState().mHistory.Undo()}) {
            FUndoContextImpl UndoContext{};
            Transactions->Undo(&UndoContext);
        }
    }

    void Redo() {
        if (FUndoTransaction * Transactions{GetState().mHistory.Redo()}) {
            FUndoContextImpl RedoContext{};
            Transactions->Redo(&RedoContext);
        }
    }
}
