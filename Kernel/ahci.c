/*
 * ahci.c
 *
 *  Created on: May 11, 2013
 *      Author: Shaun Yuan
 */

#include <kernel/ahci.h>
#include <kernel/kernel.h>
#include <kernel/pci.h>
#include <kernel/page.h>
#include <kernel/cpu.h>
#include <kernel/ide.h>
#include <kernel/mm.h>
#include <kernel/idt.h>
#include <string.h>
#include <kernel/ext2.h>
#include <kernel/irq.h>
#include <kernel/malloc.h>
#include <kernel/io_dev.h>
#include <kernel/kthread.h>
#include <list.h>
#include <kernel/assert.h>
#include <kernel/io_dev.h>
#include <kernel/bitops.h>

//this is where system hdd stored
extern ata_t ata_drives[2];

/*
 * 0x0000-0x1000 - bios data
 * 0x1000-0x2000 - s3286 trampoline
 * 0x2000-0x4000 - vesa data
 * 0x4000-0x5000 - AHCI cmd list base addr
 */
#define AHCI_BASE	0x4000

static ahci_adapter_t *adapter;
s32 ahci_read_write(ahci_port_t *ap, u32 startl,
		u32 starth, u32 count, s8 *buf,
		s32 mode);

static s32 check_type(HBA_PORT *port)
{
	u32 ssts;
	u8 ipm;
	u8 det;

	s32 j = 0;
	while (j<100){
		//ksleep(10);
		ssts = iomem_readl((void *)port+HBA_PORT_SSTS);
		det = ssts & 0x0f;
		ipm = (ssts >> 8) & 0x0F;
		if (det == HBA_PORT_DET_PRESENT_COMMU_OK)
			break;
		j++;
	}
	if (det != HBA_PORT_DET_PRESENT_COMMU_OK )
		return AHCI_DEV_NULL;
	if (ipm != HBA_PORT_IPM_ACTIVE)
		return AHCI_DEV_NULL;

	switch (iomem_readl((void *)port+HBA_PORT_SIG)){
	case SATA_SIG_ATAPI:
		return AHCI_DEV_SATAPI;
	case SATA_SIG_SEMB:
		return AHCI_DEV_SEMB;
	case SATA_SIG_PM:
		return AHCI_DEV_PM;
	default:
		return AHCI_DEV_SATA;
	}
}

void port_rebase(HBA_PORT *port, s32 portno, s32 slots)
{
	//stop_cmd(port);	// Stop command engine

	// Command list offset: 1K*portno
	// Command list entry size = 32
	// Command list entry maxim count = 32
	// Command list maxim size = 32*32 = 1K per port
	port->clb = AHCI_BASE + (portno<<10);
	port->clbu = 0;	//do not support 64bit
	memset((void*)(port->clb), 0, 1024);

	// FIS offset: 32K+256*portno
	// FIS entry size = 256 bytes per port
	port->fb = AHCI_BASE + (32<<10) + (portno<<8);
	port->fbu = 0;
	memset((void*)(port->fb), 0, 256);

	// Command table offset: 40K + 8K*portno
	// Command table size = 256*32 = 8K per port
	HBA_CMD_HEADER *cmdheader = (HBA_CMD_HEADER*)(port->clb);
	s32 i;
	for (i=0; i<slots; i++){
		cmdheader[i].prdtl = 8;	// 8 prdt entries per command table
					// 256 bytes per command table, 64+16+48+16*8
		// Command table offset: 40K + 8K*portno + cmdheader_index*256
		cmdheader[i].ctba = AHCI_BASE + (40<<10) + (portno<<13) + (i<<8);
		cmdheader[i].ctbau = 0;
		memset((void*)cmdheader[i].ctba, 0, 256);
	}

	//start_cmd(port);	// Start command engine
}

