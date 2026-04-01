#include "common.h"
#include <string.h>
#include <libtwl/rtos/rtosIrq.h>
#include <libtwl/sys/swi.h>
#include <libtwl/mem/memTwlWram.h>
#include <libtwl/dma/dmaTwl.h>
#include <libtwl/i2c/i2cMcu.h>
#include <libtwl/gfx/gfxStatus.h>
#include <libtwl/spi/spiFlash.h>
#include "core/Environment.h"
#include "clearFast.h"
#include "InverseKmpMatcher.h"
#include "fat/dldi.h"
#include "fileInfo.h"
#include "CardSaveArranger.h"
#include "gameCode.h"
#include "ApList.h"
#include "ApListFactory.h"
#include "TwlConfig.h"
#include "argv.h"
#include "DsiDeviceList.h"
#include "Blowfish.h"
#include "PicoLoaderArranger.h"
#include "ipc.h"
#include "errorDisplay/ErrorDisplay.h"
#include "TwlAes.h"
#include "DSMode.h"
#include "Arm7IoRegisterClearer.h"
#include "PatchListFactory.h"
#include "CheatPreprocessor.h"
#include "SaveState.h"
#include "NdsLoader.h"

#define AP_LIST_PATH      "/_pico/aplist.bin"
#define PATCH_LIST_PATH   "/_pico/patchlist.bin"
#define BIOS_NDS7_PATH    "/_pico/biosnds7.rom"

typedef void (*entrypoint_t)(void);

static const InverseKmpMatcher sDldiStubMatcher { (const u32[]) { ~0xBF8DA5EDu, ~0x69684320u, ~0x6D6873u } };

void NdsLoader::Load(BootMode bootMode)
{
    if (bootMode == BootMode::Normal)
    {
        LOG_DEBUG("Loading %s\n", _romPath);
    }

    u32 cardId = *(vu32*)&TWL_SHARED_MEMORY->ntrSharedMem.bootCheckInfo[0];
    u32 agbMem = *(vu32*)&TWL_SHARED_MEMORY->ntrSharedMem.isDebuggerData[0x1C];
    u32 resetParam = bootMode == BootMode::SdkResetSystem
        ? TWL_SHARED_MEMORY->ntrSharedMem.resetParam
        : 0;
    u32 bootType = TWL_SHARED_MEMORY->ntrSharedMem.multibootInfo.bootType;
    u32 romOffset = bootMode == BootMode::SdkResetSystem
        ? TWL_SHARED_MEMORY->ntrSharedMem.romOffset
        : 0;
    if (romOffset < 0x8000)
    {
        romOffset = 0;
    }

    if (bootMode != BootMode::Multiboot)
    {
        u8* gap0Buffer = nullptr;
        u8* gap200Buffer = nullptr;
        u8* pokemonBuffer = nullptr;
        u8* multibootBuffer = nullptr;
        if (bootMode == BootMode::SdkResetSystem)
        {
            gap0Buffer = new u8[sizeof(TWL_SHARED_MEMORY->ntrSharedMem.gap0)];
            memcpy(gap0Buffer, TWL_SHARED_MEMORY->ntrSharedMem.gap0, sizeof(TWL_SHARED_MEMORY->ntrSharedMem.gap0));
            gap200Buffer = new u8[sizeof(TWL_SHARED_MEMORY->ntrSharedMem.gap200)];
            memcpy(gap200Buffer, TWL_SHARED_MEMORY->ntrSharedMem.gap200, sizeof(TWL_SHARED_MEMORY->ntrSharedMem.gap200));
            multibootBuffer = new u8[sizeof(TWL_SHARED_MEMORY->ntrSharedMem.multibootInfo)];
            memcpy(multibootBuffer, &TWL_SHARED_MEMORY->ntrSharedMem.multibootInfo,
                sizeof(TWL_SHARED_MEMORY->ntrSharedMem.multibootInfo));
            if (*(vu32*)0x02FFF00C == GAMECODE("ADAJ"))
            {
                pokemonBuffer = new u8[0x160];
                memcpy(pokemonBuffer, (void*)0x02FFF000, 0x160);
            }
        }

        ClearMainMemory();

        if (Environment::IsIsNitroEmulator())
        {
            memset(&TWL_SHARED_MEMORY->ntrSharedMem, 0, sizeof(shared_memory_ntr_t));
            memset((void*)0x02FFF000, 0, 0x160);
            *(vu32*)&TWL_SHARED_MEMORY->ntrSharedMem.isDebuggerData[0x1C] = agbMem;
        }

        if (bootMode == BootMode::SdkResetSystem)
        {
            memcpy(TWL_SHARED_MEMORY->ntrSharedMem.gap0, gap0Buffer, sizeof(TWL_SHARED_MEMORY->ntrSharedMem.gap0));
            delete[] gap0Buffer;
            memcpy(TWL_SHARED_MEMORY->ntrSharedMem.gap200, gap200Buffer, sizeof(TWL_SHARED_MEMORY->ntrSharedMem.gap200));
            delete[] gap200Buffer;
            memcpy(&TWL_SHARED_MEMORY->ntrSharedMem.multibootInfo, multibootBuffer,
                sizeof(TWL_SHARED_MEMORY->ntrSharedMem.multibootInfo));
            delete[] multibootBuffer;
            if (pokemonBuffer)
            {
                memcpy((void*)0x02FFF000, pokemonBuffer, 0x160);
                delete[] pokemonBuffer;
            }
        }
    }

    if (!PicoLoaderArranger().SetupPicoLoaderInfo((loader_info_t*)TWL_SHARED_MEMORY->ntrSharedMem.cardRomHeader))
    {
        ErrorDisplay().PrintError("Failed to setup Pico Loader info.");
        return;
    }

    sendToArm9(IPC_COMMAND_ARM9_INITIALIZE_LOADER_INFO);
    receiveFromArm9();

    switch (bootMode)
    {
        case BootMode::Multiboot:
        {
            // already in memory
            break;
        }
        case BootMode::SdkResetSystem:
        {
            _romFile.obj.fs = &gFatFs;
            _romFile.obj.id = gFatFs.id;
            _romFile.obj.sclust = SHARED_ROM_FILE_INFO->clusterMap[2];
            _romFile.obj.attr = 0;
            _romFile.obj.objsize = SHARED_ROM_FILE_INFO->fileSize;
            _romFile.dir_sect = 0;
            _romFile.dir_ptr = gFatFs.win;
            _romFile.cltbl = (DWORD*)SHARED_ROM_FILE_INFO->clusterMap;
            _romFile.flag = FA_OPEN_EXISTING | FA_READ;
            _romFile.err = 0;
            _romFile.sect = 0;
            _romFile.fptr = 0;
            break;
        }
        default:
        {
            if (f_open(&_romFile, _romPath, FA_OPEN_EXISTING | FA_READ) != FR_OK)
            {
                LOG_FATAL("Failed to open rom file\n");
                ErrorDisplay().PrintError("Failed to open the rom file.\n%s", _romPath);
                return;
            }

            CreateRomClusterTable();
            break;
        }
    }

    if (bootMode == BootMode::Multiboot)
    {
        memcpy(&_romHeader, TWL_SHARED_MEMORY->ntrSharedMem.romHeader, sizeof(TWL_SHARED_MEMORY->ntrSharedMem.romHeader));
        _romFile.dir_sect = 0;
        _romFile.dir_ptr = gFatFs.win;
    }
    else
    {
        if (!TryLoadRomHeader(romOffset))
        {
            ErrorDisplay().PrintError("Failed to load rom header.");
            return;
        }
    }

    bool isHomebrew = (_romHeader.makerCode[0] == 0 && _romHeader.makerCode[1] == 0)
        || (_romHeader.arm9AutoLoadDoneHookAddress == 0 && _romHeader.arm7AutoLoadDoneHookAddress == 0)
        || _romHeader.arm7LoadAddress >= 0x03000000;

    if (isHomebrew)
    {
        LOG_DEBUG("Homebrew\n");
    }
    else
    {
        LOG_DEBUG("Sdk rom\n");

        bool isCloneBootRom = bootMode != BootMode::Multiboot && IsCloneBootRom(romOffset);
        sendToArm9(IPC_COMMAND_ARM9_SET_ROM_FILE_INFO);
        sendToArm9(_romFile.dir_sect);
        sendToArm9((u32)(_romFile.dir_ptr - _romFile.obj.fs->win));
        sendToArm9(isCloneBootRom ? 1 : 0);

        memset(&_dsiwareSaveResult, 0, sizeof(_dsiwareSaveResult));
        if (bootMode != BootMode::Multiboot)
        {
            if (_romHeader.IsDsiWare())
            {
                // There exist DS mode DSiWare applications, which don't have a save file.
                if (_romHeader.SupportsDsiMode())
                {
                    if (bootMode == BootMode::SdkResetSystem)
                    {
                        // todo: for DSiWare we should store the rom path
                        LOG_FATAL("Soft reset from DSiWare not yet supported\n");
                        return;
                    }

                    if (!TrySetupDsiWareSave())
                    {
                        ErrorDisplay().PrintError("Failed to setup DSiWare save.");
                        return;
                    }
                }
            }
            else
            {
                if (bootMode != BootMode::SdkResetSystem)
                {
                    if (!CardSaveArranger().SetupCardSave(&_romHeader, _savePath))
                    {
                        ErrorDisplay().PrintError("Failed to setup save file.");
                        return;
                    }
                }

                HandleAntiPiracy();
            }
        }
    }

    if (bootMode == BootMode::Normal)
    {
        bootType = _romHeader.IsDsiWare() ? BOOT_TYPE_NAND : BOOT_TYPE_CARD;
        if (_saveStatePath != nullptr && _saveStatePath[0] != 0)
        {
            bootType = BOOT_TYPE_MEMORY;
        }
        HandleIQueRegionFreePatching();
    }
    else if (bootMode == BootMode::Multiboot)
    {
        bootType = BOOT_TYPE_MULTIBOOT;
    }

    SetupSharedMemory(cardId, agbMem, resetParam, romOffset, bootType);

    if (Environment::IsDsiMode())
    {
        if (_romHeader.SupportsDsiMode())
        {
            SetupTwlConfig();
            TwlAes().SetupAes(&_romHeader);
        }

        RemapWram();
    }

    if (bootMode != BootMode::Multiboot)
    {
        if (!TryLoadArm9())
        {
            ErrorDisplay().PrintError("Failed to load arm9.");
            return;
        }

        if (Environment::IsDsiMode() && _romHeader.SupportsDsiMode())
        {
            if (!TryLoadArm9i())
            {
                ErrorDisplay().PrintError("Failed to load arm9i.");
                return;
            }
        }
    }

    if (!isHomebrew)
    {
        sendToArm9(IPC_COMMAND_ARM9_APPLY_PATCHES);
    }

    if (bootMode != BootMode::Multiboot)
    {
        if (!TryLoadArm7())
        {
            ErrorDisplay().PrintError("Failed to load arm7.");
            return;
        }

        if (Environment::IsDsiMode() && _romHeader.SupportsDsiMode())
        {
            if (!TryLoadArm7i())
            {
                ErrorDisplay().PrintError("Failed to load arm7i.");
                return;
            }
        }
    }

    if (!isHomebrew)
    {
        // wait for arm9 patches to be ready
        receiveFromArm9();

        HandleGameSpecificPatches();

        LOG_DEBUG("Arm9 patches done\n");

        ApplyArm7Patches();

        LOG_DEBUG("Arm7 patches done\n");
    }

    if (Environment::IsDsiMode() && _romHeader.SupportsDsiMode())
    {
        SetupDsiDeviceList();

        // Set twl wram locking (REG_MBK9) settings from rom header
        REG_MBK9 = _romHeader.mbk9Setting[0] | (_romHeader.mbk9Setting[1] << 8) | (_romHeader.mbk9Setting[2] << 16);

        u32 scfgExt7 = 0x93FBFB00 | (_romHeader.arm7ScfgExt7 & 0x40407);
        REG_SCFG_EXT = scfgExt7;
        REG_SCFG_CLK = 0x187;

        *(vu32*)0x03FFFFC4 = scfgExt7;
        *(vu32*)0x03FFFFC8 = 0xF884; // combination of various SCFG bits, including which bootrom is in use
    }
    else
    {
        // When booted through the DSi menu these are actually set to 0x12A00000 and 0xC0EC
        // It probably doesn't make a difference compared to the zero's from the DS firmware
        *(vu32*)0x03FFFFC4 = 0;
        // If this value is incorrect, SVC_GetPitchTable doesn't work correctly in SDK 5.
        // When bit 2 is set (arm9i bootrom second half protected) and bit 5 is clear (arm7i bootrom in use)
        // it enables a bugfix to get pitch table values from the arm7i bios.
        *(vu32*)0x03FFFFC8 = 0;
    }

    if (bootMode != BootMode::Multiboot)
    {
        f_close(&_romFile);
    }

    Arm7IoRegisterClearer().ClearIoRegisters();
    *(vu32*)0x03FFFFF8 = 0;
    *(vu32*)0x03FFFFFC = 0;

    if (Environment::IsDsiMode())
    {
        if (_romHeader.SupportsDsiMode())
        {
            if (!(_romHeader.twlFlags2 & 1))
            {
                DSMode().SwitchToDSTouchAndSoundMode(_romHeader.gameCode);
            }

            // Set back power button handling to irq mode
            mcu_writeReg(MCU_REG_MODE, 1);
        }
        else
        {
            DSMode().SwitchToDSMode(_romHeader.gameCode);
        }
    }

    if (isHomebrew)
    {
        InsertArgv();
        HandleHomebrewPatching();
    }

    HandleDldiPatching();
    if (!TryRestoreSaveState())
    {
        LOG_ERROR("Failed to restore savestate dump\n");
    }
    StartRom(bootMode);
}

