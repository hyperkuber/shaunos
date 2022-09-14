/*
 * elf.c
 *
 *  Created on: Jan 2, 2013
 *      Author: Shaun Yuan
 *     Copyright (c) 2013 Shaun Yuan
 */
#include <kernel/kernel.h>
#include <kernel/elf.h>
#include <kernel/page.h>
#include <string.h>
#include <unistd.h>
#include <kernel/vfs.h>
#include <kernel/malloc.h>
#include <kernel/assert.h>

//we put the resolv stub func in env page temporally,
static u32 *elf_tramp_addr = (u32 *)0xBFFFEC00;
static u8	elf_trampoline[] = {
#include "elf_tramp.h"
};

#define INIT_GOT(PROC, GOT, MODULE)	\
	do {	\
		copy_to_proc(PROC, (s8 *)&(GOT[1]), (s8 *)&MODULE, 4);	\
		copy_to_proc(PROC, (s8 *)&(GOT[2]), (s8 *)&elf_tramp_addr, 4); \
	} while (0)

s8 *ld_library_path = NULL;

int load_elf_interp(s8 *name)
{
	return 0;
}

u32 elf_hash(s8 *name)
{
	s8 *p = name;
	u32 hash = 0;
	u32 tmp;
	while  (*p) {
		hash = (hash << 4) + *p++;
		tmp = hash & 0xf0000000;
		hash ^= tmp;
		hash ^= tmp >> 24;
	}

//	while (*p) {
//		hash = (hash<<4) +*p++;
//		if ((tmp=(hash & 0xf0000000)))
//			hash ^= tmp>>24;
//		hash &= ~tmp;
//	}

	return hash;
}

Elf32_Symndx elf_hash_gnu(s8 *name)
{
	u32 h = 5381;
	s8 c;
	for (c=*name; c != '\0'; c=*++name)
		h = h*33 + c;

	return h & 0xFFFFFFFF;
}


static void inline
init_elf_tramp(struct elf_module *em)
{
	copy_to_proc(em->proc, (s8 *)elf_tramp_addr, (s8*)elf_trampoline, sizeof(elf_trampoline));
}

Elf32_Sym *
sym_match(Elf32_Sym *sym, s8 *strtab, s8 *name)
{
	if (sym->st_value == 0)
		return NULL;

	if (sym->st_shndx == STN_UNDEF)
		return NULL;

	if (ELF32_ST_TYPE(sym->st_info) > STT_FUNC &&
			ELF32_ST_TYPE(sym->st_info) != STT_COMMON)
		return NULL;


	if (strcmp(strtab+sym->st_name, name) != 0)
		return 0;

	return sym;
}


Elf32_Sym *
elf_lookup_hash(struct elf_module *em, Elf32_Sym *symtab, u32 hashnum, s8 *name)
{
	Elf32_Sym *sym;
	Elf32_Symndx symidx;
	s8 *strtab;
	u32 hn;

#define do_rem(result, n, base) ((result)=(n)%(base))

	do_rem(hn, hashnum, em->nbucket);
	strtab = (s8*)em->dynamic[DT_STRTAB];

	for (symidx=em->elf_buckets[hn]; symidx != STN_UNDEF; symidx = em->chains[symidx]) {
		sym = sym_match(&symtab[symidx], strtab, name);
		if (sym != NULL)
			return sym;
	}

	return NULL;
}

Elf32_Sym *
elf_lookup_hash_gnu(struct elf_module *em, Elf32_Sym *symtab, u32 hashnum, s8 *name)
{
	return NULL;
}

s8 *elf_find_hash(s8 *name, struct elf_module *em, int reloc_type)
{
	struct elf_module *em0;
	Elf32_Sym *symtab, *sym;
	u32 hash_num;
	for (em0=em; em0; em0=em0->next) {
		if (em0->nbucket == 0)
			continue;

		if (reloc_type == R_386_COPY && em0->type == ELF_MOD_TYPE_EXE)
			continue;

		symtab = (Elf32_Sym *)em0->dynamic[DT_SYMTAB];

		hash_num = elf_hash_gnu(name);
		sym = elf_lookup_hash_gnu(em0, symtab, hash_num, name);
		if (sym)
			break;

		hash_num = elf_hash(name);

		sym = elf_lookup_hash(em0, symtab, hash_num, name);
		if (sym != NULL)
			break;

	}

	if (sym) {
		switch (ELF32_ST_BIND(sym->st_info)) {
		case STB_WEAK:
		case STB_GLOBAL:
			return (s8 *)sym->st_value;
		}
	}

	return NULL;
}


