#pragma once

/// @brief Clears all cpu registers and jumps to the specified \p arm9EntryPoint.
/// @param arm9EntryPoint The arm9 entry point to jump to.
extern "C" void jumpToArm9EntryPoint(void* arm9EntryPoint);

/// @brief Checks for a valid savestate context in RAM; if found restores CPU state and
///        returns-from-IRQ back into the game. Otherwise does a normal entry-point boot.
/// @param arm9EntryPoint Fallback entry point when no valid context exists.
extern "C" void resumeOrBootArm9(void* arm9EntryPoint);
