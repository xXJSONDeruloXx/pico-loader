#include "common.h"
#include <memory>
#include <string.h>
#include <libtwl/ipc/ipcFifo.h>
#include <libtwl/ipc/ipcSync.h>
#include <libtwl/i2c/i2cMcu.h>
#include <libtwl/sio/sioRtc.h>
#include <libtwl/sound/sound.h>
#include <libtwl/sound/soundChannel.h>
#include <libtwl/sound/soundCapture.h>
#include "core/Environment.h"
#include "logger/NitroEmulatorOutputStream.h"
#include "logger/PicoAgbAdapterOutputStream.h"
#include "logger/NocashOutputStream.h"
#include "logger/NullLogger.h"
#include "logger/PlainLogger.h"
#include "fat/dldi.h"
#include "loader/NdsLoader.h"
#include "sharedMemory.h"
#include "SaveState.h"
#include "ndsHeader.h"
#include "globalHeap.h"
#include "mmc/tmio.h"
#include "ipcCommands.h"

#define HANDSHAKE_PART0     0xA
#define HANDSHAKE_PART1     0xB
#define HANDSHAKE_PART2     0xC
#define HANDSHAKE_PART3     0xD

ILogger* gLogger;
FATFS gFatFs;

static NdsLoader sLoader;

static void initLogger()
{
    std::unique_ptr<IOutputStream> outputStream;
    if (Environment::IsIsNitroEmulator() && Environment::SupportsAgbSemihosting())
    {
        outputStream = std::make_unique<NitroEmulatorOutputStream>();
    }
    // else if (Environment::HasPicoAgbAdapter())
    // {
    //     outputStream = std::make_unique<PicoAgbAdapterOutputStream>();
    // }
    else if (Environment::SupportsNocashPrint())
    { 
        outputStream = std::make_unique<NocashOutputStream>();
    }
    else
    {
        gLogger = new NullLogger();
        return;
    }
    gLogger = new PlainLogger(LogLevel::All, std::move(outputStream));
}

static bool mountDldi()
{
    FRESULT res = f_mount(&gFatFs, "fat:", 1);
    if (res != FR_OK)
    {
        LOG_ERROR("dldi mount failed: %d\n", res);
        return false;
    }
    f_chdrive("fat:");
    return true;
}

static bool mountDsiSd()
{
    FRESULT res = f_mount(&gFatFs, "sd:", 1);
    if (res != FR_OK)
    {
        LOG_ERROR("dsi sd mount failed: %d\n", res);
        return false;
    }
    f_chdrive("sd:");
    return true;
}

static bool mountAgbSemihosting()
{
    FRESULT res = f_mount(&gFatFs, "pc2:", 1);
    if (res != FR_OK)
    {
        LOG_ERROR("pc2 sd mount failed: %d\n", res);
        return false;
    }
    f_chdrive("pc2:");
    return true;
}

extern "C" void __libc_init_array();

static constexpr const char* sSaveStateArgumentPrefix = "__pico_state=";

static const char* extractSaveStatePathFromArguments(u32& argumentsLength)
{
    if (gLoaderHeader.loadParams.argumentsLength == 0)
    {
        return nullptr;
    }

    if (strncmp(gLoaderHeader.loadParams.arguments, sSaveStateArgumentPrefix, strlen(sSaveStateArgumentPrefix)) != 0)
    {
        return nullptr;
    }

    argumentsLength = 0;
    return gLoaderHeader.loadParams.arguments + strlen(sSaveStateArgumentPrefix);
}

static void handleSavePath()
{
    if (gLoaderHeader.loadParams.savePath[0] == 0)
    {
        char* savePath = (char*)gLoaderHeader.loadParams.savePath;
        strcpy(savePath, gLoaderHeader.loadParams.romPath);
        char* extension = strrchr(savePath, '.');
        if (!extension)
            extension = &savePath[strlen(savePath)];
        extension[0] = '.';
        extension[1] = 's';
        extension[2] = 'a';
        extension[3] = 'v';
        extension[4] = 0;
    }
    sLoader.SetSavePath(gLoaderHeader.loadParams.savePath);
}

static u32 receiveFromArm9()
{
    while (ipc_isRecvFifoEmpty());
    return ipc_recvWordDirect();
}

static bool requestArm9VramChunk(u32 command, u32 offset, u32 size)
{
    ipc_sendWordDirect(command);
    ipc_sendWordDirect(offset);
    ipc_sendWordDirect(size);
    return receiveFromArm9() != 0;
}

