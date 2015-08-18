/**
 * \file    : reset_ioc_spi.c
 * \date    : 30 August 2012
 * \author  : Cesar Fuguet <cesar.fuguet-tortolero@lip6.fr>
 *
 * This file defines the driver of a SD Card device using an SPI controller
 */

#include <reset_ioc_spi.h>
#include <reset_tty.h>
#include <reset_utils.h>
#include <defs.h>
#include <spi.h>

#ifndef SEG_IOC_BASE
#error "SEG_IOC_BASE constant must be defined in the hard_config.h file"
#endif

static struct sdcard_dev         _sdcard_device;
static struct spi_dev *const     _spi_device = (struct spi_dev*)SEG_IOC_BASE;

static const int sdcard_reset_retries = 4;
static const int spi_init_clkfreq     = 200000  ; /* Hz */
static const int spi_func_clkfreq     = 10000000; /* Hz */

/**
 * \param   sdcard: Initialized pointer to the block device
 *
 * \return  void
 *
 * \brief   Enable SD Card select signal
 */
static void _sdcard_enable(struct sdcard_dev * sdcard)
{
    spi_ss_assert(sdcard->spi, sdcard->slave_id);
}

/**
 * \param   sdcard: Initialized pointer to the block device
 *
 * \return  void
 *
 * \brief   Disable SD Card select signal
 */
static void _sdcard_disable(struct sdcard_dev * sdcard)
{
    spi_ss_deassert(sdcard->spi, sdcard->slave_id);
}

/**
 * \param   tick_count: SD Card clock ticks number
 *
 * \return  void
 *
 * \brief   Enable SD Card clock
 *          The tick count is byte measured (1 tick, 8 clock)
 */
static void _sdcard_gen_tick(struct sdcard_dev * sdcard, unsigned int tick_count)
{
    register int i = 0;
    while(i++ < tick_count) spi_put_tx(sdcard->spi, 0xFF, 0);
}

/**
 * \param   sdcard: Initialized pointer to the block device
 *
 * \return  char from the SD card
 *
 * \brief   Get a byte from the SD Card
 */
static unsigned char _sdcard_receive_char(struct sdcard_dev * sdcard)
{
    _sdcard_gen_tick(sdcard, 1);

    return spi_get_rx(sdcard->spi, 0);
}

/**
 * \param   sdcard: Initialized pointer to the block device
 *
 * \return  sdcard response
 *
 * \brief   Wait for a valid response after the send of a command
 *          This function can return if one of the next two conditions are true:
 *           1. Bit valid received
 *           2. Timeout (not valid bit received after SDCARD_COMMAND_TIMEOUT
 *              wait ticks)
 */
static unsigned char _sdcard_wait_response(struct sdcard_dev * sdcard)
{
    unsigned char sdcard_rsp;
    register int  iter;

    iter       = 0;
    sdcard_rsp = _sdcard_receive_char(sdcard);
    while (
            (iter < SDCARD_COMMAND_TIMEOUT) &&
            !SDCARD_CHECK_R1_VALID(sdcard_rsp)
          )
    {
        sdcard_rsp = _sdcard_receive_char(sdcard);
        iter++;
    }

    return sdcard_rsp;
}

/**
 * \params  sdcard: Initialized pointer to the block device
 *
 * \return  void
 *
 * \brief   Wait data block start marker
 */
static void _sdcard_wait_data_block(struct sdcard_dev * sdcard)
{
	while (_sdcard_receive_char(sdcard) != 0xFE);
}

/**
 * \param   sdcard  : Initialized pointer to block device
 * \param   index   : SD card CMD index
 * \param   app     : Type of command, 0 for normal command or 1 for application 
 *                    specific
 * \param   args    : SD card CMD arguments
 *
 * \return  response first byte
 *
 * \brief   Send command to the SD card
 */
static int _sdcard_send_command    (
        struct sdcard_dev * sdcard ,
        int                 index  ,
        int                 app    ,
        void *              args   ,
        unsigned            crc7   )
{
    unsigned char sdcard_rsp;
    unsigned char * _args;

    _sdcard_gen_tick(sdcard, 5);  

    if (app == SDCARD_ACMD)
    {
        spi_put_tx(sdcard->spi, 0x40 | 55         , 0 );/* CMD and START bit */
        spi_put_tx(sdcard->spi, 0x00              , 0 );/* Argument[0]       */
        spi_put_tx(sdcard->spi, 0x00              , 0 );/* Argument[1]       */
        spi_put_tx(sdcard->spi, 0x00              , 0 );/* Argument[2]       */
        spi_put_tx(sdcard->spi, 0x00              , 0 );/* Argument[3]       */
        spi_put_tx(sdcard->spi, 0x01 | (crc7 << 1), 0 );/* END bit           */

        sdcard_rsp = _sdcard_wait_response(sdcard);
        if (SDCARD_CHECK_R1_ERROR(sdcard_rsp))
        {
            return sdcard_rsp;        
        }
    }

    _args = (unsigned char *) args;

    _sdcard_gen_tick(sdcard, 1);  

    spi_put_tx(sdcard->spi, 0x40 | index      , 0 );
    spi_put_tx(sdcard->spi, _args[0]          , 0 );
    spi_put_tx(sdcard->spi, _args[1]          , 0 );
    spi_put_tx(sdcard->spi, _args[2]          , 0 );
    spi_put_tx(sdcard->spi, _args[3]          , 0 );
    spi_put_tx(sdcard->spi, 0x01 | (crc7 << 1), 0 );

    return _sdcard_wait_response(sdcard);
}

