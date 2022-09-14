/*
 * elf.h
 *
 *  Created on: Jan 2, 2013
 *      Author: root
 */

#ifndef ELF_H_
#define ELF_H_

#include <kernel/vfs.h>
#include <kernel/kthread.h>



typedef unsigned short Elf32_Half;
typedef unsigned long Elf32_Word;
typedef unsigned long Elf32_Addr;
typedef unsigned long Elf32_Off;
typedef  long Elf32_Sword;
typedef unsigned long	Elf32_Symndx;
#define EI_NIDENT 16



typedef struct {
	unsigned char e_ident[EI_NIDENT];
	Elf32_Half e_type;
	Elf32_Half e_machine;
	Elf32_Word e_version;
	Elf32_Addr e_entry;
	Elf32_Off e_phoff;
	Elf32_Off e_shoff;
	Elf32_Word e_flags;
	Elf32_Half e_ehsize;
	Elf32_Half e_phentsize;
	Elf32_Half e_phnum;
	Elf32_Half e_shentsize;
	Elf32_Half e_shnum;
	Elf32_Half e_shstrndx;
} Elf32_Ehdr, elfhdr_t;



// for e_type fields
#define ET_NONE	0	//No file type
#define ET_REL	1	//for relocatable file
#define ET_EXEC	2	//for executable file
#define ET_DYN	3	//for shared object file
#define ET_CORE	4	//core file
#define	ET_LOPROC	0xff00	//processor-specific
#define ET_HIPROC	0xffff	//processor-specific

// e_machine fields
#define EM_NONE  0	//No machine
#define EM_M32   1 	// AT&T WE 32100
#define EM_SPARC 2	//sparc
#define EM_386   3	//intel 386
#define EM_68K   4	//motorola 68000
#define EM_88K   5	//motorola 88000
#define EM_860   7	//intel 80860
#define EM_MIPS  8	//mips rs3000

// e_version fields
#define EV_NONE	0	//invalid version
#define EV_CURRENT	1	//current version

#define EI_MAG0	0
#define EI_MAG1	1
#define EI_MAG2	2
#define EI_MAG3	3
#define EI_CLASS	4
#define EI_DATA	5
#define EI_VERSION	6
#define EI_PAD	7

#define ELFMAG0	0x7f	//e_ident[EI_MAG0]
#define ELFMAG1	'E'		//e_ident[EI_MAG1]
#define ELFMAG2	'L'		//e_ident[EI_MAG2]
#define ELFMAG3	'F'		//e_ident[EI_MAG3]

#define ELFCLASSNONE	0	//invalid class
#define ELFCLASS32	1	//32-bit objects	,e_ident[EI_CLASS]
#define ELFCLASS64	2	//64-bit objects

//e_ident[EI_DATA]
#define ELFDATANONE	0
#define ELFDATA2LSB	1
#define ELFDATA2MSB	2


#define PT_NULL    0
#define PT_LOAD    1
#define PT_DYNAMIC 2
#define PT_INTERP  3
#define PT_NOTE    4
#define PT_SHLIB   5
#define PT_PHDR    6
#define PT_TLS     7               /* Thread local storage segment */
#define PT_LOOS    0x60000000      /* OS-specific */
#define PT_HIOS    0x6fffffff      /* OS-specific */
#define PT_LOPROC  0x70000000
#define PT_HIPROC  0x7fffffff
#define PT_GNU_EH_FRAME		0x6474e550


/* This info is needed when parsing the symbol table */
#define STB_LOCAL  0
#define STB_GLOBAL 1
#define STB_WEAK   2

#define STT_NOTYPE  0
#define STT_OBJECT  1
#define STT_FUNC    2
#define STT_SECTION 3
#define STT_FILE    4
#define STT_COMMON	5
#define STT_TLS		6
#define STN_UNDEF	0
#define ELF_ST_BIND(x)		((x) >> 4)
#define ELF_ST_TYPE(x)		(((unsigned int) x) & 0xf)
#define ELF32_ST_BIND(x)	ELF_ST_BIND(x)
#define ELF32_ST_TYPE(x)	ELF_ST_TYPE(x)
#define ELF64_ST_BIND(x)	ELF_ST_BIND(x)
#define ELF64_ST_TYPE(x)	ELF_ST_TYPE(x)