static void stop_cmd(HBA_PORT *port)
{
	volatile u32 tmp;
	LOG_INFO("in");
	tmp = iomem_readl((void *)port+HBA_PORT_CMD);
	tmp &= (~HBA_PORT_CMD_ST);
	iomem_writel((void *)port+HBA_PORT_CMD, tmp);

	// Wait until FR (bit14), CR (bit15) are cleared
	do {
		tmp = iomem_readl((void *)port + HBA_PORT_CMD);
	} while (tmp & HBA_PORT_CMD_CR);

	do {
		tmp = iomem_readl((void *)port+HBA_PORT_CMD);
	} while (tmp & HBA_PORT_CMD_FR);

	tmp &= (~HBA_PORT_CMD_FRE);
	iomem_writel((void *)port+HBA_PORT_CMD, tmp);
	LOG_INFO("end");
}

static s32 start_cmd(HBA_PORT *port)
{
	LOG_INFO("in");
	u32 tmp;
//	iomem_writel((void *)port+HBA_PORT_SERR, 0xFFFFFFFF);
//	iomem_writel((void *)port+HBA_PORT_IS, 0xFFFFFFFF);

	tmp = iomem_readl((void *)port+HBA_PORT_CMD);

	// Set FRE (bit4) and ST (bit0)
	tmp |= (HBA_PORT_CMD_FRE | HBA_PORT_CMD_ST);
	iomem_writel((void *)port+HBA_PORT_CMD, tmp);

	LOG_INFO("end");
	return 0;
}


/*
 * based on Serial ATA AHCI 1.3 Specification
 */
static void probe_port(ahci_adapter_t *adapter)
{
	HBA_MEM *abar = adapter->abar;
	u32 pi = iomem_readl((void *)abar + HBA_MEM_PI);
	s32 i = 0;
	u32 tmp;
	s32 dt;
	s32 nr_ports = (iomem_readl((void *)abar + HBA_MEM_CAP) & HBA_MEM_CAP_NP_MASK);
	s32 slots = ((iomem_readl((void *)abar + HBA_MEM_CAP) & HBA_MEM_CAP_NCS_MASK) >> 8);
	LOG_INFO("number of ports:%d %d slots per port", nr_ports, slots);
	nr_ports++;
	adapter->port_impl = pi;
	while (i < nr_ports) {
		if (pi & 1) {
			HBA_PORT *port = &adapter->abar->ports[i];
			dt = check_type(port);
			if (dt == AHCI_DEV_SATA){
				LOG_INFO("SATA drive found at port %d", i);
				//ok, we found a port implemented, make sure the port is in idle state
				adapter->ports[i] = kzalloc(sizeof(ahci_port_t));
				adapter->ports[i]->adapter = adapter;
				adapter->ports[i]->port = port;
				adapter->ports[i]->type = AHCI_DEV_SATA;
				LOG_INFO("rebase port:%d", i);
				//set port command list base address and FIS base address
				port_rebase(port, i, slots);

				tmp = iomem_readl((void *)port + HBA_PORT_CMD);
				if (!(tmp & (HBA_PORT_CMD_ST | HBA_PORT_CMD_CR | HBA_PORT_CMD_FRE | HBA_PORT_CMD_FR))){
					LOG_INFO("port is not in idle state, forcing it to be");
					stop_cmd(port);
				}

				//det reset, disable slumber and Partial state
				//reset port, send COMRESET signal
				iomem_writel((void *)port+HBA_PORT_SCTL, 0x301);
				ksleep(5);
				iomem_writel((void *)port+HBA_PORT_SCTL, 0x300);

				if (iomem_readl((void *)abar+HBA_MEM_CAP) & HBA_MEM_CAP_SSS){
					iomem_writel((void *)port+HBA_PORT_CMD,
							iomem_readl((void *)port+HBA_PORT_CMD) | HBA_PORT_CMD_SUD | HBA_PORT_CMD_POD|HBA_PORT_CMD_ICC);
					ksleep(10);
				}
				//clear s32errupt status and error status register
				iomem_writel((void *)port+HBA_PORT_IS, 0xFFFFFFFF);
				iomem_writel((void *)port+HBA_PORT_SERR, 0xFFFFFFFF);
				//enable default s32errupt
				iomem_writel((void *)port+HBA_PORT_IE, HBA_PORT_IE_DEFAULT);
			}
			else if (dt == AHCI_DEV_SATAPI){
				LOG_INFO("SATAPI drive found at port %d", i);
				adapter->ports[i]->type = AHCI_DEV_SATAPI;
			}
			else if (dt == AHCI_DEV_SEMB){
				LOG_INFO("SEMB drive found at port %d", i);
			}
			else if (dt == AHCI_DEV_PM){
				LOG_INFO("PM drive found at port %d", i);
			}
			else {
				LOG_INFO("NO drive found at port %d", i);
			}

		}
		pi >>=1;
		i++;
	}
	adapter->nr_ports = nr_ports;
}

