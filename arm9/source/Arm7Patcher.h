#pragma once

class LoaderPlatform;

/// @brief Class for patching the arm7 of retail roms.
class Arm7Patcher
{
public:
    /// @brief Applies arm7 patches using the given \p loaderPlatform.
    /// @param loaderPlatform The loader platform to use.
    /// @param cheatsLength The length of the cheats data, or zero when there are no cheats.
    /// @param hotkeyResetArm7Function Pointer to the in-game hotkey reset function, or \c nullptr when unavailable.
    /// @param cheatsPtr Pointer to where the cheats need to be stored, or \c nullptr when there are no cheats.
    /// @return A pointer to the patch space in IWRAM, or \c nullptr if the patches have been placed in main memory.
    void* ApplyPatches(const LoaderPlatform* loaderPlatform, u32 cheatsLength,
        const void* hotkeyResetArm7Function, void*& cheatsPtr) const;
};