bool NdsLoader::IsCloneBootRom(u32 romOffset)
{
    bool isCloneBootRom = false;
    if (_romHeader.ntrRomSize != 0)
    {
        UINT bytesRead = 0;
        u16 signatureHeader;
        if (f_lseek(&_romFile, romOffset + _romHeader.ntrRomSize) == FR_OK &&
            f_read(&_romFile, &signatureHeader, sizeof(signatureHeader), &bytesRead) == FR_OK &&
            bytesRead == 2 &&
            signatureHeader == 0x6361)
        {
            LOG_DEBUG("Cloneboot rom\n");
            isCloneBootRom = true;
        }
    }

    return isCloneBootRom;
}

void NdsLoader::InsertArgv()
{
    u32 argSize = 0;

    // Find the address to write argv
    u32 argDst = ((_romHeader.arm9LoadAddress + _romHeader.arm9Size + 3) & ~3) + 4;
    if (_romHeader.SupportsDsiMode())
    {
        u32 argDstTwl = ((_romHeader.arm9iLoadAddress + _romHeader.arm9iSize + 3) & ~3) + 4;
        if (argDstTwl > argDst)
        {
            argDst = argDstTwl;
        }
    }

    // If the rom path starts with / insert the drive prefix
    if (_romPath[0] == '/')
    {
        const char* drivePrefix;
        switch (gLoaderHeader.bootDrive)
        {
            case PLOAD_BOOT_DRIVE_DLDI:
            default:
            {
                drivePrefix = "fat:";
                break;
            }
            case PLOAD_BOOT_DRIVE_DSI_SD:
            {
                drivePrefix = "sd:";
                break;
            }
            case PLOAD_BOOT_DRIVE_AGB_SEMIHOSTING:
            {
                drivePrefix = "pc2:";
                break;
            }
        }

        strcpy((char*)(argDst + argSize), drivePrefix);
        argSize += strlen(drivePrefix);
    }

    // Copy the rom path
    strcpy((char*)(argDst + argSize), _romPath);
    argSize += strlen(_romPath) + 1;

    // Copy any additional arguments
    if (_argumentsLength > 0)
    {
        memcpy((void*)(argDst + argSize), _arguments, _argumentsLength);
        argSize += _argumentsLength;
    }

    HOMEBREW_ARGV->magic = HOMEBREW_ARGV_MAGIC;
    HOMEBREW_ARGV->commandLine = (char*)argDst;
    HOMEBREW_ARGV->length = argSize;
}