static s32 ahci_port_reset(HBA_PORT *port)
{
	u32 tmp;
	//LOG_INFO("in");
	stop_cmd(port);

	tmp = iomem_readl((void *)port+HBA_PORT_SERR);
	iomem_writel((void *)port+HBA_PORT_SERR, tmp);

	/*
	 * BSY or DRQ is really nasty,
	 * in linux, send s32erface COMRESET signal,
	 * where CLO(command list overflow) was token
	 * in FreeBSD,Spec says,sending CLO may cause hardware
	 * clear the BSY bit,
	 */

	tmp = iomem_readl((void *)port+HBA_PORT_TFD);
	if (tmp & (HBA_PORT_TFD_DRQ | HBA_PORT_TFD_BSY)){
		iomem_writel((void *)port+HBA_PORT_SCTL, 0x301);//comreset, disable p,s
		ksleep(1);
		iomem_writel((void *)port+HBA_PORT_SCTL, 0x300);
	}

	start_cmd(port);
	//LOG_INFO("end");
	return 0;
}

static void ahci_qc_complete(qc_t *qc)
{
	//call callback function
	qc->complete(qc);
}

static s32 ahci_port_intr(ahci_port_t *ap)
{
	//LOG_INFO("in");
	u32 tmp;
	HBA_PORT *port = ap->port;

	//ack s32errupts
	tmp = iomem_readl((void *)port+HBA_PORT_IS);
	iomem_writel((void *)port+HBA_PORT_IS, tmp);

	tmp = iomem_readl((void *)port+HBA_PORT_SERR);
	iomem_writel((void *)port+HBA_PORT_SERR, tmp);

	if (tmp & HBA_PORT_IS_ERR_DEF){
		ahci_port_reset(port);
		ap->sactive = 0;
		return -1;
	}

	//we should figure out in which slot the s32errupt happened
	//sact register  auto clear by controller,
	tmp = iomem_readl((void *)port+HBA_PORT_CI);
	//iomem_readl((void *)port+HBA_PORT_SACT);
	//get done work bit mask
	tmp ^= ap->sactive;

	//LOG_INFO("tmp:%x", tmp);
	while (tmp){
		s32 tag = find_first_bit(&tmp, 0);
		//LOG_INFO("tag:%x", tag);
		if (tag > 31 || tag < 0)
			break;
		qc_t *qc = &ap->cmd_queue[tag];
		ahci_qc_complete(qc);
		tmp &= (~(1<<tag));
		//clear the active slot flag
		ap->sactive &= (~(1<<tag));
		//iomem_writel((void *)port+HBA_PORT_SACT, ap->sactive);
	}
	//LOG_INFO("end");
	return 0;

}


