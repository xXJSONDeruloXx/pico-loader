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

    // arm9 context save is already written above,
    // palette/OAM are captured by the ARM7 dump routine via the V3 header sections

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