void NdsLoader::HandleGameSpecificPatches()
{
    auto patchList = PatchListFactory().CreateFromFile(PATCH_LIST_PATH);
    if (patchList)
    {
        auto entry = patchList->FindEntry(_romHeader.gameCode, _romHeader.softwareVersion);
        if (entry)
        {
            auto patch = &entry->firstPatch;
            for (int patchIndex = 0; patchIndex < entry->patchCount; patchIndex++)
            {
                switch (patch->common.patchType)
                {
                    case PatchListPatchType::Replace:
                    {
                        LOG_DEBUG("Applying replace patch. %d bytes to 0x%08X\n",
                            patch->replacePatch.dataLength, patch->replacePatch.address);
                        memcpy((void*)patch->replacePatch.address, patch->replacePatch.data, patch->replacePatch.dataLength);
                        patch = (const PatchListEntryPatch*)&patch->replacePatch.data[(patch->replacePatch.dataLength + 3) & ~3];
                        break;
                    }
                    case PatchListPatchType::Metafortress:
                    {
                        LOG_DEBUG("Applying Metafortress patch. %d addresses\n", patch->metafortressPatch.addressCount);
                        auto addresses = patch->metafortressPatch.addresses;
                        for (uint32_t addressIndex = 0; addressIndex < patch->metafortressPatch.addressCount; addressIndex++)
                        {
                            uint32_t address = addresses[addressIndex];
                            if ((address & 1) != 0)
                            {
                                // thumb
                                *(u16*)(address & ~1) = 0x4280; // cmp r0, r0
                            }
                            else
                            {
                                // arm
                                *(u32*)address = 0xE1500000; // cmp r0, r0
                            }
                        }
                        patch = (const PatchListEntryPatch*)&addresses[patch->metafortressPatch.addressCount];
                        break;
                    }
                    default:
                    {
                        LOG_ERROR("Unknown patch type\n");
                        return;
                    }
                }
            }
        }
        else
        {
            LOG_DEBUG("No entry found in patch list\n");
        }
    }
}

void NdsLoader::HandleHomebrewPatching()
{
    if (_launcherPath != nullptr && _launcherPath[0] != 0)
    {
        sendToArm9(IPC_COMMAND_ARM9_SETUP_HOMEBREW_BOOTSTUB);
        sendToArm9(16 * 1024); // required dldi space
        void* dldiSpace = (void*)receiveFromArm9();
        char* launcherPath = (char*)receiveFromArm9();
        if (dldiSpace != nullptr)
        {
            dldi_copyTo(dldiSpace);
        }
        if (launcherPath != nullptr)
        {
            strncpy(launcherPath, _launcherPath, 256);
            launcherPath[255] = 0;
        }
    }
}

void NdsLoader::ApplyArm7Patches()
{
    sendToArm9(IPC_COMMAND_ARM9_APPLY_ARM7_PATCHES);
    sendToArm9(_cheats ? _cheats->length : 0);
    PreprocessCheats();
    void* patchSpaceStart = (void*)receiveFromArm9();
    void* cheatsPtr = (void*)receiveFromArm9();
    if (cheatsPtr != nullptr && _cheats != nullptr)
    {
        memcpy(cheatsPtr, _cheats, _cheats->length);
    }
    if (patchSpaceStart)
    {
        u32 mbk6 = 0;
        u32 mbk7 = 0;
        u32 mbk8 = 0;
        if (Environment::IsDsiMode() && _romHeader.SupportsDsiMode())
        {
            mbk6 = REG_MBK6;
            mbk7 = REG_MBK7;
            mbk8 = REG_MBK8;
            REG_MBK6 = 0;
            REG_MBK7 = 0;
            REG_MBK8 = 0;
        }

        const u32 patchSpaceSize = 2 * 1024;
        auto patchCode = std::make_unique_for_overwrite<u8[]>(patchSpaceSize);
        void* srcAddress = (void*)(((u32)patchSpaceStart & 0x7FFF) + 0x037F8000);
        if ((u32)srcAddress + patchSpaceSize > 0x03800000)
        {
            u32 part1Size = (u32)0x03800000 - (u32)srcAddress;
            memcpy(patchCode.get(), srcAddress, part1Size);
            memcpy(patchCode.get() + part1Size, (void*)0x037F8000, patchSpaceSize - part1Size);
        }
        else
        {
            memcpy(patchCode.get(), srcAddress, patchSpaceSize);
        }

        if (Environment::IsDsiMode() && _romHeader.SupportsDsiMode())
        {
            REG_MBK6 = mbk6;
            REG_MBK7 = mbk7;
            REG_MBK8 = mbk8;
        }

        memcpy(patchSpaceStart, patchCode.get(), patchSpaceSize);
    }
}

void NdsLoader::PreprocessCheats()
{
    if (_cheats != nullptr)
    {
        CheatPreprocessor cheatPreprocessor;
        auto cheat = &_cheats->firstCheat;
        for (u32 i = 0; i < _cheats->numberOfCheats; i++)
        {
            cheatPreprocessor.PreprocessCheat(cheat);
            cheat = (pload_cheat_t*)((u8*)cheat + sizeof(u32) + cheat->length);
        }
    }
}

void NdsLoader::SetupSharedMemory(u32 cardId, u32 agbMem, u32 resetParam, u32 romOffset, u32 bootType)
{
    memcpy(TWL_SHARED_MEMORY->ntrSharedMem.cardRomHeader, &_romHeader, sizeof(nds_header_ntr_t) - 0x10);
    memcpy(TWL_SHARED_MEMORY->ntrSharedMem.romHeader, &_romHeader, sizeof(nds_header_ntr_t));
    *(vu32*)&TWL_SHARED_MEMORY->ntrSharedMem.bootCheckInfo[0] = cardId;
    *(vu32*)&TWL_SHARED_MEMORY->ntrSharedMem.bootCheckInfo[4] = cardId;
    *(vu16*)&TWL_SHARED_MEMORY->ntrSharedMem.bootCheckInfo[8] = _romHeader.headerCrc;
    *(vu16*)&TWL_SHARED_MEMORY->ntrSharedMem.bootCheckInfo[0xA] = _romHeader.secureAreaCrc;
    *(vu32*)&TWL_SHARED_MEMORY->ntrSharedMem.bootCheckInfo[0x10] = 0x5835; // use correct card id address
    *(vu32*)&TWL_SHARED_MEMORY->ntrSharedMem.isDebuggerData[0x1C] = agbMem;
    TWL_SHARED_MEMORY->ntrSharedMem.resetParam = resetParam;
    TWL_SHARED_MEMORY->ntrSharedMem.romOffset = romOffset;
    TWL_SHARED_MEMORY->ntrSharedMem.multibootInfo.bootType = bootType;

    LoadFirmwareUserSettings();

    if (!_romHeader.SupportsDsiMode())
    {
        memcpy((void*)0x027FF800, (void*)0x02FFF800, sizeof(shared_memory_ntr_t));
        memcpy((void*)0x023FF800, (void*)0x02FFF800, sizeof(shared_memory_ntr_t));
        // for Pokemon
        memcpy((void*)0x027FF000, (void*)0x02FFF000, 0x160);
        memcpy((void*)0x023FF000, (void*)0x02FFF000, 0x160);
    }
    else
    {
        strcpy(TWL_SHARED_MEMORY->sysMenuVersionInfoContentId, "00000009");
        TWL_SHARED_MEMORY->sysMenuVersionInfoContentLastInitialCode = 'P'; // europe
    }

    memcpy(TWL_SHARED_MEMORY->twlCardRomHeader, &_romHeader, sizeof(nds_header_twl_t));
    memcpy(TWL_SHARED_MEMORY->twlRomHeader, &_romHeader, sizeof(nds_header_twl_t));
}

void NdsLoader::LoadFirmwareUserSettings()
{
    u32 flashSettingsAddress;
    flash_readBytes(0x20, (u8*)&flashSettingsAddress, 2);
    flashSettingsAddress *= 8;

    u8 updateCounter1;
    flash_readBytes(flashSettingsAddress + 0x70, &updateCounter1, 1);

    u8 updateCounter2;
    flash_readBytes(flashSettingsAddress + 0x170, &updateCounter2, 1);

    if ((updateCounter1 & 0x7F) == ((updateCounter2 + 1) & 0x7F))
    {
        flash_readBytes(flashSettingsAddress, TWL_SHARED_MEMORY->ntrSharedMem.firmwareUserData, 0x74);
    }
    else
    {
        flash_readBytes(flashSettingsAddress + 0x100, TWL_SHARED_MEMORY->ntrSharedMem.firmwareUserData, 0x74);
    }

    flash_readBytes(0x36, &TWL_SHARED_MEMORY->ntrSharedMem.firmwareUserData[0x74], 6); // mac address
    *(u16*)&TWL_SHARED_MEMORY->ntrSharedMem.firmwareUserData[0x7A] = 0x1041; // wifi channels

    *(u32*)&TWL_SHARED_MEMORY->ntrSharedMem.firmwareUserData[0xE8] = 0x3E; // supported languages
    *(u32*)&TWL_SHARED_MEMORY->ntrSharedMem.firmwareUserData[0xEC] = 0; // wifi not force disabled
    TWL_SHARED_MEMORY->ntrSharedMem.firmwareUserData[0xF0] = 2; // region
}

