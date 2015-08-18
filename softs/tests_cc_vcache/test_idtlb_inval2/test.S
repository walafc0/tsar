/*
 * itlb inval: a write to a PTE2 should invalidate the corresponding
 * itlb and dtlb entries. 
 */
#include <registers.h>
#include <misc.h>
#include <vcache.h>

	.text
	.globl  _start
_start:
	.set noreorder
	la	k0, TTY_BASE
	la	k1, EXIT_BASE

	PRINT(startstr)

	/* reset cop0 status (keep BEV) */
	lui	a0, 0x0040;
	mtc0	a0, COP0_STATUS

	la	a0, pte1_a
	srl	a0, a0, 13
	mtc2	a0, VC_PTPR
	nop

	li	a0, VC_TLB_EN_ITLB | VC_TLB_EN_DTLB | VC_TLB_EN_ICACHE | VC_TLB_EN_DCACHE
	mtc2 	a0, VC_TLB_EN

	PRINT(mmustr)
	/* cause a DTLB miss */
	la	t0, roval
	lw	a0, 0(t0)
	PRINTX
	PUTCHAR(' ')

	/* cause a itlb miss */
	jal	doload
	nop
	PRINTX
	PUTCHAR(' ')

	la	t0, pte2_a
	mtc2 	t0, VC_DTLB_INVAL /* force tlb miss on write */
	la	a0, (BOOT_ADDRESS+0x2000) >> 12
	sw	a0, 12(t0) /* change PTE2 */
	sync
1:
	ll	a0, 8(t0)
	mtc2 	t0, VC_DTLB_INVAL /* force tlb miss on write */
	la	a0, PTE2_V | PTE2_C | PTE2_X
	sc	a0, 8(t0)
	beqz	a0, 1b
	nop

	/* now we should get the second values */
	la	t0, roval
	lw	a0, 0(t0)
	PRINTX
	PUTCHAR(' ')

	jal	doload 
	nop
	PRINTX
	PUTCHAR('\n')
	/* we should get there */
	EXIT(0)

	.globl excep
excep:
	.set noreorder
	PRINT(statusstr)
	mfc0	a0, COP0_STATUS
	PRINTX

	PRINT(causestr)
	mfc0	a0, COP0_CAUSE
	PRINTX

	PRINT(pcstr)
	mfc0	a0, COP0_EXPC
	PRINTX

	PRINT(badvastr)
	mfc0	a0, COP_0_BADVADDR
	PRINTX

	PUTCHAR('\n')
	/* we should not get there */
	EXIT(1)

	.rodata:
statusstr: .ascii "status \0"
causestr: .ascii " cause \0"
pcstr: .ascii " pc \0"
badvastr: .ascii " badva \0"
mmustr: .ascii "mmu started \0"
startstr: .ascii "start\n\0"

	.org EXCEP_ADDRESS - BOOT_ADDRESS
	.globl evect
evect:
	j	excep
	nop

	/*
	 * code that will be switched by MMU switch.
	 * we use a ldscript trick here, to load this function at
	 * the appropriate address
	 */
	.section .text2, "ax"
	.globl doload
doload:
	jr      ra
	li      a0, MAGIC1
	/* we should not get there */
	EXIT(1)
	nop
roval:	.word MAGIC2
	.align 12
	.globl doload2
doload2:
	jr      ra
	li      a0, MAGIC2
	/* we should not get there */
	EXIT(1)
	nop
roval2:	.word MAGIC1

	.data
	.word MAGIC1
testval:
	.word MAGIC2
	.globl pte2_a

/*
 * one PD with a level 2 PTP: we change an entry in the PTP and
 * check that the ITLB has been invalidated
 */
pte2_a:
	.align 12
	.word PTE2_V | PTE2_C | PTE2_X
	.word BOOT_ADDRESS >> 12
	.word PTE2_V | PTE2_C | PTE2_X
	.word (BOOT_ADDRESS+0x1000) >> 12
	.org pte2_a + 4092
	.word 0
	.globl pte2_b
	.globl pte1_a
pte1_a:
	.align 13
	.word PTE1_V | PTE1_C | PTE1_W | 0x0 /* map PA 0 at VA 0 */
	.word 0x0
	.org pte1_a + (BOOT_ADDRESS >> 21) * 4
	.word PTE1_V | PTE1_T | (0x1000 >> 12) /* map PA 0xbfc00000 at VA 0xbfc00000 with 4k page: check real address of pte2_a !!! */
	.org pte1_a + (TTY_BASE >> 21) * 4
	.word PTE1_V | PTE1_W | (TTY_BASE >> 21) /* map PA 0xd0200000 at VA 0xd0200000 */
	.org pte1_a + (EXIT_BASE >> 21) * 4
	.word PTE1_V | PTE1_W | (EXIT_BASE >> 21) /* map PA 0xe0000000 at VA 0xe0000000 */
	.org pte1_a + 8192
