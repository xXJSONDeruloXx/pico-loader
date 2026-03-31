#include "common.h"
#include "fastSearch.h"
#include "VBlankHotkeyPatchCode.h"
#include "VBlankHotkeyPatch.h"

static constexpr u32 MAX_HANDLER_LEN = 50 * sizeof(u32);
static constexpr u32 MAX_HANDLER_LEN_ALT = 0x200 * sizeof(u32);

static const u32 sHandlerStartSig[] =
{
    0xE92D4000u,
    0xE3A0C301u,
    0xE28CCE21u,
    0xE51C1008u
};

static const u32 sHandlerStartSigAlt[] =
{
    0xE3A0C301u,
    0xE5BC2208u,
    0xE1EC00D8u,
    0xE3520000u
};

static const u32 sHandlerEndSig[] =
{
    0xE59F1008u,
    0xE7910100u,
    0xE59FE004u,
    0xE12FFF10u
};

static const u32 sHandlerEndSigAlt[] =
{
    0xE59F100Cu,
    0xE5813000u,
    0xE5813004u,
    0xEAFFFFB8u
};

bool VBlankHotkeyPatch::FindPatchTarget(PatchContext& patchContext)
{
    auto handlerLocation = patchContext.FindPattern32(sHandlerStartSig, sizeof(sHandlerStartSig));
    bool alt = false;
    if (!handlerLocation)
    {
        handlerLocation = patchContext.FindPattern32(sHandlerStartSigAlt, sizeof(sHandlerStartSigAlt));
        alt = true;
    }
    if (!handlerLocation)
    {
        return false;
    }

    const u32* handlerEndSig = alt ? sHandlerEndSigAlt : sHandlerEndSig;
    u32 searchLength = alt ? MAX_HANDLER_LEN_ALT : MAX_HANDLER_LEN;
    auto handlerEnd = (u32*)fastSearch16(handlerLocation, searchLength, handlerEndSig);
    if (!handlerEnd)
    {
        return false;
    }

    auto wordsLocation = handlerEnd + 4;
    _irqTable = (u32*)wordsLocation[0];
    LOG_DEBUG("ARM9 IRQ table found at %p\n", _irqTable);
    return _irqTable != nullptr;
}

void VBlankHotkeyPatch::ApplyPatch(PatchContext& patchContext)
{
    if (!_irqTable || !_rebootFunction)
    {
        return;
    }

    auto patchCode = patchContext.GetPatchCodeCollection().AddUniquePatchCode<VBlankHotkeyPatchCode>(
        patchContext.GetPatchHeap(),
        (const void*)_irqTable[0],
        _rebootFunction);
    _irqTable[0] = (u32)patchCode->GetHandlerFunction();
    LOG_DEBUG("ARM9 VBlank hotkey handler enabled\n");
}
