.section ".itcm", "ax"
.arm
.syntax unified

// save_state_cpu_context_t layout (from common/SaveState.h):
// +0   magic
// +4   cpsr  (IRQ mode cpsr at capture)
// +8   spsr  (game's cpsr - what we restore to)
// +12  sp    (IRQ sp at capture - not used for resume)
// +16  lr    (game return PC - the address to resume at)
// +20  pc    (original handler addr - not used)
// +24  r[0]  .. +72  r[12]

#define CTX_BASE        0x023FF000
#define CTX_MAGIC       0x43545831
#define CTX_SPSR        8
#define CTX_LR          16
#define CTX_R0          24

// r0: arm9EntryPoint (fallback when no valid context)
.global resumeOrBootArm9
.type resumeOrBootArm9, %function
resumeOrBootArm9:
    // --- Check for valid savestate context ---
    ldr r1, ctx_base_addr
    ldr r2, [r1]                    // load magic
    ldr r3, ctx_magic_val
    cmp r2, r3
    bne do_normal_boot              // no valid context, do normal boot

    // --- Valid context found: consume it (clear magic) ---
    mov r2, #0
    str r2, [r1]                    // clear magic so it won't be used again

    // --- Enter IRQ mode with IRQ+FIQ disabled ---
    mrs r2, cpsr
    bic r2, r2, #0x1F
    orr r2, r2, #0x000000D2         // IRQ mode | I | F
    msr cpsr_c, r2

    // --- Restore SPSR = game's cpsr ---
    ldr r2, [r1, #CTX_SPSR]
    msr spsr_cxsf, r2

    // --- Restore LR_irq = game return PC ---
    ldr lr, [r1, #CTX_LR]

    // --- Restore r4-r12 (r1 still = context ptr) ---
    ldr r4,  [r1, #(CTX_R0 + 16)]  // r[4]
    ldr r5,  [r1, #(CTX_R0 + 20)]  // r[5]
    ldr r6,  [r1, #(CTX_R0 + 24)]  // r[6]
    ldr r7,  [r1, #(CTX_R0 + 28)]  // r[7]
    ldr r8,  [r1, #(CTX_R0 + 32)]  // r[8]
    ldr r9,  [r1, #(CTX_R0 + 36)]  // r[9]
    ldr r10, [r1, #(CTX_R0 + 40)]  // r[10]
    ldr r11, [r1, #(CTX_R0 + 44)]  // r[11]
    ldr r12, [r1, #(CTX_R0 + 48)]  // r[12]

    // --- Restore r0, r2, r3 then r1 last (r1 is context ptr until now) ---
    ldr r0, [r1, #CTX_R0]          // r[0]
    ldr r2, [r1, #(CTX_R0 + 8)]    // r[2]
    ldr r3, [r1, #(CTX_R0 + 12)]   // r[3]
    ldr r1, [r1, #(CTX_R0 + 4)]    // r[1] - last

    // --- Return from exception: SPSR -> CPSR, PC = LR ---
    // This is the canonical ARM IRQ return sequence.
    // Jumps back to exactly where the game was interrupted.
    movs pc, lr

do_normal_boot:
    // --- Normal boot: clear all regs and jump to entry point ---
    push {r0}
    mov r0, #0
    mov r1, #0
    mov r2, #0
    mov r3, #0
    mov r4, #0
    mov r5, #0
    mov r6, #0
    mov r7, #0
    mov r8, #0
    mov r9, #0
    mov r10, #0
    mov r11, #0
    mov r12, #0
    mov lr, #0
    pop {pc}

.balign 4
ctx_base_addr:
    .word CTX_BASE
ctx_magic_val:
    .word CTX_MAGIC
.pool
