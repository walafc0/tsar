/**
 * \file    : reset_utils.c
 * \date    : August 2012
 * \author  : Cesar Fuguet
 *
 * Definition of utilities functions used by the TSAR pre-loader
 */

#include <reset_utils.h>
#include <reset_tty.h>
#include <reset_ioc.h>
#include <io.h>

/*
 * Cache block data buffer and cached block index
 */
struct aligned_blk blk_buf;
int blk_buf_idx;

/**
 * \param file_offset: Disk relative offset of file
 * \param buf: Destination buffer
 * \param nbyte: Number of bytes to read
 * \param offset: File relative offset
 *
 * \brief read from disk into buffer "nbyte" bytes from (file_offset + offset)
 *
 * \note Absolute disk offset (in bytes) is (file_offset + offset)
 */
int pread(size_t file_offset, void *buf, size_t nbyte, size_t offset) {
    if (nbyte == 0) return 0;

    char *dst;
    int offset_blk;
    int unaligned_nbyte;
    int read_nbyte;

    dst = (char*) buf;

    /*
     * Offset parameter is relative to file, therefore compute disk relative
     * offset (in bytes)
     */
    offset += file_offset;

    /*
     * Read unaligned bytes at start of segment passing by block cache
     */
    offset_blk = (offset / BLOCK_SIZE);
    offset = (offset % BLOCK_SIZE);
    unaligned_nbyte = BLOCK_SIZE - offset;
    read_nbyte = 0;
    if (offset) {
        /*
         * Check cache block hit: if miss, read new block else, use cache block
         * data
         */
        if (offset_blk != blk_buf_idx) {
            if (reset_ioc_read(offset_blk, (void*)&blk_buf, 1)) {
                return -1;
            }
        }
        blk_buf_idx = offset_blk;
        read_nbyte = (nbyte > unaligned_nbyte) ? unaligned_nbyte : nbyte;
        memcpy((void*)dst, (void*)&blk_buf.b[offset], read_nbyte);
        nbyte -= read_nbyte;
        offset_blk += 1;
    }

    /*
     * Read aligned bytes directly to buffer
     */
    size_t nblk = nbyte / BLOCK_SIZE;
    if (nblk) {
        if (reset_ioc_read(offset_blk, (void*)&dst[read_nbyte], nblk)) {
            return -1;
        }
        read_nbyte += (nblk * BLOCK_SIZE);
        nbyte -= (nblk * BLOCK_SIZE);
        offset_blk += nblk;
    }

    /*
     * Read unaligned bytes at the end of segment passing by block cache
     */
    if (nbyte) {
        if (reset_ioc_read(offset_blk, (void*)&blk_buf, 1)) {
            return -1;
        }
        blk_buf_idx = offset_blk;
        memcpy((void*)&dst[read_nbyte], (void*)&blk_buf, nbyte);
        read_nbyte += nbyte;
    }
    return read_nbyte;
}

/**
 * \param _dst   : Destination buffer base address
 * \param _src   : Source buffer base address
 * \param size   : Number of bytes to transfer
 *
 * \brief Transfer data between two memory buffers.
 */
void* memcpy(void *_dst, const void *_src, size_t n)
{
    unsigned int *dst = _dst;
    const unsigned int *src = _src;
    if (!((unsigned int)dst & 3) && !((unsigned int)src & 3)) {
        while (n > 3) {
            *dst++ = *src++;
            n -= 4;
        }
    }

    unsigned char *cdst = (unsigned char*) dst;
    unsigned char *csrc = (unsigned char*) src;
    while (n--) {
        *cdst++ = *csrc++;
    }
    return _dst;
}

/**
 * \param _dst   : Destination buffer base address
 * \param value  : Initialization value
 * \param size   : Number of bytes to initialize
 *
 * \brief Initialize memory buffers with predefined value.
 */