s32 ahci_interrupt(s32 irq, void *data, trap_frame_t *regs)
{

	HBA_MEM *abar = adapter->abar;
	ahci_port_t *port;
	u32 tmp;
	u32 irq_ack = 0;
	s32 i;

	//read irq pending status register
	tmp = iomem_readl((void *)abar + HBA_MEM_IPS);
	tmp &= adapter->port_impl;
	if (!tmp)
		return 0;

	//LOG_INFO("s32errupt:%d", regs->vector);
	for (i=0; i<adapter->nr_ports; i++){
		port = adapter->ports[i];
		if ((tmp & (1<<i)) && port){
			ahci_port_intr(port);
			irq_ack |= (1<<i);
		}
	}
	//send ack
	iomem_writel((void *)abar+HBA_MEM_IPS, irq_ack);

	return 0;
}



void ahci_print_info(HBA_MEM *abar)
{
	u32 ver, cap, impl;
	ver = iomem_readl((void *)abar+0x10);
	cap = iomem_readl((void *)abar);
	impl = iomem_readl((void *)abar+0x0c);
	s8 *speed_s;
	s32 speed = (cap >> 20) & 0x0f;
	if (speed == 1)
		speed_s = "1.5";
	else if (speed == 2)
		speed_s = "3";
	else
		speed_s = "?";

	LOG_INFO("%x%x.%x%x %d slots %d ports %s gbps %x impl",
			(ver >>24) & 0xFF,
			(ver >> 16) & 0xFF,
			(ver >> 8) & 0xFF,
			ver & 0xFF,
			((cap >> 8) & 0x1f) +1,
			(cap & 0x1f) +1,
			speed_s,
			impl);


}


static s32 ahci_reset(ahci_adapter_t *adapter)
{
	volatile u32 tmp;
	HBA_MEM *abar = adapter->abar;
	//enable ahci
	tmp = iomem_readl((void *)abar + HBA_MEM_GHC);
	if (!(tmp & HBA_MEM_GHC_AE)){
		tmp |= HBA_MEM_GHC_AE;
		iomem_writel((void *)abar+HBA_MEM_GHC, tmp);
	}
	//send reset command
	tmp = iomem_readl(((void *)abar + HBA_MEM_GHC));
	if (!(tmp & HBA_MEM_GHC_HR)){
		tmp |= HBA_MEM_GHC_HR;
		iomem_writel((void *)abar + HBA_MEM_GHC, tmp);
		//sleep 1s
		ksleep(1000);
	}

	tmp = iomem_readl((void *)abar+HBA_MEM_GHC);
	if (tmp & HBA_MEM_GHC_HR){
		LOG_INFO("reset ahci controller timeout\n");
		return -EIO;
	}

	//reenable ahci mode
	tmp = iomem_readl((void *)abar+HBA_MEM_GHC);
	if (!(tmp & HBA_MEM_GHC_AE)){
		tmp |= HBA_MEM_GHC_AE;
		iomem_writel((void *)abar+HBA_MEM_GHC, tmp);
	}

	//clear pending s32errupt
	//iomem_writel((void *)abar+HBA_MEM_IPS, 0);

	//enable s32errupt
	iomem_writel((void *)abar+HBA_MEM_GHC,
			iomem_readl((void *)abar+HBA_MEM_GHC) | HBA_MEM_GHC_IE);

	return 0;
}

static s32 sata_rw_block(ata_t *ata, void *buff, s32 mode, s32 sec_cnt)
{
	ahci_port_t *ap;
	ahci_adapter_t *adapter = (ahci_adapter_t *)ata->controller;
	ap = adapter->ports[ata->drive];
	s32 retval;
	//LOG_INFO("in");
	if (ata->flag & ATA_FLAG_DMA){
		if (ata->lba48 || ata->lba28){
			if (sec_cnt > 128){
				//do not support read 128 sectors one time
				retval = -ENOSYS;
			} else {
				retval = ahci_read_write(ap, ata->currentblock, 0, sec_cnt, (s8 *)PA(buff), mode);
				goto END;
			}
		}
		retval = -ENOSYS;
	}
END:
	//LOG_INFO("end");
	return retval;
}



