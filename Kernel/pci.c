/*
 * pci.c
 *
 *  Created on: Feb 27, 2013
 *      Author: Shaun Yuan
 *      Copyright (c) 2013 Shaun Yuan
 */

#include <kernel/kernel.h>
#include <asm/io.h>
#include <list.h>
#include <kernel/pci.h>
#include <kernel/malloc.h>
#include <kernel/assert.h>
#include <string.h>


#define PCI_ADDR	0xCF8
#define PCI_DATA	0xCFC

struct linked_list pci_root_list;
//we only support configuration Mechanism One

#define PCI_ADDRESS(b,d,f,r)	\
	(0x80000000UL | (b << 16) | (d << 11) | (f << 8) | (r & ~3))




int pci_read_config_b(int bus, int dev, int func, int reg, unsigned char *value)
{
	if (bus > 255 || bus < 0 || dev > 32 || dev < 0 || func > 8 || func < 0
			|| reg > 255 || reg < 0 || value == NULL)
		return -1;
	int x;
	local_irq_save(&x);

	outl(PCI_ADDR, PCI_ADDRESS(bus, dev, func, reg));

	*value = inb(PCI_DATA + (reg & 3));

	local_irq_restore(x);
	return 0;
}

int pci_read_config_w(int bus, int dev, int func, int reg, unsigned short *value)
{
	if (bus > 255 || bus < 0 || dev > 32 || dev < 0 || func > 8 || func < 0
			|| reg > 255 || reg < 0 || value == NULL)
		return -1;
	int x;
	local_irq_save(&x);

	outl(PCI_ADDR, PCI_ADDRESS(bus, dev, func, reg));

	*value = inw(PCI_DATA + (reg & 2));

	local_irq_restore(x);
	return 0;
}

int pci_read_config_l(int bus, int dev, int func, int reg, unsigned long *value)
{
	if (bus > 255 || bus < 0 || dev > 32 || dev < 0 || func > 8 || func < 0
			|| reg > 255 || reg < 0 || value == NULL)
		return -1;
	int x;
	local_irq_save(&x);

	outl(PCI_ADDR, PCI_ADDRESS(bus, dev, func, reg));

	*value = inl(PCI_DATA);

	local_irq_restore(x);
	return 0;
}

int pci_write_config_b(int bus, int dev, int func, int reg, unsigned long value)
{
	if (bus > 255 || bus < 0 || dev > 32 || dev < 0 || func > 8 || func < 0
			|| reg > 255 || reg < 0)
		return -1;
	int x;
	local_irq_save(&x);

	outl(PCI_ADDR, PCI_ADDRESS(bus, dev, func, reg));

	outb(PCI_DATA + (reg & 3), (unsigned char)value);

	local_irq_restore(x);
	return 0;
}

int pci_write_config_w(int bus, int dev, int func, int reg, unsigned int value)
{
	if (bus > 255 || bus < 0 || dev > 32 || dev < 0 || func > 8 || func < 0
			|| reg > 255 || reg < 0)
		return -1;
	int x;
	local_irq_save(&x);

	outl(PCI_ADDR, PCI_ADDRESS(bus, dev, func, reg));

	outw(PCI_DATA + (reg & 2), (unsigned short)value);

	local_irq_restore(x);
	return 0;
}

int pci_write_config_l(int bus, int dev, int func, int reg, unsigned int value)
{
	if (bus > 255 || bus < 0 || dev > 32 || dev < 0 || func > 8 || func < 0
			|| reg > 255 || reg < 0)
		return -1;
	int x;
	local_irq_save(&x);

	outl(PCI_ADDR, PCI_ADDRESS(bus, dev, func, reg));

	outl(PCI_DATA, value);

	local_irq_restore(x);
	return 0;
}

unsigned long pci_size(unsigned long base, unsigned long maxbase, unsigned long mask)
{
	unsigned long size = maxbase & mask;
	if (!size)
		return 0;
	size = (size & ~(size-1)) - 1;

	if (base == maxbase && ((base | size) & mask) != mask)
		return 0;

	return size;

}

