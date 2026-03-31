#pragma once
#include "../Patch.h"

class VBlankHotkeyPatch : public Patch
{
public:
    explicit VBlankHotkeyPatch(const void* rebootFunction)
        : _rebootFunction(rebootFunction) { }

    bool FindPatchTarget(PatchContext& patchContext) override;
    void ApplyPatch(PatchContext& patchContext) override;

private:
    const void* _rebootFunction;
    u32* _irqTable = nullptr;
};
