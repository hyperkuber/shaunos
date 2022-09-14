/*
 * e1000.c
 *
 *  Created on: Jun 18, 2013
 *      Author: Shaun Yuan
 *      Copyright (c) 2013 Shaun Yuan
 */

#include <kernel/kernel.h>
#include <kernel/types.h>
#include <kernel/e1000.h>
#include <kernel/pci.h>
#include <kernel/page.h>
#include <kernel/cpu.h>
#include <kernel/malloc.h>
#include <asm/io.h>
#include <asm/system.h>
#include <kernel/mm.h>
#include <kernel/net/if_ether.h>
#include <kernel/net/if.h>
#include <string.h>
#include <kernel/irq.h>
//#include <kernel/e1000_hw.h>
#define PCI_ETHER_BAR	0

#define NUM_TX_DESCRIPTORS E1000_NUM_TX_DESCRIPTORS
#define NUM_RX_DESCRIPTORS E1000_NUM_RX_DESCRIPTORS
#define E1000_WRITE_FLUSH() e1000_rr32(E1000_STATUS)

#define MAX_FRAME_SIZE	ETH_MTU

typedef struct eth_adapter eth_adapter_t;

struct eth_adapter *adp;

u32 e1000_rr32(u32 offset)
{
	if (adp->iomem) {
		return iomem_readl((void *)adp->iomem + offset);
	} else {
		return inl(adp->ioaddr + offset);
	}
}

void e1000_wr32(u32 offset, u32 val)
{
	if (adp->iomem) {
		iomem_writel((void *)adp->iomem + offset, val);
	} else {
		outl(adp->ioaddr + offset, val);
	}
}

u16 e1000_read_eeprom(u32 offset) {

	u16 eeprom_data;
	u32 eeprom_reg_val = e1000_rr32(E1000_EECD);

	// Request access to the EEPROM, then wait for access
	eeprom_reg_val |=  E1000_EECD_REQ;
	e1000_wr32(E1000_EECD, eeprom_reg_val);
        while((e1000_rr32(E1000_EECD) & E1000_EECD_GNT) == 0);

	// We now have access, write what value we want to read, then wait for access
	eeprom_reg_val = E1000_EERD_START | (offset << E1000_EERD_ADDR_SHIFT);
	e1000_wr32(E1000_EERD, eeprom_reg_val);
	while(((eeprom_reg_val = e1000_rr32(E1000_EERD)) & E1000_EERD_DONE) == 0);
	eeprom_data = (eeprom_reg_val & E1000_EERD_DATA_MASK) >> E1000_EERD_DATA_SHIFT;

	// Read the value (finally)
	eeprom_reg_val = e1000_rr32(E1000_EECD);

	// Tell the EEPROM we are done.
	e1000_wr32(E1000_EECD, eeprom_reg_val & ~E1000_EECD_REQ);

	return eeprom_data;

}

void e1000_setup_rx_ring(struct eth_adapter *adp, u32 des_num, u8 reset_buffer)
{

	adp->rx_ring.rx_desc[des_num].csum = 0;
	adp->rx_ring.rx_desc[des_num].errors = 0;
	adp->rx_ring.rx_desc[des_num].length = 0;
	adp->rx_ring.rx_desc[des_num].special= 0;;
	adp->rx_ring.rx_desc[des_num].status = 0;

	if (reset_buffer) {
		// Alloc a buffer, this may total use 512k in kernel heap
		char *rx_buffer = kmalloc(E1000_RX_MAX_BUFFER_SIZE);
		if (rx_buffer == NULL)
			panic ("Can't allocate page for RX Buffer");
		// Set the buffer addr
		adp->rx_ring.rx_desc[des_num].buffer_addr = PA(rx_buffer);
	}

}


void e1000_setup_tx_ring(struct eth_adapter *adp, u32 des_num)
{
	char *tx_buffer = kzalloc(E1000_TX_MAX_BUFFER_SIZE);
	if(tx_buffer == NULL)
		panic("cannot alloc buffer for tx");

	adp->tx_desc[des_num].buffer_addr = PA(tx_buffer);
}