void* memset(void *_dst, int c, size_t len)
{
    if (len == 0) return _dst;

    unsigned char val = (unsigned char) c;

    /*
     * Set not word aligned bytes at start of destination buffer
     */
    unsigned char* cdst = (unsigned char*) _dst;
    while (((addr_t)cdst & 3) && len--) {
        *cdst++ = val;
    }

    /*
     * Set 4 bytes words on destination buffer
     */
    unsigned int word = (val << 24) | (val << 16) | (val << 8 ) | val;
    addr_t *wdst = (addr_t*)cdst;
    while (len > 3) {
        *wdst++ = word;
        len -= 4;
    }

    /*
     * Set not word aligned bytes at end of destination buffer
     */
    cdst = (unsigned char*) wdst;
    while(len--) {
        *cdst++ = val;
    }
    return _dst;
}

/**
 * \param ehdr : ELF header pointer
 *
 * \brief Verify that ELF file is valid and that the number of program headers
 *        does not exceed the defined maximum
 */
void check_elf_header(Elf32_Ehdr *ehdr)
{
    /*
     * Verification of ELF Magic Number
     */
    if ((ehdr->e_ident[EI_MAG0] != ELFMAG0) ||
        (ehdr->e_ident[EI_MAG1] != ELFMAG1) ||
        (ehdr->e_ident[EI_MAG2] != ELFMAG2) ||
        (ehdr->e_ident[EI_MAG3] != ELFMAG3))
    {
        reset_puts("\n[RESET ERROR] Unrecognized file format (not an ELF format)\n");
        reset_exit();
    }

    /*
     * Verification of Program Headers table size. It must be
     * smaller than the work size allocated for the
     * elf_pht[RESET_PHDR_ARRAY_SIZE] array
     */
    if (ehdr->e_phnum > RESET_PHDR_ARRAY_SIZE)
    {
        reset_puts("[RESET ERROR] ELF PHDR table size too large\n");
        reset_exit();
    }
}

/**
 * \param elf_phdr_ptr : Pointer to the ELF program header to print
 *
 * \brief Print some fields of a ELF program header
 */
void reset_print_elf_phdr(Elf32_Phdr * elf_phdr_ptr)
{
    reset_puts("- type   : ");
    reset_putx(elf_phdr_ptr->p_type);
    reset_puts("\n- offset : ");
    reset_putx(elf_phdr_ptr->p_offset);
    reset_puts("\n- vaddr  : ");
    reset_putx(elf_phdr_ptr->p_vaddr);
    reset_puts("\n- paddr  : ");
    reset_putx(elf_phdr_ptr->p_paddr);
    reset_puts("\n- filesz : ");
    reset_putx(elf_phdr_ptr->p_filesz);
    reset_puts("\n- memsz  : ");
    reset_putx(elf_phdr_ptr->p_memsz);
    reset_puts("\n- flags  : ");
    reset_putx(elf_phdr_ptr->p_flags);
    reset_puts("\n- align  : ");
    reset_putx(elf_phdr_ptr->p_align);
    reset_puts("\n");
}

/**
 * \param buffer : Pointer to the char buffer
 *
 * \brief Print a 512 bytes buffer
 */
#if (RESET_DEBUG == 1 )
void reset_display_block( char* buffer )
{
    unsigned int line;
    unsigned int word;

    reset_puts("\n***********************************************************************\n");
    for ( line = 0 ; line < 32 ; line++ )
    {
        // display line index 
        reset_putx( line );
        reset_puts(" : ");

        // display 8*4 bytes hexa
        for ( word=0 ; word<4 ; word++ )
        {
            unsigned int byte  = (line<<4) + (word<<2);
            unsigned int hexa  = (buffer[byte  ]<<24) |
                                 (buffer[byte+1]<<16) |
                                 (buffer[byte+2]<< 8) |
                                 (buffer[byte+3]);
            reset_putx( hexa );
            reset_puts(" | ");
        }
        reset_puts("\n");
    }
    reset_puts("***********************************************************************\n");
}   
#endif

/*
 * vim: tabstop=4 : softtabstop=4 : shiftwidth=4 : expandtab
 */
