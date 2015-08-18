/**
 * \file    : reset_elf_loader.c
 * \date    : August 2012
 * \author  : Cesar Fuguet
 *
 * This file defines an elf file loader which reads an executable .elf file
 * starting at a sector passed as argument on a disk and copy the different
 * ELF program segments in the appropriate memory address using as information
 * the virtual address read from the .elf file.
 */

#include <reset_ioc.h>
#include <elf-types.h>
#include <reset_tty.h>
#include <reset_utils.h>
#include <defs.h>

extern int blk_buf_idx;
extern int dtb_start, dtb_end;

addr_t dtb_addr;

///////////////////////////////////////////////////////////////////////////////
void * reset_elf_loader(size_t lba)
///////////////////////////////////////////////////////////////////////////////
{
    size_t file_offset = lba * BLOCK_SIZE;

    reset_puts("\n[RESET] Start reset_elf_loader at cycle ");
    reset_putd( proctime() );
    reset_puts("\n");

    /*
     * Init the cache block index
     */
    blk_buf_idx = -1;

    /*
     * Load ELF HEADER
     */
    Elf32_Ehdr elf_header;
    if (pread(file_offset, (void*)&elf_header, sizeof(Elf32_Ehdr), 0) < 0) {
        goto error;
    }

#if (RESET_DEBUG > 1)
    reset_display_block( (char*)&elf_header );
#endif

    check_elf_header(&elf_header);

    /*
     * Load ELF PROGRAM HEADER TABLE
     */
    Elf32_Phdr elf_pht[RESET_PHDR_ARRAY_SIZE];
    size_t phdr_nbyte = sizeof(Elf32_Phdr) * elf_header.e_phnum;
    size_t phdr_off = elf_header.e_phoff;
    if (pread(file_offset, (void*)&elf_pht, phdr_nbyte, phdr_off) < 0) {
        goto error;
    }

    /*
     * Search for loadable segments in the ELF file
     */
    int pseg;
    for (pseg = 0; pseg < elf_header.e_phnum; pseg++) {
        if(elf_pht[pseg].p_type != PT_LOAD) continue;

#if (RESET_DEBUG == 1)
        reset_puts("\n[RESET DEBUG] Loadable segment found:\n");
        reset_print_elf_phdr(&elf_pht[pseg]);
#endif

        addr_t p_paddr = elf_pht[pseg].p_paddr;
        size_t p_filesz = elf_pht[pseg].p_filesz;
        size_t p_memsz = elf_pht[pseg].p_memsz;
        size_t p_offset = elf_pht[pseg].p_offset;

        /*
         * Copy program segment from ELF executable into corresponding physical
         * address
         */
        if (pread(file_offset, (void*)p_paddr, p_filesz, p_offset) < 0) {
            goto error;
        }

        /*
         * Fill remaining bytes with zero (filesz < memsz)
         */
        char* pseg_ptr = (char*)p_paddr;
        memset((void*)&pseg_ptr[p_filesz], 0, (p_memsz - p_filesz));

        reset_puts("\n[RESET] Segment loaded : address = ");
        reset_putx(p_paddr);
        reset_puts(" / size = ");
        reset_putx(p_filesz);
        reset_puts("\n");
    }

    if (pseg == 0) {
        reset_puts("\n[RESET ERROR] No loadable segments");
        goto error;
    }

    /* By default, the used device tree is the one in the ROM */
    dtb_addr = (addr_t)&dtb_start;

#if OS_LINUX
    /* When loading a Linux kernel, the device tree should be located in low
     * memory addresses before the kernel itself.
     * - When the ROM-contained DTB is located before the kernel no need to copy
     *   the DTB elsewhere.
     * - When the ROM-contained DTB is located after the kernel the DTB is
     *   copied before the kernel. */
    const int copy_dtb = ((addr_t)&dtb_start > elf_pht[0].p_paddr);
    if (copy_dtb) {
        size_t dtb_size = (size_t)&dtb_end - (size_t)&dtb_start;
        dtb_addr = SEG_RAM_BASE;
        if ((dtb_addr + (addr_t)dtb_size) >= elf_pht[0].p_paddr) {
            reset_puts("\n[RESET ERROR] Insufficient space to copy the DTB");
            goto error;
        }

        memcpy((void*)dtb_addr, (void*)&dtb_start, dtb_size);
        reset_puts("\n[RESET] Device tree blob copied / address = ");
        reset_putx(dtb_addr);
        reset_puts(" / size = ");
        reset_putx(dtb_size);
    }
#endif

    reset_puts("\n[RESET] Complete reset_elf_loader at cycle ");
    reset_putd(proctime());
    reset_puts(" / boot entry = ");
    reset_putx((addr_t)elf_header.e_entry);
    reset_puts("\n");

    return ((void*)elf_header.e_entry);

error:
    reset_puts("\n[RESET ERROR] Error while loading ELF file");
    reset_exit();
    return 0;
}

// vim: tabstop=4 : softtabstop=4 : shiftwidth=4 : expandtab
