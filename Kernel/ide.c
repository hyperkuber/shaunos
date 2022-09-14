/*
 * ide.c
 *
 * Copyright (c) 2012 Shaun Yuan
 *
 */

#include <kernel/ide.h>
#include <asm/io.h>
#include <kernel/kernel.h>
#include <string.h>
#include <kernel/types.h>
#include <kernel/io_dev.h>


ata_t ata_drives[2];

struct ide_channel_registers {
   unsigned short base;  // I/O Base.
   unsigned short ctrl;  // Control Base
   unsigned short bmide; // Bus Master IDE
   unsigned char  nitr;  // nIEN (No Interrupt);
} channels[2];

struct ide_device {
   unsigned char  reserved;    // 0 (Empty) or 1 (This Drive really exists).
   unsigned char  channel;     // 0 (Primary Channel) or 1 (Secondary Channel).
   unsigned char  drive;       // 0 (Master Drive) or 1 (Slave Drive).
   unsigned short type;        // 0: ATA, 1:ATAPI.
   unsigned short signature;   // Drive Signature
   unsigned short capabilities;// Features.
   unsigned int   commandsets; // Command Sets Supported.
   unsigned int   size;        // Size in Sectors.
   unsigned char  model[40];   // Model in string.
} ide_devices[4];


char *ata_strcpy(char *dst, char *src, int size)
{
	int i;
	char tmp;

	for(i=0; i<size; i+=2){
		tmp = src[i];
		src[i] = src[i+1];
		src[i+1] = tmp;
	}

	for(i = size-1; i>=0; i--){
		if(src[i] != ' ')
			break;
	}
	memcpy(dst, src, i+1);
	dst[i+1] = 0;
	return dst;
}

 int ata_rw_block(ata_t *ata, void *buff, int mode, int sec_cnt)
{
	while (inb(ata->base + ATA_REG_STATUS) & IDE_STATUS_DRIVE_BUSY);


	/*
	 * Send 0xE0 for the "master" or 0xF0 for the "slave", ORed with the highest 4 bits of the LBA to port 0x1F6:
	 * outb(0x1F6, 0xE0 | (slavebit << 4) | ((LBA >> 24) & 0x0F))
	 * Send a NULL byte to port 0x1F1, if you like (it is ignored and wastes lots of CPU time): outb(0x1F1, 0x00)
	 * Send the sectorcount to port 0x1F2: outb(0x1F2, (unsigned char) count)
	 * Send the low 8 bits of the LBA to port 0x1F3: outb(0x1F3, (unsigned char) LBA))
	 * Send the next 8 bits of the LBA to port 0x1F4: outb(0x1F4, (unsigned char)(LBA >> 8))
	 * Send the next 8 bits of the LBA to port 0x1F5: outb(0x1F5, (unsigned char)(LBA >> 16))
	 * Send the "READ SECTORS" command (0x20) to port 0x1F7: outb(0x1F7, 0x20)
	 * Wait for an IRQ or poll.
	 * Transfer 256 words, a word at a time, into your buffer from I/O port 0x1F0.
	 * (In assembler, REP INSW works well for this.)
	 */
	if (ata->lba28){

		outb(ata->base + ATA_REG_HDDEVSEL, 0xE0 | (ata->drive << 4) | ((ata->currentblock >> 24) & 0x0F));
		outb(ata->base + ATA_REG_SECCOUNT0, sec_cnt);	//only read one sector every time
		outb(ata->base + ATA_REG_LBA0, (unsigned char)ata->currentblock);
		outb(ata->base + ATA_REG_LBA1, (unsigned char)(ata->currentblock >> 8));
		outb(ata->base + ATA_REG_LBA2, (unsigned char)(ata->currentblock >> 16));

		if (mode == ATA_READ){
			//send read command

			outb(ata->base + ATA_REG_COMMAND, ATA_CMD_READ_PIO);
			//poll
			while (inb(ata->base + ATA_REG_STATUS) & IDE_STATUS_DRIVE_BUSY);
			// test if drive is ok
			if (inb(ata->base + ATA_REG_STATUS) & ATA_SR_DRQ ){
				u16 *p = (u16 *)buff;
				int n = ata->blocksize * sec_cnt / 2;

				while (n--){
					*p++ = inw(ata->base + ATA_REG_DATA);
				}
			}else
				return -1;
		} else if (mode == ATA_WRITE){

			outb(ata->base + ATA_REG_COMMAND, ATA_CMD_WRITE_PIO);
			while (inb(ata->base + ATA_REG_STATUS) & IDE_STATUS_DRIVE_BUSY);

			if (inb(ata->base + ATA_REG_STATUS) & ATA_SR_DRQ ){

				u16 *p = (u16 *)buff;
				int n = ata->blocksize * sec_cnt / 2;
				while (n--){
					outw(ata->base + ATA_REG_DATA, *p++);
				}
			}else
				return -1;
		} else
			return -1;
	}else if (ata->lba48){
		//not supported
		return -1;
	}else{
		//chs mode, not supported
		return -1;
	}

	return 0;
}