static s32 sata_read_write(s32 device, s8 *buffer, size_t size, s32 mode)
{
	s32 blocks = 0;
	ata_t *ata = &ata_drives[DEV_MINOR(device)];
	//LOG_INFO("in");
	if(size < ata->blocksize)
		size = ata->blocksize;

	// round up to next block
	if(size % ata->blocksize != 0)
		size = size + (size % ata->blocksize);

	// calculate number of blocks
	blocks = size / ata->blocksize;

	if (sata_rw_block(ata, (void *)buffer, mode, blocks) < 0)
		return -1;

	ata->currentblock += blocks;
	//LOG_INFO("end");
	return blocks * ata->blocksize;
}


static s32 do_sata_seek(s32 device, s32 pos, s32 origin)
{
	ata_t *ata = &ata_drives[DEV_MINOR(device)];

	if (origin == SEEK_START)
		ata->currentblock = (pos / ata->blocksize + ata->partition_block_offset);
	else if (origin == SEEK_CURRENT)
		ata->currentblock += pos / ata->blocksize;
	else
		return -1;

	return ata->currentblock * ata->blocksize;
}

s32 sata_read(dev_t *dev, void *buf, size_t size)
{
	return sata_read_write(dev->devid, (s8 *)buf, size, ATA_READ);
}

s32 sata_write(dev_t *dev, void *buf, size_t size)
{
	return sata_read_write(dev->devid, (s8 *)buf, size, ATA_WRITE);
}

s32 sata_seek(dev_t *dev, s32 pos, s32 whence)
{
	return do_sata_seek(dev->devid, pos, whence);
}


s32 sata_open(dev_t *dev)
{
	s32 minor = DEV_MINOR(dev->devid);
	if (ata_drives[minor].locked == 1)
		return -1;
	ata_drives[minor].locked = 1;
	return 0;

}

s32 sata_close(dev_t *dev)
{
	s32 minor = DEV_MINOR(dev->devid);
	ata_drives[minor].locked = 0;
	return 0;
}



s32 sata_do_request(struct _io_request *req)
{
	s32 minor = DEV_MINOR(req->dev->devid);
	ata_t *ata = &ata_drives[minor];
	s32 ret;

	if(req == NULL)
		return -EINVAL;

	if (!(req->flag & IO_TARGET_BLOCK))
		return -EINVAL;
	//LOG_INFO("sata io request in");
	ata->locked = 1;
	sata_seek(req->dev, (req->_u.b_req.block_start * ata->blocksize), SEEK_START);

	ret = sata_read_write(req->dev->devid, req->buf, (req->_u.b_req.block_nums * ata->blocksize), req->io_req_type);
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
	.open = sata_open,
	.read = sata_read,
	.write = sata_write,
	.close = sata_close,
	.io_request = sata_do_request,
};

// Find a free command list slot
static s32 find_cmdslot(ahci_port_t *ap)
{
	//LOG_INFO("in");
	u32 slots;
	s32 i;
	HBA_PORT *port = ap->port;
	//slots = iomem_readl((void *)port+HBA_PORT_SACT);
	slots = ap->sactive;
	slots |= iomem_readl((void *)port+HBA_PORT_CI);

	for (i=0; i<32; i++){
		if ((slots & 1) == 0)
			return i;
		slots >>= 1;
	}
	LOG_INFO("Cannot find free command list entry\n");
	return -1;
}

