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
    // 생성의 취소는 곧 삭제

    Context->NotifyObjectDeleted(mTargetGuid);
}

void FRecordObjectSpawned::ApplyRedo(IUndoContext* Context) {
    // 다시 생성 (저장해둔 데이터로 부활)

    Context->NotifyObjectSpawned(mTargetGuid, mSavedData, std::move(mTargetTypeName));
}

void FRecordObjectDestroyed::ApplyUndo(IUndoContext* Context) {
    // 삭제의 취소는 곧 부활 (저장해둔 죽기 직전 데이터로 부활)

    Context->NotifyObjectSpawned(mTargetGuid, mSavedData, std::move(mTargetTypeName));
}

void FRecordObjectDestroyed::ApplyRedo(IUndoContext* Context) {
    // 다시 삭제

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
