.cpu arm7tdmi
.section "patch_hotkeystatecapture", "ax"
.syntax unified
.thumb

.global hotkeystatecapture_entry
.type hotkeystatecapture_entry, %function
hotkeystatecapture_entry:
    push {r0-r7, lr}

    ldr r0, contextAddress
    ldr r1, contextMagic
    stmia r0!, {r1}

    movs r1, #0
    stmia r0!, {r1} // cpsr placeholder
    stmia r0!, {r1} // spsr placeholder

    mov r1, sp
    adds r1, #36
    stmia r0!, {r1} // original sp

    ldr r1, [sp, #32]
    stmia r0!, {r1} // original lr

    mov r1, pc
    stmia r0!, {r1} // resume pc placeholder

    ldr r1, [sp, #0]
    stmia r0!, {r1}
    ldr r1, [sp, #4]
    stmia r0!, {r1}
    ldr r1, [sp, #8]
    stmia r0!, {r1}
    ldr r1, [sp, #12]
    stmia r0!, {r1}
    ldr r1, [sp, #16]
    stmia r0!, {r1}
    ldr r1, [sp, #20]
    stmia r0!, {r1}
    ldr r1, [sp, #24]
    stmia r0!, {r1}
    ldr r1, [sp, #28]
    stmia r0!, {r1}

    mov r1, r8
    stmia r0!, {r1}
    mov r1, r9
    stmia r0!, {r1}
    mov r1, r10
    stmia r0!, {r1}
    mov r1, r11
    stmia r0!, {r1}
    mov r1, r12
    stmia r0!, {r1}

    movs r1, #0
    stmia r0!, {r1} // user sp placeholder
    stmia r0!, {r1} // user lr placeholder

    // Capture a small set of ARM7 IO state.
    ldr r0, ioStateAddress
    ldr r1, ioStateMagic
    stmia r0!, {r1}

    ldr r2, =0x04000100
    ldr r1, [r2, #0x0]
    stmia r0!, {r1}
    ldr r1, [r2, #0x4]
    stmia r0!, {r1}
    ldr r1, [r2, #0x8]
    stmia r0!, {r1}
    ldr r1, [r2, #0xC]
    stmia r0!, {r1}

    ldr r2, =0x04000210
    ldr r1, [r2]
    stmia r0!, {r1}
    ldr r2, =0x04000208
    ldr r1, [r2]
    stmia r0!, {r1}

    ldr r2, =0x04000500
    ldrh r1, [r2]
    stmia r0!, {r1}
    ldr r2, =0x04000508
    ldrb r1, [r2]
    ldrb r3, [r2, #1]
    lsls r3, r3, #8
    orrs r1, r3
    stmia r0!, {r1}

    ldr r2, =0x040000B0
    movs r3, #4
1:
    ldr r1, [r2, #0x0]
    stmia r0!, {r1}
    ldr r1, [r2, #0x4]
    stmia r0!, {r1}
    ldr r1, [r2, #0x8]
    stmia r0!, {r1}
    adds r2, #0xC
    subs r3, #1
    bne 1b

    ldr r2, =0x04000400
    movs r3, #16
2:
    ldr r1, [r2, #0x0]
    stmia r0!, {r1}
    ldr r1, [r2, #0x4]
    stmia r0!, {r1}
    ldr r1, [r2, #0x8]
    stmia r0!, {r1}
    ldr r1, [r2, #0xC]
    stmia r0!, {r1}
    adds r2, #0x10
    subs r3, #1
    bne 2b

    ldr r2, =0x04000510
    ldr r1, [r2, #0x0]
    stmia r0!, {r1}
    ldr r1, [r2, #0x4]
    stmia r0!, {r1}
    ldr r1, [r2, #0x8]
    stmia r0!, {r1}
    ldr r1, [r2, #0xC]
    stmia r0!, {r1}

    ldr r2, =0x04000134
    ldrh r1, [r2]
    stmia r0!, {r1}

    pop {r0-r7, pc}

.balign 4
contextAddress:
    .word 0x023FF080
contextMagic:
    .word 0x43545831
ioStateAddress:
    .word 0x023FF1C0
ioStateMagic:
    .word 0x494F5431

.pool
.end