static int ata_read_write(int device, char *buffer, size_t size, int mode)
{
	int blocks = 0;
	ata_t *ata = &ata_drives[DEV_MINOR(device)];

	if(size < ata->blocksize)
		size = ata->blocksize;

	// round up to next block
	if(size % ata->blocksize != 0)
		size = size + (size % ata->blocksize);

	// calculate number of blocks
	blocks = size / ata->blocksize;

	if (ata_rw_block(ata, (void *)buffer, mode, blocks) < 0)
		return -1;

	ata->currentblock += blocks;
	return blocks * ata->blocksize;

//	while(blocks-- > 0){
//
//		if(ata_rw_block(ata, (void *)buffer, mode) == -1)
//			break;
//
//		buffer += ata->blocksize;
//		bytesrw += ata->blocksize;
//		ata->currentblock++;
//	}
	//return bytesrw;
}

static int do_ata_seek(int device, int pos, int origin)
{
	ata_t *ata = &ata_drives[DEV_MINOR(device)];

	if (origin == SEEK_START)
		ata->currentblock = (pos / ata->blocksize) + ata->partition_block_offset;
	else if (origin == SEEK_CURRENT)
		ata->currentblock += pos / ata->blocksize;
	else
		return -1;

	return ata->currentblock * ata->blocksize;
}

int ata_read(dev_t *dev, void *buf, size_t size)
{
	return ata_read_write(dev->devid, (char *)buf, size, ATA_READ);
}

int ata_write(dev_t *dev, void *buf, size_t size)
{
	return ata_read_write(dev->devid, (char *)buf, size, ATA_WRITE);
}

int ata_seek(dev_t *dev, int pos, int whence)
{
	return do_ata_seek(dev->devid, pos, whence);
}

int ata_open(dev_t *dev)
{
	int minor = DEV_MINOR(dev->devid);
	if (ata_drives[minor].locked == 1)
		return -1;
	ata_drives[minor].locked = 1;
	return 0;

}

int ata_close(dev_t *dev)
{
	int minor = DEV_MINOR(dev->devid);
	ata_drives[minor].locked = 0;
	return 0;
}

int ata_do_request(struct _io_request *req)
{
	int minor = DEV_MINOR(req->dev->devid);
	ata_t *ata = &ata_drives[minor];
	int ret;

	if(req == NULL)
		return -EINVAL;

	if (!(req->flag & IO_TARGET_BLOCK))
		return -EINVAL;
	LOG_INFO("ata io request in");
	ata->locked = 1;
	ata_seek(req->dev, (req->_u.b_req.block_start * ata->blocksize), SEEK_START);

	ret = ata_read_write(req->dev->devid, req->buf, (req->_u.b_req.block_nums * ata->blocksize), req->io_req_type);
	if (ret < 0){
		LOG_INFO("req ret:%d", ret);
		req->io_state = IO_ERROR;
		req->error = ret;
		goto END;
	}

	req->io_state = IO_COMPLETED;
	req->error = ret;
END:
	ata->locked = 0;
	return ret;
}

