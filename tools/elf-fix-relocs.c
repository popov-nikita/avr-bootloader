#include <stdint.h>
#include <elf.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <unistd.h>
#include <fcntl.h>

struct ram_section {
	const char *name;
	struct {
		const char *sym_start, *sym_end;
		uint32_t start, end;
	} flash, ram;
};

struct ram_section ram_sections[] = {
	{
		.name = ".data",
		.flash = {
			.sym_start = "__data_start",
			.sym_end   = "__data_end",
			.start     = -1,
			.end       = -1,
		},
		.ram = {
			.sym_start = "__data_ram_start",
			.sym_end   = "__data_ram_end",
			.start     = -1,
			.end       = -1,
		},
	},
	{
		.name = ".bss",
		.flash = {
			.sym_start = "__bss_start",
			.sym_end   = "__bss_end",
			.start     = -1,
			.end       = -1,
		},
		.ram = {
			.sym_start = "__bss_ram_start",
			.sym_end   = "__bss_ram_end",
			.start     = -1,
			.end       = -1,
		},
	},
};

static int open_file(const char *filename,
                     void **ptr,
                     unsigned long *size)
{
	int fd, ret_val = -1;
	struct stat s;
	void *_ptr;
	unsigned long _size;

	fd = open(filename, O_RDWR);
	if (fd < 0)
		goto ret;

	if (fstat(fd, &s) < 0)
		goto ret_close;
	_size = s.st_size;

	_ptr = mmap(NULL, _size, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0);
	if (_ptr == MAP_FAILED)
		goto ret_close;

	ret_val = 0;
	*ptr = _ptr;
	*size = _size;

ret_close:
	close(fd);
ret:
	return ret_val;
}

struct elf32_section {
	const char *name;
	void *data;
	unsigned long size;
	Elf32_Shdr *shptr;
};

static int parse_elf32(void *ptr,
                       unsigned long size,
                       struct elf32_section **sections,
                       unsigned long *nsections,
                       struct elf32_section **symtab)
{
	static const char elf_mag[] = "\177ELF";
	Elf32_Ehdr *eh;
	Elf32_Shdr *s, *init_s;
	const char *shstrtab;
	struct elf32_section *_sections;
	unsigned long _nsections;
	struct elf32_section *_symtab;

	if (size < sizeof(*eh))
		return 0;

	eh = ptr;
	if (memcmp(eh, elf_mag, sizeof(elf_mag) - 1) != 0)
		return 0;
	if (eh->e_ident[EI_CLASS] != ELFCLASS32)
		return 0;
	if (eh->e_ident[EI_DATA] != ELFDATA2LSB)
		return 0;
	if (eh->e_shoff + eh->e_shentsize * eh->e_shnum > size ||
	    eh->e_shentsize != sizeof(*s) ||
	    eh->e_shnum == 0)
		return 0;
	if (eh->e_shstrndx >= eh->e_shnum)
		return 0;

	init_s = (Elf32_Shdr *) ((char *) ptr + eh->e_shoff);
	shstrtab = (const char *) ptr + init_s[eh->e_shstrndx].sh_offset;
	_nsections = eh->e_shnum;
	_sections  = malloc(_nsections * sizeof(*_sections));
	_symtab    = NULL;

	for (s = init_s;
	     (unsigned long)(s - init_s) < _nsections;
	     s++) {
		struct elf32_section *d;

		if (s->sh_offset + s->sh_size > size &&
		    s->sh_type != SHT_NOBITS) {
			fprintf(stderr,
			        "%lu-th section is truncated!\n",
			        (unsigned long)(s - init_s));
			goto parse_error;
		}

		d = &_sections[(unsigned long)(s - init_s)];
		d->name  = &shstrtab[s->sh_name];
		d->data  = (char *) ptr + s->sh_offset;
		d->size  = s->sh_size;
		d->shptr = s;

		if (s->sh_type == SHT_SYMTAB) {
			if (_symtab) {
				fprintf(stderr,
				        "Multiple symtabs!\n");
				goto parse_error;
			}
			_symtab = d;
		}

		continue;
parse_error:
		free(_sections);
		return 0;
	}

	if (!_symtab) {
		fprintf(stderr,
		        "No symtab!\n");
		free(_sections);
		return 0;
	}

	_sections[0].data = NULL;
	*sections  = _sections;
	*nsections = _nsections;
	*symtab    = _symtab;

	return 1;
}

static int get_address_info(struct elf32_section *symtab,
                            struct elf32_section *strtab)
{
	const Elf32_Sym *syms, *sym;
	unsigned long i, j, nsyms;
	const char *strings, *symname;
	unsigned int counter;

	syms    = symtab->data;
	nsyms   = symtab->shptr->sh_size / sizeof(*syms);
	strings = strtab->data;
	counter = 0;

