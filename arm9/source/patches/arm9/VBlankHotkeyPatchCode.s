.cpu arm946e-s
.section "patch_vblankhotkey", "ax"
.syntax unified
.arm

.global patch_vblankhotkey_handler
.type patch_vblankhotkey_handler, %function
patch_vblankhotkey_handler:
    stmfd sp!, {r0-r3, lr}

    ldr r0, keyinputAddress
    ldrh r1, [r0]
    ldr r0, hotkeyMask
    ands r0, r1, r0
    bne hotkey_not_pressed

    ldr r2, hotkeyPressedState
    ldr r0, [r2]
    cmp r0, #0
    bne hotkey_chain_original

    mov r0, #1
    str r0, [r2]

    ldr r0, contextAddress
    ldr r1, contextMagic
    str r1, [r0], #4
    mrs r1, cpsr
    str r1, [r0], #4
    mrs r1, spsr
    str r1, [r0], #4
    mov r1, sp
    str r1, [r0], #4
    ldr r1, [sp, #16]
    str r1, [r0], #4
    ldr r1, patch_vblankhotkey_originalHandler_address
    str r1, [r0], #4
    ldr r1, [sp, #0]
    str r1, [r0], #4
    ldr r1, [sp, #4]
    str r1, [r0], #4
    ldr r1, [sp, #8]
    str r1, [r0], #4
    ldr r1, [sp, #12]
    str r1, [r0], #4
    str r4, [r0], #4
    str r5, [r0], #4
    str r6, [r0], #4
    str r7, [r0], #4
    str r8, [r0], #4
    str r9, [r0], #4
    str r10, [r0], #4
    str r11, [r0], #4
    str r12, [r0], #4

    // Save a small set of ARM9-visible IO state for better resume fidelity.
    ldr r0, ioStateAddress
    ldr r1, ioStateMagic
    str r1, [r0], #4

    ldr r1, =0x04000000
    ldr r2, [r1]
    str r2, [r0], #4          // REG_DISPCNT
    ldr r3, =0x04001000
    ldr r2, [r3]
    str r2, [r0], #4          // REG_DISPCNT_SUB
    ldr r2, [r1, #0x8]
    str r2, [r0], #4          // BG0/1 CNT main
    ldr r2, [r1, #0xC]
    str r2, [r0], #4          // BG2/3 CNT main
    ldr r2, [r3, #0x8]
    str r2, [r0], #4          // BG0/1 CNT sub
    ldr r2, [r3, #0xC]
    str r2, [r0], #4          // BG2/3 CNT sub
    ldrh r2, [r1, #0x4]
    str r2, [r0], #4          // REG_DISPSTAT (lower 16 bits)
    ldrh r2, [r1, #0x6C]
    str r2, [r0], #4          // REG_MASTER_BRIGHT
    ldrh r2, [r3, #0x6C]
    str r2, [r0], #4          // REG_MASTER_BRIGHT_SUB
    ldr r12, =0x04000304
    ldrh r2, [r12]
    str r2, [r0], #4          // REG_POWERCNT
    ldrh r2, [r1, #0x4C]
    str r2, [r0], #4          // REG_MOSAIC
    ldrh r2, [r3, #0x4C]
    str r2, [r0], #4          // REG_MOSAIC_SUB
    ldr r2, [r1, #0x64]
    str r2, [r0], #4          // REG_DISPCAPCNT

    ldr r1, =0x04000100
    ldr r2, [r1, #0x0]
    str r2, [r0], #4          // TM0
    ldr r2, [r1, #0x4]
    str r2, [r0], #4          // TM1
    ldr r2, [r1, #0x8]
    str r2, [r0], #4          // TM2
    ldr r2, [r1, #0xC]
    str r2, [r0], #4          // TM3

    ldr r1, =0x04000210
    ldr r2, [r1]
    str r2, [r0], #4          // REG_IE
    ldr r1, =0x04000208
    ldr r2, [r1]
    str r2, [r0], #4          // REG_IME

    ldr r1, =0x04000010
    ldr r2, [r1, #0x0]
    str r2, [r0], #4          // BG0HOFS/VOFS
    ldr r2, [r1, #0x4]
    str r2, [r0], #4          // BG1HOFS/VOFS
    ldr r2, [r1, #0x8]
    str r2, [r0], #4          // BG2HOFS/VOFS
    ldr r2, [r1, #0xC]
    str r2, [r0], #4          // BG3HOFS/VOFS

    ldr r3, =0x04001010
    ldr r2, [r3, #0x0]
    str r2, [r0], #4          // BG0HOFS/VOFS SUB
    ldr r2, [r3, #0x4]
    str r2, [r0], #4          // BG1HOFS/VOFS SUB
    ldr r2, [r3, #0x8]
    str r2, [r0], #4          // BG2HOFS/VOFS SUB
    ldr r2, [r3, #0xC]
    str r2, [r0], #4          // BG3HOFS/VOFS SUB

    ldr r1, =0x04000020
    ldr r2, [r1, #0x0]
    str r2, [r0], #4          // BG2PA/PB
    ldr r2, [r1, #0x4]
    str r2, [r0], #4          // BG2PC/PD
    ldr r2, [r1, #0x10]
    str r2, [r0], #4          // BG3PA/PB
    ldr r2, [r1, #0x14]
    str r2, [r0], #4          // BG3PC/PD

    ldr r3, =0x04001020
    ldr r2, [r3, #0x0]
    str r2, [r0], #4          // BG2PA/PB SUB
    ldr r2, [r3, #0x4]
    str r2, [r0], #4          // BG2PC/PD SUB
    ldr r2, [r3, #0x10]
    str r2, [r0], #4          // BG3PA/PB SUB
    ldr r2, [r3, #0x14]
    str r2, [r0], #4          // BG3PC/PD SUB

    ldr r1, =0x04000040
    ldr r2, [r1, #0x0]
    str r2, [r0], #4          // WIN0H/WIN1H
    ldr r2, [r1, #0x4]
    str r2, [r0], #4          // WIN0V/WIN1V
    ldr r2, [r1, #0x8]
    str r2, [r0], #4          // WININ/WINOUT

    ldr r3, =0x04001040
    ldr r2, [r3, #0x0]
    str r2, [r0], #4          // WIN0H/WIN1H SUB
    ldr r2, [r3, #0x4]
    str r2, [r0], #4          // WIN0V/WIN1V SUB
    ldr r2, [r3, #0x8]
    str r2, [r0], #4          // WININ/WINOUT SUB

    ldr r1, =0x04000050
    ldr r2, [r1, #0x0]
    str r2, [r0], #4          // BLDCNT/BLDALPHA
    ldrh r2, [r1, #0x4]
    str r2, [r0], #4          // BLDY

    ldr r3, =0x04001050
    ldr r2, [r3, #0x0]
    str r2, [r0], #4          // BLDCNT/BLDALPHA SUB
    ldrh r2, [r3, #0x4]
    str r2, [r0], #4          // BLDY SUB

    ldr r0, sdk5MainMemoryCmdAddress
    mov r1, #0x54
    orr r1, r1, #0x5300
    strh r1, [r0]
    ldr r0, ntrMainMemoryCmdAddress
    strh r1, [r0]

    ldmfd sp!, {r0-r3, lr}
    ldr pc, patch_vblankhotkey_reboot_address

hotkey_not_pressed:
    ldr r2, hotkeyPressedState
    mov r0, #0
    str r0, [r2]

hotkey_chain_original:
    ldmfd sp!, {r0-r3, lr}
    ldr pc, patch_vblankhotkey_originalHandler_address

.balign 4

keyinputAddress:
    .word 0x04000130

sdk5MainMemoryCmdAddress:
    .word 0x02FFFFFC

ntrMainMemoryCmdAddress:
    .word 0x027FFFFC

contextAddress:
    .word 0x023FF000

contextMagic:
    .word 0x43545831

ioStateAddress:
    .word 0x023FF100

ioStateMagic:
    .word 0x494F5431

hotkeyMask:
    .word 0x30C

hotkeyPressedState:
    .word hotkeyPressedStateValue

hotkeyPressedStateValue:
    .word 0

.global patch_vblankhotkey_originalHandler_address
patch_vblankhotkey_originalHandler_address:
    .word 0

.global patch_vblankhotkey_reboot_address
patch_vblankhotkey_reboot_address:
    .word 0

.pool
.end
