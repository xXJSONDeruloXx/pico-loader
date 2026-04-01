#pragma once
#include <nds/ndstypes.h>
#include "sharedMemory.h"

#define SAVE_STATE_FILE_MAGIC_V1   0x30535350u // PSS0
#define SAVE_STATE_FILE_MAGIC_V2   0x31535350u // PSS1
#define SAVE_STATE_FILE_MAGIC_V3   0x32535350u // PSS2
#define SAVE_STATE_FILE_VERSION_V3 3u
#define SAVE_STATE_FILE_VERSION_V2 2u
#define SAVE_STATE_CONTEXT_MAGIC   0x43545831u
#define SAVE_STATE_IO_MAGIC        0x494F5431u

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

struct save_state_arm9_io_state_t
{
    u32 magic;
    u32 dispcnt;
    u32 dispcntSub;
    u32 bgCnt01;
    u32 bgCnt23;
    u32 bgCntSub01;
    u32 bgCntSub23;
    u32 dispstat;
    u32 masterBright;
    u32 masterBrightSub;
    u32 timer0;
    u32 timer1;
    u32 timer2;
    u32 timer3;
    u32 ie;
    u32 ime;
};

static_assert(sizeof(save_state_arm9_io_state_t) == 64);

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
    // v3 extensions
    u32 vramOffset;
    u32 vramSize;
    u32 oamOffset;
    u32 oamSize;
    u32 paletteOffset;
    u32 paletteSize;
};

static_assert(sizeof(save_state_file_header_t) == 56);
