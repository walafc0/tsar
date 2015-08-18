/**
 * \file  : spi.h
 * \date  : 30 August 2012
 * \author: Cesar Fuguet <cesar.fuguet-tortolero@lip6.fr>
 *
 * This file contains the definition of a driver for the SPI controller
 */

#ifndef SPI_H
#define SPI_H

#include <io.h>

/**
 * SPI type definition
 */
struct spi_dev
{
    /**
     * RX/TX registers of the SPI controller 
     */
    unsigned int rx_tx[4];

    /**
     * Control register of the SPI controller
     */
    unsigned int ctrl;

    /**
     * Divider register for the SPI controller generated clock signal
     */
    unsigned int divider;

    /**
     * Slave select register of the SPI controller
     */
    unsigned int ss;
    unsigned int dma_base;
    unsigned int dma_baseh;
    unsigned int dma_count;
};

/**
 * \param   spi     : initialized pointer to a SPI controller.
 * \param   byte    : Byte to send to the SPI controller
 * \param   index   : index of the TX register in the SPI (TX[index])
 *
 * \return  void
 *
 * \brief   Send a byte to one of the tx buffer registers of the
 *          SPI controller
 */
void spi_put_tx(struct spi_dev * spi, unsigned char byte, int index);

/**
 * \param   spi     : initialized pointer to a SPI controller.
 * \param   index   : index of the RX register in the SPI (RX[index])
 *
 * \return  byte from the RX[index] register
 *
 * \brief   Get a byte from one of the rx buffer registers of the
 *          SPI controller
 */
volatile unsigned char spi_get_rx(struct spi_dev * spi, int index);

/**
 * \param   spi     : initialized pointer to a SPI controller.
 * \param   buf     : buffer to store data read
 * \param   count   : byte count to read
 *
 * \return  void
 *
 * \brief   get a data block from the SPI controller using 128bits
 *          reads if possible
 */
void spi_get_data(struct spi_dev * spi, void *buf, unsigned int count);

/**
 * \param   spi     : initialized pointer to a SPI controller.
 * \param   index   : index of the slave select signal to assert
 *
 * \return  void
 *
 * \brief   Set the index selected slave select signal (ss[index] <= '0')
 */
void spi_ss_assert(struct spi_dev * spi, int index);

/**
 * \param   spi     : initialized pointer to a SPI controller.
 * \param   index   : index of the slave select signal to deassert
 *
 * \return  void
 *
 * \brief   Unset the index selected slave select signal (ss[index] <= '0')
 */
void spi_ss_deassert(struct spi_dev * spi, int index);

/**
 * \param   spi         : initialized pointer to a SPI controller.
 * \param   spi_freq    : SPI Master to Slave clock frequency (in Hz)
 * \param   sys_freq    : System clock frequency (in Hz)
 * \param   char_len    : number to bits to transmit in one transfer
 * \param   tx_edge     : when 0, the Master Out Slave In signal is changed
 *                        on the falling edge of the clock
 * \param   rx_edge     : when 0, the Master In Slave Out signal is latched
 *                        on the falling edge of the clock
 *
 * \return  void
 *
 * \brief   Configure the SPI controller
 * \note    Any of the arguments can be less than 0 if you want to keep the old value
 */
void spi_dev_config (
        struct spi_dev * spi,
        int spi_freq        ,
        int sys_freq        ,
        int char_len        ,
        int tx_edge         ,
        int rx_edge         );

/**
 * SPI macros and constants
 */
#define SPI_TX_POSEDGE         1           /**< MOSI is changed on neg edge   */
#define SPI_TX_NEGEDGE         0           /**< MOSI is changed on pos edge   */
#define SPI_RX_POSEDGE         1           /**< MISO is latched on pos edge   */
#define SPI_RX_NEGEDGE         0           /**< MISO is latched on neg edge   */

#define SPI_CTRL_ASS_EN        ( 1 << 13 ) /**< Auto Slave Sel Assertion      */
#define SPI_CTRL_IE_EN         ( 1 << 12 ) /**< Interrupt Enable              */
#define SPI_CTRL_LSB_EN        ( 1 << 11 ) /**< LSB are sent first            */
#define SPI_CTRL_TXN_EN        ( 1 << 10 ) /**< MOSI is changed on neg edge   */
#define SPI_CTRL_RXN_EN        ( 1 << 9  ) /**< MISO is latched on neg edge   */
#define SPI_CTRL_GO_BSY        ( 1 << 8  ) /**< Start the transfer            */
#define SPI_CTRL_DMA_BSY        (1 << 16)  /***   DMA in progress             */
#define SPI_CTRL_CHAR_LEN_MASK (  0xFF   ) /**< Bits transmited in 1 transfer */
#define SPI_RXTX_MASK          (  0xFF   ) /**< Mask for the an RX/TX value   */

#define SPI_DMA_COUNT_READ      (1 << 0) /* operation is a read (else write) */

/** 
 * \param  x : Initialized pointer to the SPI controller
 *
 * \return 1 if there is an unfinished transfer in the SPI controller
 *
 * \brief  Check the GO_BUSY bit of the SPI Controller
 */
#define SPI_IS_BUSY(x)         ((ioread32(&x->ctrl) & (SPI_CTRL_GO_BSY|SPI_CTRL_DMA_BSY)) != 0) ? 1 : 0

#endif

/*
 * vim: tabstop=4 : shiftwidth=4 : expandtab : softtabstop=4
 */
