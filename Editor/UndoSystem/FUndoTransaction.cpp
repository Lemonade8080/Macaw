#include "pch.h"
#include "FUndoTransaction.h"

#include "IUndoRecord.h"
#include <ranges>

FUndoTransaction::~FUndoTransaction() = default;

void FUndoTransaction::Undo(IUndoContext* Context) {
    for (const auto& Record : std::views::reverse(mRecords)) {
        Record->ApplyUndo(Context);
    }
}

void FUndoTransaction::Redo(IUndoContext* Context) {
    for (const auto& Record : mRecords) {
        Record->ApplyRedo(Context);
    }
}

FUndoTransaction::FUndoTransaction(const FString& InName)
    : mTransactionName(InName) {
}

void FUndoTransaction::AddRecord(std::unique_ptr<IUndoRecord> Record) {
    mRecords.push_back(std::move(Record));
}