void e1000_setup_rings(struct eth_adapter *adp)
{

	int i;
	LOG_INFO("in");
	// Get the pages
	adp->rx_ring.rx_desc = (struct e1000_rx_desc *)alloc_pages(ALLOC_ZEROED|ALLOC_NORMAL, 0);
	if (adp->rx_ring.rx_desc == NULL)
		panic("cannot alloc memory for rx ring");
	adp->tx_desc = (struct e1000_tx_desc *)alloc_pages(ALLOC_ZEROED|ALLOC_NORMAL, 0);
	if (adp->tx_desc == NULL)
		panic("connot alloc memory for tx ring");

	adp->rx_ring.rx_head = 0;
	adp->rx_ring.rx_tail = NUM_RX_DESCRIPTORS - 1;
	adp->rx_ring.rx_free = 0;

	// Configure each descriptor.
	for (i = 0; i < NUM_RX_DESCRIPTORS; i++)
		e1000_setup_rx_ring(adp, i, 1); // True == Allocate memory for the descriptor

	for (i = 0; i < NUM_TX_DESCRIPTORS; i++)
		e1000_setup_tx_ring(adp, i);
	LOG_INFO("end");
}


int e1000_reset_hw_82540(int val)
{
	u32 manc;

	LOG_INFO("in");
	e1000_wr32(E1000_IMC, 0xFFFFFFFF);

	e1000_wr32(E1000_RCTL, 0);
	e1000_wr32(E1000_TCTL, E1000_TCTL_PSP);
	E1000_WRITE_FLUSH();

	e1000_wr32(E1000_CTRL, e1000_rr32(E1000_CTRL) | E1000_CTRL_RST);
	ksleep(5);
	/* Disable HW ARPs on ASF enabled adapters */
	manc = e1000_rr32(E1000_MANC);
	manc &= ~E1000_MANC_ARP_EN;
	e1000_wr32(E1000_MANC, manc);

	e1000_wr32(E1000_IMC, 0xffffffff);
	e1000_rr32(E1000_ICR);
	LOG_INFO("end");
	return 0;
}

void e1000_irq_enable()
{
	e1000_wr32(E1000_IMS, IMS_ENABLE_MASK);
	E1000_WRITE_FLUSH();
}


void e1000_handle_rx_packet(struct eth_adapter *adp)
{
	u32 status, error;
	//u32 head = e1000_rr32(E1000_RDH);
	u32 head = adp->rx_ring.rx_head;
	u32 frame_size = 0;
	caddr_t rx_buffer;
	struct e1000_rx_desc *rx_desc = &adp->rx_ring.rx_desc[head];


	while (rx_desc->status & E1000_RXD_STAT_DD) {
		status = rx_desc->status;
		error = rx_desc->errors;
		rx_buffer = (caddr_t)VA(rx_desc->buffer_addr);
		frame_size = rx_desc->length;

		if (++head == NUM_RX_DESCRIPTORS)
			head = 0;

		if (!(status & E1000_RXD_STAT_EOP)){
			LOG_INFO("desc without eop");
			goto next;
		}

		if (error & E1000_RXD_ERR_FRAME_ERR_MASK) {
			LOG_INFO("desc with error:%d", error);
			goto next;
		}


		leread(adp->unit, rx_buffer, frame_size);
		next:
		rx_desc->status = 0;
		adp->rx_ring.rx_free++;
		adp->rx_ring.rx_head = head;
		rx_desc = &adp->rx_ring.rx_desc[head];
	}

//	do {
//		// Get the descriptor status
//
//
//		// If the status is 0x00, it means we are somehow trying to process
//		//  a packet that hasnt been written by the NIC yet.
//		if (status == 0x0) {
//			LOG_ERROR("ERROR: E1000: Packet owned by hardware has 0 status value\n");
//			/* It's possible we are processing a packet that is a fragment
//			 * before the entire packet arrives.  The code currently assumes
//			 * that all of the packets fragments are there, so it assumes the
//			 * next one is ready.  We'll spin until it shows up...  This could
//			 * deadlock, and sucks in general, but will help us diagnose the
//			 * driver's issues.  TODO: determine root cause and fix this shit.*/
//			while(rx_ring[rx_index_curr].status == 0x0)
//				cpu_relax();
//			status = rx_ring[rx_index_curr].status;
//		}
//
//		frame_size = rx_ring[rx_index_curr].length;
//
//		if ((status & E1000_RXD_STAT_DD) == 0x0) {
//			panic("RX Descriptor Ring OWN out of sync");
//		}
//
//		if (frame_size  > adp->max_frame_size) {
//			LOG_ERROR("Nic sent %u byte packet. Max is %u\n", frame_size, adp->max_frame_size);
//		}
//
//		rx_buffer = (caddr_t)VA(rx_ring[rx_index_curr].buffer_addr);
//		// Move the fragment data into the buffer
//		//memcpy(rx_buffer + frame_size, (void *)VA(rx_ring[rx_loop_index].buffer_addr), frag_size);
//
//		e1000_setup_rx_ring(adp, rx_index_curr, 0);
//		// Advance to the next descriptor
//		rx_index_curr = (rx_index_curr + 1) % NUM_RX_DESCRIPTORS;
//		leread(adp->unit, rx_buffer, frame_size);
//	} while ((status & E1000_RXD_STAT_EOP) == 0);

	//Log where we should start reading from next time we trap
	//adp->rx_index = rx_index_curr;
	//LOG_INFO("head:%d, index:%d", head, rx_index_curr);
	// Bump the tail pointer. It should be 1 behind where we start reading from.
	//e1000_wr32(E1000_RDT, (rx_index_curr + 1) % NUM_RX_DESCRIPTORS);

}