bool NdsLoader::ShouldAttemptDldiPatch()
{
    // Some games contain a fake dldi header to trick flashcards
    // See also: https://shutterbug2000.github.io/very-clever/
    switch (_romHeader.gameCode)
    {
        // Final Fantasy IV
        case GAMECODE("YF4P"):
        case GAMECODE("YF4E"):
        case GAMECODE("YF4J"):
        // Final Fantasy Crystal Chronicles - Ring of Fates
        case GAMECODE("AFXE"):
        case GAMECODE("AFXP"):
        // Nanashi no Geemu
        case GAMECODE("YFQJ"):
        {
            return false;
        }
    }
    return true;
}

void NdsLoader::ClearMainMemory()
{
    if (Environment::IsDsiMode())
    {
        dma_twlSetFixedArbitration();
        dma_twlFill32(0, 0, (void*)0x02000000, 0x01000000);
    }
    else
    {
        sendToArm9(IPC_COMMAND_ARM9_CLEAR_MAIN_MEM);
        receiveFromArm9();
    }

    LOG_DEBUG("Main memory cleared\n");
}

void NdsLoader::CreateRomClusterTable()
{
    DWORD* clusterTab = (DWORD*)SHARED_ROM_FILE_INFO->clusterMap;
    clusterTab[0] = sizeof(SHARED_ROM_FILE_INFO->clusterMap) / sizeof(u32);
    _romFile.cltbl = clusterTab;
    FRESULT result = f_lseek(&_romFile, CREATE_LINKMAP);
    if (result != FR_OK)
    {
        LOG_FATAL("Failed to make cluster table. Result: %d\n", result);
        return;
    }
    SHARED_ROM_FILE_INFO->clusterShift = __builtin_ctz(_romFile.obj.fs->csize);
    SHARED_ROM_FILE_INFO->database = _romFile.obj.fs->database;
    SHARED_ROM_FILE_INFO->clusterMask = _romFile.obj.fs->csize - 1;
    SHARED_ROM_FILE_INFO->fileSize = f_size(&_romFile);

    LOG_DEBUG("Made cluster table\n");
}

bool NdsLoader::TryLoadRomHeader(u32 romOffset)
{
    if (f_lseek(&_romFile, romOffset) != FR_OK)
    {
        LOG_FATAL("Failed to seek to header\n");
        return false;
    }
    UINT bytesRead = 0;
    FRESULT result = f_read(&_romFile, &_romHeader, sizeof(_romHeader), &bytesRead);
    if (result != FR_OK || bytesRead < 512)
    {
        LOG_FATAL("Failed to read rom header. Result: %d, bytesRead: %d\n", result, bytesRead);
        return false;
    }

    LOG_DEBUG("Rom header loaded\n");
    return true;
}

void NdsLoader::HandleAntiPiracy()
{
    auto apList = ApListFactory().CreateFromFile(AP_LIST_PATH);
    if (apList)
    {
        auto entry = apList->FindEntry(_romHeader.gameCode, _romHeader.softwareVersion);
        if (entry)
        {
            entry->Dump();
            sendToArm9(IPC_COMMAND_ARM9_SET_AP_INFO);
            sendToArm9(((u32*)entry)[0]);
            sendToArm9(((u32*)entry)[1]);
            sendToArm9(((u32*)entry)[2]);
            sendToArm9(((u32*)entry)[3]);
        }
        else
        {
            LOG_DEBUG("No entry found in ap list\n");
        }
    }
}

void NdsLoader::RemapWram()
{
    mem_unlockAllTwlWram();
    if (_romHeader.SupportsDsiMode())
    {
        REG_MBK6 = _romHeader.arm7MbkSettings[0];
        REG_MBK7 = _romHeader.arm7MbkSettings[1];
        REG_MBK8 = _romHeader.arm7MbkSettings[2];
        sendToArm9(IPC_COMMAND_ARM9_WRAM_CONFIG);
        sendToArm9(_romHeader.wramCntSetting);
        sendToArm9(_romHeader.arm9MbkSettings[0]);
        sendToArm9(_romHeader.arm9MbkSettings[1]);
        sendToArm9(_romHeader.arm9MbkSettings[2]);
        sendToArm9(_romHeader.globalMbkSettings[0]);
        sendToArm9(_romHeader.globalMbkSettings[1]);
        sendToArm9(_romHeader.globalMbkSettings[2]);
        sendToArm9(_romHeader.globalMbkSettings[3]);
        sendToArm9(_romHeader.globalMbkSettings[4]);
        receiveFromArm9();
    }
    else
    {
        REG_MBK6 = 0;
        REG_MBK7 = 0;
        REG_MBK8 = 0;
        sendToArm9(IPC_COMMAND_ARM9_WRAM_CONFIG);
        sendToArm9(3);
        sendToArm9(0);
        sendToArm9(0);
        sendToArm9(0);
        sendToArm9(0);
        sendToArm9(0);
        sendToArm9(0);
        sendToArm9(0);
        sendToArm9(0);
        receiveFromArm9();
    }

    LOG_DEBUG("Wram configured\n");
}

bool NdsLoader::TryLoadArm9()
{
    if (f_lseek(&_romFile, _romHeader.arm9RomOffset + TWL_SHARED_MEMORY->ntrSharedMem.romOffset) != FR_OK)
    {
        LOG_FATAL("Failed to seek to arm9\n");
        return false;
    }

    UINT bytesRead = 0;
    FRESULT result = f_read(&_romFile, (void*)_romHeader.arm9LoadAddress, _romHeader.arm9Size, &bytesRead);
    if (result != FR_OK || bytesRead != _romHeader.arm9Size)
    {
        LOG_FATAL("Failed to read arm9. Result: %d, bytesRead: %d\n", result, bytesRead);
        return false;
    }

    if (!TryDecryptSecureArea())
    {
        LOG_WARNING("Failed to decrypt secure area\n");
        // Keep going. In case the rom doesn't actually have a(n encrypted) secure area
        // it will run anyway. This can happen for homebrew for example.
    }

    LOG_DEBUG("Arm9 loaded\n");
    return true;
}

bool NdsLoader::TryLoadArm9i()
{
    if (_romHeader.arm9iSize == 0)
    {
        return true;
    }

    if (f_lseek(&_romFile, _romHeader.arm9iRomOffset) != FR_OK)
    {
        LOG_FATAL("Failed to seek to arm9i\n");
        return false;
    }

    UINT bytesRead = 0;
    FRESULT result = f_read(&_romFile, (void*)_romHeader.arm9iLoadAddress, _romHeader.arm9iSize, &bytesRead);
    if (result != FR_OK || bytesRead != _romHeader.arm9iSize)
    {
        LOG_FATAL("Failed to read arm9i. Result: %d, bytesRead: %d\n", result, bytesRead);
        return false;
    }

    LOG_DEBUG("Arm9i loaded\n");

    return TryDecryptArm9i();
}

bool NdsLoader::TryDecryptArm9i()
{
    if (!(_romHeader.twlFlags & (1 << 1)))
    {
        return true;
    }

    if (_romHeader.modcryptArea1Offset != _romHeader.arm9iRomOffset)
    {
        LOG_FATAL("Could not decrypt arm9i. Encrypted area offset is not equal to arm9i offset.\n");
        return false;
    }

    TwlAes().DecryptModuleAes((void*)_romHeader.arm9iLoadAddress, _romHeader.modcryptArea1Size,
        (const aes_u128_t*)_romHeader.arm9Sha1Hmac);

    LOG_DEBUG("Arm9i decrypted\n");
    return true;
}

bool NdsLoader::TryDecryptArm7i()
{
    if (!(_romHeader.twlFlags & (1 << 1)) || _romHeader.modcryptArea2Size == 0)
    {
        return true;
    }

    if (_romHeader.modcryptArea2Offset != _romHeader.arm7iRomOffset)
    {
        LOG_FATAL("Could not decrypt arm7i. Encrypted area offset is not equal to arm7i offset.\n");
        return false;
    }

    TwlAes().DecryptModuleAes((void*)_romHeader.arm7iLoadAddress, _romHeader.modcryptArea2Size,
        (const aes_u128_t*)_romHeader.arm7Sha1Hmac);

    LOG_DEBUG("Arm7i decrypted\n");
    return true;
}

