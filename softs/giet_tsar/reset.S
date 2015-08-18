/********************************************************************************
*   File : reset.S
*   Author : Alain Greiner
*   Date : 15/01/2014
*********************************************************************************
* This is a boot code for a generic multi-clusters / multi-processors
* TSAR architecture (up to 256 clusters / up to 4  processors per cluster).
* The physical address is 40 bits, and the 8 MSB bits A[39:32] define the
* cluster index.
*
* As we don't want to use the virtual memory, the physical address is
* equal to  the virtual address (identity mapping) and all processors stacks
* and code segments are allocated in the physical memory bank in cluster 0.
*
* Both the reset base address and the kernel base address must be redefined
* to use a physical memory bank smaller than 2 Gbytes.
*
* There is one XCU iand one MMC per cluster.
*
* There is one IOPIC component in cluster_io.
*
* There is two sets of peripherals:
*
* 1) A block device and a single channel TTY controller are available
*    in cluster(0,0).
*
* 2) Other peripherals (including another Blockdevice, a multi-channels TTY
*    contrôler, a Frame buffer) are located in cluster_io.
*    For those externals peripherals, hardware interrupts (HWI) are translated
*    to software interrupts (WTI) by and IOPIC component, that is programmed
*    to route all SWI to to processor 0 in cluster (0,0).
*
* The boot sequence is the following:
*   - Each processor initializes the stack pointer ($29) depending on proc_id.
*   - Each processor initializes the CP0 EBASE register
*   - Only processor 0 initializes the Interrupt vector.
*	- Only processor 0 initializes the IOPIC component.
*   - Each processor initializes its private XCU mask.
*   - Each processor initializes the Status Register (SR)
*   - Each processor jumps to the same main address in kernel mode...
********************************************************************************/

#include "hard_config.h"
#include "mips32_registers.h"

    .section .reset,"ax",@progbits

    .extern seg_stack_base
    .extern seg_xcu_base
	.extern	seg_pic_base
    .extern seg_kcode_base
    .extern _interrupt_vector
    .extern _ioc_isr
    .extern _mmc_isr
    .extern _tty_isr
    .extern main

    .globl  reset
    .ent    reset
    .align  2

reset:
        .set noreorder

/* each proc computes  proc_id, lpid, cluster_xy */
    mfc0    $26,    CP0_PROCID
    andi    $26,    $26,    0x3FF       /* at most 1024 processors */
    move    $10,    $26                 /* $10 <= proc_id  */
    li      $27,    NB_PROCS_MAX
    divu    $26,    $27
    mfhi    $11                         /* $11 <= lpid */
    mflo    $12                         /* $12 <= cluster_xy */

/* each proc initializes stack pointer (64K per processor) */
    srl     $8,     $12,    Y_WIDTH     /* $8  <= x */
    li      $9,     Y_SIZE
    mul     $8,     $8,     $9          /* $8  <= x * Y_SIZE */
    andi    $13,    $12,    (1<<Y_WIDTH)-1
    addu    $8,     $8,     $13         /* $8  <= x * Y_SIZE + y */
    mul     $8,     $8,     $27         /* $8  <= (x*Y_SIZE+y)*NB_PROCS_MAX */
    addu    $10,    $11,    $8          /* $10 <= (x*Y_SIZE+y)*NB_PROCS_MAX + lpid */

    la      $27,    seg_stack_base
    addi    $26,    $10,    1           /* $26 <= (proc_id + 1)           */
    sll     $26,    $26,    14          /* $26 <= (proc_id + 1) * 16K     */
    addu    $29,    $27,    $26         /* $29 <= seg_stack_base(proc_id) */

/* each proc initializes CP0 EBASE register */
    la      $26,    seg_kcode_base
    mtc0    $26,    CP0_EBASE           /* CP0_EBASE <= seg_kcode_base */

/* only proc (0,0,0) initializes interrupt vector for IOC, TTY, MMC     */
    bne     $10,    $0,    reset_xcu
    nop

    la      $26,    _interrupt_vector   /* interrupt vector address                */
    la      $27,    _mmc_isr
    sw      $27,    32($26)             /* interrupt_vector[8] <= _mmc_isr         */
    la      $27,    _ioc_isr
    sw      $27,    36($26)             /* interrupt_vector[9] <= _ioc_isr         */
    la      $27,    _tty_isr
    sw      $27,    40($26)             /* interrupt_vector[10] <= _tty_isr        */

/* only proc (0,0,0) initializes IOPIC : IOPIC_ADDRESS[i] <= &XICU[0].WTI_REG[i]   */

#if USE_IOPIC

    li      $20,    X_SIZE
    addi    $20,    $20,    -1
    sll     $20,    $20,    4
    li      $21,    Y_SIZE
    add     $22,    $20,    $21         /* $22 <= cluster(X_SIZE-1, Y_SIZE)        */

    mtc2    $22,    CP2_PADDR_EXT       /* CP2_PADDR_EXT <= cluster_io             */

    li      $24,    16                  /* $24  iteration (de)counter              */
    la      $27,    seg_xcu_base        /* $27 <= &(XICU[0].WTI_REG[0])            */
    la      $26,    seg_pic_base        /* $26 <= &IOPIC_ADDRESS[0]                */

reset_loop:
    sw      $27,    0($26)              /* IOPIC_ADDRESS[i] <= &XICU[0].WTI_REG[i] */
    addi    $24,    $24,    -1          /* decrement iteration index               */
    addi    $27,    $27,     4          /* $27 <= &(XICU[0].WTI_REG[i++]           */
    addi    $26,    $26,     16         /* $26 <= &IOPIC_ADDRESS[i++]              */
    bne     $24,    $0, reset_loop
    nop

    mtc2    $0,     CP2_PADDR_EXT       /* CP2_PADDR_EXT <= zero                   */

#endif

reset_xcu:

/* only proc (x,y,0) receive IRQs and initialise HWI and WTI XICU masks */
    bne     $11,    $0,     reset_end
    nop
    la      $26,    seg_xcu_base
    li      $27,    0b010010000000      /* offset for MSK_HWI_ENABLE[lpid == 0]    */
    addu    $24,    $26,    $27         /* $24 <= &HWI_MASK                        */
    li      $25,    0x0700		        /* TTY:HWI[10]  IOC:HWI[9]  MEMC:HWI[8]    */
    sw      $25,    0($24)              /* set HWI mask                            */

    li      $27,    0b011010000000      /* offset for MSK_WTI_ENABLE[lpid == 0]    */
    addu    $24,    $26,    $27         /* $24 <= $WTI_MASK                        */
    li      $25,    0xFFFFFFFF          /* all WTI enabled                         */
    sw      $25,    0($24)              /* set WTI mask                            */

reset_end:

/* initializes SR register */
    li      $26,    0x0000FF01
    mtc0    $26,    $12                 /* SR <= kernel mode / IRQ enable */

/* jumps to main in kernel mode */
    la      $26,    main
    jr      $26
    nop

    .end    reset

    .set reorder
