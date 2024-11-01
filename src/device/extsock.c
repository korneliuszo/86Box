/*
 * Authors: Korneliusz Osmenda
 */
#include <stdarg.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <wchar.h>
#include <assert.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <errno.h>
#define HAVE_STDARG_H
#include <86box/86box.h>
#include <86box/io.h>
#include <86box/mem.h>
#include <86box/dma.h>
#include <86box/device.h>
#include <86box/plat.h>
#include <86box/ui.h>
#include <86box/extsock.h>
#include <86box/thread.h>
#include <86box/timer.h>
#include <86box/pic.h>
#include <extsock_protocol.h>

static const device_config_t extsock_config[] = {
    { .name           = "path",
     .description    = "Socket path",
     .type           = CONFIG_FNAME,
     .default_string = "/tmp/86extsock",
     .file_filter = "Sockets |*"
    },
    { .type = CONFIG_END }
};

struct extsock_device
{
	int sock;
    thread_t * thread;
    pc_timer_t timer;
    volatile uint8_t rpacket[32];
    volatile size_t rlen;
    volatile uint8_t rtpacket[32];
    volatile size_t rtlen;
    volatile int stopping;
    event_t * syncer;
    event_t * syncer2;
    event_t * syncert;
    struct io_res_alloc
	{
		uint16_t base;
		uint16_t size;
	} io_regs[10];
	int io_used;
	mem_mapping_t mem_maps[10];
	int mem_used;
};

static uint8_t mem_readb(uint32_t addr, void *priv)
{
	struct extsock_device* epriv = (struct extsock_device*)priv;

	uint8_t buff[5];
	buff[0] = EXTSOCK_MEM_READB_REQUEST;
	buff[1] = addr >>24;
	buff[2] = addr >>16;
	buff[3] = addr >>8;
	buff[4] = addr >>0;

	send(epriv->sock,buff,5,0);
	thread_wait_event(epriv->syncer,-1);
	thread_reset_event(epriv->syncer);
	assert(epriv->rpacket[0]==EXTSOCK_MEM_READB_RESPONSE);
	uint8_t ret = epriv->rpacket[1];
	thread_set_event(epriv->syncer2);
	return ret;
}

static void mem_writeb(uint32_t addr, uint8_t val, void *priv)
{
	struct extsock_device* epriv = (struct extsock_device*)priv;

	uint8_t buff[6];
	buff[0] = EXTSOCK_MEM_WRITEB_REQUEST;
	buff[1] = addr >>24;
	buff[2] = addr >>16;
	buff[3] = addr >>8;
	buff[4] = addr >>0;
	buff[5] = val;

	send(epriv->sock,buff,6,0);
	thread_wait_event(epriv->syncer,-1);
	thread_reset_event(epriv->syncer);
	assert(epriv->rpacket[0]==EXTSOCK_MEM_WRITEB_RESPONSE);
	thread_set_event(epriv->syncer2);
	return;
}

static uint8_t io_readb(uint16_t addr, void *priv)
{
	struct extsock_device* epriv = (struct extsock_device*)priv;

	uint8_t buff[5];
	buff[0] = EXTSOCK_IO_READB_REQUEST;
	buff[1] = addr >>8;
	buff[2] = addr >>0;

	send(epriv->sock,buff,3,0);
	thread_wait_event(epriv->syncer,-1);
	thread_reset_event(epriv->syncer);
	assert(epriv->rpacket[0]==EXTSOCK_IO_READB_RESPONSE);
	uint8_t ret = epriv->rpacket[1];
	thread_set_event(epriv->syncer2);
	return ret;
}

static void io_writeb(uint16_t addr, uint8_t val, void *priv)
{
	struct extsock_device* epriv = (struct extsock_device*)priv;

	uint8_t buff[6];
	buff[0] = EXTSOCK_IO_WRITEB_REQUEST;
	buff[1] = addr >>8;
	buff[2] = addr >>0;
	buff[3] = val;

	send(epriv->sock,buff,4,0);
	thread_wait_event(epriv->syncer,-1);
	thread_reset_event(epriv->syncer);
	assert(epriv->rpacket[0]==EXTSOCK_IO_WRITEB_RESPONSE);
	thread_set_event(epriv->syncer2);
	return;
}