	for (i = 1; i < nsyms; i++) {
		struct ram_section *s;

		sym     = &syms[i];
		symname = &strings[sym->st_name];
		if (sym->st_shndx != SHN_ABS)
			continue;

		for (j = 0; j < sizeof(ram_sections) / sizeof(*s); j++) {
			unsigned int saved_counter;

			s = &ram_sections[j];
			saved_counter = counter;

			if (strcmp(s->flash.sym_start, symname) == 0) {
				s->flash.start = sym->st_value;
				counter++;
			} else if (strcmp(s->flash.sym_end, symname) == 0) {
				s->flash.end = sym->st_value;
				counter++;
			} else if (strcmp(s->ram.sym_start, symname) == 0) {
				s->ram.start = sym->st_value;
				counter++;
			} else if (strcmp(s->ram.sym_end, symname) == 0) {
				s->ram.end = sym->st_value;
				counter++;
			}

			if (counter != saved_counter)
				break;
		}
	}

	return (counter == ((sizeof(ram_sections) / sizeof(ram_sections[0])) * 4));
}

static inline struct ram_section *get_ram_section(const char *name)
{
	struct ram_section *s;
	unsigned long i;

	for (i = 0; i < sizeof(ram_sections) / sizeof(*s); i++) {
		s = &ram_sections[i];
		if (strcmp(s->name, name) == 0)
			return s;
	}

	return NULL;
}

static int ____fix_relocations(const Elf32_Rela *r,
                               void *loc,
                               uint32_t orig_addr,
                               uint32_t new_addr)
{
/*
	This is all relocation types ever encountered at the moment which deal with RAM.
	If new types appear this function should be adjusted to include them.
	After all without modification it fails indicating unknown types.
*/
#define R_AVR_16           4
#define R_AVR_LO8_LDI      6
#define R_AVR_HI8_LDI      7
#define R_AVR_LO8_LDI_NEG  9
#define R_AVR_HI8_LDI_NEG  10
#define R_AVR_LO8_LDI_GS   24
#define R_AVR_HI8_LDI_GS   25
	uint16_t imm16;
	unsigned int reloc_type;

	(void) orig_addr;
	reloc_type = ELF32_R_TYPE(r->r_info);
	if (reloc_type == R_AVR_LO8_LDI_NEG) {
		new_addr = -new_addr;
		reloc_type = R_AVR_LO8_LDI;
	} else if (reloc_type == R_AVR_HI8_LDI_NEG) {
		new_addr = -new_addr;
		reloc_type = R_AVR_HI8_LDI;
	} else if (reloc_type == R_AVR_LO8_LDI_GS) {
		new_addr >>= 1;
		reloc_type = R_AVR_LO8_LDI;
	} else if (reloc_type == R_AVR_HI8_LDI_GS) {
		new_addr >>= 1;
		reloc_type = R_AVR_HI8_LDI;
	}	
	
	switch (reloc_type) {
	case R_AVR_16:
		imm16 = (uint16_t) (new_addr & 0x0000ffffU);
		*((uint16_t *) loc) = imm16;
		break;
	case R_AVR_LO8_LDI:
		imm16 = *((uint16_t *) loc);
		imm16 &= 0xf0f0U;
		imm16 |= (uint16_t) ((new_addr & 0x0000000fU));
		imm16 |= (uint16_t) ((new_addr & 0x000000f0U) << 4);
		*((uint16_t *) loc) = imm16;
		break;
	case R_AVR_HI8_LDI:
		imm16 = *((uint16_t *) loc);
		imm16 &= 0xf0f0U;
		imm16 |= (uint16_t) ((new_addr & 0x00000f00U) >> 8);
		imm16 |= (uint16_t) ((new_addr & 0x0000f000U) >> 4);
		*((uint16_t *) loc) = imm16;
		break;
	default:
		fprintf(stderr,
		        "Unsupported relocation type [%u]!\n",
		        reloc_type);
		return -1;
	}

	return 0;
#undef R_AVR_16
#undef R_AVR_LO8_LDI
#undef R_AVR_HI8_LDI
#undef R_AVR_LO8_LDI_NEG
#undef R_AVR_HI8_LDI_NEG
#undef R_AVR_LO8_LDI_GS
#undef R_AVR_HI8_LDI_GS
}