int pci_device_read_bases(pci_dev_t *dev, int count, int rom)
{
	unsigned  long sz, l;
	int i;
	struct resource *res;

	memset(dev->resource, 0, sizeof(dev->resource));

	for (i=0; i<count; i++){
		unsigned  long reg = PCI_BASE_ADDR0 + (i<<2);
		res = &dev->resource[i];
		pci_read_config_l(dev->bus, dev->dev, dev->func, reg, &l);
		pci_write_config_l(dev->bus, dev->dev, dev->func, reg, (~0UL));
		pci_read_config_l(dev->bus, dev->dev, dev->func, reg, &sz);
		pci_write_config_l(dev->bus, dev->dev, dev->func, reg, l);


		if (!sz || sz == 0xFFFFFFFF)
			continue;
		if (l == 0xFFFFFFFF)
			l = 0;

		if ((l & 0x01) == 0x00){
			//memory address
			sz = pci_size(l, sz, (~0x0FUL));
			if (!sz)
				continue;

			res->start = l & (~0x0FUL);
			res->flag |= RESOURCE_TYPE_MEM;
			if (l & 0x08)
				res->flag |= RESOURCE_TYPE_PREFETCH;

			switch(l & 0x06){
			case 0x00:	//32bit wide address
				break;
			case 0x01:	//64bit wide address
			case 0x02:	//16bit wide address
				LOG_ERROR("unsupported address wide");
				break;
			}

		} else if ((l & 0x01) == 0x01){
			//io address
			sz = pci_size(l, sz, (~0x03UL));
			if (!sz)
				continue;

			res->start = l & (~0x03UL);
			res->flag |= RESOURCE_TYPE_IO;

		} else
			continue;

		res->end = res->start + (unsigned long)sz;
	}

	if (rom){
		res = &dev->resource[PCI_ROM_RESOURCE];
		pci_read_config_l(dev->bus, dev->dev, dev->func, rom, &l);
		pci_write_config_l(dev->bus, dev->dev, dev->func, rom, ~0x01);
		pci_read_config_l(dev->bus, dev->dev, dev->func, rom, &sz);
		pci_write_config_l(dev->bus, dev->dev, dev->func, rom, l);

		if (sz && sz != 0xFFFFFFFF){
			sz = pci_size(l, sz, (~0x7FFUL));
			if (sz){
				res->start = l & (~0x7FFUL);
				res->end = res->start + (unsigned long)sz;
				res->flag |= RESOURCE_TYPE_MEM | RESOURCE_TYPE_READONLY;
			}
		}

	}


	return 0;

}

int
pci_bridge_read_bases(pci_dev_t *dev)
{

	struct resource *res;
	unsigned char io_base_low = 0, io_limit_low = 0;
	unsigned short mem_base_low = 0, mem_limit_low = 0;
	unsigned long base, limit;


	res = &dev->resource[PCI_BRIDGE_RESOURCE];
	pci_read_config_b(dev->bus, dev->dev, dev->func, PCI_IO_BASE_LOW, &io_base_low);
	pci_read_config_b(dev->bus, dev->dev, dev->func, PCI_IO_LIMIT_LOW, &io_limit_low);

	base = (io_base_low & ~0x0F) << 8;
	limit = (io_limit_low & ~ 0x0F) << 8;

	if ((io_base_low & 0x0F) == 0x01){
		unsigned short io_base_hi, io_limit_hi;
		pci_read_config_w(dev->bus, dev->dev, dev->func, PCI_IO_BASE_UPPER, &io_base_hi);
		pci_read_config_w(dev->bus, dev->dev, dev->func, PCI_IO_LIMIT_UPPER, &io_limit_hi);
		base |= (io_base_hi << 16);
		limit |= (io_limit_hi << 16);
	}

	if (base <= limit){
		res->start = base;
		res->end = limit + 0xFFF;
		res->flag |= RESOURCE_TYPE_IO;
	}

	res = &dev->resource[PCI_BRIDGE_RESOURCE+1];
	pci_read_config_w(dev->bus, dev->dev, dev->func, PCI_MEM_BASE, &mem_base_low);
	pci_read_config_w(dev->bus, dev->dev, dev->func, PCI_MEM_LIMIT, &mem_limit_low);

	base = (mem_base_low & ~0x0F) << 16;
	limit = (mem_limit_low & ~0x0F) << 16;

	if (base <= limit){
		res->start = base;
		res->end = limit + 0xFFFFF;
		res->flag |= RESOURCE_TYPE_MEM;
	}

	res = &dev->resource[PCI_BRIDGE_RESOURCE + 2];
	pci_read_config_w(dev->bus, dev->dev, dev->func, PCI_PREFETCH_MEM_BASE_LOW, &mem_base_low);
	pci_read_config_w(dev->bus, dev->dev, dev->func, PCI_PREFETCH_MEM_LIMIT_LOW, &mem_limit_low);

	base = (mem_base_low & ~0x0FUL) << 16;
	limit = (mem_limit_low &~0x0FUL) << 16;

	if (base <= limit){
		res->start = base;
		res->end = base + 0xFFFFF;
		res->flag |= RESOURCE_TYPE_MEM;
	}

	return 0;

}