static void extsock_thread(void* priv)
{
	struct extsock_device* epriv = (struct extsock_device*)priv;

	struct timeval timeout;
	timeout.tv_sec = 1;
	timeout.tv_usec = 0;
	assert(setsockopt (epriv->sock, SOL_SOCKET, SO_RCVTIMEO, &timeout,sizeof timeout) >= 0);

	while(1) {
		if(epriv->stopping)
			break;
		uint8_t buff[32];
		int rlen = recv(epriv->sock,buff,32,0);
    	if((rlen == -1) && ((errno == EAGAIN) || (errno == EWOULDBLOCK)))
    		continue;
		assert(rlen>0);

		if(buff[0] == EXTSOCK_DMA_READ_REQUEST
		|| buff[0] == EXTSOCK_DMA_WRITE_REQUEST
		|| buff[0] == EXTSOCK_IRQ_RAISE_REQUEST
		|| buff[0] == EXTSOCK_IRQ_LOWER_REQUEST
		)
		{
			thread_wait_event(epriv->syncert,-1);
			thread_reset_event(epriv->syncert);
			memcpy((void*)epriv->rtpacket,buff,rlen);
			epriv->rtlen = rlen;
		}
		else
		{
			thread_wait_event(epriv->syncer2,-1);
			thread_reset_event(epriv->syncer2);
			memcpy((void*)epriv->rpacket,buff,rlen);
			epriv->rlen = rlen;
			thread_set_event(epriv->syncer);
		}
	};
}

static void extsock_timer_callback(void * priv)
{
	struct extsock_device* epriv = (struct extsock_device*)priv;

	//one tsc
	timer_advance_u64(&epriv->timer,0x100000000);

	if(!epriv->rtlen)
		return;

	if(epriv->rtpacket[0] == EXTSOCK_DMA_READ_REQUEST)
	{
		int rx = dma_channel_read(epriv->rtpacket[1]);
		uint8_t sbuff[4];
		sbuff[0] = EXTSOCK_DMA_READ_RESPONSE;
		sbuff[1] =
				((rx == DMA_NODATA) ? 0x01:0x00) |
				((rx & DMA_OVER) ? 0x02:0x00);
		sbuff[2] = rx>>8;
		sbuff[3] = rx;
		epriv->rtlen = 0;
		thread_set_event(epriv->syncert);
		send(epriv->sock,sbuff,4,0);
	}
	else if(epriv->rtpacket[0] == EXTSOCK_DMA_WRITE_REQUEST)
	{
		int rx = dma_channel_write(epriv->rtpacket[1],
				(epriv->rtpacket[2]<<8)|epriv->rtpacket[3]);
		uint8_t sbuff[2];
		sbuff[0] = EXTSOCK_DMA_WRITE_RESPONSE;
		sbuff[1] =
				((rx == DMA_NODATA) ? 0x01:0x00) |
				((rx & DMA_OVER) ? 0x02:0x00);
		epriv->rtlen = 0;
		thread_set_event(epriv->syncert);
		send(epriv->sock,sbuff,2,0);
	}
	else if(epriv->rtpacket[0] == EXTSOCK_IRQ_RAISE_REQUEST)
	{
		picint(1<<epriv->rtpacket[1]);
		uint8_t sbuff[1];
		sbuff[0] = EXTSOCK_IRQ_RAISE_RESPONSE;
		epriv->rtlen = 0;
		thread_set_event(epriv->syncert);
		send(epriv->sock,sbuff,1,0);
	}
	else if(epriv->rtpacket[0] == EXTSOCK_IRQ_LOWER_REQUEST)
	{
		picintc(1<<epriv->rtpacket[1]);
		uint8_t sbuff[1];
		sbuff[0] = EXTSOCK_IRQ_LOWER_RESPONSE;
		epriv->rtlen = 0;
		thread_set_event(epriv->syncert);
		send(epriv->sock,sbuff,1,0);
	}
	else {
		assert(0);
	}
}