// Code that is executed when an interrupt comes in on IRQ e1000_irq
void e1000_interrupt_handler(int irq, void *data, trap_frame_t *regs)
{
	u32 icr = e1000_rr32(E1000_ICR);
	u32 tail;
	if (!icr)
		return;

	LOG_INFO("Interrupt status: %x", icr);

	if (icr & E1000_ICR_RXT0)
		e1000_handle_rx_packet(adp);
	if (icr & E1000_ICR_LSC) {
		//link status change;
		if (e1000_rr32(E1000_STATUS) & 2){
			LOG_DEBG("link status down");
			//adp->flags &= ~IFF_UP;
		}

	}

	if (icr & E1000_ICR_TXDW) {
		LOG_DEBG("transmit written back");
	}

	if (icr & E1000_ICR_RXDMT0) {
		LOG_INFO("rx low, rdh:%d rdt:%d, index:%d", e1000_rr32(E1000_RDH), e1000_rr32(E1000_RDT), adp->rx_ring.rx_head);
		tail = adp->rx_ring.rx_tail + adp->rx_ring.rx_free;
		tail %= E1000_NUM_RX_DESCRIPTORS;
		adp->rx_ring.rx_tail = tail;
		adp->rx_ring.rx_free = 0;
		e1000_wr32(E1000_RDT, tail);
	}

	LOG_INFO("end");
}

void e1000_setup_interrupts(struct eth_adapter *adp)
{

	// Kernel based interrupt stuff
	register_irq(adp->irq, (u32)e1000_interrupt_handler,(void *)adp, IRQ_SHARED);
	// Set throttle register
	e1000_wr32(E1000_ITR, 1000);

	e1000_wr32(E1000_IMC, E1000_IMC_ALL);
	// Clear interrupts
	e1000_wr32(E1000_IMS, 0xFFFFFFFF);

	// Set interrupts
	e1000_irq_enable();
	enable_irq(adp->irq);
}


void e1000_setup_mac(struct eth_adapter *adp)
{

	u16 eeprom_data = 0;
	u32 mmio_data = 0;
	int i;
	char device_mac[6] = {0};
	u32 rar_low, rar_high;
	LOG_INFO("in");

	if (adp->devid == INTEL_82540EM_ID) {
		// This is ungodly slow. Like, person perceivable time slow.
		for (i = 0; i < 3; i++) {
			eeprom_data = e1000_read_eeprom(i);
			device_mac[2*i] = eeprom_data & 0x00FF;
			device_mac[2*i + 1] = (eeprom_data & 0xFF00) >> 8;
		}
	} else {
		LOG_INFO("get mac from ra");
		// Get the data from RAL
		mmio_data = e1000_rr32(E1000_RAL);
		// Do the big magic rain dance
		device_mac[0] = mmio_data & 0xFF;
		device_mac[1] = (mmio_data >> 8) & 0xFF;
		device_mac[2] = (mmio_data >> 16) & 0xFF;
		device_mac[3] = (mmio_data >> 24) & 0xFF;

		// Get the rest of the MAC data from RAH.
		mmio_data = e1000_rr32(E1000_RAH);
		// Continue magic dance.
		device_mac[4] = mmio_data & 0xFF;
		device_mac[5] = (mmio_data >> 8) & 0xFF;
		LOG_INFO("read mac from ra end");
	}

	mmio_data = e1000_rr32(E1000_STATUS);
	if (mmio_data & E1000_STATUS_FUNC_MASK) {
		device_mac[5] ^= 0x0100;
	}

	rar_low = ((u32)device_mac[0] | ((u32)device_mac[1] << 8) |
			((u32)device_mac[2] << 16) |
			((u32)device_mac[3] << 24));
	rar_high = ((u32)device_mac[4] | ((u32)device_mac[5] << 8));

	if (rar_low || rar_high)
		rar_high |= E1000_RAH_AV;

	//this will hang vmware for 82545em card
//	e1000_wr32(E1000_RAL, rar_low);
//	E1000_WRITE_FLUSH();
//	e1000_wr32(E1000_RAH, rar_high);
//	E1000_WRITE_FLUSH();

	mmio_data = e1000_rr32(E1000_RAL);
	device_mac[0] = mmio_data & 0xFF;
	device_mac[1] = (mmio_data >> 8) & 0xFF;
	device_mac[2] = (mmio_data >> 16) & 0xFF;
	device_mac[3] = (mmio_data >> 24) & 0xFF;
	mmio_data = e1000_rr32(E1000_RAH);
	device_mac[4] = mmio_data & 0xFF;
	device_mac[5] = (mmio_data >> 8) & 0xFF;

	// Clear the MAC's from all the other filters
	// Must clear high to low.
	for (i = 1; i < 16; i++) {
		e1000_wr32(E1000_RAH + 8 * i, 0x0);
		E1000_WRITE_FLUSH();
		e1000_wr32(E1000_RAL + 8 * i, 0x0);
		E1000_WRITE_FLUSH();
	}
	memcpy((void *)adp->mac_addr, (void *)device_mac, ETHER_ADDR_LEN);
	LOG_INFO("end");
}


