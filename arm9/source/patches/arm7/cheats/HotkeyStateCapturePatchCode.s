.cpu arm7tdmi
.section "patch_hotkeystatecapture", "ax"
.syntax unified
.thumb

.global hotkeystatecapture_entry
.type hotkeystatecapture_entry, %function
hotkeystatecapture_entry:
    push {r4-r7, lr}

    ldr r0, contextAddress
    ldr r1, contextMagic
    stmia r0!, {r1}

    mov r1, pc
    stmia r0!, {r1}

    mov r1, sp
    stmia r0!, {r1}

    mov r1, lr
    stmia r0!, {r1}

    stmia r0!, {r2-r7}

    pop {r4-r7, pc}

.balign 4
contextAddress:
    .word 0x023FF080
contextMagic:
    .word 0x43545831

.pool
.end