int kelf_resolv(void *e, int reloc_entry)
{
	Elf32_Rel *reloc;
	Elf32_Sym *symtab;
	int sym_index,reloc_type;
	s8 *rel_addr;
	s8 *strtab;
	s8 *symname;
	u32 instr_addr;
	s8 *new_addr;
	s8 **gotaddr;

	struct elf_module *em = (struct elf_module *)e;
	if (em->magic != 0xdeadbeaf) {
		LOG_ERROR("invalid elf module.");
	}

	rel_addr = (s8 *)em->dynamic[DT_JMPREL];
	reloc = (Elf32_Rel *)(rel_addr + reloc_entry);
	reloc_type = ELF32_R_TYPE(reloc->r_info);
	sym_index = ELF32_R_SYM(reloc->r_info);

	symtab = (Elf32_Sym *)em->dynamic[DT_SYMTAB];
	strtab = (s8 *)em->dynamic[DT_STRTAB];
	symname = strtab + symtab[sym_index].st_name;

	instr_addr = (u32)reloc->r_offset;
	gotaddr = (s8 **)instr_addr;

	//FIX:
	new_addr = elf_find_hash(symname, em->next, reloc_type);
	if (!new_addr) {
		LOG_ERROR("can not resolve symbol :%s", symname);
		return -1;
	}

	*gotaddr = new_addr;

	return (u32)new_addr;
}

int load_elf_shared_library(struct elf_module *em, s8 *fullname)
{
	int ret = -1;
	file_t *file;
	struct elf_module *em0, **p;
	s8 *header = NULL, *addr = NULL;
	elfphdr_t *ph = NULL;
	elfhdr_t *hdr;
	Elf32_Dyn *dy = NULL;
	int phsize;
	int i;
	int ispic = 1;
	int flag = PAGE_PRE | PAGE_USR;
	u32 *got;
	u32 mapaddr = 0;
	Elf32_Symndx *hash_addr;
	ret = vfs_open(em->proc->p_currdir, fullname, O_RDONLY, &file);
	if (ret < 0) {
		LOG_ERROR("open file:%s failed", fullname);
		return ret;
	}

	for (em0=em; em0; em0=em->next) {
		if (em0->devid == file->f_vnode->v_device && em0->i_no == file->f_vnode->v_id) {
			//already loaded
			vfs_close(file);
			em0->refcnt++;
			return 0;
		}
	}

	header = kzalloc(128);
	if (!header) {
		ret = -ENOMEM;
		goto end;
	}

	ret = vfs_read(file, (void *)header, 128);
	if (ret < 0) {
		LOG_ERROR("load elf header failed");
		goto end;
	}

	if ((ret=elf_header_check((elfhdr_t *)header)) < 0) {
		LOG_ERROR("invalid elf header");
		goto end;
	}
	hdr = (elfhdr_t *)header;
	phsize = hdr->e_phnum * hdr->e_phentsize;
	ph = (elfphdr_t *)kzalloc(phsize);
	if (!ph) {
		ret = -ENOMEM;
		goto end;
	}
	vfs_seek(file, hdr->e_phoff, SEEK_SET);
	ret = vfs_read(file, (void *)ph, phsize);
	if (ret != phsize){
		LOG_ERROR("read program header error");
		goto end;
	}


	for (i=0; i<hdr->e_phnum; i++) {

		if (ph[i].p_type == PT_LOAD) {
			//figure out whether it is a pic lib or not
			if (ph[i].p_vaddr > 0x1000000) {
				//not a pic lib, why?
				ispic = 0;
			}

			addr = (s8 *)kzalloc(ph[i].p_memsz);
			assert(addr != NULL);
			vfs_seek(file,  ph[i].p_offset, SEEK_SET);
			ret = vfs_read(file, (void *)addr, ph[i].p_filesz);
			if (ret < 0)
				goto end;

			if (ph[i].p_flags & PF_W)
				flag |= PAGE_RW;

			if (map_pages(ph[i].p_vaddr, ph[i].p_memsz, flag, em->proc) < 0) {
				ret = -EFAULT;
				LOG_ERROR("can not map addr:%s", ph[i].p_vaddr);
				goto end;
			}

			if (copy_to_proc(em->proc, (s8*)ph[i].p_vaddr, (s8 *)addr, ph[i].p_memsz) < 0) {
				ret = -EFAULT;
				LOG_ERROR("copy process data to  user space error");
				goto end;
			}
			kfree(addr);
			addr = NULL;
			if (!mapaddr)
				mapaddr = ph[i].p_vaddr - ph[i].p_offset;
		}

		if (ph[i].p_type == PT_DYNAMIC) {
			dy = (Elf32_Dyn *)kzalloc(ph[i].p_memsz);
			assert(dy != NULL);

			ret = copy_from_proc(em->proc, (s8 *)dy, (s8 *)ph[i].p_vaddr, ph[i].p_memsz);
			if (ret < 0) {
				LOG_ERROR("copy dynamic section error");
				goto end;
			}
		}
	}


	//now, we have loaded a new module, alloc a em for it.
	em0 = (struct elf_module *)kzalloc(sizeof(struct elf_module));
	if (!em0) {
		ret = -ENOMEM;
		goto end;
	}

	p = &em->next;
	while ((*p))
		p = &(*p)->next;
	*p = em0;

	em0->loadaddr = mapaddr;
	em0->dyaddr = dy;
	em0->type = ELF_MOD_TYPE_SO;
	em0->proc = em->proc;
	em0->i_no = file->f_vnode->v_id;
	em0->devid = file->f_vnode->v_device;
	em0->magic = 0xdeadbeaf;
	get_dynamic_info(em0, dy, 0);

	if (em0->dynamic[DT_HASH] != 0) {
		hash_addr = (Elf32_Symndx *)em0->dynamic[DT_HASH];
		copy_from_proc(em0->proc, (s8 *)&em0->nbucket, (s8 *)hash_addr, 4);
		hash_addr++;
		copy_from_proc(em0->proc, (s8 *)&em0->nchain, (s8 *)hash_addr, 4);
		hash_addr++;
		em0->elf_buckets = hash_addr;
		hash_addr += em0->nbucket;
		em0->chains = hash_addr;
	}


	//init got
	got = (u32 *)em0->dynamic[DT_PLTGOT];
	if (got) {
		LOG_INFO("got:%x", (u32)got);
		INIT_GOT(em0->proc, got, em0);
	}

	ret = 0;
	goto end1;
end:
	if (dy)
		kfree(dy);
end1:
	if (addr)
		kfree(addr);
	if (ph)
		kfree(ph);
	if (header)
		kfree(header);
	vfs_close(file);
	return ret;

}

