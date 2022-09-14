/*
 * e1000.h
 *
 *  Created on: Jun 18, 2013
 *      Author: root
 */

#ifndef SHAUN_E1000_H_
#define SHAUN_E1000_H_


#include <kernel/types.h>
#include <kernel/pci.h>
#define E1000_IRQ_CPU		0

#define INTEL_VENDOR_ID		0x8086
/* e1000s.  For some info (and lists of more of these, check out
 * http://pci-ids.ucw.cz/read/PC/8086/
 * Some notes:
 * 	- in 2009, paul mentioned that the 82576{,NS} are supported by the igb
 * 	driver in Linux, since they support a more advanced feature set.
 * 	- There are many more e1000s.  We could import the list from pci-ids, or
 * 	something more clever.  This list mostly just tracks devices we've seen
 * 	before. */

#define INTEL_82543GC_ID	0x1004
#define INTEL_82540EM_ID	0x100e		/* qemu's device */
#define INTEL_82545EM_ID	0x100f
#define INTEL_82576_ID		0x10c9
#define INTEL_82576NS_ID	0x150a

#include <kernel/e1000_hw.h>
#include <kernel/net/if_ether.h>
// Offset used for indexing IRQs
#define KERNEL_IRQ_OFFSET	32

// Intel Descriptor Related Sizing
#define E1000_NUM_TX_DESCRIPTORS	256
#define E1000_NUM_RX_DESCRIPTORS	256

// This should be in line with the setting of BSIZE in RCTL
#define E1000_RX_MAX_BUFFER_SIZE 2048
#define E1000_TX_MAX_BUFFER_SIZE 2048

#define MAXIMUM_ETHERNET_VLAN_SIZE 1522

#define E1000_COLLISION_THRESHOLD       15
#define E1000_CT_SHIFT                  4
#define E1000_COLLISION_DISTANCE        63
#define E1000_COLD_SHIFT                12


#define E1000_TXD_POPTS_SHIFT 8         /* POPTS shift */
#define E1000_TXD_POPTS_IXSM 0x01       /* Insert IP checksum */
#define E1000_TXD_POPTS_TXSM 0x02       /* Insert TCP/UDP checksum */


/* Receive Checksum Control */
#define E1000_RXCSUM_PCSS_MASK 0x000000FF   /* Packet Checksum Start */
#define E1000_RXCSUM_IPOFL     0x00000100   /* IPv4 checksum offload */
#define E1000_RXCSUM_TUOFL     0x00000200   /* TCP / UDP checksum offload */
#define E1000_RXCSUM_IPV6OFL   0x00000400   /* IPv6 checksum offload */
#define E1000_RXCSUM_CRCOFL    0x00000800   /* CRC32 offload enable */
#define E1000_RXCSUM_IPPCSE    0x00001000   /* IP payload checksum enable */
#define E1000_RXCSUM_PCSD      0x00002000   /* packet checksum disabled */

struct e1000_rx_ring {
	u32 rx_head;
	u32 rx_tail;
	u32 rx_free;
	struct e1000_rx_desc *rx_desc;
};


struct eth_adapter {
	u32 devid;
	u32 vendor_id;
	u32 *iomem;
	u8	irq;
	u8	unit;
	u8	mac_addr[6];
	pci_dev_t *pci;
	u8 mac_type;
	u8 media_type;
	u16 ioaddr;
	u32 rx_buffer_len;
	u32 max_frame_size;
	u32	min_frame_size;
	//struct e1000_rx_desc *rx_desc;
	struct e1000_tx_desc *tx_desc;
	u32 flags;
	struct e1000_rx_ring rx_ring;
	u32	tx_init_delay;
	u32 rx_init_delay;
	u32	tx_cmd;
	u32 rx_csum;
	//u32 rx_index;
	u32 tx_index;
};
extern struct eth_adapter *adp;


int e1000_init(struct _le_softc *le);

#endif /* SHAUN_E1000_H_ */
