/**
 * \file    : exceptions.c
 * \date    : December 2012
 * \author  : Manuel Bouyer
 *
 * This file defines a simple exceptions handler
 */

#include <reset_tty.h>

void handle_except(int status, int cause, int epc, int dbvar)
{
	reset_puts("\n[RESET] exception handler called: \r\n    status ");
	reset_putx(status);
	reset_puts("\r\n    cause  ");
	reset_putx(cause);
	reset_puts(" (exception ");
	reset_putx((cause >> 2) & 0x1f);
	reset_puts(")\r\n    epc    ");
	reset_putx(epc);
	reset_puts("\r\n    dbvar  ");
	reset_putx(dbvar);
	reset_puts("\r\n");
	while (1);
}
