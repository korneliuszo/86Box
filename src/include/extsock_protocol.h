/*
 * This file describes extsock device protocol
 */

#ifndef _EXTSOCK_PROTOCOL_H_
#define _EXTSOCK_PROTOCOL_H_

//first byte of message
enum {
	// Sent by device to start emulation
	// no arguments
	EXTSOCK_INTERFACES_ADDED = 0x00,

	// Sent by device to add memory mapping
	// args in format ">II"
	// first argument - base of mapping
	// second argument - size of mapping
	EXTSOCK_ADD_MEM_INTERFACE = 0x01,

	// Sent by CPU to do byte memory read
	// args in format ">I"
	// first argument - address to read
	// expected response - EXTSOCK_MEM_READB_RESPONSE
	EXTSOCK_MEM_READB_REQUEST = 0x02,

	// Sent by device providing byte to read
	// args in format ">B"
	// first argument - read byte
	EXTSOCK_MEM_READB_RESPONSE = 0x03,

	// Sent by CPU to do byte memory write
	// args in format ">IB"
	// first argument - address to read
	// second argument - byte to write
	// expected response - EXTSOCK_MEM_WRITEB_RESPONSE
	EXTSOCK_MEM_WRITEB_REQUEST = 0x04,

	// Sent by device acknowledging write
	// no arguments
	EXTSOCK_MEM_WRITEB_RESPONSE = 0x05,

	// Sent by device to add io mapping
	// args in format ">HH"
	// first argument - base of mapping
	// second argument - size of mapping
	EXTSOCK_ADD_IO_INTERFACE = 0x06,

	// Sent by CPU to do byte io read
	// args in format ">H"
	// first argument - address to read
	// expected response - EXTSOCK_MEM_READB_RESPONSE
	EXTSOCK_IO_READB_REQUEST = 0x07,

	// Sent by device providing byte to read
	// args in format ">B"
	// first argument - read byte
	EXTSOCK_IO_READB_RESPONSE = 0x08,

	// Sent by CPU to do byte io write
	// args in format ">IB"
	// first argument - address to read
	// second argument - byte to write
	// expected response - EXTSOCK_MEM_WRITEB_RESPONSE
	EXTSOCK_IO_WRITEB_REQUEST = 0x09,

	// Sent by device acknowledging write
	// no arguments
	EXTSOCK_IO_WRITEB_RESPONSE = 0x0A,

	// Sent by device to read DMA channel
	// args in format ">B"
	// first argument - dma channel to read
	EXTSOCK_DMA_READ_REQUEST = 0x0B,

	// Sent by CPU acknowledging DMA read
	// args in format ">BH"
	// first argument - status flags
	//  - bit0 - DMA channel not provided data
	//  - bit1 - last byte in transfer
	// second argument - data read
	EXTSOCK_DMA_READ_RESPONSE = 0x0C,

	// Sent by device to write DMA channel
	// args in format ">BH"
	// first argument - dma channel to read
	// second argument - data to write
	EXTSOCK_DMA_WRITE_REQUEST = 0x0D,

	// Sent by CPU acknowledging DMA write
	// args in format ">B"
	// first argument - status flags
	//  - bit0 - DMA channel not ready to accept data
	//  - bit1 - last byte in transfer
	EXTSOCK_DMA_WRITE_RESPONSE = 0x0E,

	// Sent by device to raise IRQ
	// args in format ">B"
	// first argument - irq number
	EXTSOCK_IRQ_RAISE_REQUEST = 0x0F,

	// Sent by CPU acknowledging IRQ raising
	// no arguments
	EXTSOCK_IRQ_RAISE_RESPONSE = 0x10,

	// Sent by device to raise IRQ
	// args in format ">B"
	// first argument - irq number
	EXTSOCK_IRQ_LOWER_REQUEST = 0x11,

	// Sent by CPU acknowledging IRQ raising
	// no arguments
	EXTSOCK_IRQ_LOWER_RESPONSE = 0x12,
};

#endif