void e1000_set_multi(struct eth_adapter *adp)
{
	u32 rctl;

	rctl = e1000_rr32(E1000_RCTL);
	rctl |= E1000_RCTL_UPE;
	if (adp->flags & IFF_PROMISC) {
		rctl |= (E1000_RCTL_UPE | E1000_RCTL_MPE);
		rctl &= ~E1000_RCTL_VFE;
	} else {
		if (adp->flags & IFF_ALLMULTI) {
			rctl |= E1000_RCTL_MPE;
			rctl &= ~E1000_RCTL_UPE;
		} else
			rctl &= ~(E1000_RCTL_MPE);
	}

	e1000_wr32(E1000_RCTL, rctl);
}

void e1000_configure_tx(struct eth_adapter *adp)
{
	u32 tctl;
	u32 txdctl;
	e1000_wr32(E1000_TDBAL, PA(adp->tx_desc));
	e1000_wr32(E1000_TDBAH, 0x0);
	e1000_wr32(E1000_TDLEN, ((NUM_TX_DESCRIPTORS / 8) << 7));

	e1000_wr32(E1000_TDH, 0);
	e1000_wr32(E1000_TDT, 0);

	e1000_wr32(E1000_TIDV, adp->tx_init_delay);

	tctl = e1000_rr32(E1000_TCTL);
	tctl &= ~E1000_TCTL_CT;
	tctl |= E1000_TCTL_PSP | E1000_TCTL_RTLC | E1000_TCTL_EN |
		(E1000_COLLISION_THRESHOLD << E1000_CT_SHIFT);

	adp->tx_cmd = E1000_TXD_CMD_EOP | E1000_TXD_CMD_IFCS;

	if (adp->mac_type < e1000_82543)
		adp->tx_cmd |= E1000_TXD_CMD_RPS;
	else
		adp->tx_cmd |= E1000_TXD_CMD_RS;

	e1000_wr32(E1000_TCTL, tctl);

	txdctl = e1000_rr32(E1000_TXDCTL);
	if (adp->flags & IFF_CHKSUM_OFFLOAD)
		txdctl |= (((E1000_TXD_POPTS_IXSM | E1000_TXD_POPTS_TXSM) << E1000_TXD_POPTS_SHIFT) | E1000_TXD_CMD_IC);

	txdctl |= E1000_TXDCTL_MAGIC | E1000_TXDCTL_ENABLE;
	e1000_wr32(E1000_TXDCTL, txdctl);

	// Transmit inter packet gap register
	// XXX: Recomended magic. See 13.4.34
	e1000_wr32(E1000_TIPG, 0x00702008);
}