int search_named_lib(struct elf_module *em, s8 *name, s8 *path_list)
{
	int ret = -1;
	s8 *p = path_list;
	int done = 0;
	s8 pbuf[256];
	s8 *mylib = path_list;

	do {
		if (*p == 0) {
			*p = ':';
			done = 1;
		}

		if (*p == ':') {
			*p = 0;
			memset(pbuf, 0, 256);
			if (*mylib) {
				//we should add a guard for mylib length here
				strcpy(pbuf, mylib);
				strcat(pbuf, "/");
				strcat(pbuf, name);
				LOG_INFO("loading lib:%s", pbuf);
				ret = load_elf_shared_library(em, pbuf);
				if (ret >= 0) {
					goto end;
				}

			} else {
				strcpy(pbuf, "./");
				strcat(pbuf, name);
				LOG_INFO("loading lib:%s", pbuf);
				ret = load_elf_shared_library(em, pbuf);
				if (ret >= 0) {
					goto end;
				}
			}
			mylib = p+1;
		}
		p++;
	} while (!done);

end:
	return ret;
}


int search_shared_library(struct elf_module *em, s8 *path)
{
	s8 *name;
	s8 path_list[PATH_LIST_MAX];
	s8 *ptr;
	int ret;
	name = strrchr(path, '/');
	if (name)
		name++;	//skip '/'
	else	//no '/' found
		name = path;

	//Based on ABI Spec. rpath searched first.
	ptr = (s8 *)em->dynamic[DT_RPATH];
	if (ptr) {
		ptr += em->dynamic[DT_STRTAB];
		ret = copy_from_proc(em->proc, path_list, ptr, PATH_LIST_MAX);
		if (ret < 0) {
			LOG_ERROR("get rpath from proc space error");
			goto end;
		}

		LOG_INFO("searching for path:%s", path_list);
		ret = search_named_lib(em, name, path_list);
		if (ret >= 0)	//we get it, return
			goto end;
	}

	//second, LD_LIBRARY_PATH,
	if (ld_library_path) {
		ret = search_named_lib(em, name, ld_library_path);
		if (ret >= 0)
			goto end;
	}

	//third, RUNPATH
	ptr = (s8 *)em->dynamic[DT_RUNPATH];
	if (ptr) {
		ptr += em->dynamic[DT_STRTAB];
		ret = copy_from_proc(em->proc, path_list, ptr, PATH_LIST_MAX);
		if (ret < 0) {
			LOG_ERROR("copy runpath from proc space error");
			goto end;
		}
		LOG_INFO("searching for runpath:%s", path_list);
		ret = search_named_lib(em, name, path_list);
		if (ret >= 0)
			goto end;
	}

	//forth, search in cache in theory,not yet implemented.
	//fifth, search in default path, /lib,/usr/lib
	memset(path_list, 0, PATH_LIST_MAX);
	strcpy(path_list, "/lib:/usr/lib");
	LOG_INFO("searching for default path:%s", path_list);
	ret = search_named_lib(em, name, path_list);
	if (ret < 0) {
		LOG_ERROR("can not find lib:%s", name);
		goto end;
	}
	LOG_INFO("load lib:%s ok", name);
end:
	return ret;
}