int pci_device_read_irq(pci_dev_t *dev)
{
	unsigned char irq_pin = 0, irq = 0;

	pci_read_config_b(dev->bus, dev->dev, dev->func, PCI_IRQ_PIN, &irq_pin);

	if (irq_pin){
		pci_read_config_b(dev->bus, dev->dev, dev->func, PCI_IRQ_LINE, &irq);
		dev->irq = irq;
	}

	return 0;
}

int pci_setup_device(pci_dev_t *dev)
{
	unsigned long val;
	int ret;
	ret = pci_read_config_l(dev->bus, dev->dev, dev->func, PCI_REVISION_ID, &val);
	if (ret < 0){
		LOG_ERROR("read revision error");
		return -1;
	}

	(*(unsigned int *)&(dev->revisionid)) = val;

	ret = pci_read_config_l(dev->bus, dev->dev, dev->func, PCI_CACHE_SIZE, &val);
	if (ret < 0){
		LOG_ERROR("read cache size error");
		return -1;
	}

	(*(unsigned long *)&(dev->cache_size)) = val;

	dev->multifunction = dev->header_type & 0x80;
	dev->header_type &= 0x7f;

	switch(dev->header_type){
	case PCI_HEADER_TYPE_NORMAL:
		//a general device
		pci_device_read_irq(dev);
		pci_device_read_bases(dev, 6, PCI_ROM_ADDR);
		pci_read_config_w(dev->bus, dev->dev, dev->func, PCI_SUBVENDOR_ID, &dev->sub_vendor_id);
		pci_read_config_w(dev->bus, dev->dev, dev->func, PCI_SUBSYS_ID, &dev->sub_sys_id);
		LOG_INFO("class code:%d sub class:%d", dev->class_code, dev->sub_class);
		break;
	case PCI_HEADER_TYPE_BRIDGE:
		//LOG_INFO("bridge found");
		if (dev->class_code != 0x06 && dev->class_code != 0x04)
			return -1;
		pci_device_read_bases(dev, 2, PCI_BRIDGE_ROM_ADDR);
		break;
	case PCI_HEADER_TYPE_CARDBUS:
		break;
	default:
		break;

	}
	return 0;


}

pci_dev_t *pci_scan_device(int bus, int devno, int func)
{

	int ret;
	unsigned long val = 0;
	pci_dev_t *dev = NULL;
	struct linked_list *pos;


	ret = pci_read_config_l(bus, devno, func, PCI_VENDOR_ID, &val);
	if (ret < 0)
		return NULL;

	if (val == 0xFFFFFFFF || val == 0x00000000 ||
			val == 0x0000FFFF || val == 0xFFFF0000)
		return NULL;

	for (pos=pci_root_list.next; pos != &pci_root_list; pos=pos->next){
		dev = container_of(pos, pci_dev_t, list);
		if (dev->vendor == (val & 0xFFFF) && dev->device == ((val >> 16) & 0xFFFF)){
			//this maybe a ghost device, for vmware, vendor=0x15ad, dev=0x7a0
			//and i only check vendor and device, for fully check, see linux kernel,
			//pcibios_fixup_ghosts()
			//LOG_INFO("find ghost device,ven:%x dev:%x", val & 0xFFFF, (val >> 16) & 0xFFFF);
			return NULL;
		}
	}

	//we should handle retry cases
	dev = (pci_dev_t *)kmalloc(sizeof(pci_dev_t));
	assert(dev != NULL);

	dev->vendor = val & 0xFFFF;
	dev->device = (val >> 16) & 0xFFFF;
	dev->bus = bus;
	dev->dev = devno;
	dev->func = func;

	list_init(&dev->list);

	ret = pci_setup_device(dev);
	if (ret < 0){
		LOG_ERROR("setup device error");
		kfree(dev);
		return NULL;
	}

	list_add_tail(&pci_root_list, &dev->list);

	return dev;



}


int pci_scan_slot(int bus, int devno)
{
	int func;

	for (func=0; func<8; func++){
		pci_dev_t *dev = NULL;
		dev = pci_scan_device(bus, devno, func);
		if (dev){
			if (!dev->multifunction) {
				if (func > 0)
					dev->multifunction = 1;
				else
					break;
			}

		}

	}

	return 0;
}

int pci_scan_bus(int bus)
{

	int dev;

	if (bus > 255 || bus < 0)
		return -1;


	for (dev=0; dev < 32; dev++)
		pci_scan_slot(bus, dev);

	return 0;
}

