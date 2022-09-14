/*
 * ahci.h
 *
 *  Created on: May 11, 2013
 *      Author: root
 */

#ifndef AHCI_H_
#define AHCI_H_

#include <kernel/types.h>


typedef enum
{
	FIS_TYPE_REG_H2D	= 0x27,	// Register FIS - host to device
	FIS_TYPE_REG_D2H	= 0x34,	// Register FIS - device to host
	FIS_TYPE_DMA_ACT	= 0x39,	// DMA activate FIS - device to host
	FIS_TYPE_DMA_SETUP	= 0x41,	// DMA setup FIS - bidirectional
	FIS_TYPE_DATA		= 0x46,	// Data FIS - bidirectional
	FIS_TYPE_BIST		= 0x58,	// BIST activate FIS - bidirectional
	FIS_TYPE_PIO_SETUP	= 0x5F,	// PIO setup FIS - device to host
	FIS_TYPE_DEV_BITS	= 0xA1,	// Set device bits FIS - device to host
} FIS_TYPE;

typedef enum {
	AHCI_DEV_NULL = 0,
	AHCI_DEV_SATA,
	AHCI_DEV_SATAPI,
	AHCI_DEV_SEMB,
	AHCI_DEV_PM
} AHCI_DEV;


typedef struct tagFIS_REG_H2D
{
	// DWORD 0
	unsigned char	fis_type;	// FIS_TYPE_REG_H2D

	unsigned char	pmport:4;	// Port multiplier
	unsigned char	rsv0:3;		// Reserved
	unsigned char	c:1;		// 1: Command, 0: Control

	unsigned char	command;	// Command register
	unsigned char	featurel;	// Feature register, 7:0

	// DWORD 1
	unsigned char	lba0;		// LBA low register, 7:0
	unsigned char	lba1;		// LBA mid register, 15:8
	unsigned char	lba2;		// LBA high register, 23:16
	unsigned char	device;		// Device register

	// DWORD 2
	unsigned char	lba3;		// LBA register, 31:24
	unsigned char	lba4;		// LBA register, 39:32
	unsigned char	lba5;		// LBA register, 47:40
	unsigned char	featureh;	// Feature register, 15:8

	// DWORD 3
	unsigned char	countl;		// Count register, 7:0
	unsigned char	counth;		// Count register, 15:8
	unsigned char	icc;		// Isochronous command completion
	unsigned char	control;	// Control register

	// DWORD 4
	unsigned char	rsv1[4];	// Reserved
} FIS_REG_H2D;



typedef struct tagFIS_REG_D2H
{
	// DWORD 0
	unsigned char	fis_type;    // FIS_TYPE_REG_D2H

	unsigned char	pmport:4;    // Port multiplier
	unsigned char	rsv0:2;      // Reserved
	unsigned char	i:1;         // Interrupt bit
	unsigned char	rsv1:1;      // Reserved

	unsigned char	status;      // Status register
	unsigned char	error;       // Error register

	// DWORD 1
	unsigned char	lba0;        // LBA low register, 7:0
	unsigned char	lba1;        // LBA mid register, 15:8
	unsigned char	lba2;        // LBA high register, 23:16
	unsigned char	device;      // Device register

	// DWORD 2
	unsigned char	lba3;        // LBA register, 31:24
	unsigned char	lba4;        // LBA register, 39:32
	unsigned char	lba5;        // LBA register, 47:40
	unsigned char	rsv2;        // Reserved

	// DWORD 3
	unsigned char	countl;      // Count register, 7:0
	unsigned char	counth;      // Count register, 15:8
	unsigned char	rsv3[2];     // Reserved

	// DWORD 4
	unsigned char	rsv4[4];     // Reserved
} FIS_REG_D2H;

typedef struct tagFIS_DATA
{
	// DWORD 0
	unsigned char	fis_type;	// FIS_TYPE_DATA

	unsigned char	pmport:4;	// Port multiplier
	unsigned char	rsv0:4;		// Reserved

	unsigned char	rsv1[2];	// Reserved

	// DWORD 1 ~ N
	unsigned long	data[1];	// Payload
} FIS_DATA;