static void
ahci_setup_cmdfis(ahci_port_t * ap, s32 slot, u32 low, u32 high, u32 count, s8 cmd)
{
	HBA_PORT *port = ap->port;
	HBA_CMD_HEADER *hdr = (HBA_CMD_HEADER *)port->clb;
	hdr += slot;
	HBA_CMD_TBL *cmdtbl = (HBA_CMD_TBL *)hdr->ctba;
	FIS_REG_H2D *cmdfis = (FIS_REG_H2D*)(&cmdtbl->cfis);
	cmdfis->fis_type = FIS_TYPE_REG_H2D;
	cmdfis->c = 1;	// Command
	cmdfis->command = cmd;	//only support lba48
	cmdfis->featureh = 0x01;

	cmdfis->lba0 = (u8)low;
	cmdfis->lba1 = (u8)(low>>8);
	cmdfis->lba2 = (u8)(low>>16);
	cmdfis->device = (1<<6);	// LBA mode

	cmdfis->lba3 = (u8)(low>>24);
	cmdfis->lba4 = (u8)high;
	cmdfis->lba5 = (u8)(high>>8);

	cmdfis->countl = (count & 0xFF);
	cmdfis->counth = (count & 0xFF00) >> 8;

	cmdfis->control = 0x08;
}

static s32 ahci_port_is_ready(HBA_PORT *port)
{
	//first check bsy and drq
	u32 tmp;
	tmp = iomem_readl((void *)port+HBA_PORT_TFD);
	if (tmp & (HBA_PORT_TFD_DRQ | HBA_PORT_TFD_BSY))
		return -1;

	//check whether port is running or not
	tmp = iomem_readl((void *)port+HBA_PORT_CMD);
	if (!(tmp & HBA_PORT_CMD_ST))
		return -1;
	return 0;
}


static s32 ahci_issue_cmd(HBA_PORT *port, s32 slot)
{
	//LOG_INFO("in");
	if (ahci_port_is_ready(port) < 0){
		LOG_INFO("port is not ready");
		ahci_port_reset(port);
	}
	//set sact register
	iomem_writel((void *)port + HBA_PORT_SACT,
			iomem_readl((void *)port+HBA_PORT_SACT) | (1<<slot));

	// Issue command
	iomem_writel((void *)port + HBA_PORT_CI, (1<<slot));
	//LOG_INFO("end");
	return 0;

}
//this is the s32errupt handler call back function
static void ahci_common_complete(qc_t *qc)
{
	//LOG_INFO("in");
	wait_queue_head_t *wq = (wait_queue_head_t *)qc->private_data;
	wakeup_waiters(wq);
	//LOG_INFO("end");
}

static void
ahci_setup_slot(ahci_port_t *ap, s32 slot, u32 count, s8 *buf, s32 mode)
{
	HBA_PORT *port = ap->port;
	HBA_CMD_HEADER *cmdhdr = (HBA_CMD_HEADER *)port->clb;
	cmdhdr += slot;

	cmdhdr->cfl = sizeof(FIS_REG_H2D) / sizeof(u32);
	cmdhdr->w = ((mode == ATA_READ) ? 0 : 1);
	cmdhdr->prdtl = (u16) ((count-1)>>4) + 1;

	HBA_CMD_TBL *cmdtbl = (HBA_CMD_TBL*)(cmdhdr->ctba);
	memset(cmdtbl, 0, sizeof(HBA_CMD_TBL) +
 		(cmdhdr->prdtl-1)*sizeof(HBA_PRDT_ENTRY));

	// 8K bytes (16 sectors) per PRDT
	s32 i;
	for (i=0; i<cmdhdr->prdtl-1; i++){
		cmdtbl->prdt_entry[i].dba = (u32)buf;
		cmdtbl->prdt_entry[i].dbc = (8*1024)-1;	// 8K bytes
		cmdtbl->prdt_entry[i].i = 0;
		buf += 8*1024;	// 8K bytes
		count -= 16;	// 16 sectors
	}
	// Last entry
	cmdtbl->prdt_entry[i].dba = (u32)buf;
	cmdtbl->prdt_entry[i].dbc = (count<<9)-1;	// 512 bytes per sector
	cmdtbl->prdt_entry[i].i = 0;
}