static void handlePendingSaveStateDump()
{
    if (NTR_SHARED_MEMORY->mainMemoryCmd != MAIN_MEMORY_CMD_SAVE_STATE_DUMP &&
        NTR_SHARED_MEMORY_SDK5->mainMemoryCmd != MAIN_MEMORY_CMD_SAVE_STATE_DUMP)
    {
        return;
    }

    auto clearPendingSaveStateMarkers = []() {
        NTR_SHARED_MEMORY->mainMemoryCmd = MAIN_MEMORY_CMD_NONE;
        NTR_SHARED_MEMORY_SDK5->mainMemoryCmd = MAIN_MEMORY_CMD_NONE;
        *(volatile u32*)SAVE_STATE_ARM9_CONTEXT_ADDRESS = 0;
        *(volatile u32*)SAVE_STATE_ARM7_CONTEXT_ADDRESS = 0;
        ((volatile save_state_arm9_io_state_t*)SAVE_STATE_ARM9_IO_STATE_ADDRESS)->magic = 0;
        ((volatile save_state_arm7_io_state_t*)SAVE_STATE_ARM7_IO_STATE_ADDRESS)->magic = 0;
    };

    if (gLoaderHeader.loadParams.savePath[0] == 0)
    {
        LOG_ERROR("Savestate dump requested, but no path was provided\n");
        clearPendingSaveStateMarkers();
        return;
    }

    char tempSaveStatePath[272];
    size_t saveStatePathLength = strlen(gLoaderHeader.loadParams.savePath);
    if (saveStatePathLength + 5 >= sizeof(tempSaveStatePath))
    {
        LOG_ERROR("Savestate dump path is too long: %s\n", gLoaderHeader.loadParams.savePath);
        clearPendingSaveStateMarkers();
        return;
    }

    strcpy(tempSaveStatePath, gLoaderHeader.loadParams.savePath);
    strcat(tempSaveStatePath, ".tmp");
    f_unlink(tempSaveStatePath);

    FIL file;
    if (f_open(&file, tempSaveStatePath, FA_WRITE | FA_CREATE_ALWAYS) != FR_OK)
    {
        LOG_ERROR("Failed to open temp savestate dump file: %s\n", tempSaveStatePath);
        clearPendingSaveStateMarkers();
        return;
    }

    auto abortPendingSaveStateDump = [&]() {
        f_close(&file);
        f_unlink(tempSaveStatePath);
        clearPendingSaveStateMarkers();
    };

    constexpr u32 kPaletteBase      = 0x05000000;
    constexpr u32 kPaletteSize      = 0x800;
    constexpr u32 kOamBase          = 0x07000000;
    constexpr u32 kOamSize          = 0x800;
    constexpr u32 kRamBase          = 0x02000000;
    constexpr u32 kRamSize          = 0x400000u;
    constexpr u32 kSharedWramBase   = 0x03000000;
    constexpr u32 kSharedWramSize   = 0x8000;
    constexpr u32 kArm7WramBase     = 0x03800000;
    constexpr u32 kArm7WramSize     = 0x10000;
    constexpr u32 kVramSize         = 0xA4000;
    constexpr u32 kHeaderSize       = sizeof(save_state_file_header_v6_t);
    constexpr u32 kContextBlockSize = kHeaderSize + sizeof(save_state_cpu_context_t) * 2;
    constexpr u32 kSharedWramOff    = kContextBlockSize + kRamSize;
    constexpr u32 kArm7WramOff      = kSharedWramOff + kSharedWramSize;
    constexpr u32 kVramOff          = kArm7WramOff + kArm7WramSize;
    constexpr u32 kPaletteOff       = kVramOff + kVramSize;
    constexpr u32 kOamOff           = kPaletteOff + kPaletteSize;

    auto romHeader = (const nds_header_ntr_t*)TWL_SHARED_MEMORY->ntrSharedMem.romHeader;

    save_state_file_header_v6_t header
    {
        .magic              = SAVE_STATE_FILE_MAGIC_V6,
        .version            = SAVE_STATE_FILE_VERSION_V6,
        .arm9ContextOffset  = kHeaderSize,
        .arm9ContextSize    = sizeof(save_state_cpu_context_t),
        .arm7ContextOffset  = kHeaderSize + sizeof(save_state_cpu_context_t),
        .arm7ContextSize    = sizeof(save_state_cpu_context_t),
        .ramOffset          = kContextBlockSize,
        .ramSize            = kRamSize,
        .sharedWramOffset   = kSharedWramOff,
        .sharedWramSize     = kSharedWramSize,
        .arm7WramOffset     = kArm7WramOff,
        .arm7WramSize       = kArm7WramSize,
        .vramOffset         = kVramOff,
        .vramSize           = kVramSize,
        .paletteOffset      = kPaletteOff,
        .paletteSize        = kPaletteSize,
        .oamOffset          = kOamOff,
        .oamSize            = kOamSize,
        .gameCode           = romHeader->gameCode,
        .headerCrc          = romHeader->headerCrc,
    };

    UINT bytesWritten = 0;
    if (f_write(&file, &header, sizeof(header), &bytesWritten) != FR_OK || bytesWritten != sizeof(header))
    {
        LOG_ERROR("Failed to write savestate header\n");
        abortPendingSaveStateDump();
        return;
    }

    if (f_write(&file, (const void*)SAVE_STATE_ARM9_CONTEXT_ADDRESS, sizeof(save_state_cpu_context_t), &bytesWritten) != FR_OK || bytesWritten != sizeof(save_state_cpu_context_t))
    {
        LOG_ERROR("Failed to write ARM9 savestate context\n");
        abortPendingSaveStateDump();
        return;
    }

    if (f_write(&file, (const void*)SAVE_STATE_ARM7_CONTEXT_ADDRESS, sizeof(save_state_cpu_context_t), &bytesWritten) != FR_OK || bytesWritten != sizeof(save_state_cpu_context_t))
    {
        LOG_ERROR("Failed to write ARM7 savestate context\n");
        abortPendingSaveStateDump();
        return;
    }

    constexpr u32 kChunkSize = SAVE_STATE_TRANSFER_BUFFER_SIZE;
    for (u32 offset = 0; offset < header.ramSize; offset += kChunkSize)
    {
        u32 chunkSize = header.ramSize - offset;
        if (chunkSize > kChunkSize)
            chunkSize = kChunkSize;

        if (f_write(&file, (const void*)(kRamBase + offset), chunkSize, &bytesWritten) != FR_OK || bytesWritten != chunkSize)
        {
            LOG_ERROR("Failed while writing savestate dump at RAM offset 0x%x\n", offset);
            abortPendingSaveStateDump();
            return;
        }
    }

    if (f_write(&file, (const void*)kSharedWramBase, kSharedWramSize, &bytesWritten) != FR_OK || bytesWritten != kSharedWramSize)
    {
        LOG_ERROR("Failed to write shared WRAM savestate section\n");
        abortPendingSaveStateDump();
        return;
    }

    if (f_write(&file, (const void*)kArm7WramBase, kArm7WramSize, &bytesWritten) != FR_OK || bytesWritten != kArm7WramSize)
    {
        LOG_ERROR("Failed to write ARM7 WRAM savestate section\n");
        abortPendingSaveStateDump();
        return;
    }

    for (u32 offset = 0; offset < header.vramSize; offset += kChunkSize)
    {
        u32 chunkSize = header.vramSize - offset;
        if (chunkSize > kChunkSize)
            chunkSize = kChunkSize;

        if (!requestArm9VramChunk(IPC_COMMAND_ARM9_COPY_VRAM_CHUNK, offset, chunkSize))
        {
            LOG_ERROR("Failed to copy VRAM savestate chunk at offset 0x%x\n", offset);
            abortPendingSaveStateDump();
            return;
        }

        if (f_write(&file, (const void*)SAVE_STATE_TRANSFER_BUFFER_ADDRESS, chunkSize, &bytesWritten) != FR_OK || bytesWritten != chunkSize)
        {
            LOG_ERROR("Failed to write VRAM savestate section at offset 0x%x\n", offset);
            abortPendingSaveStateDump();
            return;
        }
    }

    if (f_write(&file, (const void*)kPaletteBase, kPaletteSize, &bytesWritten) != FR_OK || bytesWritten != kPaletteSize)
    {
        LOG_ERROR("Failed to write palette savestate section\n");
        abortPendingSaveStateDump();
        return;
    }

    if (f_write(&file, (const void*)kOamBase, kOamSize, &bytesWritten) != FR_OK || bytesWritten != kOamSize)
    {
        LOG_ERROR("Failed to write OAM savestate section\n");
        abortPendingSaveStateDump();
        return;
    }

    if (f_close(&file) != FR_OK)
    {
        LOG_ERROR("Failed to close temp savestate dump file\n");
        f_unlink(tempSaveStatePath);
        clearPendingSaveStateMarkers();
        return;
    }

    f_unlink(gLoaderHeader.loadParams.savePath);
    if (f_rename(tempSaveStatePath, gLoaderHeader.loadParams.savePath) != FR_OK)
    {
        LOG_ERROR("Failed to commit savestate dump file: %s\n", gLoaderHeader.loadParams.savePath);
        f_unlink(tempSaveStatePath);
        clearPendingSaveStateMarkers();
        return;
    }

    clearPendingSaveStateMarkers();
    LOG_DEBUG("Savestate v6 dump written to %s\n", gLoaderHeader.loadParams.savePath);
}