typedef struct tagFIS_PIO_SETUP
{
	// DWORD 0
	unsigned char	fis_type;	// FIS_TYPE_PIO_SETUP

	unsigned char	pmport:4;	// Port multiplier
	unsigned char	rsv0:1;		// Reserved
	unsigned char	d:1;		// Data transfer direction, 1 - device to host
	unsigned char	i:1;		// Interrupt bit
	unsigned char	rsv1:1;

	unsigned char	status;		// Status register
	unsigned char	error;		// Error register

	// DWORD 1
	unsigned char	lba0;		// LBA low register, 7:0
	unsigned char	lba1;		// LBA mid register, 15:8
	unsigned char	lba2;		// LBA high register, 23:16
	unsigned char	device;		// Device register

	// DWORD 2
	unsigned char	lba3;		// LBA register, 31:24
	unsigned char	lba4;		// LBA register, 39:32
	unsigned char	lba5;		// LBA register, 47:40
	unsigned char	rsv2;		// Reserved

	// DWORD 3
	unsigned char	countl;		// Count register, 7:0
	unsigned char	counth;		// Count register, 15:8
	unsigned char	rsv3;		// Reserved
	unsigned char	e_status;	// New value of status register

	// DWORD 4
	unsigned short	tc;		// Transfer count
	unsigned char	rsv4[2];	// Reserved
} FIS_PIO_SETUP;


typedef struct tagFIS_DMA_SETUP
{
	// DWORD 0
	unsigned char	fis_type;	// FIS_TYPE_DMA_SETUP

	unsigned char	pmport:4;	// Port multiplier
	unsigned char	rsv0:1;		// Reserved
	unsigned char	d:1;		// Data transfer direction, 1 - device to host
	unsigned char	i:1;		// Interrupt bit
	unsigned char	a:1;            // Auto-activate. Specifies if DMA Activate FIS is needed

	unsigned char    rsved[2];       // Reserved
	//DWORD 1&2
	unsigned long   DMAbufferID1;    // DMA Buffer Identifier. Used to Identify DMA buffer in host memory. SATA Spec says host specific and not in Spec. Trying AHCI spec might work.
	unsigned long   DMAbufferID2;

	unsigned long   rsvd;           //More reserved

	unsigned long   DMAbufOffset;   //unsigned char offset into buffer. First 2 bits must be 0

	unsigned long   TransferCount;  //Number of bytes to transfer. Bit 0 must be 0

	unsigned long   resvd;          //Reserved


} FIS_DMA_SETUP;

