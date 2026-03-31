#pragma once
#include <nds/ndstypes.h>
#include "sharedMemory.h"

#define SAVE_STATE_FILE_MAGIC_V1   0x30535350u // PSS0
#define SAVE_STATE_FILE_MAGIC_V2   0x31535350u // PSS1
#define SAVE_STATE_FILE_VERSION_V2 2u

struct save_state_cpu_context_t
{
    u32 magic;
    u32 cpsr;
    u32 spsr;
    u32 sp;
    u32 lr;
    u32 pc;
    u32 r[13];
};

static_assert(sizeof(save_state_cpu_context_t) == 76);

struct save_state_file_header_t
{
    u32 magic;
    u32 version;
    u32 arm9ContextOffset;
    u32 arm9ContextSize;
    u32 arm7ContextOffset;
    u32 arm7ContextSize;
    u32 ramOffset;
    u32 ramSize;
};

static_assert(sizeof(save_state_file_header_t) == 32);
