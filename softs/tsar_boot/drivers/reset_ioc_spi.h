/**
 * \file reset_ioc_spi.h
 * \date 30 August 2012
 * \author Cesar fuguet <cesar.fuguet-tortolero@lip6.fr>
 *
 * This file defines the driver of a SD Card device using an SPI controller
 */

#ifndef RESET_IOC_SPI_H
#define RESET_IOC_SPI_H

#include <spi.h>

/**
 * \brief SD Card type definition
 */
struct sdcard_dev
{ 
    /** 
     * SPI controller pointer 
     */
    struct spi_dev * spi;

    /**
     * Block length of the SDCARD
     */
    unsigned int block_length;

    /**
     * Access pointer representing the offset in bytes used
     * to read or write in the SDCARD.
     *
     * \note this driver is for cards SDSD, therefore this offset
     *       must be multiple of the block length
     */ 
    unsigned int access_pointer;

    /**
     * Slave ID. This ID represents the number of the slave select signal
     * used in the hardware platform
     */
    int    slave_id;

    /* is the card high capacity ? */
    int sdhc;
};

/**
 * \param   sdcard  : uninitialized pointer. This parameter will contain
 *                    a pointer to the initialized block device or NULL otherwise
 * \param   spi     : initialized pointer to the spi controller
 * \param   ss      : slave select signal number
 *
 * \return  0 when initialization succeeds or an error code value otherwise.
 *          The error codes are defined in this header file.
 *
 * \brief   Initialize the block device
 */

int sdcard_dev_open(struct sdcard_dev * sdcard, struct spi_dev * spi, int ss);

/**
 * \param   sdcard  : Pointer to the initialized block device
 * \param   buf     : Pointer to a memory segment wherein store
 * \param   count   : number of bytes to read
 *
 * \return  0 when read succeeds or an error code value otherwise.
 *          The error codes are defined in this header file.
 *
 * \brief   Read in the block device
 *
 * The read is made in the current block device access pointer.
 * In the read succeeds, the block device access pointer is
 * relocated to the next block.
 */

int sdcard_dev_read(struct sdcard_dev * sdcard, void * buf, unsigned int count);

/**
 * \param   sdcard  : Pointer to the initialized block device
 * \param   buf     : Pointer to a memory segment wherein the
 * \param   count   : number of blocks to write
 *
 * \return  0 when write succeeds or an error code value otherwise.
 *          The error codes are defined in this header file.
 *
 * \brief   Write in the block device
 *
 * The write is made in the current block device access pointer.
 * In the write succeeds, the block device access pointer is
 * relocated to the next block.
 */

unsigned int sdcard_dev_write(struct sdcard_dev * sdcard, void * buf, unsigned int count);

/**
 * \param   sdcard  : Pointer to the initialized block device
 * \param   pos     : Position where the block device access
 *                    pointer must be move
 *
 * \return  void
 *
 * \brief   Change block device access pointer position
 *  
 * The block device access pointer is relocated in terms of blocks
 */

void sdcard_dev_lseek(struct sdcard_dev * sdcard, unsigned int pos);

/**
 * \param   sdcard  : Pointer to the initialized block device
 * \param   len     : Block device length to set
 *
 * \return  0 when succeed or error code value otherwise
 *
 * \brief   Set the block length of the device
 */

int sdcard_dev_set_blocklen(struct sdcard_dev * sdcard, unsigned int len);

/**
 * \return  0 when succeed or error code value otherwise
 *
 * \brief   Initialize both the SD Card and the SD Card controller
 */

int reset_spi_init();

/**
 * \param   lba     : First block index on device
 * \param   buffer  : Destination memory buffer address
 * \param   count   : Number of bloks to be read
 *
 * \return  0 when succeed or error code value otherwise
 *
 * \brief   Transfer count blocks from device to memory
 */

int reset_spi_read( unsigned int lba, void* buffer, unsigned int count );



/**
 * SD Card constants
 */

/** Number of retries after an unacknowledge command */
#define SDCARD_COMMAND_TIMEOUT      100

/** This command is a simple SD commmand */
#define SDCARD_CMD                  0

/** This is an application specific command */
#define SDCARD_ACMD                 1

/** The transmition is done in the negative edge of the clock */
#define SDCARD_TX_NEGEDGE           0

/** The transmition is done in the positive edge of the clock */
#define SDCARD_TX_POSEDGE           1

/** The reception is done in the negative edge of the clock */
#define SDCARD_RX_NEGEDGE           0

/** The reception is done in the positive edge of the clock */
#define SDCARD_RX_POSEDGE           1

/**
 * SD Card macros
 */

/** Check if the response is valid */
#define SDCARD_CHECK_R1_VALID(x)    (~x & SDCARD_R1_RSP_VALID) ? 1 : 0

/**
 * Check if there is an error in the response
 *
 * \note this macro must be used after verify that the response is
 *       valid
 */
#define SDCARD_CHECK_R1_ERROR(x)    ( x & 0x7E)                ? 1 : 0

/**
 * SD Card Response 1 (R1) format constants
 */
#define SDCARD_R1_IN_IDLE_STATE     ( 1 << 0 ) /**< \brief R1 bit 0 */
#define SDCARD_R1_ERASE_RESET       ( 1 << 1 ) /**< \brief R1 bit 1 */
#define SDCARD_R1_ILLEGAL_CMD       ( 1 << 2 ) /**< \brief R1 bit 2 */
#define SDCARD_R1_COM_CRC_ERR       ( 1 << 3 ) /**< \brief R1 bit 3 */
#define SDCARD_R1_ERASE_SEQ_ERR     ( 1 << 4 ) /**< \brief R1 bit 4 */
#define SDCARD_R1_ADDRESS_ERR       ( 1 << 5 ) /**< \brief R1 bit 5 */
#define SDCARD_R1_PARAMETER_ERR     ( 1 << 6 ) /**< \brief R1 bit 6 */
#define SDCARD_R1_RSP_VALID         ( 1 << 7 ) /**< \brief R1 bit 7 */

#endif

/*
 * vim: tabstop=4 : shiftwidth=4 : expandtab : softtabstop=4
 */
