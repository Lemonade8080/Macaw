#pragma once

#include "Core/Common.h"

class FRevisionCursor {
public:
    FRevisionCursor() = default;

public:
    Uint64 GetRevision() const;
    bool IsCurrent(Uint64 Revision) const;

    void Commit(Uint64 Revision);
    void Invalidate();

private:
    Uint64 mRevision{};
    bool mValid{};
};
