#ifndef RESET_TTY_H
#define RESET_TTY_H

void reset_exit();
int  reset_getc(char * c);
void reset_putc(const char c);
void reset_puts(const char *buffer);
void reset_putx(unsigned int val);
void reset_putd(unsigned int val);

#endif
