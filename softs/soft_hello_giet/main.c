#include "hard_config.h"
#include "stdio.h"

void main()
{
    char c;
    unsigned int proc_id = _procid();
    unsigned int l       = (proc_id % NB_PROCS_MAX);
    unsigned int x       = (proc_id / NB_PROCS_MAX) >> Y_WIDTH;
    unsigned int y       = (proc_id / NB_PROCS_MAX) & ((1<<Y_WIDTH) - 1);
    while ( 1 )
    {
       _tty_printf("hello world... from processor [%d,%d,%d]\n",x,y,l);

       _tty_getc( &c );
    }
} // end main()

// Local Variables:
// tab-width: 3
// c-basic-offset: 3
// c-file-offsets:((innamespace . 0)(inline-open . 0))
// indent-tabs-mode: nil
// End:

// vim: filetype=cpp:expandtab:shiftwidth=3:tabstop=3:softtabstop=3