typedef volatile struct tagHBA_PORT
{
	volatile unsigned long	clb;		// 0x00, command list base address, 1K-byte aligned
	volatile unsigned long	clbu;		// 0x04, command list base address upper 32 bits
	volatile unsigned long	fb;		// 0x08, FIS base address, 256-byte aligned
	volatile unsigned long	fbu;		// 0x0C, FIS base address upper 32 bits
	volatile unsigned long	is;		// 0x10, interrupt status
	volatile unsigned long	ie;		// 0x14, interrupt enable
	volatile unsigned long	cmd;		// 0x18, command and status
	volatile unsigned long	rsv0;		// 0x1C, Reserved
	volatile unsigned long	tfd;		// 0x20, task file data
	volatile unsigned long	sig;		// 0x24, signature
	volatile unsigned long	ssts;		// 0x28, SATA status (SCR0:SStatus)
	volatile unsigned long	sctl;		// 0x2C, SATA control (SCR2:SControl)
	volatile unsigned long	serr;		// 0x30, SATA error (SCR1:SError)
	volatile unsigned long	sact;		// 0x34, SATA active (SCR3:SActive)
	volatile unsigned long	ci;		// 0x38, command issue
	volatile unsigned long	sntf;		// 0x3C, SATA notification (SCR4:SNotification)
	volatile unsigned long	fbs;		// 0x40, FIS-based switch control
	volatile unsigned long	rsv1[11];	// 0x44 ~ 0x6F, Reserved
	volatile unsigned long	vendor[4];	// 0x70 ~ 0x7F, vendor specific
} HBA_PORT;
#define HBA_PORT_CLB	0x0
#define HBA_PORT_CLBU	0x04
#define HBA_PORT_FB		0x08
#define HBA_PORT_IS		0x10
#define 	HBA_PORT_IS_DHRS	(1<<0)	//Device to Host Register FIS Interrupt
#define		HBA_PORT_IS_PSS		(1<<1)	//PIO Setup FIS Interrupt
#define 	HBA_PORT_IS_DSS		(1<<2)	//DMA Setup FIS Interrupt
#define 	HBA_PORT_IS_SDBS	(1<<3)	//Set Device Bits Interrupt
#define 	HBA_PORT_IS_UFS		(1<<4)	//Unknown FIS Interrupt
#define 	HBA_PORT_IS_DPS		(1<<5)	//Descriptor Processed
#define		HBA_PORT_IS_PCS		(1<<6)	//Port Connect Change Status
#define		HBA_PORT_IS_DMPS	(1<<7)	//Device Mechanical Presence Status
#define 	HBA_PORT_IS_INFS	(1<<26)	//Interface Non-fatal Error Status (INFS)
#define		HBA_PORT_IS_IFS		(1<<27)	//Interface Fatal Error Status
#define 	HBA_PORT_IS_HBDS	(1<<28)	//Host Bus Data Error Status
#define 	HBA_PORT_IS_HBFS	(1<<29)	//Host Bus Fatal Error Status
#define		HBA_PORT_IS_TFES	(1<<30)	//Task File Error Status
#define 	HBA_PORT_IS_CPDS	(1<<31)	//Cold Port Detect Status
#define		HBA_PORT_IS_ERR_DEF	(HBA_PORT_IS_IFS | HBA_PORT_IS_HBDS | HBA_PORT_IS_HBDS	| \
		HBA_PORT_IS_HBFS | HBA_PORT_IS_TFES )

#define HBA_PORT_IE		0x14
#define 	HBA_PORT_IE_DHRE	(1<<0)	//Device to Host Register FIS Interrupt Enable
#define		HBA_PORT_IE_PSE		(1<<1)	//PIO Setup FIS Interrupt Enable
#define 	HBA_PORT_IE_DSE		(1<<2)	//DMA Setup FIS Interrupt Enable
#define 	HBA_PORT_IE_SDBE	(1<<3)	//Set Device Bits Interrupt Enable
#define 	HBA_PORT_IE_UFE		(1<<4)	//Unknown FIS Interrupt
#define 	HBA_PORT_IE_DPE		(1<<5)	//Descriptor Processed Interrupt Enable
#define		HBA_PORT_IE_PCE		(1<<6)	//Port Connect Change Interrupt Enable
#define		HBA_PORT_IE_DMPE	(1<<7)	//Device Mechanical Presence Interrupt Enable
#define		HBA_PORT_IE_DEFAULT	(0xFF)

