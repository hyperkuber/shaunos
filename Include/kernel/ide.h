/*
 * ide.h
 *
 * Copyright (c) 2012 Shaun Yuan
 *
 */

#ifndef IDE_H_
#define IDE_H_


#include <kernel/types.h>
#include <kernel/io_dev.h>
#include <kernel/pci.h>

// Channels:
#define ATA_PRIMARY      0x00
#define ATA_SECONDARY    0x01

// Directions:
#define ATA_READ      0x00
#define ATA_WRITE     0x01


#define ATA_REG_DATA       0x00
#define ATA_REG_ERROR      0x01
#define ATA_REG_FEATURES   0x01
#define ATA_REG_SECCOUNT0  0x02
#define ATA_REG_LBA0       0x03
#define ATA_REG_LBA1       0x04
#define ATA_REG_LBA2       0x05
#define ATA_REG_HDDEVSEL   0x06
#define ATA_REG_COMMAND    0x07
#define ATA_REG_STATUS     0x07
#define ATA_REG_SECCOUNT1  0x08
#define ATA_REG_LBA3       0x09
#define ATA_REG_LBA4       0x0A
#define ATA_REG_LBA5       0x0B
#define ATA_REG_CONTROL    0x0C
#define ATA_REG_ALTSTATUS  0x0C
#define ATA_REG_DEVADDRESS 0x0D



#define ATA_SR_BSY     0x80
#define ATA_SR_DRDY    0x40
#define ATA_SR_DF      0x20
#define ATA_SR_DSC     0x10
#define ATA_SR_DRQ     0x08
#define ATA_SR_CORR    0x04
#define ATA_SR_IDX     0x02
#define ATA_SR_ERR     0x01

#define IDE_STATUS_DRIVE_BUSY		0x80
#define IDE_STATUS_DRIVE_READY		0x40


#define IDE_ATA        0x00
#define IDE_ATAPI      0x01

#define ATA_MASTER     0x00
#define ATA_SLAVE      0x01

#define ATA_FLAG_DMA	(1<<0)



#define ATA_CMD_READ_PIO          0x20
#define ATA_CMD_READ_PIO_EXT      0x24
#define ATA_CMD_READ_DMA          0xC8
#define ATA_CMD_READ_DMA_EXT      0x25
#define ATA_CMD_WRITE_PIO         0x30
#define ATA_CMD_WRITE_PIO_EXT     0x34
#define ATA_CMD_WRITE_DMA         0xCA
#define ATA_CMD_WRITE_DMA_EXT     0x35
#define ATA_CMD_CACHE_FLUSH       0xE7
#define ATA_CMD_CACHE_FLUSH_EXT   0xEA
#define ATA_CMD_PACKET            0xA0
#define ATA_CMD_IDENTIFY_PACKET   0xA1
#define ATA_CMD_IDENTIFY          0xEC


#define ATA_IDENT_DEVICETYPE   0
#define ATA_IDENT_CYLINDERS    2
#define ATA_IDENT_HEADS        6
#define ATA_IDENT_SECTORS      12
#define ATA_IDENT_SERIAL       20
#define ATA_IDENT_MODEL        54
#define ATA_IDENT_CAPABILITIES 98
#define ATA_IDENT_FIELDVALID   106
#define ATA_IDENT_MAX_LBA      120
#define ATA_IDENT_COMMANDSETS  164
#define ATA_IDENT_MAX_LBA_EXT  200


#define SEEK_START	0
#define SEEK_CURRENT	1
#define SEEK_END	2


void ide_initialize(void);

typedef enum __ide_dev_type {
	ATADEV_UNKNOWN = 0,
	ATADEV_PATA,
	ATADEV_SATA,
	ATADEV_SATAPI,
	ATADEV_PATAPI
} ATADEV_TYPE;

typedef struct _ata_params
{
	u32 base;
	void *controller;
	u32 drive;
	u32 locked;
	u32 currentblock;
	u32 blocksize;
	u32 flag;
	u32 cylinders;
	u32 heads;
	u32 sectors;
	u8 serial_number[20];
	u8 firmware_revision[8];
	u8 model[40];
	u32 lba28;
	u32 lba48;
	u64 blocks;
	u32 partition_block_offset;
} ata_t;


// struct _ata_port;
//
//typedef struct _ata_dev {
//	u8	driveno;
//	u8	version;
//	u16	flag;
//
//	u8	pio_mode;
//	u8	dma_mode;
//	u8 	udma_mode;
//
//	u8	pio_cap;
//	u8	dma_cap;
//	u8	udma_cap;
//
//	u32	iomem;
//	struct _ata_port *ap;
//	pci_dev_t *pci_dev;
//} ata_dev_t;



//struct _ata_port {
//	u32 flag;
//	u32 iomem;
//	u32 cap;
//	u32 ghc;
//	wait_queue_head_t wq;
//	ata_param_t *identify;
//};
//typedef struct _ata_port ata_port_t;









char *ata_strcpy(char *dst, char *src, int size);

#endif /* IDE_H_ */