void e1000_configure_rx(struct eth_adapter *adp)
{
	u32 rctl;
	u32 rx_csum;
	rctl = e1000_rr32(E1000_RCTL);
	e1000_wr32(E1000_RCTL, rctl & ~E1000_RCTL_EN);

	e1000_wr32(E1000_RDTR, adp->rx_init_delay);

	e1000_wr32(E1000_RDBAL, PA(adp->rx_ring.rx_desc));
	e1000_wr32(E1000_RDBAH, 0x0);

	e1000_wr32(E1000_RDLEN, (NUM_RX_DESCRIPTORS / 8) << 7);

	e1000_wr32(E1000_RDH, 0);
	e1000_wr32(E1000_RDT, 0);


	if (adp->mac_type >= e1000_82543) {
		rx_csum = e1000_rr32(E1000_RXCSUM);
		if (adp->rx_csum)
			rx_csum |= E1000_RXCSUM_TUOFL;
		else
			rx_csum &= ~E1000_RXCSUM_TUOFL;

		e1000_wr32(E1000_RXCSUM, rx_csum);
	}

	e1000_wr32(E1000_RCTL, rctl);

	e1000_wr32(E1000_RDT, NUM_RX_DESCRIPTORS - 1);

	e1000_wr32(E1000_RXDCTL, E1000_RXDCTL_ENABLE | E1000_RXDCTL_WBT | E1000_RXDCTL_MAGIC);
}

void e1000_setup_rctl(struct eth_adapter *adp)
{
	u32 rctl;
	u32 data;
	rctl = e1000_rr32(E1000_RCTL);

	rctl |= E1000_RCTL_EN | E1000_RCTL_BAM |
		E1000_RCTL_LBM_NO | E1000_RCTL_RDMTS_HALF |
		(1 << E1000_RCTL_MO_SHIFT);

	rctl &= ~E1000_RCTL_SBP;
	rctl |= E1000_RCTL_LPE;
	rctl &= ~E1000_RCTL_SZ_4096;
	rctl |= E1000_RCTL_SZ_2048;

	e1000_wr32(E1000_RCTL, rctl);

	// Disable packet splitting.
	data = e1000_rr32(E1000_RFCTL);
	data = data & ~E1000_RFCTL_EXTEN;
	e1000_wr32(E1000_RFCTL, data);

}

void e1000_configure(struct eth_adapter *adp)
{
	e1000_set_multi(adp);

	e1000_configure_tx(adp);

	e1000_setup_rctl(adp);

	e1000_configure_rx(adp);

}


int e1000_start(struct ifnet *ifp)
{
	struct mbuf *m0, *m;
	u32 head;
	u8 *buffer;
	u32 tx_index;

	LOG_INFO("in");

	if ((ifp->if_flags & IFF_RUNNING) == 0)
		return 0;
	tx_index = adp->tx_index;
	for (;;){
		IF_DEQUEUE(&ifp->if_snd, m0);
		if (m0 == NULL)
			break;

		if ((m0->m_flags & M_PKTHDR) == 0)
			LOG_ERROR("no header in mbuf");

		if (m0->m_pkthdr.len > ETHER_MAX_LEN)
			LOG_ERROR("mbuf overflow");

		LOG_INFO("pkt header:%d", m0->m_pkthdr.len);
		head = e1000_rr32(E1000_TDH);

//		if (((adp->tx_index + 1) % NUM_TX_DESCRIPTORS) == head){
//			LOG_ERROR("tx buffer ring full");
//			return -1;
//		}

		buffer = (u8 *)VA(adp->tx_desc[tx_index].buffer_addr);
		ifp->if_flags |= IFF_OACTIVE;
		//copy data into device buffer
		for (m=m0; m != NULL; m=m->m_next){
			memcpy((void *)buffer, (void *)mtod(m, caddr_t), m->m_len);
			buffer += m->m_len;
		}
		buffer = (u8 *)VA(adp->tx_desc[tx_index].buffer_addr);


		LOG_INFO("dstmac:%x.%x.%x.%x.%x.%x", buffer[0], buffer[1], buffer[1], buffer[3], buffer[4], buffer[5]);

		adp->tx_desc[tx_index].lower.data = ( E1000_TXD_CMD_RPS | E1000_TXD_CMD_EOP
				| E1000_TXD_CMD_IFCS | E1000_TXD_CMD_DEXT | E1000_TXD_DTYP_D | m0->m_pkthdr.len);

		if (m0 != NULL)
			m_freem(m0);

		tx_index  = (tx_index +1) % NUM_TX_DESCRIPTORS;
		e1000_wr32(E1000_TDT, tx_index);

	}

	adp->tx_index = tx_index;
	ifp->if_flags &= ~IFF_OACTIVE;

	LOG_INFO("end");
	return 0;
}

