#pragma once

#include "FRevisionCursor.h"

template <typename T>
class TRevisioned {
public:
    TRevisioned() = default;

public:
    const T& GetValue() const;
    Uint64 GetRevision() const;
    bool IsCurrent(const T& Value, Uint64 Revision) const;

    bool Update(const T& Value, Uint64 Revision);
    void Invalidate();

private:
    T mValue{};
    FRevisionCursor mRevision{};
};

template <typename T>
const T& TRevisioned<T>::GetValue() const {
    return mValue;
}

template <typename T>
Uint64 TRevisioned<T>::GetRevision() const {
    return mRevision.GetRevision();
}

template <typename T>
bool TRevisioned<T>::IsCurrent(const T& Value, Uint64 Revision) const {
    return mRevision.IsCurrent(Revision) && mValue == Value;
}

template <typename T>
bool TRevisioned<T>::Update(const T& Value, Uint64 Revision) {
    if (IsCurrent(Value, Revision)) {
        return false;
    }

    mValue = Value;
    mRevision.Commit(Revision);
    return true;
}

template <typename T>
void TRevisioned<T>::Invalidate() {
    mRevision.Invalidate();
}