static void clearSoundRegisters()
{
    REG_SOUNDCNT = 0;
    REG_SNDCAP0CNT = 0;
    REG_SNDCAP1CNT = 0;

    for (int i = 0; i < 16; i++)
    {
        REG_SOUNDxCNT(i) = 0;
        REG_SOUNDxSAD(i) = 0;
        REG_SOUNDxTMR(i) = 0;
        REG_SOUNDxPNT(i) = 0;
        REG_SOUNDxLEN(i) = 0;
    }
}

static void initIpc()
{
    ipc_clearSendFifo();
    ipc_ackFifoError();
    ipc_disableRecvFifoNotEmptyIrq();
    ipc_enableFifo();

    while (!ipc_isRecvFifoEmpty())
    {
        ipc_recvWordDirect();
    }

    ipc_setArm7SyncBits(HANDSHAKE_PART0);
    while (ipc_getArm9SyncBits() != HANDSHAKE_PART0);
    ipc_setArm7SyncBits(HANDSHAKE_PART1);
    while (ipc_getArm9SyncBits() != HANDSHAKE_PART1);
    ipc_setArm7SyncBits(HANDSHAKE_PART2);
    while (ipc_getArm9SyncBits() != HANDSHAKE_PART2);
    ipc_setArm7SyncBits(HANDSHAKE_PART3);
    while (ipc_getArm9SyncBits() == HANDSHAKE_PART2);
}