#define HBA_PORT_CMD	0x18
#define 	HBA_PORT_CMD_ST		(1<<0)		//start
#define 	HBA_PORT_CMD_SUD	(1<<1)	//spin-up device
#define 	HBA_PORT_CMD_POD	(1<<2)	//power on device
#define 	HBA_PORT_CMD_CLO	(1<<3)	//command list override
#define 	HBA_PORT_CMD_FRE	(1<<4)	//FIS receive enable
#define 	HBA_PORT_CMD_CCS	(1<<8)	//current command slot ,8-12
#define 	HBA_PORT_CMD_MPSS	(1<<13)	//Mechanical Presence Switch State
#define 	HBA_PORT_CMD_FR		(1<<14)	//FIS Receive running
#define 	HBA_PORT_CMD_CR		(1<<15)	//Command list running
#define 	HBA_PORT_CMD_CPS	(1<<16)	//cold presence state
#define 	HBA_PORT_CMD_PAM	(1<<17)	//Port Multiplier Attached
#define 	HBA_PORT_CMD_HPCP	(1<<18) //Hot Plug Capable Port
#define 	HBA_PORT_CMD_MPSP	(1<<19)	//Mechanical Presence Switch Attached to Port
#define 	HBA_PORT_CMD_CPD	(1<<20)	//Cold Presence Detection
#define 	HBA_PORT_CMD_ESP	(1<<21)	//External SATA Port
#define 	HBA_PORT_CMD_FBSCP	(1<<22)	//FIS-based Switching Capable Port
#define 	HBA_PORT_CMD_APSTE	(1<<23)	//Automatic Partial to Slumber Transitions Enabled
#define 	HBA_PORT_CMD_ATAPI	(1<<24)	//Device is ATAPI
#define 	HBA_PORT_CMD_DLAE	(1<<25)	//Drive LED on ATAPI Enable
#define 	HBA_PORT_CMD_ALPE	(1<<26)	//Aggressive Link Power Management Enable
#define 	HBA_PORT_CMD_ASP	(1<<27)	//Aggressive Slumber / Partial
#define 	HBA_PORT_CMD_ICC	(1<<28)	//Interface Communication Control
#define HBA_PORT_TFD	0x20
#define 	HBA_PORT_TFD_ERR	(1<<0)
#define		HBA_PORT_TFD_DRQ	(1<<3)
#define		HBA_PORT_TFD_BSY	(1<<7)

#define HBA_PORT_SIG	0x24
#define HBA_PORT_SSTS	0x28
#define 	HBA_PORT_SSTS_DET_MASK	(0x0F)

#define HBA_PORT_SCTL	0x2C
#define		HBA_PORT_SCTL_DET_MASK	(0x0F)

#define HBA_PORT_SERR	0x30
#define HBA_PORT_SACT	0x34
#define HBA_PORT_CI		0x38
#define HBA_PORT_SNTF	0x3c
#define	HBA_PORT_FBS	0x40
#define HBA_PORT_VENDOR	0x70



/*
 The following registers apply to the entire HBA.
	Start End Symbol Description
	00h 03h CAP Host Capabilities
	04h 07h GHC Global Host Control
	08h 0Bh IS Interrupt Status
	0Ch 0Fh PI Ports Implemented
	10h 13h VS Version
	14h 17h CCC_CTL Command Completion Coalescing Control
	18h 1Bh CCC_PORTS Command Completion Coalsecing Ports
	1Ch 1Fh EM_LOC Enclosure Management Location
	20h 23h EM_CTL Enclosure Management Control
	24h 27h CAP2 Host Capabilities Extended
	28h 2Bh BOHC BIOS/OS Handoff Control and Status
 */

typedef volatile struct tagHBA_MEM
{
	// 0x00 - 0x2B, Generic Host Control
	volatile unsigned long	cap;		// 0x00, Host capability
	volatile unsigned long	ghc;		// 0x04, Global host control
	volatile unsigned long	ips;		// 0x08, Interrupt pending status
	volatile unsigned long	pi;		// 0x0C, Port implemented
	volatile unsigned long	vs;		// 0x10, Version
	volatile unsigned long	ccc_ctl;	// 0x14, Command completion coalescing control
	volatile unsigned long	ccc_pts;	// 0x18, Command completion coalescing ports
	volatile unsigned long	em_loc;		// 0x1C, Enclosure management location
	volatile unsigned long	em_ctl;		// 0x20, Enclosure management control
	volatile unsigned long	cap2;		// 0x24, Host capabilities extended
	volatile unsigned long	bohc;		// 0x28, BIOS/OS handoff control and status

	// 0x2C - 0x9F, Reserved
	volatile unsigned char	rsv[0xA0-0x2C];

	// 0xA0 - 0xFF, Vendor specific registers
	volatile unsigned char	vendor[0x100-0xA0];

	// 0x100 - 0x10FF, Port control registers
	volatile HBA_PORT	ports[1];	// 1 ~ 32
} HBA_MEM;

