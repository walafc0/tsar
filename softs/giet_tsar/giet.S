/********************************************************************************
*	File : giet.S
*	Author : Alain Greiner
*	Date : 15/01/2014
*********************************************************************************
* This is a very simple Interrupts/Exception/Traps handler for a generic
* multi-clusters / multi-processors TSAR architecture (up to 256 clusters,
* up to 4  processors per cluster).
* The physical address is 40 bits, and the 8 MSB bits A[39:32] define the
* cluster index.
********************************************************************************/

#include "hard_config.h"

    .section .giet,"ax",@progbits
    .align 2
    .global _interrupt_vector

    .extern seg_xcu_base
    .extern seg_tty_base

    .extern _procid
    .extern _proctime
    .extern _tty_write
    .extern _tty_read
    .extern _tty_read_irq
    .extern _locks_read
    .extern _locks_write
    .extern _exit
    .extern _fb_sync_write
    .extern _fb_sync_read
    .extern _ioc_write
    .extern _ioc_read
    .extern _ioc_completed
    .extern _itoa_hex
    .extern _barrier_init
    .extern _barrier_wait

    .ent _giet

/****************************************************************
*    Cause Table (indexed by the Cause register)
****************************************************************/
tab_causes:
    .word _int_handler  /* 0000 : external interrupt */
    .word _cause_ukn    /* 0001 : undefined exception */
    .word _cause_ukn    /* 0010 : undefined exception */
    .word _cause_ukn    /* 0011 : undefined exception */
    .word _cause_adel   /* 0100 : illegal address read exception */
    .word _cause_ades   /* 0101 : illegal address write exception */
    .word _cause_ibe    /* 0110 : instruction bus error exception */
    .word _cause_dbe    /* 0111 : data bus error exception */
    .word _sys_handler  /* 1000 : system call */
    .word _cause_bp     /* 1001 : breakpoint exception */
    .word _cause_ri     /* 1010 : illegal codop exception */
    .word _cause_cpu    /* 1011 : illegal coprocessor access */
    .word _cause_ovf    /* 1100 : arithmetic overflow exception */
    .word _cause_ukn    /* 1101 : undefined exception */
    .word _cause_ukn    /* 1110 : undefined exception */
    .word _cause_ukn    /* 1111 : undefined exception */

    .space 320

/****************************************************************
*    Entry point (base + 0x180)
****************************************************************/
_giet:
    mfc0    $27,    $13             /* Cause Register analysis */
    la      $26,    seg_kcode_base  /* $26 <= tab_causes */
    andi    $27,    $27,    0x3c
    addu    $26,    $26,    $27     /* $26 <= &tab_causes[XCODE] */
    lw      $26,    ($26)
    jr      $26                     /* Jump to tab_causes[XCODE] */

    .end _giet

/****************************************************************
*   System Call Handler
*
* As the GIET_TSAR does not support system calls,
* an error message is displayed on TTY0, the program is killed.
****************************************************************/
    .ent _sys_handler

_sys_handler:
    la      $4,     _msg_uknsyscall /* $4 <= message address */
    li      $5,    36               /* $5 <= message length */
    li      $6,    0                /* $6 <= TTY0 */
    jal     _tty_write              /* print unknown message */
    nop

    la      $4,     _msg_epc        /* $4 <= message address */
    li      $5,    8                /* $5 <= message length */
    li      $6,    0                /* $6 <= TTY0 */
    jal     _tty_write              /* print EPC message */
    nop

    mfc0    $4,     $14             /* $4 <= EPC */
    la      $5,     _itoa_buffer    /* $5 <= buffer address */
    addiu   $5,     $5,     2       /* skip the 0x prefix */
    jal     _itoa_hex               /* fill the buffer */
    nop

    la      $4,     _itoa_buffer    /* $4 <= buffer address */
    li      $5,     10              /* $5 <= buffer length */
    li      $6,    0                /* $6 <= TTY0 */
    jal     _tty_write              /* print EPC value */
    nop

    j       _exit                   /* end of program */

_itoa_buffer: .ascii "0x00000000"

    .align 2

    .end _sys_handler

