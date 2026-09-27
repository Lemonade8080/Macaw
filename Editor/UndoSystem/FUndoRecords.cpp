#include "pch.h"
#include "FUndoRecords.h"
#include "IUndoContext.h"

void FRecordObjectState::ApplyUndo(IUndoContext* Context) {
    Context->NotifyObjectChanged(mTargetGuid, mBeforeData);
}

void FRecordObjectState::ApplyRedo(IUndoContext* Context) {
    Context->NotifyObjectChanged(mTargetGuid, mAfterData);
}

void FRecordObjectSpawned::ApplyUndo(IUndoContext* Context) {

    Context->NotifyObjectDeleted(mTargetGuid);
}

void FRecordObjectSpawned::ApplyRedo(IUndoContext* Context) {

    Context->NotifyObjectSpawned(mTargetGuid, mSavedData, std::move(mTargetTypeName));
}

void FRecordObjectDestroyed::ApplyUndo(IUndoContext* Context) {

    Context->NotifyObjectSpawned(mTargetGuid, mSavedData, std::move(mTargetTypeName));
}

void FRecordObjectDestroyed::ApplyRedo(IUndoContext* Context) {

    Context->NotifyObjectDeleted(mTargetGuid);
}

FRecordObjectState::FRecordObjectState(const FGuid& InGuid, const TArray<Uint8>& InBefore, const TArray<Uint8>& InAfter)
    : mTargetGuid(InGuid),
      mBeforeData(InBefore),
      mAfterData(InAfter) {
}

FRecordObjectSpawned::FRecordObjectSpawned(FGuid InputGuid, const TArray<Uint8>& InputSavedData, std::string_view InputTargetTypeName)
    : mTargetGuid(InputGuid),
      mSavedData(std::move(InputSavedData)),
      mTargetTypeName(InputTargetTypeName) {
}

FRecordObjectDestroyed::FRecordObjectDestroyed(FGuid InputGuid, TArray<Uint8> InputSavedData, std::string_view InputTargetTypeName)
    : mTargetGuid(InputGuid),
      mSavedData(std::move(InputSavedData)),
      mTargetTypeName(InputTargetTypeName) {
}