bool NdsLoader::TryLoadArm7()
{
    if (f_lseek(&_romFile, _romHeader.arm7RomOffset + TWL_SHARED_MEMORY->ntrSharedMem.romOffset) != FR_OK)
    {
        LOG_FATAL("Failed to seek to arm7\n");
        return false;
    }

    UINT bytesRead = 0;
    FRESULT result = f_read(&_romFile, (void*)_romHeader.arm7LoadAddress, _romHeader.arm7Size, &bytesRead);
    if (result != FR_OK || bytesRead != _romHeader.arm7Size)
    {
        LOG_FATAL("Failed to read arm7. Result: %d, bytesRead: %d\n", result, bytesRead);
        return false;
    }

    LOG_DEBUG("Arm7 loaded\n");
    return true;
}

bool NdsLoader::TryLoadArm7i()
{
    if (_romHeader.arm7iSize == 0)
    {
        return true;
    }

    if (f_lseek(&_romFile, _romHeader.arm7iRomOffset) != FR_OK)
    {
        LOG_FATAL("Failed to seek to arm7i\n");
        return false;
    }

    UINT bytesRead = 0;
    FRESULT result = f_read(&_romFile, (void*)_romHeader.arm7iLoadAddress, _romHeader.arm7iSize, &bytesRead);
    if (result != FR_OK || bytesRead != _romHeader.arm7iSize)
    {
        LOG_FATAL("Failed to read arm7i. Result: %d, bytesRead: %d\n", result, bytesRead);
        return false;
    }

    LOG_DEBUG("Arm7i loaded\n");

    return TryDecryptArm7i();
}

void NdsLoader::HandleDldiPatching()
{
    if (!ShouldAttemptDldiPatch())
    {
        return;
    }

    int arm9DldiAddr = sDldiStubMatcher.FindFirstOccurance(
        (const u32*)_romHeader.arm9LoadAddress, _romHeader.arm9Size >> 2) << 2;
    if (arm9DldiAddr >= 0)
    {
        dldi_header_t* arm9Dldi = (dldi_header_t*)(_romHeader.arm9LoadAddress + arm9DldiAddr);
        LOG_DEBUG("Dldi found in arm9 at address %p\n", arm9Dldi);
        dldi_patchTo(arm9Dldi);
    }

    int arm7DldiAddr = sDldiStubMatcher.FindFirstOccurance(
        (const u32*)_romHeader.arm7LoadAddress, _romHeader.arm7Size >> 2) << 2;
    if (arm7DldiAddr >= 0)
    {
        dldi_header_t* arm7Dldi = (dldi_header_t*)(_romHeader.arm7LoadAddress + arm7DldiAddr);
        LOG_DEBUG("Dldi found in arm7 at address %p\n", arm7Dldi);
        dldi_patchTo(arm7Dldi);
    }
}

static void restoreArm7IoState()
{
    auto ioState = (volatile save_state_arm7_io_state_t*)SAVE_STATE_ARM7_IO_STATE_ADDRESS;
    if (ioState->magic != SAVE_STATE_IO_MAGIC)
    {
        return;
    }

    ioState->magic = 0;
    *(vu16*)0x04000100 = (u16)ioState->timer0;
    *(vu16*)0x04000102 = (u16)(ioState->timer0 >> 16);
    *(vu16*)0x04000104 = (u16)ioState->timer1;
    *(vu16*)0x04000106 = (u16)(ioState->timer1 >> 16);
    *(vu16*)0x04000108 = (u16)ioState->timer2;
    *(vu16*)0x0400010A = (u16)(ioState->timer2 >> 16);
    *(vu16*)0x0400010C = (u16)ioState->timer3;
    *(vu16*)0x0400010E = (u16)(ioState->timer3 >> 16);
    REG_IE = ioState->ie;
    *(vu16*)0x04000500 = (u16)ioState->soundCnt;

    for (int i = 0; i < 4; i++)
    {
        *(vu32*)(0x040000B0 + i * 0xC) = ioState->dmaChannels[i].sad;
        *(vu32*)(0x040000B4 + i * 0xC) = ioState->dmaChannels[i].dad;
        *(vu32*)(0x040000B8 + i * 0xC) = ioState->dmaChannels[i].cnt;
    }

    for (int i = 0; i < 16; i++)
    {
        *(vu32*)(0x04000404 + i * 0x10) = ioState->soundChannels[i].sad;
        *(vu32*)(0x04000408 + i * 0x10) = ioState->soundChannels[i].tmrPnt;
        *(vu32*)(0x0400040C + i * 0x10) = ioState->soundChannels[i].len;
        *(vu32*)(0x04000400 + i * 0x10) = ioState->soundChannels[i].cnt;
    }

    *(vu32*)0x04000510 = ioState->sndCapDad[0];
    *(vu32*)0x04000514 = ioState->sndCapLen[0];
    *(vu32*)0x04000518 = ioState->sndCapDad[1];
    *(vu32*)0x0400051C = ioState->sndCapLen[1];
    *(vu8*)0x04000508 = (u8)ioState->sndCapCnt;
    *(vu8*)0x04000509 = (u8)(ioState->sndCapCnt >> 8);
    *(vu16*)0x04000134 = (u16)ioState->rcnt0L;
    REG_IME = ioState->ime;
}

static bool restoreArm9VramState(FIL& file, u32 fileOffset, u32 size)
{
    if (f_lseek(&file, fileOffset) != FR_OK)
    {
        return false;
    }

    UINT bytesRead = 0;
    for (u32 offset = 0; offset < size; offset += SAVE_STATE_TRANSFER_BUFFER_SIZE)
    {
        u32 chunkSize = size - offset;
        if (chunkSize > SAVE_STATE_TRANSFER_BUFFER_SIZE)
            chunkSize = SAVE_STATE_TRANSFER_BUFFER_SIZE;

        if (f_read(&file, (void*)SAVE_STATE_TRANSFER_BUFFER_ADDRESS, chunkSize, &bytesRead) != FR_OK || bytesRead != chunkSize)
        {
            return false;
        }

        sendToArm9(IPC_COMMAND_ARM9_RESTORE_VRAM_CHUNK);
        sendToArm9(offset);
        sendToArm9(chunkSize);
        if (receiveFromArm9() == 0)
        {
            return false;
        }
    }

    return true;
}

static bool restoreArm9ItcmState(FIL& file, u32 fileOffset, u32 size)
{
    if (size > SAVE_STATE_ARM9_ITCM_BUFFER_SIZE)
    {
        return false;
    }

    auto itcmInfo = (save_state_arm9_itcm_state_t*)SAVE_STATE_ARM9_ITCM_INFO_ADDRESS;
    itcmInfo->magic = 0;
    itcmInfo->size = 0;

    if (size == 0)
    {
        return true;
    }

    UINT bytesRead = 0;
    u32 firstChunkSize = size;
    if (firstChunkSize > SAVE_STATE_ARM9_ITCM_BUFFER_CHUNK_SIZE)
    {
        firstChunkSize = SAVE_STATE_ARM9_ITCM_BUFFER_CHUNK_SIZE;
    }

    if (f_lseek(&file, fileOffset) != FR_OK ||
        f_read(&file, (void*)SAVE_STATE_ARM9_ITCM_BUFFER0_ADDRESS, firstChunkSize, &bytesRead) != FR_OK ||
        bytesRead != firstChunkSize)
    {
        return false;
    }

    u32 secondChunkSize = size - firstChunkSize;
    if (secondChunkSize > 0)
    {
        if (f_read(&file, (void*)SAVE_STATE_ARM9_ITCM_BUFFER1_ADDRESS, secondChunkSize, &bytesRead) != FR_OK ||
            bytesRead != secondChunkSize)
        {
            return false;
        }
    }

    itcmInfo->size = size;
    itcmInfo->magic = SAVE_STATE_ITCM_MAGIC;
    return true;
}

void NdsLoader::StartRom(BootMode bootMode)
{
    LOG_DEBUG("Booting...\n");
    while (gfx_getVCount() != 191);
    while (gfx_getVCount() == 191);
    sendToArm9(IPC_COMMAND_ARM9_BOOT);
    sendToArm9(bootMode == BootMode::SdkResetSystem ? 1 : 0);
    sendToArm9(_saveStatePath && _saveStatePath[0] ? 1 : 0); // isResumeState

    REG_IF = ~0u;
    if (Environment::IsDsiMode())
    {
        REG_IF2 = ~0u;
    }

    restoreArm7IoState();
    ((entrypoint_t)_romHeader.arm7EntryAddress)();
}