int sdcard_dev_open(struct sdcard_dev * sdcard, struct spi_dev * spi, int ss)
{
	unsigned char args[4];
	unsigned char sdcard_rsp;
	unsigned int  iter, ersp;

	sdcard->spi      = spi;
	sdcard->slave_id = ss;

	/* 
	* Supply SD card ramp up time (min 74 cycles)
	*/
	_sdcard_gen_tick(sdcard, 10);

	/* 
	* Assert slave select signal
	* Send CMD0 (Reset Command)
	* Deassert slave select signal
	*/
	_sdcard_enable(sdcard);

	args[0] = 0;
	args[1] = 0;
	args[2] = 0;
	args[3] = 0;
	sdcard_rsp = _sdcard_send_command(sdcard, 0, SDCARD_CMD, args, 0x4A);
	if ( sdcard_rsp != 0x01 )
	{
		reset_puts("card CMD0 failed ");
		return sdcard_rsp;
	}

	_sdcard_disable(sdcard);
	/*
	 * send CMD8. If card is pre-v2, It will reply with illegal command.
	 * Otherwise we announce sdhc support.
	 */
	_sdcard_enable(sdcard);
	args[0] = 0;
	args[1] = 0;
	args[2] = 0x01;
	args[3] = 0x01;
	sdcard_rsp = _sdcard_send_command(sdcard, 8, SDCARD_CMD, args, 0x63);
	if (!SDCARD_CHECK_R1_VALID(sdcard_rsp)) {
		reset_puts("card CMD8 failed ");
		return sdcard_rsp;
	}
	if (!SDCARD_CHECK_R1_ERROR(sdcard_rsp)) {
		/* no error, command accepted. get whole reply */
		ersp = _sdcard_receive_char(sdcard);
		ersp = (ersp << 8) | _sdcard_receive_char(sdcard);
		ersp = (ersp << 8) | _sdcard_receive_char(sdcard);
		ersp = (ersp << 8) | _sdcard_receive_char(sdcard);
		if ((ersp & 0xffff) != 0x0101) {
			/* voltage mismatch */
			reset_puts("card CMD8 mismatch: ");
			reset_putx(ersp);
			return sdcard_rsp;
		}
		reset_puts("v2 or later ");
		sdcard->sdhc = 1;
	} else if ((sdcard_rsp & SDCARD_R1_ILLEGAL_CMD) == 0) {
		/* other error */
		reset_puts("card CMD8 error ");
		return sdcard_rsp;
	} else {
		sdcard->sdhc = 0;
	}
	_sdcard_disable(sdcard);
	/* send CMD41, enabling the card */
	_sdcard_enable(sdcard);
	args[0] = sdcard->sdhc ? 0x40: 0;
	args[1] = 0;
	args[2] = 0;
	args[3] = 0;

	iter = 0;
	while( iter++ < SDCARD_COMMAND_TIMEOUT )
	{
		sdcard_rsp = _sdcard_send_command(sdcard, 41, SDCARD_ACMD, args, 0x00);
		if( sdcard_rsp == 0x01 )
		{
			continue;
		}

		break;
	}

	_sdcard_disable(sdcard);
	if (sdcard_rsp) {
		reset_puts("SD ACMD41 failed ");
		return sdcard_rsp;
	}
	if (sdcard->sdhc != 0) {
		/* get the card capacity to see if it's really HC */
		_sdcard_enable(sdcard);
		args[0] = sdcard->sdhc ? 0x40: 0;
		args[1] = 0;
		args[2] = 0;
		args[3] = 0;
		sdcard_rsp = _sdcard_send_command(sdcard, 58, SDCARD_CMD,
		    args, 0x00);
		if (sdcard_rsp) {
			reset_puts("SD CMD58 failed ");
			return sdcard_rsp;
		}
		ersp = _sdcard_receive_char(sdcard);
		ersp = (ersp << 8) | _sdcard_receive_char(sdcard);
		ersp = (ersp << 8) | _sdcard_receive_char(sdcard);
		ersp = (ersp << 8) | _sdcard_receive_char(sdcard);
		if (ersp & 0x40000000) {
			reset_puts("SDHC ");
		} else {
			sdcard->sdhc = 0;
		}
		_sdcard_disable(sdcard);
	}
	reset_puts("card detected ");
	return 0;
}