s32 ahci_read_write(ahci_port_t *ap, u32 startl, u32 starth, u32 count, s8 *buf,
		s32 mode)
{
	HBA_PORT *port = ap->port;
	wait_queue_head_t wq;
	s32 x;
	s32 slot = find_cmdslot(ap);
	if (slot == -1)
		return -1;
	//LOG_INFO("find free slot:%d", slot);
	list_init(&wq.waiter);
	ap->cmd_queue[slot].ap = ap;
	ap->cmd_queue[slot].tag = slot;
	ap->cmd_queue[slot].private_data = (void *)&wq;
	ap->cmd_queue[slot].complete = ahci_common_complete;
	ap->curr_slot = slot;
	ap->sactive |= (1<<slot);

	// Setup command
	if (mode == ATA_READ){
		ahci_setup_slot(ap, slot, count, buf, ATA_READ);
		ahci_setup_cmdfis(ap, slot, startl, starth, count, ATA_CMD_READ_DMA_EXT);
	} else if (mode == ATA_WRITE){
		ahci_setup_slot(ap, slot, count, buf, ATA_WRITE);
		ahci_setup_cmdfis(ap, slot, startl, starth, count,  ATA_CMD_WRITE_DMA_EXT);
	}
	//this is necessary,
	//we should close the s32erruption before we issue a command,
	//or you will get a shit
	local_irq_save(&x);
	ahci_issue_cmd(port, slot);
	//we should define an event or something like completion in linux
	pt_wait_on(&wq);
	local_irq_restore(x);
	//LOG_INFO("end");
	return 0;
}


s32 ahci_identify(ahci_adapter_t *adapter)
{
	s32 nr = ATA_MASTER;
	s16 *aligned_buf;
	s32 slot = 0;
	s16 *ide_buf =(s16 *)kmalloc(512);
	assert(ide_buf != NULL);
	if ((u32)ide_buf & 0x01)	{
		LOG_INFO("not word aligned.");
		kfree(ide_buf);
		ide_buf = kmalloc(514);
		if ((u32)ide_buf & 0x01)
			aligned_buf =(s16 *) (((s8 *)ide_buf)+1);
	} else
		aligned_buf = ide_buf;

	/*
	 * in theory, we should identify
	 * each port implemented on adapter,
	 * here, i assume the hdd is connected
	 * to the port 0 on adapter
	 */
	HBA_PORT *port = &adapter->abar->ports[ATA_MASTER];


	HBA_CMD_HEADER *cmdheader = (HBA_CMD_HEADER*)port->clb;
	cmdheader += slot;
	cmdheader->cfl = sizeof(FIS_REG_H2D)/sizeof(u32);	// Command FIS size
	cmdheader->w = 0;		// Read from device
	cmdheader->prdtl = 1;	// PRDT entries count
	cmdheader->p = 1;

	HBA_CMD_TBL *cmdtbl = (HBA_CMD_TBL*)(cmdheader->ctba);
	memset(cmdtbl, 0, sizeof(HBA_CMD_TBL) +
	 		(cmdheader->prdtl)*sizeof(HBA_PRDT_ENTRY));

	cmdtbl->prdt_entry[0].dba = PA((u32)aligned_buf);
	//BUGFIX: dbc must be size-1, in vmware
	cmdtbl->prdt_entry[0].dbc = (1<<9)-1;
	cmdtbl->prdt_entry[0].i = 0;

	FIS_REG_H2D *cmdfis = (FIS_REG_H2D*)(&cmdtbl->cfis);
	cmdfis->fis_type = FIS_TYPE_REG_H2D;
	cmdfis->c = 1;
	cmdfis->command = ATA_CMD_IDENTIFY;
	cmdfis->device = (1<<6);
	cmdfis->control = 0x08;
	cmdfis->featurel = 0x01;
	cmdfis->countl = 1;

	ahci_issue_cmd(port, slot);

	ksleep(200);

	ata_drives[nr].controller = (void *)adapter;
	ata_drives[nr].cylinders = ide_buf[1];
	ata_drives[nr].heads = ide_buf[3];
	ata_drives[nr].sectors = ide_buf[6];
	ata_drives[nr].base = (u32)port;
	ata_drives[nr].drive = ATA_MASTER;
	ata_strcpy((s8 *)ata_drives[nr].serial_number,(s8 *)(ide_buf + 10), 20);
	ata_strcpy((s8 *)ata_drives[nr].firmware_revision, (s8 *)(ide_buf + 23), 8);
	ata_strcpy((s8 *)ata_drives[nr].model, (s8 *)(ide_buf + 27), 40);
	ata_drives[nr].serial_number[19] = 0;
	ata_drives[nr].firmware_revision[7] = 0;
	ata_drives[nr].model[39] = 0;

	ata_drives[nr].lba28 = ((ide_buf[49] & 0x200) == 0x200);
	ata_drives[nr].lba48 = ((ide_buf[83] & 0x400) == 0x400);
	if (ide_buf[49] & 0x100){
		ata_drives[nr].flag |= ATA_FLAG_DMA;
	}

	if (ata_drives[nr].lba28)
		ata_drives[nr].blocks = (ide_buf[61] << 16) + ide_buf[60];
	else if (ata_drives[nr].lba48)
		ata_drives[nr].blocks = (u64)(ide_buf[100]);


	ata_drives[nr].blocksize = 512;	//ide_buf[5]; it does not work in vmware
	ata_drives[nr].partition_block_offset = 0;
	LOG_INFO("serial number:%s\nfirmware version:%s\nmodel_number:%s\n" \
		 "cylinders:%d\nheads:%d\nsectors:%d\n%s\nblock size:%d\nblocks:%d\n",
		 ata_drives[nr].serial_number, ata_drives[nr].firmware_revision, ata_drives[nr].model,
		 ata_drives[nr].cylinders, ata_drives[nr].heads, ata_drives[nr].sectors,
		 ata_drives[nr].lba48 ? "LBA48":(ata_drives[nr].lba28 ? "LBA28":"CHS"),ata_drives[nr].blocksize, ata_drives[nr].blocks );

	kfree(ide_buf);

	return 0;

}