int pci_scan_bridge(pci_dev_t *dev)
{
	unsigned long buses;

	pci_read_config_l(dev->bus, dev->dev, dev->func, PCI_PRIMARY_BUSNO, &buses);

	LOG_INFO("scan bus:%d", (buses >> 8) & 0xFF);

	pci_scan_bus((buses >> 8) & 0xFF);

	return 0;
}


pci_dev_t *
pci_lookup_dev(int class, int subclass)
{
	pci_dev_t *dev = NULL;
	struct linked_list *pos;
	for (pos=pci_root_list.next; pos!=&pci_root_list; pos=pos->next){
		dev = container_of(pos, pci_dev_t, list);
		if (dev->class_code == class && dev->sub_class == subclass )
			return dev;
	}

	return NULL;
}


void pci_dev_enable_int(pci_dev_t *dev)
{
	if (dev){
		if (!(dev->command & (1<<10)))
			return;
		dev->command &= (~(1<<10));

		pci_write_config_l(dev->bus, dev->dev, dev->func, PCI_COMMAND, dev->command);
	}
}

void pci_dev_enable_mm(pci_dev_t *dev)
{
	if (dev){
		if (!(dev->command & (1<<1))){
			dev->command |= (1<<1);
			pci_write_config_l(dev->bus, dev->dev, dev->func, PCI_COMMAND, dev->command);
		}
	}
}

void pci_dev_set_bus_master(pci_dev_t *dev)
{
	if (dev){
		dev->command |= (1<<2);
		pci_write_config_l(dev->bus, dev->dev, dev->func, PCI_COMMAND, dev->command);
	}
}

void pci_dev_enable_io(pci_dev_t *dev)
{
	if (dev){
		if (!(dev->command & (1<<0))){
			dev->command |= (1<<0);
			pci_write_config_l(dev->bus, dev->dev, dev->func, PCI_COMMAND, dev->command);
		}
	}
}

int pci_find_capability(pci_dev_t *dev, int cap)
{
#define  PCI_STATUS		0x06	/* 16 bits */
#define  PCI_STATUS_CAP_LIST	0x10	/* Support Capability List */
#define  PCI_CAPABILITY_LIST	0x34	/* Offset of first capability list entry */
	unsigned short status;
	unsigned char pos, id;
	int ttl = 48;
	pci_read_config_w(dev->bus, dev->dev, dev->func, PCI_STATUS, &status);
	if (!(status & PCI_STATUS_CAP_LIST)){
		return -1;
	}

	pci_read_config_b(dev->bus, dev->dev, dev->func, PCI_CAPABILITY_LIST, &pos);
	while (ttl-- && pos >= 0x40){
		pos &= ~3;
		pci_read_config_b(dev->bus, dev->dev, dev->func, pos, &id);
		if (id == 0xFF)
			break;
		if (id == cap)
			return pos;
		pci_read_config_b(dev->bus, dev->dev, dev->func, pos+1, &pos);
	}

	return -1;

}


int pci_set_power_state(pci_dev_t *dev, int state)
{
	//do some check

#define PCI_PM_CTRL	4
#define PCI_PM_PMC	2
#define PCI_CAP_ID_PM	0x01


	int pm;
	pm = pci_find_capability(dev, PCI_CAP_ID_PM);
	if (pm < 0){
		return -EIO;
	}

	unsigned short pmc;
	pci_read_config_w(dev->bus, dev->dev, dev->func, pm+PCI_PM_PMC, &pmc);
	if ((pmc & 0x07) > 3){
		LOG_INFO("unsupported pm cap version:%d", pmc & 0x07);
		return -EIO;
	}

	unsigned short pmcsr = 0;
	pci_read_config_w(dev->bus, dev->dev, dev->func, pm+PCI_PM_CTRL, &pmcsr);
	pmcsr &= ~3;
	pmcsr |= state;
	LOG_INFO("set dev pm state:%x", pmcsr);
	pci_write_config_w(dev->bus, dev->dev, dev->func, pm+PCI_PM_CTRL, pmcsr);

	return 0;
}

int pci_dev_enable_dev(pci_dev_t *dev)
{
#define PCI_DEV_PD0	0
	pci_set_power_state(dev, PCI_DEV_PD0);
	pci_dev_enable_mm(dev);
	return 0;
}

int pci_init()
{
//	int i;
	struct linked_list *pos;
	list_init(&pci_root_list);
	pci_scan_bus(0);
	pci_dev_t *dev;
	for (pos=pci_root_list.next; pos != &pci_root_list; pos=pos->next){
		dev = container_of(pos, pci_dev_t, list);
		if (dev->header_type == PCI_HEADER_TYPE_BRIDGE){
			pci_bridge_read_bases(dev);
			pci_scan_bridge(dev);
		}
	}

	return 0;
}



