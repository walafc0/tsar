/*
 * \file  : reset.S
 * \date  : 01/12/2012
 * \author: Cesar FUGUET & Manuel BOUYER & Alain Greiner
 *
 * This is a generic reset code for a generic multi-clusters / multi-processors
 * TSAR architecture (up to 256 clusters / up to 4 processors per cluster).
 *
 * There is one XICU, one TTY, one DMA, and one memory bank per cluster.
 *
 * This preloader uses a stack segment allocated in cluster 0 for processor 0.
 * The stack allocation is not performed for other processors as they do not
 * need it during the preloader execution. Therefore, this allocation should be
 * done by the loaded Operating System.
 *
 * The replicated XICU is used to awake the sleeping processors:
 *      xicu_paddr_base = ICU_PADDR_BASE + (cluster_xy << 32)
 *
 * It is intended to be used with various operating systems or nano kernels,
 * including NetBSD, Linux, ALMOS, and GIET_VM.
 *
 * - Each processor initializes its Status Register (SR) to disable interrupts.
 * - Each processor initializes its Count Register.
 * - Each processor initialises its private XICU WTI mask register.
 * - Only processor 0 executes the reset_load_elf function to load into memory
 *   the system specific boot-loader stored on disk at RESET_LOADER_LBA
 * - All other processors wait in a low power consumption mode that the
 *   processor 0 wakes them using an IPI (Inter Processor Interruption)
 */

    #include <defs.h>
    #include <mips32_registers.h>

    /* These define should be consistent with values defined in map.xml file  */

    .section .reset,"ax",@progbits

    .extern reset_putc
    .extern reset_getc
    .extern reset_ioc_read
    .extern reset_elf_loader
    .extern memcpy
    .extern reset_puts
    .extern reset_putx
    .extern reset_putd
    .extern reset_ioc_init
    .extern versionstr
    .extern dtb_start
    .extern dtb_addr

    .globl  reset                    /* Makes reset an external symbol */
    .ent    reset

    .align  2
    .set noreorder

reset:
    b       _reset                   /* 0xbfc0000 */
    nop                              /* 0xbfc0004 */

    /*  Addresses of the functions provided by this reset code */

preloader_vector:
    .word   RESET_VERSION            /* 0xbfc0008 */
    .word   dtb_start                /* 0xbfc000c */
    .word   reset_putc               /* 0xbfc0010 */
    .word   reset_getc               /* 0xbfc0014 */
    .word   reset_ioc_read           /* 0xbfc0018 */
    .word   reset_elf_loader         /* 0xbfc001C */
    .word   memcpy                   /* 0xbfc0020 */
    .word   reset_puts               /* 0xbfc0024 */
    .word   reset_putx               /* 0xbfc0028 */
    .word   reset_putd               /* 0xbfc002C */

