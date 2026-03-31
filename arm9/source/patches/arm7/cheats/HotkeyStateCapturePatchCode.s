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

    pop {r0-r7, pc}

.balign 4
contextAddress:
    .word 0x023FF080
contextMagic:
    .word 0x43545831

.pool
.end