void NdsLoader::SetupTwlConfig()
{
    ConsoleRegion romRegion = GetRomRegion(_romHeader.gameCode);
    // Set language based on rom region, TODO: allow user to override this via some config or so
    UserLanguage userLang = GetLanguageByRomRegion(romRegion);

    twl_config_t* twlConfig = (twl_config_t*)0x02000400;
    *(twl_config_t**)0x02FFFDFC = twlConfig;
    *(vu8*)0x02FFFDFA = 0x80;
    *(vu8*)0x02FFFDFB = 1;
    memset(twlConfig, 0, sizeof(twl_config_t));
    twlConfig->configFlags = 0x0100000F;
    twlConfig->country = 0x4E;
    twlConfig->language = static_cast<u8>(userLang);
    twlConfig->rtcYear = TWL_SHARED_MEMORY->ntrSharedMem.firmwareUserData[0x66];
    twlConfig->rtcOffset = *(s64*)&TWL_SHARED_MEMORY->ntrSharedMem.firmwareUserData[0x68];
    twlConfig->eulaAgreeVersion[0] = 1;
    twlConfig->alarmHour = TWL_SHARED_MEMORY->ntrSharedMem.firmwareUserData[0x52];
    twlConfig->alarmMinute = TWL_SHARED_MEMORY->ntrSharedMem.firmwareUserData[0x53];
    twlConfig->alarmEnable = TWL_SHARED_MEMORY->ntrSharedMem.firmwareUserData[0x56];
    twlConfig->systemMenuUsedTitleSlots = 9;
    twlConfig->systemMenuFreeTitleSlots = 30;
    twlConfig->field24 = 3;
    memcpy(&twlConfig->touchCalibrationX1Adc, &TWL_SHARED_MEMORY->ntrSharedMem.firmwareUserData[0x58], 0xC);
    twlConfig->field3C = 0x0201209C;
    twlConfig->favoriteColor = TWL_SHARED_MEMORY->ntrSharedMem.firmwareUserData[2];
    twlConfig->birthdayMonth = TWL_SHARED_MEMORY->ntrSharedMem.firmwareUserData[3];
    twlConfig->birthdayDay = TWL_SHARED_MEMORY->ntrSharedMem.firmwareUserData[4];
    memcpy(twlConfig->nickname, &TWL_SHARED_MEMORY->ntrSharedMem.firmwareUserData[6], 0x16);
    memcpy(twlConfig->message, &TWL_SHARED_MEMORY->ntrSharedMem.firmwareUserData[0x1C], 0x36);
    u8 wifiType;
    flash_readBytes(0x1FD, &wifiType, 1);
    *(vu8*)0x020005E0 = wifiType;
    if (wifiType == 2 || wifiType == 3)
    {
        *(vu32*)0x020005E4 = 0x520000;
        *(vu32*)0x020005E8 = 0x520000;
        *(vu32*)0x020005EC = 0x020000;
    }
    else
    {
        *(vu32*)0x020005E4 = 0x500400;
        *(vu32*)0x020005E8 = 0x500000;
        *(vu32*)0x020005EC = 0x02E000;
    }
    *(vu16*)0x020005E2 = swi_getCrc16(0xFFFF, (void*)0x020005E4, 0xC);

    // Set bitmask for supported languages
    *(vu32*)0x02FFFD68 = GetSupportedLanguagesByRegion(romRegion);
    *(vu32*)0x02FFFD6C = 0;
    *(vu8*)0x02FFFD70 = static_cast<u8>(romRegion); // region
}

void NdsLoader::SetDeviceListEntry(dsi_devicelist_entry_t& deviceListEntry,
    char driveLetter, const char* deviceName, const char* path, u8 flags, u8 accessRights)
{
    deviceListEntry.driveLetter = driveLetter;
    deviceListEntry.flags = flags;
    deviceListEntry.accessRights = accessRights;
    strcpy(deviceListEntry.deviceName, deviceName);
    strcpy(deviceListEntry.path, path);
}

void NdsLoader::SetupDsiDeviceList()
{
    if (_romHeader.arm7DeviceListAddress == 0)
    {
        return;
    }

    auto deviceList = (dsi_devicelist_t*)_romHeader.arm7DeviceListAddress;
    memset(deviceList, 0, sizeof(dsi_devicelist_t));

    int listEntry = 0;
    SetDeviceListEntry(deviceList->deviceList[listEntry++], 'A', "nand", "/",
        DSI_DEVICELIST_ENTRY_FLAGS_DRIVE_SDMC | DSI_DEVICELIST_ENTRY_FLAGS_TYPE_PHYSICAL,
        DSI_DEVICELIST_ENTRY_ACCESS_RIGHTS_READ | DSI_DEVICELIST_ENTRY_ACCESS_RIGHTS_WRITE);
    SetDeviceListEntry(deviceList->deviceList[listEntry++], 'D', "shared1", "nand:/shared1",
        DSI_DEVICELIST_ENTRY_FLAGS_DRIVE_SDMC | DSI_DEVICELIST_ENTRY_FLAGS_TYPE_FOLDER,
        DSI_DEVICELIST_ENTRY_ACCESS_RIGHTS_READ | DSI_DEVICELIST_ENTRY_ACCESS_RIGHTS_WRITE);
    SetDeviceListEntry(deviceList->deviceList[listEntry++], 'F', "photo", "nand:/photo",
        DSI_DEVICELIST_ENTRY_FLAGS_DRIVE_SDMC | DSI_DEVICELIST_ENTRY_FLAGS_TYPE_FOLDER,
        DSI_DEVICELIST_ENTRY_ACCESS_RIGHTS_READ | DSI_DEVICELIST_ENTRY_ACCESS_RIGHTS_WRITE);

    if (_romHeader.twlPrivateSavSize != 0)
    {
        SetDeviceListEntry(deviceList->deviceList[listEntry++], 'G', "dataPrv", _dsiwareSaveResult.privateSavePath,
            DSI_DEVICELIST_ENTRY_FLAGS_DRIVE_SDMC | DSI_DEVICELIST_ENTRY_FLAGS_TYPE_FILE,
            DSI_DEVICELIST_ENTRY_ACCESS_RIGHTS_READ | DSI_DEVICELIST_ENTRY_ACCESS_RIGHTS_WRITE);
    }

    if (_romHeader.twlPublicSavSize != 0)
    {
        SetDeviceListEntry(deviceList->deviceList[listEntry++], 'H', "dataPub", _dsiwareSaveResult.publicSavePath,
            DSI_DEVICELIST_ENTRY_FLAGS_DRIVE_SDMC | DSI_DEVICELIST_ENTRY_FLAGS_TYPE_FILE,
            DSI_DEVICELIST_ENTRY_ACCESS_RIGHTS_READ | DSI_DEVICELIST_ENTRY_ACCESS_RIGHTS_WRITE);
    }

    SetDeviceListEntry(deviceList->deviceList[listEntry++], 'I', "sdmc", "nand:/.",
        DSI_DEVICELIST_ENTRY_FLAGS_DRIVE_SDMC | DSI_DEVICELIST_ENTRY_FLAGS_TYPE_FOLDER,
        DSI_DEVICELIST_ENTRY_ACCESS_RIGHTS_READ | DSI_DEVICELIST_ENTRY_ACCESS_RIGHTS_WRITE);

    strcpy(deviceList->appFileName, _dsiwareSaveResult.romFilePath);
}

bool NdsLoader::TrySetupDsiWareSave()
{
    return DsiWareSaveArranger().SetupDsiWareSave(_romPath, _romHeader, _dsiwareSaveResult);
}