/* sh_type */
#define SHT_NULL	0
#define SHT_PROGBITS	1
#define SHT_SYMTAB	2
#define SHT_STRTAB	3
#define SHT_RELA	4
#define SHT_HASH	5
#define SHT_DYNAMIC	6
#define SHT_NOTE	7
#define SHT_NOBITS	8
#define SHT_REL		9
#define SHT_SHLIB	10
#define SHT_DYNSYM	11
#define SHT_NUM		12
#define SHT_LOPROC	0x70000000
#define SHT_HIPROC	0x7fffffff
#define SHT_LOUSER	0x80000000
#define SHT_HIUSER	0xffffffff

/* sh_flags */
#define SHF_WRITE	0x1
#define SHF_ALLOC	0x2
#define SHF_EXECINSTR	0x4
#define SHF_MASKPROC	0xf0000000

/* special section indexes */
#define SHN_UNDEF	0
#define SHN_LORESERVE	0xff00
#define SHN_LOPROC	0xff00
#define SHN_HIPROC	0xff1f
#define SHN_ABS		0xfff1
#define SHN_COMMON	0xfff2
#define SHN_HIRESERVE	0xffff

typedef struct _ELF32_Shdr {
	Elf32_Word	sh_name;
	Elf32_Word	sh_type;
	Elf32_Word	sh_flags;
	Elf32_Addr	sh_addr;
	Elf32_Off	sh_offset;
	Elf32_Word	sh_size;
	Elf32_Word	sh_link;
	Elf32_Word	sh_info;
	Elf32_Word	sh_addralign;
	Elf32_Word	sh_entsize;
}ELF32_Shdr;

/* These constants define the permissions on sections in the program
   header, p_flags. */
#define PF_R		0x4
#define PF_W		0x2
#define PF_X		0x1

typedef struct elf32_phdr{
  Elf32_Word	p_type;
  Elf32_Off	p_offset;
  Elf32_Addr	p_vaddr;
  Elf32_Addr	p_paddr;
  Elf32_Word	p_filesz;
  Elf32_Word	p_memsz;
  Elf32_Word	p_flags;
  Elf32_Word	p_align;
} Elf32_Phdr, elfphdr_t;

typedef struct elf32_sym{
  Elf32_Word	st_name;
  Elf32_Addr	st_value;
  Elf32_Word	st_size;
  unsigned char	st_info;
  unsigned char	st_other;
  Elf32_Half	st_shndx;
} Elf32_Sym;

typedef struct elf32_rela{
  Elf32_Addr	r_offset;
  Elf32_Word	r_info;
  Elf32_Sword	r_addend;
} Elf32_Rela;

/* The following are used with relocations */
#define ELF32_R_SYM(x) ((x) >> 8)
#define ELF32_R_TYPE(x) ((x) & 0xff)

#define ELF64_R_SYM(i)			((i) >> 32)
#define ELF64_R_TYPE(i)			((i) & 0xffffffff)

typedef struct elf32_rel {
  Elf32_Addr	r_offset;
  Elf32_Word	r_info;
} Elf32_Rel;

typedef struct dynamic{
  Elf32_Sword d_tag;
  union{
    Elf32_Sword	d_val;
    Elf32_Addr	d_ptr;
  } d_un;
} Elf32_Dyn;


