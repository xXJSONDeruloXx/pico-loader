#pragma once
#include "../PatchCode.h"
#include "sections.h"

DEFINE_SECTION_SYMBOLS(patch_vblankhotkey);

extern "C" void patch_vblankhotkey_handler(void);
extern u32 patch_vblankhotkey_originalHandler_address;
extern u32 patch_vblankhotkey_reboot_address;

class VBlankHotkeyPatchCode : public PatchCode
{
public:
    VBlankHotkeyPatchCode(PatchHeap& patchHeap, const void* originalHandler, const void* rebootFunction)
        : PatchCode(SECTION_START(patch_vblankhotkey), SECTION_SIZE(patch_vblankhotkey), patchHeap)
    {
        patch_vblankhotkey_originalHandler_address = (u32)originalHandler;
        patch_vblankhotkey_reboot_address = (u32)rebootFunction;
    }

    const void* GetHandlerFunction() const
    {
        return GetAddressAtTarget((void*)patch_vblankhotkey_handler);
    }
};
