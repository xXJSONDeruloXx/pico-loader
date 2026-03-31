#pragma once
#include "patches/PatchCode.h"
#include "sections.h"

DEFINE_SECTION_SYMBOLS(patch_hotkeystatecapture);

extern "C" void hotkeystatecapture_entry(void);

class HotkeyStateCapturePatchCode : public PatchCode
{
public:
    explicit HotkeyStateCapturePatchCode(PatchHeap& patchHeap)
        : PatchCode(SECTION_START(patch_hotkeystatecapture), SECTION_SIZE(patch_hotkeystatecapture), patchHeap) { }

    const void* GetFunction() const
    {
        return GetAddressAtTarget((void*)hotkeystatecapture_entry);
    }
};