bool NdsLoader::TryRestoreSaveState()
{
    if (_saveStatePath == nullptr || _saveStatePath[0] == 0)
    {
        return true;
    }

    FIL file;
    UINT bytesRead = 0;
    if (f_open(&file, _saveStatePath, FA_OPEN_EXISTING | FA_READ) != FR_OK)
    {
        return false;
    }

    save_state_file_header_t header {};
    if (f_read(&file, &header, sizeof(header), &bytesRead) != FR_OK || bytesRead < 12)
    {
        f_close(&file);
        return false;
    }

    u32 version = header.version;
    u32 ramOffset = 0;
    u32 ramSize = 0;
    u32 sharedWramOffset = 0;
    u32 sharedWramSize = 0;
    u32 arm7WramOffset = 0;
    u32 arm7WramSize = 0;
    u32 vramOffset = 0;
    u32 vramSize = 0;
    u32 paletteOffset = 0;
    u32 paletteSize = 0;
    u32 oamOffset = 0;
    u32 oamSize = 0;
    u32 arm9ItcmOffset = 0;
    u32 arm9ItcmSize = 0;

    auto tryReadContext = [&](u32 offset, u32 size, void* dest) -> bool {
        if (size != sizeof(save_state_cpu_context_t)) return true;
        return f_lseek(&file, offset) == FR_OK &&
               f_read(&file, dest, sizeof(save_state_cpu_context_t), &bytesRead) == FR_OK &&
               bytesRead == sizeof(save_state_cpu_context_t);
    };

    ((save_state_arm9_itcm_state_t*)SAVE_STATE_ARM9_ITCM_INFO_ADDRESS)->magic = 0;

    if (header.magic == SAVE_STATE_FILE_MAGIC_V7 &&
        header.version == SAVE_STATE_FILE_VERSION_V7)
    {
        save_state_file_header_v7_t headerV7 {};
        if (f_lseek(&file, 0) != FR_OK ||
            f_read(&file, &headerV7, sizeof(headerV7), &bytesRead) != FR_OK ||
            bytesRead != sizeof(headerV7))
        {
            f_close(&file);
            return false;
        }

        if (headerV7.ramSize != 0x400000 ||
            headerV7.gameCode != _romHeader.gameCode ||
            headerV7.headerCrc != _romHeader.headerCrc ||
            headerV7.arm9ItcmSize > SAVE_STATE_ARM9_ITCM_BUFFER_SIZE)
        {
            f_close(&file);
            return false;
        }

        ramOffset = headerV7.ramOffset;
        ramSize = headerV7.ramSize;
        sharedWramOffset = headerV7.sharedWramOffset;
        sharedWramSize = headerV7.sharedWramSize;
        arm7WramOffset = headerV7.arm7WramOffset;
        arm7WramSize = headerV7.arm7WramSize;
        vramOffset = headerV7.vramOffset;
        vramSize = headerV7.vramSize;
        paletteOffset = headerV7.paletteOffset;
        paletteSize = headerV7.paletteSize;
        oamOffset = headerV7.oamOffset;
        oamSize = headerV7.oamSize;
        arm9ItcmOffset = headerV7.arm9ItcmOffset;
        arm9ItcmSize = headerV7.arm9ItcmSize;

        if (!tryReadContext(headerV7.arm9ContextOffset, headerV7.arm9ContextSize,
                            (void*)SAVE_STATE_ARM9_CONTEXT_ADDRESS) ||
            !tryReadContext(headerV7.arm7ContextOffset, headerV7.arm7ContextSize,
                            (void*)SAVE_STATE_ARM7_CONTEXT_ADDRESS))
        {
            f_close(&file);
            return false;
        }
    }
    else if (header.magic == SAVE_STATE_FILE_MAGIC_V6 &&
             header.version == SAVE_STATE_FILE_VERSION_V6)
    {
        save_state_file_header_v6_t headerV6 {};
        if (f_lseek(&file, 0) != FR_OK ||
            f_read(&file, &headerV6, sizeof(headerV6), &bytesRead) != FR_OK ||
            bytesRead != sizeof(headerV6))
        {
            f_close(&file);
            return false;
        }

        if (headerV6.ramSize != 0x400000 ||
            headerV6.gameCode != _romHeader.gameCode ||
            headerV6.headerCrc != _romHeader.headerCrc)
        {
            f_close(&file);
            return false;
        }

        ramOffset = headerV6.ramOffset;
        ramSize = headerV6.ramSize;
        sharedWramOffset = headerV6.sharedWramOffset;
        sharedWramSize = headerV6.sharedWramSize;
        arm7WramOffset = headerV6.arm7WramOffset;
        arm7WramSize = headerV6.arm7WramSize;
        vramOffset = headerV6.vramOffset;
        vramSize = headerV6.vramSize;
        paletteOffset = headerV6.paletteOffset;
        paletteSize = headerV6.paletteSize;
        oamOffset = headerV6.oamOffset;
        oamSize = headerV6.oamSize;

        if (!tryReadContext(headerV6.arm9ContextOffset, headerV6.arm9ContextSize,
                            (void*)SAVE_STATE_ARM9_CONTEXT_ADDRESS) ||
            !tryReadContext(headerV6.arm7ContextOffset, headerV6.arm7ContextSize,
                            (void*)SAVE_STATE_ARM7_CONTEXT_ADDRESS))
        {
            f_close(&file);
            return false;
        }
    }
    else if ((header.magic == SAVE_STATE_FILE_MAGIC_V5 &&
              header.version == SAVE_STATE_FILE_VERSION_V5) ||
             (header.magic == SAVE_STATE_FILE_MAGIC_V4 &&
              header.version == SAVE_STATE_FILE_VERSION_V4))
    {
        save_state_file_header_v4_t headerV4 {};
        if (f_lseek(&file, 0) != FR_OK ||
            f_read(&file, &headerV4, sizeof(headerV4), &bytesRead) != FR_OK ||
            bytesRead != sizeof(headerV4))
        {
            f_close(&file);
            return false;
        }

        if (headerV4.ramSize != 0x400000)
        {
            f_close(&file);
            return false;
        }

        ramOffset = headerV4.ramOffset;
        ramSize = headerV4.ramSize;
        sharedWramOffset = headerV4.sharedWramOffset;
        sharedWramSize = headerV4.sharedWramSize;
        arm7WramOffset = headerV4.arm7WramOffset;
        arm7WramSize = headerV4.arm7WramSize;
        vramOffset = headerV4.vramOffset;
        vramSize = headerV4.vramSize;
        paletteOffset = headerV4.paletteOffset;
        paletteSize = headerV4.paletteSize;
        oamOffset = headerV4.oamOffset;
        oamSize = headerV4.oamSize;

        if (!tryReadContext(headerV4.arm9ContextOffset, headerV4.arm9ContextSize,
                            (void*)SAVE_STATE_ARM9_CONTEXT_ADDRESS) ||
            !tryReadContext(headerV4.arm7ContextOffset, headerV4.arm7ContextSize,
                            (void*)SAVE_STATE_ARM7_CONTEXT_ADDRESS))
        {
            f_close(&file);
            return false;
        }
    }
    else if (header.magic == SAVE_STATE_FILE_MAGIC_V3 &&
        header.version == SAVE_STATE_FILE_VERSION_V3 &&
        header.ramSize == 0x400000)
    {
        ramOffset = header.ramOffset;
        ramSize = header.ramSize;
        paletteOffset = header.paletteOffset;
        paletteSize = header.paletteSize;
        oamOffset = header.oamOffset;
        oamSize = header.oamSize;
        if (!tryReadContext(header.arm9ContextOffset, header.arm9ContextSize,
                            (void*)SAVE_STATE_ARM9_CONTEXT_ADDRESS) ||
            !tryReadContext(header.arm7ContextOffset, header.arm7ContextSize,
                            (void*)SAVE_STATE_ARM7_CONTEXT_ADDRESS))
        {
            f_close(&file);
            return false;
        }
    }
    else if (header.magic == SAVE_STATE_FILE_MAGIC_V2 &&
        header.version == SAVE_STATE_FILE_VERSION_V2 &&
        header.ramSize == 0x400000)
    {
        ramOffset = header.ramOffset;
        ramSize = header.ramSize;
        if (!tryReadContext(header.arm9ContextOffset, header.arm9ContextSize,
                            (void*)SAVE_STATE_ARM9_CONTEXT_ADDRESS) ||
            !tryReadContext(header.arm7ContextOffset, header.arm7ContextSize,
                            (void*)SAVE_STATE_ARM7_CONTEXT_ADDRESS))
        {
            f_close(&file);
            return false;
        }
    }
    else if (header.magic == SAVE_STATE_FILE_MAGIC_V1 && header.version == 1 && header.arm9ContextOffset == 0x400000)
    {
        ramOffset = 12;
        ramSize = header.arm9ContextOffset;
    }
    else
    {
        f_close(&file);
        return false;
    }

    if (f_lseek(&file, ramOffset) != FR_OK)
    {
        f_close(&file);
        return false;
    }

    constexpr u32 kChunkSize = 64 * 1024;
    for (u32 offset = 0; offset < ramSize; offset += kChunkSize)
    {
        u32 chunkSize = ramSize - offset;
        if (chunkSize > kChunkSize)
            chunkSize = kChunkSize;

        if (f_read(&file, (void*)(0x02000000 + offset), chunkSize, &bytesRead) != FR_OK || bytesRead != chunkSize)
        {
            f_close(&file);
            return false;
        }
    }

    if (sharedWramSize > 0)
    {
        if (f_lseek(&file, sharedWramOffset) != FR_OK ||
            f_read(&file, (void*)0x03000000, sharedWramSize, &bytesRead) != FR_OK ||
            bytesRead != sharedWramSize)
        {
            LOG_ERROR("Failed to restore shared WRAM\n");
            f_close(&file);
            return false;
        }
    }

    if (arm7WramSize > 0)
    {
        if (f_lseek(&file, arm7WramOffset) != FR_OK ||
            f_read(&file, (void*)0x03800000, arm7WramSize, &bytesRead) != FR_OK ||
            bytesRead != arm7WramSize)
        {
            LOG_ERROR("Failed to restore ARM7 WRAM\n");
            f_close(&file);
            return false;
        }
    }

    if (vramSize > 0 && !restoreArm9VramState(file, vramOffset, vramSize))
    {
        LOG_ERROR("Failed to restore VRAM\n");
        f_close(&file);
        return false;
    }

    if (paletteSize > 0)
    {
        if (f_lseek(&file, paletteOffset) != FR_OK ||
            f_read(&file, (void*)0x05000000, paletteSize, &bytesRead) != FR_OK ||
            bytesRead != paletteSize)
        {
            LOG_ERROR("Failed to restore palette\n");
            f_close(&file);
            return false;
        }
    }

    if (oamSize > 0)
    {
        if (f_lseek(&file, oamOffset) != FR_OK ||
            f_read(&file, (void*)0x07000000, oamSize, &bytesRead) != FR_OK ||
            bytesRead != oamSize)
        {
            LOG_ERROR("Failed to restore OAM\n");
            f_close(&file);
            return false;
        }
    }

    if (arm9ItcmSize > 0 && !restoreArm9ItcmState(file, arm9ItcmOffset, arm9ItcmSize))
    {
        LOG_ERROR("Failed to restore ARM9 ITCM\n");
        f_close(&file);
        return false;
    }

    if (version < SAVE_STATE_FILE_VERSION_V5)
    {
        auto arm9IoState = (save_state_arm9_io_state_t*)SAVE_STATE_ARM9_IO_STATE_ADDRESS;
        auto arm7IoState = (save_state_arm7_io_state_t*)SAVE_STATE_ARM7_IO_STATE_ADDRESS;
        if (version >= SAVE_STATE_FILE_VERSION_V3)
        {
            if (arm9IoState->magic == SAVE_STATE_IO_MAGIC)
            {
                memset((u8*)arm9IoState + SAVE_STATE_ARM9_IO_STATE_V4_SIZE, 0,
                    sizeof(save_state_arm9_io_state_t) - SAVE_STATE_ARM9_IO_STATE_V4_SIZE);
            }
            if (arm7IoState->magic == SAVE_STATE_IO_MAGIC)
            {
                memset((u8*)arm7IoState + SAVE_STATE_ARM7_IO_STATE_V4_SIZE, 0,
                    sizeof(save_state_arm7_io_state_t) - SAVE_STATE_ARM7_IO_STATE_V4_SIZE);
            }
        }
        else
        {
            arm9IoState->magic = 0;
            arm7IoState->magic = 0;
        }
    }

    f_close(&file);
    LOG_DEBUG("Restored savestate v%d dump from %s\n", version, _saveStatePath);
    return true;
}