#define HBA_MEM_CAP	0x0
#define 	HBA_MEM_CAP_NP_MASK	(0x1F)
#define 	HBA_MEM_CAP_ESATA	(1<<5)	//esata
#define 	HBA_MEM_CAP_EMS		(1<<6)	//Enclosure Management Supported
#define		HBA_MEM_CAP_CCCS	(1<<7)	//Command Completion Coalescing Supported
#define 	HBA_MEM_CAP_NCS_MASK	(0x1F00)	//Number of Command Slots
#define		HBA_MEM_CAP_PSC		(1<<13)	//Partial State Capable
#define 	HBA_MEM_CAP_SSC		(1<<14)	//Slumber State Capable
#define		HBA_MEM_CAP_PMD		(1<<15)	//PIO Multiple DRQ Block
#define 	HBA_MEM_CAP_FBSS	(1<<16)	//FIS-based Switching Supported
#define 	HBA_MEM_CAP_SPM		(1<<17)	//Supports Port Multiplier
#define		HBA_MEM_CAP_SAM		(1<<18)	//Supports AHCI mode only
#define 	HBA_MEM_CAP_ISS_MASK	(0xF00000)	//Interface Speed Support
#define 	HBA_MEM_CAP_SCLO	(1<<24)	//Supports Command List Override
#define		HBA_MEM_CAP_SAL		(1<<25)	//Supports Activity LED
#define		HBA_MEM_CAP_SALP	(1<<26)	//Supports Aggressive Link Power Management
#define		HBA_MEM_CAP_SSS		(1<<27)	//Supports Staggered Spin-up
#define 	HBA_MEM_CAP_SMPS	(1<<28)	//Supports Mechanical Presence Switch
#define 	HBA_MEM_CAP_SSNTF	(1<<29)	//Supports SNotification Register
#define 	HBA_MEM_CAP_SNCQ	(1<<30)	//Supports Native Command Queuing
#define 	HBA_MEM_CAP_S64A	(1<<31)	//Supports 64-bit Addressing


#define HBA_MEM_GHC	0x04
#define 	HBA_MEM_GHC_HR		(1<<0)	//HBA Reset
#define		HBA_MEM_GHC_IE		(1<<1)	//Interrupt Enable
#define		HBA_MEM_GHC_MRSM	(1<<2)	//MSI Revert to Single Message
#define 	HBA_MEM_GHC_AE		(1<<31)	//AHCI Enable


#define HBA_MEM_IPS	0x08
#define HBA_MEM_PI	0x0C
#define HBA_MEM_VS	0x10
#define HBA_MEM_CCC_CTL	0x14
#define HBA_MEM_CCC_PTS	0x18
#define HBA_MEM_EM_LOC	0x1C
#define HBA_MEM_EN_CTL	0x20
#define HBA_MEM_CAP2	0x24
#define HBA_MEM_BOHC	0x28




typedef volatile struct tagHBA_FIS
{
	// 0x00
	FIS_DMA_SETUP	dsfis;		// DMA Setup FIS
	unsigned char		pad0[4];

	// 0x20
	FIS_PIO_SETUP	psfis;		// PIO Setup FIS
	unsigned char		pad1[12];

	// 0x40
	FIS_REG_D2H	rfis;		// Register Device to Host FIS
	unsigned char		pad2[4];

	// 0x58
	unsigned short	sdbfis;		// Set Device Bit FIS

	// 0x60
	unsigned char		ufis[64];

	// 0xA0
	unsigned char		rsv[96];
} HBA_FIS;


typedef struct tagHBA_CMD_HEADER
{
	// DW0
	unsigned char	cfl:5;		// Command FIS length in DWORDS, 2 ~ 16
	unsigned char	a:1;		// ATAPI
	unsigned char	w:1;		// Write, 1: H2D, 0: D2H
	unsigned char	p:1;		// Prefetchable

	unsigned char	r:1;		// Reset
	unsigned char	b:1;		// BIST
	unsigned char	c:1;		// Clear busy upon R_OK
	unsigned char	rsv0:1;		// Reserved
	unsigned char	pmp:4;		// Port multiplier port

	unsigned short	prdtl;		// Physical region descriptor table length in entries

	// DW1
	volatile
	unsigned long	prdbc;		// Physical region descriptor byte count transferred

	// DW2, 3
	unsigned long	ctba;		// Command table descriptor base address
	unsigned long	ctbau;		// Command table descriptor base address upper 32 bits

	// DW4 - 7
	unsigned long	rsv1[4];	// Reserved
} HBA_CMD_HEADER;