static void *
extsock_init(UNUSED(const device_t *info))
{
    pclog("%s\n", info->name);

    struct extsock_device* priv = (struct extsock_device*)calloc(1,sizeof(struct extsock_device));


    struct sockaddr_un sockaddr_un = {0};
    int return_value;

    priv->sock = socket( AF_UNIX, SOCK_SEQPACKET, 0 );
    if ( priv->sock == -1 ) assert( 0 );

    /* Construct the client address structure. */
    sockaddr_un.sun_family = AF_UNIX;
    strcpy( sockaddr_un.sun_path, "/tmp/86extsock" );

    return_value =
       connect(
    		priv->sock,
            (struct sockaddr *) &sockaddr_un,
            sizeof( struct sockaddr_un ) );

    if ( return_value == -1 ) assert( 0 );

    while(1)
    {
    	uint8_t buff[32];
    	int len;
    	if((len = recv(priv->sock,buff,32,0))<1)
    		assert(0);
    	if(buff[0] == EXTSOCK_INTERFACES_ADDED)
    		break; // continue
    	else if(buff[0] == EXTSOCK_ADD_MEM_INTERFACE) // mem
    	{
    		assert(len == 9);
    		uint32_t base = (buff[1]<<24)|(buff[2]<<16)|(buff[3]<<8)|(buff[4]<<0);
    		uint32_t size = (buff[5]<<24)|(buff[6]<<16)|(buff[7]<<8)|(buff[8]<<0);
    		pclog("adding mem %x %x\n",base,size);
    		mem_mapping_add(&priv->mem_maps[priv->mem_used],base,size,
    				mem_readb,NULL,NULL,
					mem_writeb,NULL,NULL,
					NULL,MEM_MAPPING_ROM_WS|MEM_MAPPING_EXTERNAL,priv);
            priv->mem_used++;
    	}
    	else if(buff[0] == EXTSOCK_ADD_IO_INTERFACE) // io
    	{
    		assert(len == 5);
    		uint32_t base = (buff[1]<<8)|(buff[2]<<0);
    		uint32_t size = (buff[3]<<8)|(buff[4]<<0);
    		pclog("adding io %x %x\n",base,size);
    		io_sethandler(base,size,
    				io_readb,NULL,NULL,
					io_writeb,NULL,NULL,
					priv);
            priv->mem_used++;
    	}
    	else
    	{
    		assert(0);
    	}
    }

    timer_add(&priv->timer, extsock_timer_callback, priv, 0);
    timer_set_delay_u64(&priv->timer,0);

    priv->syncer = thread_create_event();
	thread_reset_event(priv->syncer);
    priv->syncer2 = thread_create_event();
	thread_set_event(priv->syncer2);
    priv->syncert = thread_create_event();
	thread_set_event(priv->syncert);
	priv->thread = thread_create(extsock_thread,priv);

    return priv;
}

static void
extsock_close(void *priv)
{
	struct extsock_device* epriv = (struct extsock_device*)priv;

	epriv->stopping=1;
	timer_disable(&epriv->timer);
	thread_wait(epriv->thread);
	thread_destroy_event(epriv->syncer);
	thread_destroy_event(epriv->syncer2);
	free(priv);
}

const device_t extsock_device = {
    .name          = "ISA extsock device",
    .internal_name = "extsock",
    .flags         = DEVICE_ISA,
    .local         = 0,
    .init          = extsock_init,
    .close         = extsock_close,
    .reset         = NULL,
    { .available = NULL },
    .speed_changed = NULL,
    .force_redraw  = NULL,
    .config        = extsock_config
};