static int __fix_relocations(const struct elf32_section *sects,
                             unsigned long nsects,
                             const struct elf32_section *data_s,
                             const struct elf32_section *rela_s,
                             const struct elf32_section *symtab)
{
	unsigned long i;
	const Elf32_Rela *relocs, *r;
	unsigned long nrelocs;
	const Elf32_Sym *sym;

	relocs  = rela_s->data;
	nrelocs = rela_s->shptr->sh_size / sizeof(*relocs);

	for (i = 0; i < nrelocs; i++) {
		struct ram_section *desc;
		uint32_t orig_addr, new_addr;
		void *loc;

		r   = &relocs[i];
		sym = &((const Elf32_Sym *) (symtab->data))[ELF32_R_SYM(r->r_info)];
		if (!(0 < sym->st_shndx && sym->st_shndx < nsects))
			continue;

		desc = get_ram_section(sects[sym->st_shndx].name);
		if (!desc)
			continue;

		orig_addr = (uint32_t) ((int32_t) ((int64_t) sym->st_value + (int64_t) r->r_addend));
		if (orig_addr < desc->flash.start || orig_addr >= desc->flash.end) {
			const char *symname;
			static const char _end[] = "_end";
			unsigned int namelen;
			int ok = 0;

			/* *_end symbols may point to flash area upper edge */
			symname = &((const char *) sects[symtab->shptr->sh_link].data)[sym->st_name];
			namelen = strlen(symname);
			if (namelen >= (sizeof(_end) - 1)) {
				const char *symsuffix = &symname[namelen - (sizeof(_end) - 1)];
				ok = (strcmp(symsuffix, _end) == 0) &&
				     (orig_addr >= desc->flash.start) &&
				     (orig_addr <= desc->flash.end);
			}

			if (!ok) {
				fprintf(stderr,
					"Symbol \"%s\" has relocation "
					"targeting area beyond allocated flash space!\n"
					"Sym value = %#x, reloc target = %#x, "
					"flash space = [%#x, %#x]\n",
					symname, sym->st_value, orig_addr,
					desc->flash.start, desc->flash.end);
				return -1;
			}
		}

		new_addr  = (orig_addr - desc->flash.start) + desc->ram.start;
		loc       = &((char *) data_s->data)[r->r_offset - data_s->shptr->sh_addr];
		if (____fix_relocations(r, loc, orig_addr, new_addr) != 0)
			return -1;
	}

	return 0;
}

static int fix_relocations(const struct elf32_section *sects,
                           unsigned long nsects,
                           const struct elf32_section *symtab)
{
	unsigned long i;
	const struct elf32_section *s, *par;

	for (i = 1; i < nsects; i++) {
		s = &sects[i];
		if (s->shptr->sh_type == SHT_RELA &&
		    s->shptr->sh_info > 0 &&
		    s->shptr->sh_info < nsects) {
			par = &sects[s->shptr->sh_info];
			if (__fix_relocations(sects, nsects, par, s, symtab) != 0)
				return -1;
		}
	}

	return 0;
}

static int write_file(const char *filename,
                      void *ptr,
                      unsigned long size)
{
	int fd, ret_val = -1;
	long nbytes;

	fd = open(filename, O_CREAT | O_WRONLY | O_TRUNC, 0644);
	if (fd < 0)
		goto ret;

	nbytes = write(fd, ptr, size);
	if (((long) ((unsigned int) size)) == nbytes)
		ret_val = 0;

	close(fd);
ret:
	return ret_val;
}

int main(int argc, char **argv)
{
	const char *filename_in, *filename_out;
	void *elf;
	unsigned long size;
	struct elf32_section *sects, *symtab, *strtab;
	unsigned long nsects;

	if (argc != 3) {
		fprintf(stderr, "USAGE: %s <input ELF> <output ELF>\n", argv[0]);
		exit(1);
	}

	filename_in  = argv[1];
	filename_out = argv[2];

	if (open_file(filename_in, &elf, &size) != 0) {
		fprintf(stderr, "Couldn't open file %s\n", filename_in);
		exit(1);
	}

	if (!parse_elf32(elf, size, &sects, &nsects, &symtab)) {
		fprintf(stderr, "Couldn't parse file %s\n", filename_in);
		exit(1);
	}
	strtab = &sects[symtab->shptr->sh_link];

	if (!get_address_info(symtab, strtab)) {
		fprintf(stderr, "Couldn't get important variables from %s\n", filename_in);
		exit(1);
	}

	printf("__data_start = %#x\n", ram_sections[0].flash.start);
	printf("__data_end = %#x\n", ram_sections[0].flash.end);
	printf("__data_ram_start = %#x\n", ram_sections[0].ram.start);
	printf("__data_ram_end = %#x\n", ram_sections[0].ram.end);
	printf("__bss_start = %#x\n", ram_sections[1].flash.start);
	printf("__bss_end = %#x\n", ram_sections[1].flash.end);
	printf("__bss_ram_start = %#x\n", ram_sections[1].ram.start);
	printf("__bss_ram_end = %#x\n", ram_sections[1].ram.end);

	if (fix_relocations(sects, nsects, symtab) != 0) {
		fprintf(stderr, "Couldn't fix relocations for %s\n", filename_in);
		exit(1);
	}

	if (write_file(filename_out, elf, size) != 0) {
		fprintf(stderr, "Couldn't write result %s for %s\n", filename_out, filename_in);
		exit(1);
	}

	exit(0);
}