int elf_do_reloc(struct elf_module *em, Elf32_Rel *rloc)
{
	Elf32_Sym *symtab;
	int symtab_index, reloc_type;
	s8 *strtab;
	s8 *symname;
	u32 *reloc_addr;
	u32 symaddr;

	symtab = (Elf32_Sym *)em->dynamic[DT_SYMTAB];
	strtab = (s8 *)em->dynamic[DT_STRTAB];

	reloc_addr = (u32 *)rloc->r_offset;
	reloc_type = ELF32_R_TYPE(rloc->r_info);
	symtab_index = ELF32_R_SYM(rloc->r_info);
	symaddr = 0;

	symname = (s8 *)(strtab + symtab[symtab_index].st_name);
	if (symtab_index) {
		symaddr = (u32)elf_find_hash(symname, em, reloc_type);
		if (!symaddr)
			return -1;

	}else {
		symaddr = symtab[symtab_index].st_value;
	}

	switch (reloc_type) {
	case R_386_NONE:
		break;
	case R_386_32:
		*reloc_addr += symaddr;
		break;
	case R_386_PC32:
		*reloc_addr += (symaddr - (u32)reloc_addr);
		break;
	case R_386_GLOB_DAT:
	case R_386_JMP_SLOT:
		*reloc_addr = symaddr;
		break;
	case R_386_RELATIVE:
		*reloc_addr += 0;//(u32) em->loadaddr;
		break;
	case R_386_COPY:
		if (reloc_addr) {
			memcpy((s8 *)reloc_addr,
				   (s8 *)symaddr,
				   symtab[symtab_index].st_size);
			LOG_INFO("reloc addr:%x symaddr:%x", reloc_addr, symaddr);
		}
		break;
	default:
		return -1;

	}
	return 0;
}

int elf_reloc(struct elf_module *em)
{
	int ret;
	Elf32_Rel *reloctab;
	u32 reloc_size;
	int reloc_cnt, i;
	Elf32_Rel *rloc;

	if (em->next)
		ret = elf_reloc(em->next);

	reloc_size = em->dynamic[DT_RELSZ];
	reloctab = (Elf32_Rel *)em->dynamic[DT_REL];
	if (reloctab) {
		reloc_cnt = reloc_size / sizeof(Elf32_Rel);
		rloc = reloctab;
		for (i=0; i<reloc_cnt; i++, rloc++) {
			ret += elf_do_reloc(em, rloc);
		}
	}

	if (ret < 0) {
		LOG_INFO("ret:%d", ret);
	}

	return 0;
}

