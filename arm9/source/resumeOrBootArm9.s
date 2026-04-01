.text
.arm
.syntax unified

// save_state_cpu_context_t layout (from common/SaveState.h):
// +0   magic
// +4   cpsr  (IRQ mode cpsr at capture)
// +8   spsr  (game's cpsr - what we restore to)
// +12  sp      (IRQ sp at capture)
// +16  lr      (game return PC - the address to resume at)
// +20  pc      (original handler addr - not used)
// +24  r[0]    .. +72  r[12]
// +76  userSp  (banked user/system sp)
// +80  userLr  (banked user/system lr)

#define CTX_BASE          0x023FF000
#define CTX_MAGIC         0x43545831
#define ITCM_BUFFER0_BASE 0x02FF0000
#define ITCM_BUFFER1_BASE 0x02FF8000
#define ITCM_INFO_BASE    0x02FFBFF0
#define ITCM_MAGIC        0x4954434D
#define ITCM_BASE         0x01000000
#define ITCM_CHUNK_SIZE   0x00004000
#define ITCM_MAX_SIZE     0x00008000
#define IO_STATE_BASE     0x023FF100
#define IO_DTCM_CONTROL   268
#define IO_ITCM_CONTROL   272
#define CTX_SPSR          8
#define CTX_IRQ_SP        12
#define CTX_LR            16
#define CTX_R0            24
#define CTX_USER_SP       76

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

    // --- Valid context found: restore staged ARM9 ITCM if present ---
    ldr r4, itcm_info_addr
    ldr r5, [r4]                    // load ITCM state magic
    ldr r6, itcm_magic_val
    cmp r5, r6
    bne skip_itcm_restore

    ldr r5, [r4, #4]                // staged ITCM size
    cmp r5, #0
    beq clear_itcm_info
    ldr r6, itcm_max_size_val
    cmp r5, r6
    bhi clear_itcm_info

    ldr r6, itcm_buffer0_addr
    ldr r7, itcm_base_addr
    ldr r8, itcm_chunk_size_val
    mov r9, r8
    cmp r5, r8
    movls r9, r5
restore_itcm_chunk0_loop:
    cmp r9, #0
    beq restore_itcm_chunk0_done
    ldr r0, [r6], #4
    str r0, [r7], #4
    subs r9, r9, #4
    b restore_itcm_chunk0_loop
restore_itcm_chunk0_done:

    subs r5, r5, r8
    ble restore_itcm_done

    ldr r6, itcm_buffer1_addr
restore_itcm_chunk1_loop:
    ldr r0, [r6], #4
    str r0, [r7], #4
    subs r5, r5, #4
    bne restore_itcm_chunk1_loop

restore_itcm_done:
    mov r0, #0
    mcr p15, 0, r0, c7, c5, 0       // invalidate entire icache after ITCM rewrite

clear_itcm_info:
    mov r0, #0
    str r0, [r4]

skip_itcm_restore:
    // --- Restore ARM9 TCM control registers if captured ---
    ldr r4, io_state_addr
    ldr r2, [r4, #IO_DTCM_CONTROL]
    cmp r2, #0
    beq skip_dtcm_control_restore
    mcr p15, 0, r2, c9, c1, 0
skip_dtcm_control_restore:
    ldr r2, [r4, #IO_ITCM_CONTROL]
    cmp r2, #0
    beq skip_itcm_control_restore
    mcr p15, 0, r2, c9, c1, 1
skip_itcm_control_restore:

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

    // --- Restore IRQ and user/system banked SP/LR ---
    ldr sp, [r1, #CTX_IRQ_SP]
    ldr r2, [r1, #CTX_USER_SP]
    cmp r2, #0
    beq skip_user_bank_restore
    add r4, r1, #CTX_USER_SP
    ldmia r4, {sp, lr}^
skip_user_bank_restore:

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
itcm_buffer0_addr:
    .word ITCM_BUFFER0_BASE
itcm_buffer1_addr:
    .word ITCM_BUFFER1_BASE
itcm_info_addr:
    .word ITCM_INFO_BASE
io_state_addr:
    .word IO_STATE_BASE
itcm_magic_val:
    .word ITCM_MAGIC
itcm_base_addr:
    .word ITCM_BASE
itcm_chunk_size_val:
    .word ITCM_CHUNK_SIZE
itcm_max_size_val:
    .word ITCM_MAX_SIZE
.pool
