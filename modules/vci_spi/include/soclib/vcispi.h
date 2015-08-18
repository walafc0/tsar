/*
 * SOCLIB_LGPL_HEADER_BEGIN
 * 
 * This file is part of SoCLib, GNU LGPLv2.1.
 * 
 * SoCLib is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation; version 2.1 of the License.
 * 
 * SoCLib is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public
 * License along with SoCLib; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301 USA
 * 
 * SOCLIB_LGPL_HEADER_END
 *
 * Copyright (c) UPMC, Lip6, SoC
 *         manuel bouyer
 *
 * Maintainers: bouyer
 */
#ifndef SPISD_H
#define SPISD_H

enum SoclibBlockDeviceRegisters {
    SPI_DATA_TXRX0,
    SPI_DATA_TXRX1,
    SPI_DATA_TXRX2,
    SPI_DATA_TXRX3,
    SPI_CTRL,
    SPI_DIVIDER,
    SPI_SS,
    SPI_DMA_BASE,
    SPI_DMA_BASEH,
    SPI_DMA_COUNT
};

#define SPI_CTRL_DMA_ERR	(1 << 17) /* R   DMA error                    */
#define SPI_CTRL_DMA_BSY	(1 << 16) /* R   DMA in progress              */
#define SPI_CTRL_CPOL		(1 << 15) /* R/W Clock polarity               */
#define SPI_CTRL_CPHA		(1 << 14) /* R/W Clock phase                  */
#define SPI_CTRL_IE_EN		(1 << 12) /* R/W Interrupt Enable             */
// 9-11 reserved
#define SPI_CTRL_GO_BSY		(1 << 8 ) /* R/W Start the transfer           */
#define SPI_CTRL_CHAR_LEN_MASK	(0xFF   ) /* R/W Bits transmited in 1 transfer*/

#define SPI_DMA_COUNT_READ	(1 << 0) /* operation is a read (else write) */

#endif /* SPISD_H */

// Local Variables:
// tab-width: 4
// c-basic-offset: 4
// c-file-offsets:((innamespace . 0)(inline-open . 0))
// indent-tabs-mode: nil
// End:

// vim: filetype=cpp:expandtab:shiftwidth=4:tabstop=4:softtabstop=4