s32 ahci_init()
{
	//HBA_MEM *abar;
	pci_dev_t *dev;
	u32 start, end;
	dev = pci_lookup_dev(0x01, 0x06);
	if (dev == NULL){
		LOG_INFO("can not find ahci controller");
		return -ENODEV;
	}
	LOG_INFO("ahci controller found, vendor id:%x dev id:%x irq:%d", dev->vendor, dev->device, dev->irq);

	if (!(dev->resource[PCI_AHCI_BAR].flag & RESOURCE_TYPE_MEM)){
		LOG_INFO("ahci error, no iomem");
		return -EIO;
	}


	adapter = kmalloc(sizeof(ahci_adapter_t));
	assert(adapter != NULL);

	start = dev->resource[PCI_AHCI_BAR].start;
	end = dev->resource[PCI_AHCI_BAR].end;
	LOG_INFO("base addr:%x end:%x", start, end);
	//map iomem
	map_page(start, start, get_cr3(), PAGE_PRE | PAGE_RW, NULL);
	adapter->base_address = start;
	//enable pci memory access
	pci_dev_enable_dev(dev);
	pci_dev_set_bus_master(dev);
	adapter->abar = (HBA_MEM *)start;

	if (ahci_reset(adapter) < 0){
		LOG_INFO("reset controller failed");
		//unmap page
		return -EIO;
	}

	//probe port available
	probe_port(adapter);

	ahci_print_info(adapter->abar);
	//enable s32errupt in pci configuration space
	pci_dev_enable_int(dev);

	adapter->irq = dev->irq;

	LOG_INFO("register irq %d for ahci", dev->irq);
	register_irq(dev->irq, (u32)ahci_interrupt, (void *)adapter, IRQ_SHARED);
	enable_irq(dev->irq);

	ahci_identify(adapter);

	register_device("sata", &ata_ops, MKDEVID(3,0), (void *)adapter);
	return 0;
}