#define	DT_NULL		0	/* last entry in list */
#define	DT_NEEDED	1	/* a needed object */
#define	DT_PLTRELSZ	2	/* size of relocations for the PLT */
#define	DT_PLTGOT	3	/* addresses used by procedure linkage table */
#define	DT_HASH		4	/* hash table */
#define	DT_STRTAB	5	/* string table */
#define	DT_SYMTAB	6	/* symbol table */
#define	DT_RELA		7	/* addr of relocation entries */
#define	DT_RELASZ	8	/* size of relocation table */
#define	DT_RELAENT	9	/* base size of relocation entry */
#define	DT_STRSZ	10	/* size of string table */
#define	DT_SYMENT	11	/* size of symbol table entry */
#define	DT_INIT		12	/* _init addr */
#define	DT_FINI		13	/* _fini addr */
#define	DT_SONAME	14	/* name of this shared object */
#define	DT_RPATH	15	/* run-time search path */
#define	DT_SYMBOLIC	16	/* shared object linked -Bsymbolic */
#define	DT_REL		17	/* addr of relocation entries */
#define	DT_RELSZ	18	/* size of relocation table */
#define	DT_RELENT	19	/* base size of relocation entry */
#define	DT_PLTREL	20	/* relocation type for PLT entry */
#define	DT_DEBUG	21	/* pointer to r_debug structure */
#define	DT_TEXTREL	22	/* text relocations remain for this object */
#define	DT_JMPREL	23	/* pointer to the PLT relocation entries */
#define	DT_BIND_NOW	24	/* perform all relocations at load of object */
#define	DT_INIT_ARRAY	25	/* pointer to .initarray */
#define	DT_FINI_ARRAY	26	/* pointer to .finiarray */
#define	DT_INIT_ARRAYSZ	27	/* size of .initarray */
#define	DT_FINI_ARRAYSZ	28	/* size of .finiarray */
#define	DT_RUNPATH	29	/* run-time search path */
#define	DT_FLAGS	30	/* state flags - see DF_* */
#define DT_GNU_HASH	0x6ffffef5
#define DT_GNU_HASH_IDX	0	//
#define DT_NUM	30
#define PATH_LIST_MAX	256


#define	R_386_NONE		0	/* relocation type */
#define	R_386_32		1
#define	R_386_PC32		2
#define	R_386_GOT32		3
#define	R_386_PLT32		4
#define	R_386_COPY		5
#define	R_386_GLOB_DAT		6
#define	R_386_JMP_SLOT		7
#define	R_386_RELATIVE		8
#define	R_386_GOTOFF		9
#define	R_386_GOTPC		10
#define	R_386_32PLT		11
#define	R_386_TLS_GD_PLT	12
#define	R_386_TLS_LDM_PLT	13
#define	R_386_TLS_TPOFF		14
#define	R_386_TLS_IE		15
#define	R_386_TLS_GOTIE		16
#define	R_386_TLS_LE		17
#define	R_386_TLS_GD		18
#define	R_386_TLS_LDM		19
#define	R_386_16		20
#define	R_386_PC16		21
#define	R_386_8			22
#define	R_386_PC8		23
#define	R_386_UNKNOWN24		24
#define	R_386_UNKNOWN25		25
#define	R_386_UNKNOWN26		26
#define	R_386_UNKNOWN27		27
#define	R_386_UNKNOWN28		28
#define	R_386_UNKNOWN29		29
#define	R_386_UNKNOWN30		30
#define	R_386_UNKNOWN31		31
#define	R_386_TLS_LDO_32	32
#define	R_386_UNKNOWN33		33
#define	R_386_UNKNOWN34		34
#define	R_386_TLS_DTPMOD32	35
#define	R_386_TLS_DTPOFF32	36
#define	R_386_UNKNOWN37		37
#define	R_386_NUM		38

#define	ELF_386_MAXPGSZ		0x10000	/* maximum page size */

#define ELF_MOD_TYPE_EXE	0x01
#define ELF_MOD_TYPE_SO		0x02
#define ELF_MOD_TYPE_AOUT	0x03

extern int load_elf(kprocess_t *proc, file_t *file, char *header, unsigned long *entry);
extern int elf_header_check(elfhdr_t * header);

struct elf_module {
	u32 loadaddr;
	u32 magic;
	u32 type;
	u32	dynamic[DT_NUM];
	Elf32_Dyn *dyaddr;
	kprocess_t *proc;
	u32 devid;
	u32 i_no;
	u32 refcnt;
	u32 entry;
	Elf32_Symndx nbucket;
	Elf32_Symndx nchain;
	Elf32_Symndx *elf_buckets;
	Elf32_Symndx *chains;
	struct elf_module *next;

};
extern int get_dynamic_info(struct elf_module *em, Elf32_Dyn *dy, u32 loadoff);
extern int kelf_resolv(void *e, int reloc_entry);
#endif /* ELF_H_ */