bool NdsLoader::TryDecryptSecureArea()
{
    if (_romHeader.arm9RomOffset < 0x4000 || _romHeader.arm9RomOffset >= 0x8000)
    {
        return true;
    }

    if (((u32*)_romHeader.arm9LoadAddress)[0] == 0xE7FFDEFF &&
        ((u32*)_romHeader.arm9LoadAddress)[1] == 0xE7FFDEFF)
    {
        return true;
    }

    u16 secureAreaCrc = swi_getCrc16(0xFFFF,
        (const void*)_romHeader.arm9LoadAddress, 0x8000 - _romHeader.arm9RomOffset);
    if (secureAreaCrc != _romHeader.secureAreaCrc)
    {
        return true;
    }

    auto bios7File = std::make_unique<FIL>();
    auto keyTable = std::make_unique_for_overwrite<Blowfish::KeyTable>();
    UINT bytesRead = 0;
    if (f_open(bios7File.get(), BIOS_NDS7_PATH, FA_OPEN_EXISTING | FA_READ) != FR_OK ||
        f_lseek(bios7File.get(), 0x30) != FR_OK ||
        f_read(bios7File.get(), keyTable.get(), sizeof(Blowfish::KeyTable), &bytesRead) != FR_OK ||
        bytesRead != sizeof(Blowfish::KeyTable))
    {
        return false;
    }

    auto blowfish = std::make_unique<Blowfish>(keyTable.get());
    blowfish->TransformTable(_romHeader.gameCode, 3, 8);
    blowfish->Decrypt(
        (const void*)_romHeader.arm9LoadAddress,
        (void*)_romHeader.arm9LoadAddress,
        0x4800 - _romHeader.arm9RomOffset);
    ((u32*)_romHeader.arm9LoadAddress)[0] = 0xE7FFDEFF;
    ((u32*)_romHeader.arm9LoadAddress)[1] = 0xE7FFDEFF;

    LOG_DEBUG("Decrypted secure area\n");
    return true;
}

void NdsLoader::HandleIQueRegionFreePatching()
{
    if ((_romHeader.flags & 0x80) == 0x80)
    {
        _romHeader.flags &= ~0x80;
        _romHeader.headerCrc = swi_getCrc16(0xFFFF, (void*)&_romHeader, 0x15E);
    }
}

ConsoleRegion NdsLoader::GetRomRegion(u32 gameCode)
{
    u8 gameRegionCode = (gameCode >> 24) & 0xFF;
    if (gameRegionCode != 'A' && gameRegionCode != 'O')
    {
        // Determine region by TID
        if (gameRegionCode == 'J')
        {
            return ConsoleRegion::Japan;
        }
        else if (gameRegionCode == 'E' || gameRegionCode == 'T')
        {
            return ConsoleRegion::America;
        }
        else if (gameRegionCode == 'P' || gameRegionCode == 'V')
        {
            return ConsoleRegion::Europe;
        }
        else if (gameRegionCode == 'U')
        {
            return ConsoleRegion::Australia;
        }
        else if (gameRegionCode == 'C')
        {
            return ConsoleRegion::China;
        }
        else if (gameRegionCode== 'K')
        {
            return ConsoleRegion::Korea;
        }
    }

    return ConsoleRegion::Europe; // Default to EUR
}

UserLanguage NdsLoader::GetLanguageByRomRegion(ConsoleRegion romRegion)
{
    UserLanguage userLang = static_cast<UserLanguage>(TWL_SHARED_MEMORY->ntrSharedMem.firmwareUserData[0x64] & 0x07);

    if (romRegion == ConsoleRegion::Japan)
    {
        return UserLanguage::Japanese;
    }
    else if (romRegion == ConsoleRegion::America &&
            (userLang != UserLanguage::English ||
             userLang != UserLanguage::French ||
             userLang != UserLanguage::Spanish))
    {
        return UserLanguage::English;
    }
    else if (romRegion == ConsoleRegion::Europe && userLang < UserLanguage::English && userLang > UserLanguage::Spanish)
    {
        return UserLanguage::English;
    }
    else if (romRegion == ConsoleRegion::China)
    {
        return UserLanguage::Chinese;
    }
    else if (romRegion == ConsoleRegion::Korea)
    {
        return UserLanguage::Korean;
    }

    return userLang;
}

u32 NdsLoader::GetSupportedLanguagesByRegion(ConsoleRegion region)
{
    switch (region)
    {
        case ConsoleRegion::Japan:
        {
            return 0x01;
        }
        case ConsoleRegion::America:
        {
            return 0x26;
        }
        case ConsoleRegion::Europe:
        {
            return 0x3E;
        }
        case ConsoleRegion::Australia:
        {
            return 0x02;
        }
        case ConsoleRegion::China:
        {
            return 0x40;
        }
        case ConsoleRegion::Korea:
        {
            return 0x80;
        }
    }

    return 0x3E;
}
