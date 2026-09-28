#pragma once
#include "Core/Channel/FMessageChannel.h"
#include "Core/Base/UObject.h"
#include "Core/Asset/IAssetRegistry.h"

enum class EUndoType {
    StateChange,
    Spawn,
    Destroy,

    End
};

namespace FUndoSystem {
    // =================================================================
    // Message Sender
    // =================================================================
    void InitializeSenderToWorldChannel(FMessageChannel::FSender&& SenderToWorldChannel);

    // =================================================================
    // Undo/Redo API
    // =================================================================
    void BeginTransaction(const FString& TransactionName);
    void RecordObject(UObject* TargetObject, EUndoType UndoType, const IAssetRegistry* AssetRegistry);
    void EndTransaction();

    void Undo();
    void Redo();
}