int resolv_depends(struct elf_module *em, Elf32_Dyn *dy)
{
	s8 path[32] = {0};
	Elf32_Dyn *dy0 = dy;
	struct elf_module *em0 = NULL;
	int ret = 0;

	//first, we load library needed,
	for (em0=em; em0; em0=em0->next) {
		for (dy0=em0->dyaddr; dy0->d_tag; dy0++) {

			if (dy0->d_tag == DT_NEEDED) {
				s8 *name;
				name = (s8 *)(em0->dynamic[DT_STRTAB] + dy0->d_un.d_val);
				if (copy_from_proc(em0->proc, path, name, 32) < 0) {
					LOG_ERROR("copy lib name error");
					ret = -EFAULT;
					goto end;
				}

				if (strcmp(path, "ld-linux.so.2") == 0)
					continue;

				LOG_INFO("lib:%s is needed.", path);
				if ((ret = search_shared_library(em0, path)) < 0) {
					LOG_ERROR("load lib:%s error,%d", path, ret);
					goto end;
				}

			}
		}
	}

	//second, do reloc stuff
	//em0 = em->next;
	ret = elf_reloc(em);
	LOG_INFO("elf reloc ret:%d", ret);

end:
	return ret;
}

int elf_header_check(elfhdr_t * header)
{

	elfhdr_t	*hdr = (elfhdr_t *)header;
	if (header == NULL)
		return -1;

	if (hdr->e_ident[EI_MAG0] != ELFMAG0 ||
				hdr->e_ident[EI_MAG1] != ELFMAG1 ||
				hdr->e_ident[EI_MAG2] != ELFMAG2 ||
				hdr->e_ident[EI_MAG3] != ELFMAG3 ){
		LOG_ERROR("invalid elf format");
		return -1;

	}

	if (hdr->e_type != ET_EXEC && hdr->e_type != ET_DYN){
		LOG_ERROR("invalid elf type");
		return -1;
	}

	if (hdr->e_machine != EM_386){
		LOG_ERROR("invalid elf arch");
		return -1;
	}

	if (hdr->e_phentsize != sizeof (elfphdr_t)){
		LOG_ERROR("invalid elf program header size");
		return -1;
	}

	if (hdr->e_phnum < 1 ||
			hdr->e_phnum > 65535U / sizeof(elfphdr_t)){
		LOG_ERROR("invalid program header numbers");
		return -1;
	}
	return 0;
}


int get_dynamic_info(struct elf_module *em, Elf32_Dyn *dy, u32 loadoff)
{
	Elf32_Dyn *dy0;

	for (dy0=dy; dy0->d_tag; dy0++){
		if (dy0->d_tag < DT_NUM) {
			em->dynamic[dy0->d_tag] = dy0->d_un.d_val;
			if (dy0->d_tag == DT_BIND_NOW)
				em->dynamic[DT_BIND_NOW] = 1;
			if (dy0->d_tag == DT_TEXTREL)
				em->dynamic[DT_TEXTREL] = 1;
		}

		if (dy0->d_tag == DT_GNU_HASH)
			em->dynamic[DT_GNU_HASH_IDX] = dy0->d_un.d_ptr;

	}

	em->dynamic[DT_PLTGOT] = loadoff + em->dynamic[DT_PLTGOT];
	em->dynamic[DT_STRTAB] = loadoff + em->dynamic[DT_STRTAB];
	em->dynamic[DT_SYMTAB] = loadoff + em->dynamic[DT_SYMTAB];
	em->dynamic[DT_REL] = loadoff + em->dynamic[DT_REL];
	em->dynamic[DT_JMPREL] = loadoff + em->dynamic[DT_JMPREL];
	em->dynamic[DT_HASH] = loadoff + em->dynamic[DT_HASH];
	return 0;
}

