#include "pch.h"
#include "FRevisionCursor.h"

Uint64 FRevisionCursor::GetRevision() const {
    return mRevision;
}

bool FRevisionCursor::IsCurrent(Uint64 Revision) const {
    return mValid && mRevision == Revision;
}

void FRevisionCursor::Commit(Uint64 Revision) {
    mRevision = Revision;
    mValid = true;
}

void FRevisionCursor::Invalidate() {
    mRevision = 0;
    mValid = false;
}