_reset:

    /* All processors Disable interruptions, keep STATUSbev enabled */

    li      k0,     (1 << 22)
    mtc0    k0,     CP0_STATUS

    /*
     * All processors compute gpid, lpid, cluster_xy
     * gpid = ebase[11:0] = X_WIDTH : Y_WIDTH : P_WIDTH
     *                        x         y       lpid
     * X, Y and LPID fields are left-aligned
     */

    mfc0    k0,     CP0_EBASE
    andi    t0,     k0,     0xFFF            /* t0 <= gpid (<= 4096 procs)  */
    andi    t1,     t0,     ((1<<P_WIDTH)-1) /* t1 <= lpid                  */
    srl     t2,     t0,     P_WIDTH          /* t2 <= cluster_xy            */

    /* All processors initialise the count register in CP0 */

    mtc0    zero,   CP0_COUNT

    /*
     * All processors enable the WTI for XICU
     * Each processor may have IRQ_PER_PROC irq outputs from the XICU
     * In each cluster, the XICU base address depends on the cluster_xy
     */

    la      t3,     SEG_XCU_BASE      /* t3 <= ICU base address             */
    move    t4,     t1                /* t4 <= local_id                     */
    li      t5,     IRQ_PER_PROCESSOR /* t5 <= IRQ_PER_PROCESSOR            */
    multu   t4,     t5
    mflo    t6                       /* t6 <= IRQ_PER_PROC * local_id       */
    sll     t4,     t6,     2        /* t4 <= OUT_INDEX = t6 * 4            */

    li      t5,     (0xC << 7)       /* t5 <= FUNC      = XICU_MSK_WTI      */
    or      t4,     t4,     t5       /* t4 <= FUNC | INDEX | 00             */
    or      t5,     t3,     t4       /* t5 <= &XICU[MSK_WTI][OUT_INDEX]     */

    /* All processors set WTI mask using the physical address extension */

    li      t4,     1
    sllv    t4,     t4,     t1       /* Set XICU[MSK_WTI][INDEX][local_id]  */

    mtc2    t2,     CP2_PADDR_EXT    /* set PADDR extension                 */
    sw      t4,     0(t5)            /* XICU[MSK_WTI][INDEX] <= t4          */
    mtc2    zero,   CP2_PADDR_EXT    /* reset PADDR extension               */

    /*
     * Only processor 0 in cluster 0 loads and executes the boot-loader
     * We have:
     * t0: global pid
     * t1: local pid
     * t2: cluster_xy
     * t3: xicu physical base address in cluster 0
     */

    bne     zero,   t0,     _reset_wait
    nop

    /* Processor 0 initializes stack pointer */

    la      k0,     _stack
    li      k1,     RESET_STACK_SIZE /* k1 <= P0 stack size                 */
    addu    sp,     k0,     k1       /* P0 stack from base to (base + size) */

    /* Processor 0 displays version for this reset code */

    la      a0,     versionstr
    jal     reset_puts
    nop

    /* Processor 0 initializes the block device */

    jal     reset_ioc_init
    nop

    /*
     * Processor 0 jumps to the reset_elf_loader routine passing as argument
     * the block number in which is loaded the .elf file
     */

    li      a0,     RESET_LOADER_LBA
    jal     reset_elf_loader
    nop

    /*
     * Processor O jumps to the entry address defined in the .elf file, and
     * returned by reset_elf_loader function.
     * First argument is pointer to the preloader function vectors other
     * function arguments are 0
     */

    la      a0,     preloader_vector
    lw      a1,     dtb_addr
    move    a2,     zero
    move    a3,     zero
    jr      v0
    nop

    /*
     * All processor (but processor 0) wait in low power mode until processor 0
     * wakes them using an IPI.
     * We have:
     * t0: global id
     * t1: local id
     * t2: cluster id
     * t3: xicu physical base address in cluster 0
     */

_reset_wait:

    sll     t4,     t1,     2        /* t4 <= local_id * 4                  */
    addu    t5,     t4,     t3       /* t5 <= &XICU[WTI_REG][local_id]      */

    wait

    /*
     * All other processors, when exiting wait mode, read from XICU the address
     * to jump.
     * This address is the boot-loader entry address that has been written in
     * the mailbox by the IPI sent by processor 0
     */

    mtc2    t2,     CP2_PADDR_EXT    /* set PADDR extension                 */
    lw      k0,     0(t5)            /* k0 <= XICU[WTI_REG][local_id]       */
    mtc2    zero,   CP2_PADDR_EXT    /* reset PADDR extension               */

    jr      k0
    nop

/* Exception entry point */

.org 0x0380
_excep:
    mfc0    a0,     CP0_STATUS       /* first arg is status                 */
    mfc0    a1,     CP0_CAUSE        /* second arg is cause                 */
    mfc0    a2,     CP0_EPC          /* third argc is epc                   */
    mfc2    a3,     CP2_DBVAR        /* fourth argc is dbvar                */
    nop
    j       handle_except
    nop

    .end reset

    .set reorder

    .section .data

_stack:

    .space RESET_STACK_SIZE

/*
 * vim: tabstop=4 : shiftwidth=4 : expandtab
 */