int e1000_set_mac_type(struct eth_adapter *adp)
{
	int retval = 0;
	switch (adp->devid) {
	case E1000_DEV_ID_82542:
		adp->mac_type = e1000_82542;
		break;
	case E1000_DEV_ID_82543GC_FIBER:
	case E1000_DEV_ID_82543GC_COPPER:
		adp->mac_type = e1000_82543;
		break;
	case E1000_DEV_ID_82544EI_COPPER:
	case E1000_DEV_ID_82544EI_FIBER:
	case E1000_DEV_ID_82544GC_COPPER:
	case E1000_DEV_ID_82544GC_LOM:
		adp->mac_type = e1000_82544;
		break;
	case E1000_DEV_ID_82540EM:
	case E1000_DEV_ID_82540EM_LOM:
	case E1000_DEV_ID_82540EP:
	case E1000_DEV_ID_82540EP_LOM:
	case E1000_DEV_ID_82540EP_LP:
		adp->mac_type = e1000_82540;
		break;
	case E1000_DEV_ID_82545EM_COPPER:
	case E1000_DEV_ID_82545EM_FIBER:
		adp->mac_type = e1000_82545;
		break;
	case E1000_DEV_ID_82545GM_COPPER:
	case E1000_DEV_ID_82545GM_FIBER:
	case E1000_DEV_ID_82545GM_SERDES:
		adp->mac_type = e1000_82545_rev_3;
		break;
	case E1000_DEV_ID_82546EB_COPPER:
	case E1000_DEV_ID_82546EB_FIBER:
	case E1000_DEV_ID_82546EB_QUAD_COPPER:
		adp->mac_type = e1000_82546;
		break;
	case E1000_DEV_ID_82546GB_COPPER:
	case E1000_DEV_ID_82546GB_FIBER:
	case E1000_DEV_ID_82546GB_SERDES:
	case E1000_DEV_ID_82546GB_PCIE:
	case E1000_DEV_ID_82546GB_QUAD_COPPER:
	case E1000_DEV_ID_82546GB_QUAD_COPPER_KSP3:
		adp->mac_type = e1000_82546_rev_3;
		break;
	case E1000_DEV_ID_82541EI:
	case E1000_DEV_ID_82541EI_MOBILE:
	case E1000_DEV_ID_82541ER_LOM:
		adp->mac_type = e1000_82541;
		break;
	case E1000_DEV_ID_82541ER:
	case E1000_DEV_ID_82541GI:
	case E1000_DEV_ID_82541GI_LF:
	case E1000_DEV_ID_82541GI_MOBILE:
		adp->mac_type = e1000_82541_rev_2;
		break;
	case E1000_DEV_ID_82547EI:
	case E1000_DEV_ID_82547EI_MOBILE:
		adp->mac_type = e1000_82547;
		break;
	case E1000_DEV_ID_82547GI:
		adp->mac_type = e1000_82547_rev_2;
		break;
	default:
		retval = -1;
		break;
	}
	return retval;
}

void e1000_irq_disable()
{
	e1000_wr32(E1000_IMC, ~0);
	E1000_WRITE_FLUSH();
}

void e1000_clear_vlan_tbl()
{
	int offset;
	LOG_INFO("in");
	for (offset=0; offset < 128; offset++) {
		e1000_wr32(E1000_VFTA+(offset << 2), 0);
		E1000_WRITE_FLUSH();
	}
	LOG_INFO("end");
}

int e1000_set_phy_mode_82540(struct eth_adapter *adp)
{
	u32 ctrl;
	LOG_INFO("in");
	ctrl = e1000_rr32(E1000_CTRL);
	ctrl &= ~(E1000_CTRL_ILOS |E1000_CTRL_PHY_RST);
	e1000_wr32(E1000_CTRL, ctrl);

	// Set PHY mode
	ctrl = e1000_rr32(E1000_CTRL_EXT);
	ctrl = (ctrl & ~E1000_CTRL_EXT_LINK_MODE_MASK) | E1000_CTRL_EXT_LINK_MODE_GMII;
	e1000_wr32(E1000_CTRL_EXT, ctrl);

	// Set full-duplex
	ctrl = e1000_rr32(E1000_CTRL);
	ctrl = ctrl & E1000_CTRL_FD;
	e1000_wr32(E1000_CTRL, ctrl);
	LOG_INFO("end");
	return 0;
}

int e1000_setup_copper_link_82540(struct eth_adapter *adp)
{
	u32 ctrl;
	LOG_INFO("in");
	ctrl = e1000_rr32(E1000_CTRL);
	ctrl |= E1000_CTRL_SLU;
	ctrl |= E1000_CTRL_ASDE;
	ctrl |= (E1000_CTRL_FRCSPD | E1000_CTRL_FRCDPX );
	ctrl &= ~(E1000_CTRL_LRST);
	e1000_wr32(E1000_CTRL, ctrl);

	e1000_set_phy_mode_82540(adp);
	LOG_INFO("end");
	return 0;
}

