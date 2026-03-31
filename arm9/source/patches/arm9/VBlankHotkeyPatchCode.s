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
