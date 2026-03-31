#pragma once
#include "patches/Patch.h"

/// @brief Arm7 patch for injecting the cheat engine and hotkey reset handler in the vblank interrupt handler.
class CheatEnginePatch : public Patch
{
public:
    CheatEnginePatch(const void* cheats, const void* hotkeyResetArm7Function)
        : _cheats(cheats), _hotkeyResetArm7Function(hotkeyResetArm7Function) { }

    bool FindPatchTarget(PatchContext& patchContext) override;
    void ApplyPatch(PatchContext& patchContext) override;

private:
    const void* _cheats;
    const void* _hotkeyResetArm7Function;
    u32* _vblankIrqHandler = nullptr;
    const u32* _foundPattern = nullptr;
    u16 _thumb = false;
};