int e1000_setup_fiber_serdes_link_82540(struct eth_adapter *adp)
{
	return 0;
}

void e1000_setup_link(struct eth_adapter *adp)
{
	LOG_INFO("in");
	if (adp->media_type == e1000_media_type_copper)
		e1000_setup_copper_link_82540(adp);
	else
		e1000_setup_fiber_serdes_link_82540(adp);

	//setup flow control
	e1000_wr32(E1000_FCAL, 0x00);
	e1000_wr32(E1000_FCAH, 0x00);
	e1000_wr32(E1000_FCT,  0x00);
	LOG_INFO("end");
}

void e1000_clear_hw_cntrs_82540(struct eth_adapter *adp)
{
	e1000_rr32(E1000_CRCERRS);
	e1000_rr32(E1000_SYMERRS);
	e1000_rr32(E1000_MPC);
	e1000_rr32(E1000_SCC);
	e1000_rr32(E1000_ECOL);
	e1000_rr32(E1000_MCC);
	e1000_rr32(E1000_LATECOL);
	e1000_rr32(E1000_COLC);
	e1000_rr32(E1000_DC);
	e1000_rr32(E1000_SEC);
	e1000_rr32(E1000_RLEC);
	e1000_rr32(E1000_XONRXC);
	e1000_rr32(E1000_XONTXC);
	e1000_rr32(E1000_XOFFRXC);
	e1000_rr32(E1000_XOFFTXC);
	e1000_rr32(E1000_FCRUC);
	e1000_rr32(E1000_GPRC);
	e1000_rr32(E1000_BPRC);
	e1000_rr32(E1000_MPRC);
	e1000_rr32(E1000_GPTC);
	e1000_rr32(E1000_GORCL);
	e1000_rr32(E1000_GORCH);
	e1000_rr32(E1000_GOTCL);
	e1000_rr32(E1000_GOTCH);
	e1000_rr32(E1000_RNBC);
	e1000_rr32(E1000_RUC);
	e1000_rr32(E1000_RFC);
	e1000_rr32(E1000_ROC);
	e1000_rr32(E1000_RJC);
	e1000_rr32(E1000_TORL);
	e1000_rr32(E1000_TORH);
	e1000_rr32(E1000_TOTL);
	e1000_rr32(E1000_TOTH);
	e1000_rr32(E1000_TPR);
	e1000_rr32(E1000_TPT);
	e1000_rr32(E1000_MPTC);
	e1000_rr32(E1000_BPTC);

	e1000_rr32(E1000_PRC64);
	e1000_rr32(E1000_PRC127);
	e1000_rr32(E1000_PRC255);
	e1000_rr32(E1000_PRC511);
	e1000_rr32(E1000_PRC1023);
	e1000_rr32(E1000_PRC1522);
	e1000_rr32(E1000_PTC64);
	e1000_rr32(E1000_PTC127);
	e1000_rr32(E1000_PTC255);
	e1000_rr32(E1000_PTC511);
	e1000_rr32(E1000_PTC1023);
	e1000_rr32(E1000_PTC1522);

	e1000_rr32(E1000_ALGNERRC);
	e1000_rr32(E1000_RXERRC);
	e1000_rr32(E1000_TNCRS);
	e1000_rr32(E1000_CEXTERR);
	e1000_rr32(E1000_TSCTC);
	e1000_rr32(E1000_TSCTFC);

	e1000_rr32(E1000_MGTPRC);
	e1000_rr32(E1000_MGTPDC);
	e1000_rr32(E1000_MGTPTC);
}


void e1000_power_up_phy(struct eth_adapter *adp)
{
	u32 mii_reg = 0;

	mii_reg = e1000_rr32(E1000_CTRL);
	mii_reg &= ~0x0800;
	e1000_wr32(E1000_CTRL, mii_reg);
}

void e1000_init_hw_82540(struct eth_adapter *adp)
{
	int i;
	u32 txdctl;
	LOG_INFO("in");
	//disable vlan filtering
	if (adp->mac_type < e1000_82545_rev_3)
		e1000_wr32(E1000_VET, 0);

	e1000_clear_vlan_tbl();

	switch (adp->devid) {
	case E1000_DEV_ID_82545EM_FIBER:
	case E1000_DEV_ID_82545GM_FIBER:
	case E1000_DEV_ID_82546EB_FIBER:
	case E1000_DEV_ID_82546GB_FIBER:
		adp->media_type = e1000_media_type_fiber;
		break;
	case E1000_DEV_ID_82545GM_SERDES:
	case E1000_DEV_ID_82546GB_SERDES:
		adp->media_type = e1000_media_type_internal_serdes;
		break;
	default:
		adp->media_type = e1000_media_type_copper;
		break;
	}
	//clear multicast hash table
	for (i=0; i<128; i++) {
		e1000_wr32(E1000_MTA+(i<<2), 0);
		E1000_WRITE_FLUSH();
	}

	if (adp->media_type == e1000_media_type_copper)
		e1000_power_up_phy(adp);

	e1000_setup_link(adp);


	txdctl = e1000_rr32(0x03828);
	txdctl = (txdctl & ~E1000_TXDCTL_WTHRESH) |
	         E1000_TXDCTL_FULL_TX_DESC_WB;
	e1000_wr32(0x03828, txdctl);

	e1000_clear_hw_cntrs_82540(adp);
	LOG_INFO("end");
}