extern "C" void loaderMain()
{
    __libc_init_array();

    clearSoundRegisters();
    rtos_initIrq();
    rtos_startMainThread();
    initIpc();

    bool dsiMode = ipc_getArm9SyncBits() == 1;

    Environment::Initialize(dsiMode);
    heap_init();
    initLogger();

    rtc_init(); // ensure rtc irqs are disabled

    LOG_DEBUG("Pico Loader ARM7 started\n");

    if (Environment::IsDsiMode())
    {
        // Let the mcu handle the power button
        mcu_writeReg(MCU_REG_MODE, 0);
        TMIO_init();
    }

    memset(&gFatFs, 0, sizeof(gFatFs));
    bool multiboot = (gLoaderHeader.bootDrive & PLOAD_BOOT_DRIVE_MULTIBOOT_FLAG) != 0;
    gLoaderHeader.bootDrive &= ~PLOAD_BOOT_DRIVE_MULTIBOOT_FLAG;
    switch (gLoaderHeader.bootDrive)
    {
        case PLOAD_BOOT_DRIVE_DLDI:
        {
            if (dldi_init())
            {
                mountDldi();
            }
            break;
        }
        case PLOAD_BOOT_DRIVE_DSI_SD:
        {
            if (Environment::IsDsiMode())
            {
                mountDsiSd();
            }
            break;
        }
        case PLOAD_BOOT_DRIVE_AGB_SEMIHOSTING:
        {
            if (Environment::SupportsAgbSemihosting())
            {
                mountAgbSemihosting();
            }
            break;
        }
    }

    handlePendingSaveStateDump();

    if (gLoaderHeader.v3.cheats != nullptr && gLoaderHeader.v3.cheats->numberOfCheats != 0)
    {
        // Copy cheats to vram
        auto cheats = (pload_cheats_t*)malloc(gLoaderHeader.v3.cheats->length);
        memcpy(cheats, gLoaderHeader.v3.cheats, gLoaderHeader.v3.cheats->length);
        sLoader.SetCheats(cheats);
    }

    if (multiboot)
    {
        LOG_DEBUG("Multiboot\n");
        sLoader.Load(BootMode::Multiboot);
    }
    else if (((nds_header_ntr_t*)TWL_SHARED_MEMORY->ntrSharedMem.romHeader)->arm7EntryAddress == (u32)gLoaderHeader.entryPoint)
    {
        LOG_DEBUG("Retail soft reset detected\n");
        u32 originalArm7EntryAddress = ((nds_header_ntr_t*)TWL_SHARED_MEMORY->ntrSharedMem.cardRomHeader)->arm7EntryAddress;
        ((nds_header_ntr_t*)TWL_SHARED_MEMORY->ntrSharedMem.romHeader)->arm7EntryAddress = originalArm7EntryAddress;
        sLoader.Load(BootMode::SdkResetSystem);
    }
    else
    {
        sLoader.SetRomPath(gLoaderHeader.loadParams.romPath);
        handleSavePath();
        u32 argumentsLength = gLoaderHeader.loadParams.argumentsLength;
        sLoader.SetSaveStatePath(extractSaveStatePathFromArguments(argumentsLength));
        sLoader.SetArguments(gLoaderHeader.loadParams.arguments, argumentsLength);
        sLoader.SetLauncherPath(gLoaderHeader.v2.launcherPath);
        sLoader.Load(BootMode::Normal);
    }

    while (true);
}