/****************************************************************
*   Interrupt Handler
* This simple interrupt handler cannot be interrupted.
* It uses the SoCLib VCI_XICU component.
* It makes the assumption that there is only one interrupt per
* entry in the interrupt vector (it can be PTI, HWI, or WTI).
* In case of a multi-clusters architecture, it exist one XCU
* per cluster, and the base address of the ICU segment depends
* on both the cluster_xy and the proc_id:
* - base_address = seg_xcu_base + (32*local_id) + (4G*cluster_xy)
* - cluster_xy = proc_id / NB_PROCS_MAX
* - local_id   = proc_id % NB_PROCS_MAX
* If no active interrupt is found in the PRIO register,
* nothing is done.
* The interrupt vector (32 ISR addresses array stored at
* _interrupt_vector address) is initialised with the default
* ISR address. The actual ISR addresses are supposed to be written
* in the interrupt vector array by the boot code.
* All non persistant registers, such as $1 to $15, and $24 to $25,
* as well as register $31 and EPC, are saved in the interrupted
* program stack, before calling the Interrupt Service Routine.
* These registers can be used by the ISR code.
***************************************************************/
    .ent _int_handler

_int_handler:
    addiu   $29,    $29,    -23*4   /* stack space reservation */
    .set noat
    sw      $1,     4*4($29)        /* save $1 */
    .set at
    sw      $2,     4*5($29)        /* save $2 */
    sw      $3,     4*6($29)        /* save $3 */
    sw      $4,     4*7($29)        /* save $4 */
    sw      $5,     4*8($29)        /* save $5 */
    sw      $6,     4*9($29)        /* save $6 */
    sw      $7,     4*10($29)       /* save $7 */
    sw      $8,     4*11($29)       /* save $8 */
    sw      $9,     4*12($29)       /* save $9 */
    sw      $10,    4*13($29)       /* save $10 */
    sw      $11,    4*14($29)       /* save $11 */
    sw      $12,    4*15($29)       /* save $12 */
    sw      $13,    4*16($29)       /* save $13 */
    sw      $14,    4*17($29)       /* save $14 */
    sw      $15,    4*18($29)       /* save $15 */
    sw      $24,    4*19($29)       /* save $24 */
    sw      $25,    4*20($29)       /* save $25 */
    sw      $31,    4*21($29)       /* save $31 */
    mfc0    $27,    $14
    sw      $27,    4*22($29)       /* save EPC */

    /* XICU PRIO register address computation         */
    /* It depends on both the cluster_xy & local_id,  */
    /* and we must use the physical address extension */
    mfc0    $10,    $15,    1        /* $10 <= proc_id                                 */
    andi    $10,    $10,    0x3FF    /* at most 1024 processors                        */
    li      $11,    NB_PROCS_MAX
    divu    $10,    $11
    mflo    $12                      /* $12 <= cluster_xy                              */
    mfhi    $13                      /* $13 <= local_id                                */
    la      $14,    seg_xcu_base     /* $14 <= seg_xcu_base                            */

    li      $7,     0b011110000000   /* $7 <= PRIO offset                              */
    sll     $8,     $13,    2        /* $8 <= local_id*4                               */
    addu    $9,     $7,     $8       /* $9 <= PRIO offset + local_id*4                 */
    addu    $26,    $9,     $14      /* $26 <= seg_icu_base + PRIO offset + local_id*4 */

    /* XCU[cluster_xy] access to get PRIO register value                               */
    mtc2    $12,    $24              /* set PADDR extension                            */
    lw      $15,    0($26)           /* $15 <= PRIO register value                     */
    mtc2    $0,     $24              /* reset PADDR extension                          */

    /* test PTI, then HWI, then WTI                                                    */
    andi    $27,    $15,    0x1      /* test bit T in PRIO register                    */
    bne     $27,    $0,     _int_PTI /* branch to PTI handler                          */
    andi    $27,    $15,    0x2      /* test bit W in PRIO register                    */
    bne     $27,    $0,     _int_HWI /* branch to HWI handler                          */
    andi    $27,    $15,    0x4      /* test bit W in PRIO register                    */
    bne     $27,    $0,     _int_WTI /* branch to WTI handler                          */

    /* exit interrupt handler: restore registers */
_int_restore:
    .set noat
    lw      $1,     4*4($29)
    .set at
    lw      $2,     4*5($29)
    lw      $3,     4*6($29)
    lw      $4,     4*7($29)
    lw      $5,     4*8($29)
    lw      $6,     4*9($29)
    lw      $7,     4*10($29)
    lw      $8,     4*11($29)
    lw      $9,     4*12($29)
    lw      $10,    4*13($29)
    lw      $11,    4*14($29)
    lw      $12,    4*15($29)
    lw      $13,    4*16($29)
    lw      $14,    4*17($29)
    lw      $15,    4*18($29)
    lw      $24,    4*19($29)
    lw      $25,    4*20($29)
    lw      $31,    4*21($29)
    lw      $27,    4*22($29)        /* get EPC */
    addiu   $29,    $29,    23*4     /* restore SP */
    mtc0    $27,    $14              /* restore EPC */
    eret                             /* exit GIET */

    /* The PTI handler get PTI index, */
    /* acknowledge the PTI register   */
    /* and call the corresponding ISR */
