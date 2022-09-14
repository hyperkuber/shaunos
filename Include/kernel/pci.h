/*
 * pci.h
 *
 *  Created on: Feb 27, 2013
 *      Author: root
 */

#ifndef SHAUN_PCI_H_
#define SHAUN_PCI_H_


#include <list.h>


#define PCI_VENDOR_ID	0x00
#define PCI_DEVICE_ID	0x02
#define PCI_COMMAND		0x04
#define PCI_STATUS		0x06
#define PCI_REVISION_ID	0x08
#define PCI_PROG_IF		0x09
#define PCI_SUB_CLASS	0x0A
#define PCI_CLASS_CODE	0x0B
#define PCI_CACHE_SIZE	0x0C
#define PCI_LAT_TIMER	0x0D
#define PCI_HEAD_TYPE	0x0E
#define PCI_BIST		0x0F
#define PCI_BASE_ADDR0	0x10
#define PCI_BASE_ADDR1	0x14
#define PCI_BASE_ADDR2	0x18
#define PCI_BASE_ADDR3	0x1c
#define PCI_BASE_ADDR4	0x20
#define PCI_BASE_ADDR5	0x24
#define PCI_CARDBUS_PTR	0x28
#define PCI_SUBVENDOR_ID	0x2c
#define PCI_SUBSYS_ID	0x2E
#define PCI_ROM_ADDR	0x30
#define PCI_CAP_PTR		0x34
#define PCI_IRQ_LINE	0x3C
#define PCI_IRQ_PIN		0x3D
#define PCI_MIN_GNT		0x3E
#define PCI_MAX_LAT		0x3F

#define PCI_HEADER_TYPE_NORMAL	0x00
#define PCI_HEADER_TYPE_BRIDGE	0x01
#define PCI_HEADER_TYPE_CARDBUS	0x02
//type 01
#define PCI_PRIMARY_BUSNO	0x18
#define PCI_SECONDARY_BUSNO	0x19
#define PCI_SUBORDINATE_BUSNO	0x1A
#define PCI_SEC_LATENCY_TIMER	0x1B
#define PCI_IO_BASE_LOW	0x1C
#define PCI_IO_LIMIT_LOW	0x1D
#define PCI_SECONDARY_STATUS	0x1E
#define PCI_MEM_BASE	0x20
#define PCI_MEM_LIMIT	0x22
#define PCI_PREFETCH_MEM_BASE_LOW	0x24
#define PCI_PREFETCH_MEM_LIMIT_LOW	0x26
#define PCI_PREFETCH_MEM_BASE_UPPER	0x28
#define PCI_PREFETCH_MEM_LIMIT_UPPER	0x2C
#define PCI_IO_BASE_UPPER	0x30
#define PCI_IO_LIMIT_UPPER	0x32
#define PCI_BRIDGE_CAP_POINTER	0x34
#define PCI_BRIDGE_ROM_ADDR	0x38
#define PCI_BIRDGE_CONTROL	0x3E


struct resource {
	unsigned int start;
	unsigned int end;
	unsigned int flag;
};

#define RESOURCE_TYPE_IO	0x100
#define RESOURCE_TYPE_MEM	0x200
#define RESOURCE_TYPE_IRQ	0x400

#define RESOURCE_TYPE_PREFETCH	0x1000
#define RESOURCE_TYPE_READONLY	0x2000


#define PCI_ROM_RESOURCE	6
#define PCI_BRIDGE_RESOURCE	7

struct __pci_device {
	unsigned short vendor;
	unsigned short device;
	unsigned short command;
	unsigned short status;
	unsigned char revisionid;
	unsigned char prog_if;
	unsigned char sub_class;
	unsigned char class_code;
	unsigned char cache_size;
	unsigned char latency_timer;
	unsigned char header_type;
	unsigned char bist;
	struct resource resource[12];
	unsigned char irq;
//	unsigned char irq_pin;
	unsigned char capabilities;
	unsigned char rsvd;
	unsigned short sub_vendor_id;
	unsigned short sub_sys_id;
	unsigned long rom_addr;
	unsigned char bus;
	unsigned char dev;
	unsigned char func;
	unsigned char multifunction;
	struct linked_list list;

} __attribute__((__packed__));
typedef struct __pci_device pci_dev_t;




int pci_init();


pci_dev_t *
pci_lookup_dev(int class, int subclass);
void pci_dev_enable_int(pci_dev_t *dev);
void pci_dev_enable_mm(pci_dev_t *dev);
void pci_dev_enable_io(pci_dev_t *dev);
void pci_dev_set_bus_master(pci_dev_t *dev);
int pci_dev_enable_dev(pci_dev_t *dev);

#endif /* SHAUN_PCI_H_ */
