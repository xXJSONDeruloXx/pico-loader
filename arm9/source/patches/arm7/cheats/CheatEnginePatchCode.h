#pragma once
#include "sections.h"
#include "patches/PatchCode.h"

DEFINE_SECTION_SYMBOLS(patch_cheatengine);

extern "C" void cheatengine_entry_arm(void);
extern "C" void cheatengine_entry(void);

extern const void* cheatengine_cheatsPtr;
extern const void* cheatengine_hotkeyResetArm7_address;

class CheatEnginePatchCode : public PatchCode
{
public:
    CheatEnginePatchCode(PatchHeap& patchHeap, const void* cheatsAddress, const void* hotkeyResetArm7Function)
        : PatchCode(SECTION_START(patch_cheatengine), SECTION_SIZE(patch_cheatengine), patchHeap)
    {
        cheatengine_cheatsPtr = cheatsAddress;
        cheatengine_hotkeyResetArm7_address = hotkeyResetArm7Function;
    }

    const void* GetCheatEngineFunction() const
    {
        return GetAddressAtTarget((void*)cheatengine_entry);
    }

    const void* GetCheatEngineFunctionArm() const
    {
        return GetAddressAtTarget((void*)cheatengine_entry_arm);
    }
};
