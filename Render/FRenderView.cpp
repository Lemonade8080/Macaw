#include "pch.h"
#include "FRenderView.h"

bool FRenderView::IsPassEnabled(ERenderPass Pass) const {
    const std::size_t Index{static_cast<std::size_t>(Pass)};
    if (Index >= mPasses.size() || !mPasses.test(Index)) {
        return false;
    }
    return Pass != ERenderPass::SelectionOutline || mPasses.test(static_cast<std::size_t>(ERenderPass::SceneGeometry));
}

void FRenderView::SetPassEnabled(ERenderPass Pass, bool Enabled) {
    const std::size_t Index{static_cast<std::size_t>(Pass)};
    if (Index < mPasses.size()) {
        mPasses.set(Index, Enabled);
    }
}