_int_PTI:
    srl     $26,    $15,    6        /* $26 <= PRIO >> 6             */
    andi    $26,    $26,    0x7C     /* $26 <= PTI_INDEX * 4         */
    addi    $27,    $14,    0x180    /* $27 <= &PTI_ACK[0]           */
    add     $27,    $27,    $26      /* $27 <= &PTI_ACK[PTI_INDEX]   */
    lw      $0,     ($27)            /* acknowledge XICU PTI         */
    la      $27,    _interrupt_vector
    addu    $26,    $26,    $27
    lw      $26,    ($26)            /* read ISR address             */
    jalr    $26                      /* call ISR                     */
    nop
    j       _int_restore             /* return from INT handler      */
    nop

    /* The HWI handler get HWI index  */
    /* and call the corresponding ISR */
_int_HWI:
    srl     $26,    $15,    14       /* $26 <= PRIO >> 14            */
    andi    $26,    $26,    0x7C     /* $26 <= HWI_INDEX * 4         */
    la      $27,    _interrupt_vector
    addu    $26,    $26,    $27      /* $26 <= &ISR[HWI_INDEX        */
    lw      $26,    ($26)            /* read ISR address             */
    jalr    $26                      /* call ISR                     */
    nop
    j       _int_restore             /* return from INT handler      */
    nop

    /* The WTI handler get WTI index, */
    /* acknowledge the WTI register   */
    /* and call the corresponding ISR */
_int_WTI:
    srl     $26,    $15,    22       /* $26 <= PRIO >> 22            */
    andi    $26,    $26,    0x7C     /* $26 <= WTI_INDEX * 4         */
    add     $27,    $14,    $26      /* $27 <= &WTI_REG[WTI_INDEX]   */
    lw      $0,     ($27)            /* acknowledge XICU WTI         */
    la      $27,    _interrupt_vector
    addu    $26,    $26,    $27      /* $26 <= &ISR[WTI_INDEX]       */
    lw      $26,    ($26)            /* read ISR address             */
    jalr    $26                      /* call ISR                     */
    nop
    j       _int_restore             /* return from INT handler */
    nop

/* The default ISR is called when no specific ISR has been installed */
/* in the interrupt vector. It simply displays a message on TTY0     */

isr_default:
    addiu   $29,    $29,    -20     /* get space in stack */
    sw      $31,    16($29)         /* to save the return address */
    la      $4,     _msg_default    /* $4 <= string address */
    addi    $5,     $0,     36      /* $5 <= string length */
    li      $6,     0               /* $6 <= TTY0 */
    jal     _tty_write
    lw      $31,    16($29)         /* restore return address */
    addiu   $29,    $29,    20      /* free space */
    jr      $31                     /* returns to interrupt handler */

/****************************************************************
*  Interrupt Vector Table (indexed by interrupt index)
*  32 words corresponding to 32 ISR addresses
****************************************************************/
_interrupt_vector:
    .word isr_default   /* ISR 0 */
    .word isr_default   /* ISR 1 */
    .word isr_default   /* ISR 2 */
    .word isr_default   /* ISR 3 */
    .word isr_default   /* ISR 4 */
    .word isr_default   /* ISR 5 */
    .word isr_default   /* ISR 6 */
    .word isr_default   /* ISR 7 */
    .word isr_default   /* ISR 8 */
    .word isr_default   /* ISR 9 */
    .word isr_default   /* ISR 10 */
    .word isr_default   /* ISR 11 */
    .word isr_default   /* ISR 12 */
    .word isr_default   /* ISR 13 */
    .word isr_default   /* ISR 14 */
    .word isr_default   /* ISR 15 */
    .word isr_default   /* ISR 16 */
    .word isr_default   /* ISR 17 */
    .word isr_default   /* ISR 18 */
    .word isr_default   /* ISR 19 */
    .word isr_default   /* ISR 20 */
    .word isr_default   /* ISR 21 */
    .word isr_default   /* ISR 22 */
    .word isr_default   /* ISR 23 */
    .word isr_default   /* ISR 24 */
    .word isr_default   /* ISR 25 */
    .word isr_default   /* ISR 26 */
    .word isr_default   /* ISR 27 */
    .word isr_default   /* ISR 28 */
    .word isr_default   /* ISR 29 */
    .word isr_default   /* ISR 30 */
    .word isr_default   /* ISR 31 */

    .end _int_handler