typedef struct tagHBA_PRDT_ENTRY
{
	unsigned long	dba;		// Data base address
	unsigned long	dbau;		// Data base address upper 32 bits
	unsigned long	rsv0;		// Reserved

	// DW3
	unsigned long	dbc:22;		// Byte count, 4M max
	unsigned long	rsv1:9;		// Reserved
	unsigned long	i:1;		// Interrupt on completion
} HBA_PRDT_ENTRY;

typedef struct tagHBA_CMD_TBL
{
	// 0x00
	unsigned char	cfis[64];	// Command FIS

	// 0x40
	unsigned char	acmd[16];	// ATAPI command, 12 or 16 unsigned chars

	// 0x50
	unsigned char	rsv[48];	// Reserved

	// 0x80
	HBA_PRDT_ENTRY	prdt_entry[1];	// Physical region descriptor table entries, 0 ~ 65535
} HBA_CMD_TBL;



#define	SATA_SIG_ATA	0x00000101	// SATA drive
#define	SATA_SIG_ATAPI	0xEB140101	// SATAPI drive
#define	SATA_SIG_SEMB	0xC33C0101	// Enclosure management bridge
#define	SATA_SIG_PM		0x96690101	// Port multiplier

//Interface Power Management (IPM): Indicates the current interface state:
//08:11
#define HBA_PORT_IPM_NOPRESENT 0x00
#define HBA_PORT_IPM_NOESTABLISHED 0x00
#define HBA_PORT_IPM_ACTIVE 0x01
#define HBA_PORT_IPM_PAITIAL 0x02
#define HBA_PORT_IPM_SLUMBER 0x06

//Device Detection (DET): Indicates the interface device detection and Phy state
//00:03
#define HBA_PORT_DET_NOPRESENT	0x00
#define HBA_PORT_DET_PRESENT_COMMU_NG 0x01
#define HBA_PORT_DET_PRESENT_COMMU_OK	0x03
#define HBA_PORT_DET_DISABLED	0x06

#define HBA_PxCMD_CR	(1<<15UL)
#define HBA_PxCMD_FRE	(1<<4UL)
#define HBA_PxCMD_ST	(0x01)
#define HBA_PxCMD_FR	(1<<14UL)
#define HBA_PxIS_TFES	(1<<30UL)


#define PCI_AHCI_BAR	5
#define AHCI_MAX_PORT_COUNT 32


typedef struct _ahci_adapter ahci_adapter_t;
typedef struct _ahci_queue_command qc_t;
typedef struct _ahci_port ahci_port_t;


struct _ahci_queue_command {
	ahci_port_t *ap;
	u8 tag;
	void *private_data;
	void (*complete)(struct _ahci_queue_command *);
} ;



 struct _ahci_port {
	ahci_adapter_t *adapter;
	HBA_PORT	*port;
	HBA_CMD_HEADER *cmd_header;
	u32 type;
	u32	curr_slot;
	u32 sactive;
	struct _ahci_queue_command cmd_queue[32];
};

 struct _ahci_adapter {
	u32 base_address;
	u32 vendor_id;
	u32 device_id;
	u32 error;
	u32 state;
	u32 port_impl;
	u32 nr_ports;
	HBA_MEM *abar;
	u32 version;
	u32 is;
	u32 cap;
	u32 cap2;
	u8 irq;
	ahci_port_t *ports[AHCI_MAX_PORT_COUNT];
};






int ahci_init();
int ahci_read(ahci_port_t *ap, unsigned long startl, unsigned long starth, unsigned long count, char *buf);



#endif /* AHCI_H_ */
