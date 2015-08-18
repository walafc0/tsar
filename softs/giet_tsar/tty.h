#ifndef TTY_REGS_H
#define TTY_REGS_H

enum SoclibTtyRegisters {
    TTY_WRITE   = 0,
    TTY_STATUS  = 1,
    TTY_READ    = 2,
    TTY_CONFIG  = 3,
    /**/
    TTY_SPAN    = 4,
};

#endif

// Local Variables:
// tab-width: 4;
// c-basic-offset: 4;
// c-file-offsets:((innamespace . 0)(inline-open . 0));
// indent-tabs-mode: nil;
// End:
//
// vim: filetype=cpp:expandtab:shiftwidth=4:tabstop=4:softtabstop=4