void e1000_init_82540(struct eth_adapter *adp)
{

	adp->rx_buffer_len = MAXIMUM_ETHERNET_VLAN_SIZE;
	adp->max_frame_size = ETH_MTU + ETH_HLEN + ETH_FCS_LEN;
	adp->min_frame_size = ETH_ZLEN + ETH_FCS_LEN;

	e1000_setup_rings(adp);
	//disable irq
	e1000_irq_disable();

	e1000_reset_hw_82540(0);

	e1000_init_hw_82540(adp);

	e1000_wr32(E1000_VET, ETHERNET_IEEE_VLAN_TYPE);
	e1000_wr32(E1000_AIT, 0);

	e1000_setup_mac(adp);

	e1000_configure(adp);

	e1000_setup_interrupts(adp);

}

int e1000_init(struct _le_softc *le)
{
	pci_dev_t *dev;
	u32 iomem_start, iomem_end;
	u32 retval = 0;
	struct ifnet *ifp = &le->sc_ac.ac_if;
	dev = pci_lookup_dev(0x02, 0x00);
	if (dev == NULL){
		LOG_INFO("can not find ether  controller");
		return -ENODEV;
	}

	LOG_INFO("Ethernet controller found, vendor id:%x dev id:%x irq:%d",
			dev->vendor, dev->device, dev->irq);

	if (dev->vendor != INTEL_VENDOR_ID){
		//only support intel card
		LOG_WARN("not intel card");
	}

	if (!(dev->resource[PCI_ETHER_BAR].flag & RESOURCE_TYPE_MEM)){
		LOG_INFO("Ethernet error, no iomem");
		return -EIO;
	}

	pci_dev_enable_dev(dev);

	pci_dev_set_bus_master(dev);

	iomem_start = dev->resource[PCI_ETHER_BAR].start;
	iomem_end = dev->resource[PCI_ETHER_BAR].end;
	LOG_INFO("start:%x end:%x", iomem_start, iomem_end);
	for (; iomem_start < iomem_end; iomem_start += PAGE_SIZE){
		map_page(iomem_start, iomem_start, get_cr3(), PAGE_PRE|PAGE_RW, NULL);
	}

	adp = kzalloc(sizeof(struct eth_adapter));
	adp->devid = dev->device;
	adp->iomem = (u32 *)dev->resource[PCI_ETHER_BAR].start;
	adp->irq = dev->irq;
	adp->vendor_id = dev->vendor;
	adp->pci = dev;
	adp->unit = ifp->if_unit;
	adp->rx_init_delay = 0;
	adp->tx_init_delay = 0;

	e1000_set_mac_type(adp);

	switch (adp->mac_type) {
	case e1000_82542:
		//e1000_init_82542(adp);
		panic("unsupported ether card");
		break;
	case e1000_82543:
	case e1000_82544:
		//e1000_init_82543(adp);
		panic("unsupported ether card");
		break;
	case e1000_82540:
	case e1000_82545:
	case e1000_82545_rev_3:
	case e1000_82546:
	case e1000_82546_rev_3:
		e1000_init_82540(adp);
		break;
	case e1000_82541:
	case e1000_82541_rev_2:
	case e1000_82547:
	case e1000_82547_rev_2:
		//e1000_init_82541(adp);
		panic("unsupported ether card");
		break;
	default:
		LOG_DEBG("Hardware not supported");
		retval = -1;
		break;
	}


	ifp->if_start = e1000_start;
	ifp->if_reset = e1000_reset_hw_82540;

	ifp->if_softc = (void *)le;

	le->iomem = (void *)adp->iomem;

	memcpy((void *)le->sc_ac.ac_enaddr, (void *)adp->mac_addr, ETHER_ADDR_LEN);
	LOG_INFO("end");
	return retval;
}
