#pragma once

#include "IUndoContext.h"

class IUndoRecord {
public:
    virtual ~IUndoRecord() = default;

    virtual void ApplyUndo(IUndoContext* Context) = 0;

    virtual void ApplyRedo(IUndoContext* Context) = 0;
};
