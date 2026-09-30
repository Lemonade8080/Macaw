#include "pch.h"
#include "FRenderProbe.h"

#include <atomic>

Uint64 AllocateRenderSceneId() {
    static std::atomic<Uint64> NextId{1};
    return NextId.fetch_add(1, std::memory_order_relaxed);
}

Uint32 operator|(ERenderObjectFlags Left, ERenderObjectFlags Right) {
    return static_cast<Uint32>(Left) | static_cast<Uint32>(Right);
}
