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
 * Copyright (c) UPMC, Lip6, Asim
 *         alain greiner
 *
 * Maintainers: alain
 */
#ifndef IO_BRIDGE_REGS_H
#define IO_BRIDGE_REGS_H

// IOB Configuration registers
// Minimal required segment size = 64 bytes (16 words)
enum 
{
    IOB_IOMMU_PTPR       = 0,     // R/W  : Page Table Pointer Register
    IOB_IOMMU_ACTIVE     = 1,     // R/W  : IOMMU activated if not 0
    IOB_IOMMU_BVAR       = 2,     // R    : Bad Virtual Address
    IOB_IOMMU_ETR        = 3,     // R    : Error type
    IOB_IOMMU_BAD_ID     = 4,     // R    : Faulty peripheral Index (SRCID)
    IOB_INVAL_PTE        = 5,     // W    : Invalidate PTE (using virtual address)
    IOB_WTI_ENABLE       = 6,     // R/W  : Enable WTI (both IOB and external IRQs)
    IOB_WTI_ADDR_LO      = 7,     // R/W  : IOB WTI address (32 LSB bits)
    IOB_WTI_ADDR_HI      = 8,     // R/W  : IOB WTI address (32 MSB bits)
    /**/
    IOB_SPAN             = 16,                                
};

    
// IOMMU Error Type
enum mmu_error_type_e 
{
    MMU_NONE                      = 0x0000, // None
    MMU_WRITE_ACCES_VIOLATION     = 0x0008, // Write access to non writable page 
    MMU_WRITE_PT1_ILLEGAL_ACCESS  = 0x0040, // Write Bus Error accessing Table 1
    MMU_READ_PT1_UNMAPPED 	      = 0x1001, // Read  Page fault on Page Table 1 
    MMU_READ_PT2_UNMAPPED 	      = 0x1002, // Read  Page fault on Page Table 2 
    MMU_READ_PT1_ILLEGAL_ACCESS   = 0x1040, // Read  Bus Error in Table1 access 
    MMU_READ_PT2_ILLEGAL_ACCESS   = 0x1080, // Read  Bus Error in Table2 access
    MMU_READ_DATA_ILLEGAL_ACCESS  = 0x1100, // Read  Bus Error in cache access 
};

#endif /* IO_BRIDGE_REGS_H */

// Local Variables:
// tab-width: 4
// c-basic-offset: 4
// c-file-offsets:((innamespace . 0)(inline-open . 0))
// indent-tabs-mode: nil
// End:

// vim: filetype=cpp:expandtab:shiftwidth=4:tabstop=4:softtabstop=4