int load_elf(kprocess_t *proc, file_t *file, char *header, u32 *entry)
{
	elfhdr_t	*hdr = (elfhdr_t *)header;
	elfphdr_t *ph = NULL;
	int i;
	int flag = 0;
	u32 k = 0;
	int load_addr_set = 0, load_addr;
	u32 code_start = ~0UL, data_start = 0, bss_start = 0;
	u32 code_end = 0, data_end = 0, bss_end = 0;
	s8 *addr = NULL;
	s8 *interp = NULL;
	u32 *got;
	Elf32_Symndx *hash_addr;
	int size = hdr->e_phnum * sizeof (elfphdr_t);
	struct elf_module *em = NULL;

	if (elf_header_check(hdr) < 0){
		return -EINVAL;
	}
	if (hdr->e_type == ET_DYN) {
		LOG_ERROR("kernel does not support running dynamic library directly");
		return -ELIBEXEC;
	}
	ph = (elfphdr_t *)kmalloc(size);
	assert(ph != NULL);
	em = (struct elf_module *)kzalloc(sizeof(*em));
	assert(em != NULL);

	vfs_seek(file, hdr->e_phoff, SEEK_SET);
	int ret = vfs_read(file, (void *)ph, size);
	if (ret != size){
		LOG_ERROR("read program header error");
		goto END;
	}

	for (i=0; i<hdr->e_phnum; i++){
		if (ph[i].p_type == PT_LOAD){
			LOG_INFO("elf program header:%d, vaddr:%x, p_offset:%x", i, ph[i].p_vaddr, ph[i].p_offset);
			addr = (s8 *)kzalloc(ph[i].p_memsz);
			assert(addr != NULL);
			vfs_seek(file,  ph[i].p_offset, SEEK_SET);
			ret = vfs_read(file, (void *)addr, ph[i].p_filesz);
			if (ret < 0)
				goto err_out;
			//prepare for process address space

			if (ph[i].p_flags & PF_W)
				flag |= PAGE_RW;
			map_pages(ph[i].p_vaddr, ph[i].p_memsz, flag, proc);

			if (copy_to_proc(proc, (s8*)ph[i].p_vaddr, (s8 *)addr, ph[i].p_memsz) < 0) {
				ret = -EFAULT;
				LOG_ERROR("copy process data to  user space error");
				goto err_out;
			}
			kfree(addr);
			addr = NULL;
			//figure out the code_start, data_start, bss_start
			//this piece of code mainly comes from linux kernel
			if (!load_addr_set){
				load_addr_set = 1;
				load_addr = ph[i].p_vaddr - ph[i].p_offset;
				em->loadaddr = load_addr;
			}

			k = ph[i].p_vaddr;

			if (k < code_start)
				code_start = k;
			if (data_start < k)
				data_start = k;

			k = ph[i].p_vaddr + ph[i].p_filesz;
			if (bss_start < k){
				bss_start = k;
				bss_end = ph[i].p_vaddr + ph[i].p_memsz;
			}

			if (ph[i].p_flags & PF_X && code_end < k)
				code_end = k;
			if (data_end < k)
				data_end = k;

			//we do not need to zero the bss, for map_pages allways use
			//zeroed page
		}

		if (ph[i].p_type == PT_DYNAMIC) {
			Elf32_Dyn *dy = (Elf32_Dyn *)kzalloc(ph[i].p_memsz);
			assert(dy != NULL);

			em->proc = proc;
			em->dyaddr = dy;
			em->devid = file->f_vnode->v_device;
			em->i_no = file->f_vnode->v_id;
			em->type = ELF_MOD_TYPE_EXE;
			if (copy_from_proc(proc, (s8*)dy, (s8 *)ph[i].p_vaddr, ph[i].p_memsz) < 0) {
				ret = -EFAULT;
				if (dy)
					kfree(dy);
				LOG_ERROR("copy data from proc address space error");
				goto err_out;
			}

			LOG_INFO("dynamic section addr:%x", (u32)ph[i].p_vaddr);

			get_dynamic_info(em, dy, 0);

			if (em->dynamic[DT_HASH] != 0) {
				hash_addr = (Elf32_Symndx *)em->dynamic[DT_HASH];
				em->nbucket = *hash_addr++;
				em->nchain = *hash_addr++;
				em->elf_buckets = hash_addr;
				hash_addr += em->nbucket;
				em->chains = hash_addr;
			}

			if ((ret=resolv_depends(em, dy)) < 0) {
				LOG_ERROR("resolve dependences failed");
				goto err_out;
			}

			got = (u32 *)em->dynamic[DT_PLTGOT];
			if (got) {
				LOG_INFO("got:%x, em:%x", (u32)got, (u32)em);
				em->magic = 0xdeadbeaf;
				INIT_GOT(em->proc, got, em);
				init_elf_tramp(em);
			}

		}
	}

	proc->mm.image.code_start = code_start;
	proc->mm.image.code_end = code_end;
	proc->mm.image.data_satrt = data_start;
	proc->mm.image.data_end = data_end;
	proc->mm.image.bss_start = bss_start;
	proc->mm.image.bss_end = bss_end;		//the same as brk
	proc->mm.image.brk = PAGE_ALIGN(bss_end);

	*entry = hdr->e_entry;
	ret = 0;
//interp_out:
	if (interp)
		kfree((void *)interp);
err_out:
	if (addr)
		kfree((void *)addr);
END:
	if (ph)
		kfree((void *)ph);
	return ret;
}
