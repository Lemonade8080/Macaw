#pragma once
#include "Core/STL.h"
#include "IUndoContext.h"
#include "IUndoRecord.h"

class FUndoTransaction {
public:
    explicit FUndoTransaction(const FString& InName);

    ~FUndoTransaction();

    // move 만 사용
    FUndoTransaction(const FUndoTransaction&) = delete;
    FUndoTransaction& operator=(const FUndoTransaction&) = delete;

    FUndoTransaction(FUndoTransaction&&) noexcept = default;
    FUndoTransaction& operator=(FUndoTransaction&&) noexcept = default;

public:
    void AddRecord(std::unique_ptr<IUndoRecord> Record);

    void Undo(IUndoContext* Context);
    void Redo(IUndoContext* Context);

private:
    FString mTransactionName{};
    TArray<std::unique_ptr<IUndoRecord>> mRecords{};
};
