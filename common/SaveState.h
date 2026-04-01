#pragma once
#include <nds/ndstypes.h>
#include "sharedMemory.h"

#define SAVE_STATE_FILE_MAGIC_V1   0x30535350u // PSS0
#define SAVE_STATE_FILE_MAGIC_V2   0x31535350u // PSS1
#define SAVE_STATE_FILE_MAGIC_V3   0x32535350u // PSS2
#define SAVE_STATE_FILE_MAGIC_V4   0x33535350u // PSS3
#define SAVE_STATE_FILE_MAGIC_V5   0x34535350u // PSS4
#define SAVE_STATE_FILE_VERSION_V5 5u
#define SAVE_STATE_FILE_VERSION_V4 4u
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

#define SAVE_STATE_ARM9_IO_STATE_V4_SIZE 220u
#define SAVE_STATE_ARM7_IO_STATE_V4_SIZE 36u

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
    u32 powerCnt;
    u32 mosaic;
    u32 mosaicSub;
    u32 dispCapCnt;
    u32 timer0;
    u32 timer1;
    u32 timer2;
    u32 timer3;
    u32 ie;
    u32 ime;

    u32 vramCntA;
    u32 vramCntB;
    u32 vramCntC;
    u32 vramCntD;
    u32 vramCntE;
    u32 vramCntF;
    u32 vramCntG;
    u32 vramCntH;
    u32 vramCntI;

    u32 bgOfsMain0;
    u32 bgOfsMain1;
    u32 bgOfsMain2;
    u32 bgOfsMain3;
    u32 bgOfsSub0;
    u32 bgOfsSub1;
    u32 bgOfsSub2;
    u32 bgOfsSub3;

    u32 bgAffineMain20;
    u32 bgAffineMain24;
    u32 bgAffineMain30;
    u32 bgAffineMain34;
    u32 bgAffineSub20;
    u32 bgAffineSub24;
    u32 bgAffineSub30;
    u32 bgAffineSub34;

    u32 winMain01;
    u32 winMain23;
    u32 winMainInOut;
    u32 winSub01;
    u32 winSub23;
    u32 winSubInOut;

    u32 blendMain0;
    u32 blendMain1;
    u32 blendSub0;
    u32 blendSub1;

    u32 dmaSad[4];
    u32 dmaDad[4];
    u32 dmaCnt[4];
};

static_assert(sizeof(save_state_arm9_io_state_t) == 268);

struct save_state_arm7_dma_channel_t
{
    u32 sad;
    u32 dad;
    u32 cnt;
};

static_assert(sizeof(save_state_arm7_dma_channel_t) == 12);

struct save_state_arm7_sound_channel_t
{
    u32 cnt;
    u32 sad;
    u32 tmrPnt;
    u32 len;
};

static_assert(sizeof(save_state_arm7_sound_channel_t) == 16);

struct save_state_arm7_io_state_t
{
    u32 magic;
    u32 timer0;
    u32 timer1;
    u32 timer2;
    u32 timer3;
    u32 ie;
    u32 ime;
    u32 soundCnt;
    u32 sndCapCnt;
    save_state_arm7_dma_channel_t dmaChannels[4];
    save_state_arm7_sound_channel_t soundChannels[16];
    u32 sndCapDad[2];
    u32 sndCapLen[2];
    u32 rcnt0L;
};

static_assert(sizeof(save_state_arm7_io_state_t) == 360);

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

struct save_state_file_header_v4_t
{
    u32 magic;
    u32 version;
    u32 arm9ContextOffset;
    u32 arm9ContextSize;
    u32 arm7ContextOffset;
    u32 arm7ContextSize;
    u32 ramOffset;
    u32 ramSize;
    u32 sharedWramOffset;
    u32 sharedWramSize;
    u32 arm7WramOffset;
    u32 arm7WramSize;
    u32 vramOffset;
    u32 vramSize;
    u32 paletteOffset;
    u32 paletteSize;
    u32 oamOffset;
    u32 oamSize;
};

static_assert(sizeof(save_state_file_header_v4_t) == 72);