///////////////////////////////////////////////////////////////////////////////
int sdcard_dev_read(struct sdcard_dev * sdcard, void * buf, unsigned int count)
{
    unsigned char args[4];
    unsigned char sdcard_rsp;
    register int  i;

    for (i = 0; i < 4; i++)
    {
        args[i] = (sdcard->access_pointer >> (32 - (i+1)*8)) & 0xFF;
    }

    _sdcard_enable(sdcard);

    sdcard_rsp = _sdcard_send_command(sdcard, 17, SDCARD_CMD, args, 0x00);
    if ( SDCARD_CHECK_R1_ERROR(sdcard_rsp) )
    {
        _sdcard_disable(sdcard);
        return sdcard_rsp;
    }

    _sdcard_wait_data_block(sdcard);

    spi_get_data(sdcard->spi, buf, count);

    /*
     * Get the remainder of the block bytes and the CRC16 (comes
     * at the end of the data block)
     */
    i = count;
    while( i++ < (512 + 2) ) _sdcard_receive_char(sdcard);

    _sdcard_disable(sdcard);

    /*
     * Move the access pointer to the next block
     */
    sdcard->access_pointer += sdcard->block_length;

    return 0;
}

////////////////////////////////////////////////////////////////////////////////////////
unsigned int sdcard_dev_write(struct sdcard_dev *sdcard, void * buf, unsigned int count)
{
	return 0;
}

///////////////////////////////////////////////////////////////////////
void sdcard_dev_lseek(struct sdcard_dev * sdcard, unsigned int blk_pos)
{
    sdcard->access_pointer = sdcard->block_length * blk_pos;
}

/////////////////////////////////////////////////////////////////////////
int sdcard_dev_set_blocklen(struct sdcard_dev * sdcard, unsigned int len)
{
    unsigned char args[4];
    unsigned char sdcard_rsp;
    register int i;

    if (len != 512)
	return 1;

    if (sdcard->sdhc) {
	sdcard->block_length = 1;
	return 0;
    }

    for (i = 0; i < 4; i++)
        args[i] = (len >> (32 - (i+1)*8)) & 0xFF;

    _sdcard_enable(sdcard);

    sdcard_rsp = _sdcard_send_command(sdcard, 16, SDCARD_CMD, args, 0x00);
    if ( SDCARD_CHECK_R1_ERROR(sdcard_rsp) )
    {
        _sdcard_disable(sdcard);
        return sdcard_rsp;
    }

    _sdcard_disable(sdcard);

    sdcard->block_length = len;

	return 0;
}

////////////////////
int reset_spi_init()
{
    unsigned char sdcard_rsp;

    reset_puts("Initializing block device\n\r");

    /**
     * Initializing the SPI controller
     */
    spi_dev_config (
      _spi_device            ,
      spi_init_clkfreq       ,
      RESET_SYSTEM_CLK * 1000,
      8                      ,
      SPI_TX_NEGEDGE         ,
      SPI_RX_POSEDGE
    );

    /**
     * Initializing the SD Card
     */
    unsigned int iter = 0;
    while(1)
    {
        reset_puts("Trying to initialize SD card... ");

        sdcard_rsp = sdcard_dev_open(&_sdcard_device, _spi_device, 0);
        if (sdcard_rsp == 0)
        {
            reset_puts("OK\n");
            break;
        }

        reset_puts("KO\n");
        reset_sleep(1000);
        if (++iter >= sdcard_reset_retries)
        {
            reset_puts("\nERROR: During SD card reset to IDLE state\n"
                      "/ card response = ");
            reset_putx(sdcard_rsp);
            reset_puts("\n");
            reset_exit();
        }
    }

    /**
     * Set the block length of the SD Card
     */
    sdcard_rsp = sdcard_dev_set_blocklen(&_sdcard_device, 512);
    if (sdcard_rsp)
    {
        reset_puts("ERROR: During SD card blocklen initialization\n");
        reset_exit();
    }

    /**
     * Incrementing SDCARD clock frequency for normal function
     */
    spi_dev_config (
        _spi_device            ,
        spi_func_clkfreq       ,
        RESET_SYSTEM_CLK * 1000,
        -1                     ,
        -1                     ,
        -1
    );

    reset_puts("Finish block device initialization\n\r");

    return 0;
}  // end reset_spi_init() 

////////////////////////////////////////////////////////////////////////
int reset_spi_read( unsigned int lba, void* buffer, unsigned int count )
{
    unsigned int rsp;
    unsigned int i;

    sdcard_dev_lseek(&_sdcard_device, lba);

    for(i = 0; i < count; i++)
    {
        unsigned char* buf = (unsigned char *) buffer + (512 * i);

        if (( rsp = sdcard_dev_read ( &_sdcard_device, buf, 512 ) ))
        {
            reset_puts("ERROR in reset_spi_read() in SDCARD access / code = ");
            reset_putx( rsp );
            reset_puts("\n");
            return 1;
        }
    }
    return 0;
}  // end reset_spi_read()

/*
 * vim: tabstop=4 : softtabstop=4 : shiftwidth=4 : expandtab
 */