/****************************************************************
*    Exception Handler
* Same code for all fatal exceptions :
* Print the exception type and the values of EPC & BAR
* on the TTY correspondintg to the processor PROCID,
* and the user program is killed.
****************************************************************/
    .ent _exc_handler

_exc_handler:
_cause_bp:
_cause_ukn:
_cause_ri:
_cause_ovf:
_cause_adel:
_cause_ades:
_cause_ibe:
_cause_dbe:
_cause_cpu:
    mfc0    $26,    $13             /* $26 <= CR */
    andi    $26,    $26,    0x3C    /* $26 <= _cause_index * 4 */
    la      $27,    _mess_causes    /* mess_cause table base address */
    addu    $27,    $26,    $27     /* $26 <= message base address */

    /* take the lock on TTY0 */
    li      $4,     0
    jal     _tty_get_lock
    nop

    /* display exception type */
    lw      $4,     ($27)           /* $4 <= message address */
    li      $5,     36              /* $5 <= message length */
    li      $6,     0               /* $6 <= TTY0 */
    jal     _tty_write
    nop

    /* display EPC value */
    la      $4,     _msg_epc        /* $4 <= message address */
    li      $5,     8               /* $5 <= message length */
    li      $6,     0               /* $6 <= TTY0 */
    jal     _tty_write
    nop

    mfc0    $4,     $14             /* $4 <= EPC value */
    la      $5,     _itoa_buffer    /* $5 <= buffer address */
    addiu   $5,     $5,     2       /* skip 0x prefix */
    jal     _itoa_hex               /* fill buffer */
    nop

    la      $4,     _itoa_buffer    /* $4 <= buffer address */
    li      $5,     10              /* $5 <= buffer length */
    li      $6,     0               /* $6 <= TTY0 */
    jal     _tty_write
    nop

    /* display BAR value */
    la      $4,     _msg_bar        /* $4 <= message address */
    li      $5,     8               /* $5 <= message length */
    li      $6,     0               /* $6 <= TTY0 */
    jal     _tty_write
    nop

    mfc0    $4,     $8              /* $4 <= BAR value */
    la      $5,     _itoa_buffer    /* $5 <= buffer address */
    addiu   $5,     $5,     2       /* skip 0x prefix */
    jal     _itoa_hex               /* fill buffer */
    nop

    la      $4,     _itoa_buffer    /* $4 <= message address */
    li      $5,     10              /* $5 <= message length */
    li      $6,     0               /* $6 <= TTY0 */
    jal     _tty_write
    nop


    /* release the lock on TTY0 */
    li      $4,     0
    jal     _tty_get_lock
    nop

    j       _exit                   /* kill user program */

/* Exceptions Messages table (indexed by XCODE)  */
_mess_causes:
    .word _msg_ukncause
    .word _msg_ukncause
    .word _msg_ukncause
    .word _msg_ukncause
    .word _msg_adel
    .word _msg_ades
    .word _msg_ibe
    .word _msg_dbe
    .word _msg_ukncause
    .word _msg_bp
    .word _msg_ri
    .word _msg_cpu
    .word _msg_ovf
    .word _msg_ukncause
    .word _msg_ukncause
    .word _msg_ukncause

/********************************************************************
*    All messages
* Messages length are fixed : 8 or 36 characters...
********************************************************************/
_msg_bar:        .asciiz "\nBAR  = "
_msg_epc:        .asciiz "\nEPC  = "
_msg_default:    .asciiz "\n\n  !!! Default ISR  !!!           \n"
_msg_uknsyscall: .asciiz "\n\n  !!! Undefined System Call !!!  \n"
_msg_ukncause:   .asciiz "\n\nException : strange unknown cause\n"
_msg_adel:       .asciiz "\n\nException : illegal read address \n"
_msg_ades:       .asciiz "\n\nException : illegal write address\n"
_msg_ibe:        .asciiz "\n\nException : inst bus error       \n"
_msg_dbe:        .asciiz "\n\nException : data bus error       \n"
_msg_bp:         .asciiz "\n\nException : breakpoint           \n"
_msg_ri:         .asciiz "\n\nException : reserved instruction \n"
_msg_ovf:        .asciiz "\n\nException : arithmetic overflow  \n"
_msg_cpu:        .asciiz "\n\nException : illegal coproc access\n"

    .end _exc_handler