static struct dev_ops ata_ops = {
	.open = ata_open,
	.read = ata_read,
	.write = ata_write,
	.close = ata_close,
	.io_request = ata_do_request,
};





void ide_initialize(void)
{

   int i, j, k, count = 0, nr=0;
   short ide_buf[256];
   // 1- Detect I/O Ports which interface IDE Controller:
   channels[ATA_PRIMARY  ].base  =  0x1F0;
   channels[ATA_PRIMARY  ].ctrl  =  0x3F4;
   channels[ATA_SECONDARY].base  =  0x170;
   channels[ATA_SECONDARY].ctrl  =  0x374;
   channels[ATA_PRIMARY  ].bmide =  0; // Bus Master IDE
   channels[ATA_SECONDARY].bmide =  8; // Bus Master IDE


   // 2- Disable IRQs:
   outb(channels[ATA_PRIMARY].ctrl + 2, 2);	//3f6
   outb(channels[ATA_SECONDARY].ctrl + 2, 2); //376

   while(inb(channels[ATA_PRIMARY].base + 7) & IDE_STATUS_DRIVE_BUSY)
	   ;


   for (i = 0; i < 2; i++){
	   for (j = 0; j < 2; j++){
	         unsigned char type = ATADEV_UNKNOWN, status, cl, ch ;
	         ide_devices[count].reserved = 0; // Assuming that no drive here.
	         memset(ide_buf, 0, 256);
	         //select drive
	         outb(channels[i].base + ATA_REG_HDDEVSEL,  (j == 0 ? 0xA0 : 0xB0));
	         //sleep for a while
	         for (k = 0; k < 1000; k++)	nop;

	         outb(channels[i].base + ATA_REG_COMMAND, ATA_CMD_IDENTIFY);

	         if (inb(channels[i].base + ATA_REG_COMMAND) == 0)
	        	 continue;		//device does not exist

	         while (inb(channels[i].base + ATA_REG_STATUS) & IDE_STATUS_DRIVE_BUSY);
	         cl = inb(channels[i].base + 4);
	         ch = inb(channels[i].base + 5);

	         if (cl == 0x14 && ch == 0xEB) type = ATADEV_PATAPI;
	         if (cl == 0x69 && ch == 0x96) type = ATADEV_SATAPI;
	         if ((cl == 0 || cl == 0xFF)  && ch == 0) 	  type = ATADEV_PATA;
	         if (cl == 0x3c && ch == 0xc3) type = ATADEV_SATA;

	         if (type == ATADEV_UNKNOWN){
	        	 LOG_INFO("atadev unknow type:%d, cl:%d, ch:%d\n", type, cl, ch);
	        	// continue;
	         }

	         if (type == ATADEV_PATAPI || type == ATADEV_SATAPI ){
	        	 outb(channels[i].base + ATA_REG_COMMAND, ATA_CMD_IDENTIFY_PACKET);

	        	 while (inb(channels[i].base + ATA_REG_STATUS) & IDE_STATUS_DRIVE_BUSY);
	         }

	         status = inb(channels[i].base + ATA_REG_STATUS);
	         if (status & ATA_SR_DRQ){

				 for (k = 0; k < 256; k++)
					 ide_buf[k] = inw(channels[i].base + ATA_REG_DATA);

				 ide_devices[count].reserved     = 1;
				 ide_devices[count].type         = type;
				 ide_devices[count].channel      = i;
				 ide_devices[count].drive        = j;
				 ide_devices[count].signature    = *((unsigned short *)(ide_buf + ATA_IDENT_DEVICETYPE));
				 ide_devices[count].capabilities = *((unsigned short *)(ide_buf + ATA_IDENT_CAPABILITIES / 2));
				 ide_devices[count].commandsets  = *((unsigned int *)(ide_buf + ATA_IDENT_COMMANDSETS / 2));

				 if (ide_devices[count].commandsets & (1 << 26))
					// Device uses 48-Bit Addressing:
					ide_devices[count].size   = *((unsigned int *)(ide_buf + ATA_IDENT_MAX_LBA_EXT / 2));
				 else
					// Device uses CHS or 28-bit Addressing:
					ide_devices[count].size   = *((unsigned int *)(ide_buf + ATA_IDENT_MAX_LBA / 2));

				 // no more details about ATAPI, FIXME:ATAPI
				 if (type == ATADEV_PATA){
					 ata_drives[nr].controller = (void *)&channels[i];
					 ata_drives[nr].cylinders = ide_buf[1];
					 ata_drives[nr].heads = ide_buf[3];
					 ata_drives[nr].sectors = ide_buf[6];
					 ata_drives[nr].base = channels[i].base;
					 ata_drives[nr].drive = j;
					 ata_strcpy((char *)ata_drives[nr].serial_number,(char *)(ide_buf + 10), 20);
					 ata_strcpy((char *)ata_drives[nr].firmware_revision, (char *)(ide_buf + 23), 8);
					 ata_strcpy((char *)ata_drives[nr].model, (char *)(ide_buf + 27), 40);
					 ata_drives[nr].serial_number[19] = 0;
					 ata_drives[nr].firmware_revision[7] = 0;
					 ata_drives[nr].model[39] = 0;
					 strcpy((char *)ide_devices[count].model, (char *)ata_drives[nr].model);

					 ata_drives[nr].lba28 = ((ide_buf[49] & 0x200) == 0x200);
					 ata_drives[nr].lba48 = ((ide_buf[83] & 0x200) == 0x200);

					 if (ide_buf[53] & 1){
						 ata_drives[nr].cylinders = ide_buf[54];
						 ata_drives[nr].heads = ide_buf[55];
						 ata_drives[nr].sectors = ide_buf[56];
					 }

					 if (ata_drives[nr].lba28)
						 ata_drives[nr].blocks = (ide_buf[61] << 16) + ide_buf[60];
					 else if (ata_drives[nr].lba48)
						 ata_drives[nr].blocks = (u64)(ide_buf[100]);
					 else
						 ata_drives[nr].blocks = ata_drives[nr].cylinders * ata_drives[nr].heads * ata_drives[nr].sectors;

					 ata_drives[nr].blocksize = 512;	//ide_buf[5]; it does not work in vmware

					 strcpy((char *)ide_devices[count].model, (char *)ata_drives[nr].model);

					 LOG_INFO("serial number:%s\nfirmware version:%s\nmodel_number:%s\n" \
							 "cylinders:%d\nheads:%d\nsectors:%d\n%s\nblock size:%d\nblocks:%d\n",
							 ata_drives[nr].serial_number, ata_drives[nr].firmware_revision, ata_drives[nr].model,
							 ata_drives[nr].cylinders, ata_drives[nr].heads, ata_drives[nr].sectors,
							 ata_drives[nr].lba48 ? "LBA48":(ata_drives[nr].lba28 ? "LBA28":"CHS"),ata_drives[nr].blocksize, ata_drives[nr].blocks );
				 } else {
					 // String indicates model of device (like Western Digital HDD and SONY DVD-RW...):
					 ata_strcpy((char *)ide_devices[count].model, (char *)(ide_buf + 27), 40);
					 ide_devices[count].model[39] = 0; // Terminate String.
				 }

	         }
	         nr++;
	         count++;
	   }

   }


   register_device("ata", &ata_ops, 0, NULL);
}
