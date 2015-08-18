/**
 * \file    spi.c
 * \date    31 August 2012
 * \author  Cesar Fuguet <cesar.fuguet-tortolero@lip6.fr>
 */
#include <spi.h>
#include <reset_ioc.h>
#include <reset_utils.h>

/**
 * \param   x: input value
 *
 * \return  byte-swapped value
 *
 * \brief   byte-swap a 32bit word
 */
static unsigned int bswap32(unsigned int x)
{
  unsigned int y;
  y =  (x & 0x000000ff) << 24;
  y |= (x & 0x0000ff00) <<  8;
  y |= (x & 0x00ff0000) >>  8;
  y |= (x & 0xff000000) >> 24;
  return y;
}

/**
 * \param   spi :   Initialized pointer to the SPI controller
 *
 * \brief   Wait until the SPI controller has finished a transfer
 *
 * Wait until the GO_BUSY bit of the SPI controller be deasserted
 */
static void _spi_wait_if_busy(struct spi_dev * spi)
{
    register int delay;

    while(SPI_IS_BUSY(spi))
    {
        for (delay = 0; delay < 100; delay++);
    }
}

/**
 * \param   spi : Initialized pointer to the SPI controller
 *
 * \return  void
 *
 * \brief   Init transfer of the tx registers to the selected slaves
 */
static void _spi_init_transfer(struct spi_dev * spi)
{
    unsigned int spi_ctrl = ioread32(&spi->ctrl);

    iowrite32(&spi->ctrl, spi_ctrl | SPI_CTRL_GO_BSY);
}

/**
 * \param   spi_freq    : Desired frequency for the generated clock from the SPI
 *                        controller
 * \param   sys_freq    : System clock frequency
 *
 * \brief   Calculated the value for the divider register in order to obtain the SPI
 *          desired clock frequency 
 */
static unsigned int _spi_calc_divider_value  (
    unsigned int spi_freq   ,
    unsigned int sys_freq   )
{
    return ((sys_freq / (spi_freq * 2)) - 1);
}

void spi_put_tx(struct spi_dev * spi, unsigned char byte, int index)
{
    _spi_wait_if_busy(spi);
    {
        iowrite8(&spi->rx_tx[index % 4], byte);
        _spi_init_transfer(spi);
    }
    _spi_wait_if_busy(spi);
}

volatile unsigned char spi_get_rx(struct spi_dev * spi, int index)
{
    return ioread8(&spi->rx_tx[index % 4]);
}

void spi_get_data(struct spi_dev * spi, void *buf, unsigned int count)
{
    unsigned int *data = buf;
    unsigned char *data8;
    unsigned int spi_ctrl0, spi_ctrl;
    int i;

    _spi_wait_if_busy(spi);

    spi_ctrl0 = ioread32(&spi->ctrl);
#ifdef IOC_USE_DMA
    if (count == 512 && ((int)buf & 0x3f) == 0) {
	/* use DMA */
	spi->dma_base = (int)buf;
	spi->dma_baseh = 0;
	spi->dma_count = count | SPI_DMA_COUNT_READ;
        _spi_wait_if_busy(spi);
	i = count / 4;
    } else
#endif /* IOC_USE_DMA */
    {
        /* switch to 128 bits words */
        spi_ctrl = (spi_ctrl0 & ~SPI_CTRL_CHAR_LEN_MASK) | 128;
        iowrite32(&spi->ctrl, spi_ctrl);

        /* read data */
        for (i = 0; i + 3 < count / 4; i += 4) {
            iowrite32(&spi->rx_tx[0], 0xffffffff);
            iowrite32(&spi->rx_tx[1], 0xffffffff);
            iowrite32(&spi->rx_tx[2], 0xffffffff);
            iowrite32(&spi->rx_tx[3], 0xffffffff);
            iowrite32(&spi->ctrl,  spi_ctrl | SPI_CTRL_GO_BSY);

            _spi_wait_if_busy(spi);
        
            *data = bswap32(ioread32(&spi->rx_tx[3]));
            data++;
            *data = bswap32(ioread32(&spi->rx_tx[2]));
            data++;
            *data = bswap32(ioread32(&spi->rx_tx[1]));
            data++;
            *data = bswap32(ioread32(&spi->rx_tx[0]));
            data++;
        }
    }

    /* switch back to original word size */
    iowrite32(&spi->ctrl, spi_ctrl0);

    /* read missing bits */
    data8 = (void *)data;
    i = i * 4;
    for (; i < count; i++)
    {
        iowrite32(&spi->rx_tx[0], 0xffffffff);
        iowrite32(&spi->ctrl,  spi_ctrl0 | SPI_CTRL_GO_BSY);
    
        _spi_wait_if_busy(spi);
    
        *data8 = spi_get_rx(spi, 0);
        data8++;
    }
    return;
}

void spi_ss_assert(struct spi_dev * spi, int index)
{
    unsigned int spi_ss = ioread32(&spi->ss);

    iowrite32(&spi->ss, spi_ss | (1 << index));
}

void spi_ss_deassert(struct spi_dev * spi, int index)
{
    unsigned int spi_ss = ioread32(&spi->ss);

    iowrite32(&spi->ss, spi_ss & ~(1 << index));
}

void spi_dev_config (
    struct spi_dev * spi,
    int spi_freq        ,
    int sys_freq        ,
    int char_len        ,
    int tx_edge         ,
    int rx_edge         )
{
    unsigned int spi_ctrl = ioread32(&spi->ctrl);

    if      ( tx_edge == 0 ) spi_ctrl |=  SPI_CTRL_TXN_EN;
    else if ( tx_edge == 1 ) spi_ctrl &= ~SPI_CTRL_TXN_EN;
    if      ( rx_edge == 0 ) spi_ctrl |=  SPI_CTRL_RXN_EN;
    else if ( rx_edge == 1 ) spi_ctrl &= ~SPI_CTRL_RXN_EN;
    if      ( char_len > 0 ) spi_ctrl  = (spi_ctrl & ~SPI_CTRL_CHAR_LEN_MASK) |
                                         (char_len &  SPI_CTRL_CHAR_LEN_MASK);

    iowrite32(&spi->ctrl, spi_ctrl);

    if (spi_freq > 0 && sys_freq > 0)
        iowrite32(&spi->divider, _spi_calc_divider_value(spi_freq, sys_freq));
}

/*
 * vim: tabstop=4 : shiftwidth=4 : expandtab : softtabstop=4
 */
