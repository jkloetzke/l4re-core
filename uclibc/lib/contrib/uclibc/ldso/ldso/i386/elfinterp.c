/* i386 ELF shared library loader suppport
 *
 * Copyright (c) 1994-2000 Eric Youngdale, Peter MacDonald,
 *                              David Engel, Hongjiu Lu and Mitch D'Souza
 * Copyright (C) 2001-2004 Erik Andersen
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. The name of the above contributors may not be
 *    used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "ldso.h"

/* Program to load an ELF binary on a linux system, and run it.
   References to symbols in sharable libraries can be resolved by either
   an ELF sharable library or a linux style of shared library. */

/* Disclaimer:  I have never seen any AT&T source code for SVr4, nor have
   I ever taken any courses on internals.  This program was developed using
   information available through the book "UNIX SYSTEM V RELEASE 4,
   Programmers guide: Ansi C and Programming Support Tools", which did
   a more than adequate job of explaining everything required to get this
   working. */

extern int _dl_linux_resolve(void);

unsigned long
_dl_linux_resolver(struct elf_resolve *tpnt, int reloc_entry)
{
	ELF_RELOC *this_reloc;
	char *strtab;
	ElfW(Sym) *symtab;
	int symtab_index;
	char *rel_addr;
	char *new_addr;
	char **got_addr;
	unsigned long instr_addr;
	char *symname;

	rel_addr = (char *)tpnt->dynamic_info[DT_JMPREL];
	this_reloc = (ELF_RELOC *)(intptr_t)(rel_addr + reloc_entry);
	symtab_index = ELF_R_SYM(this_reloc->r_info);

	symtab = (ElfW(Sym) *)(intptr_t)tpnt->dynamic_info[DT_SYMTAB];
	strtab = (char *)tpnt->dynamic_info[DT_STRTAB];
	symname = strtab + symtab[symtab_index].st_name;

	/* Address of the jump instruction to fix up. */
	instr_addr = ((unsigned long)this_reloc->r_offset +
		      (unsigned long)tpnt->loadaddr);
	got_addr = (char **)instr_addr;

	/* Get the address of the GOT entry. */
	new_addr = _dl_find_hash(symname, &_dl_loaded_modules->symbol_scope, tpnt, ELF_RTYPE_CLASS_PLT, NULL);
	if (unlikely(!new_addr)) {
		_dl_dprintf(2, "%s: can't resolve symbol '%s' in lib '%s'.\n", _dl_progname, symname, tpnt->libname);
		_dl_exit(1);
	}

#if defined (__SUPPORT_LD_DEBUG__)
	if ((unsigned long)got_addr < 0x40000000) {
		if (_dl_debug_bindings) {
			_dl_dprintf(_dl_debug_file, "\nresolve function: %s", symname);
			if (_dl_debug_detail)
				_dl_dprintf(_dl_debug_file,
				            "\n\tpatched: %p ==> %p @ %p",
				            *got_addr, new_addr, got_addr);
		}
	}
	if (!_dl_debug_nofixups) {
		*got_addr = new_addr;
	}
#else
	*got_addr = new_addr;
#endif

	return (unsigned long)new_addr;
}

static int
_dl_parse(struct elf_resolve *tpnt, struct r_scope_elem *scope,
	  unsigned long rel_addr, unsigned long rel_size,
	  int (*reloc_fnc)(struct elf_resolve *tpnt, struct r_scope_elem *scope,
			   ELF_RELOC *rpnt, ElfW(Sym) *symtab, char *strtab))
{
	unsigned int i;
	char *strtab;
	ElfW(Sym) *symtab;
	ELF_RELOC *rpnt;
	int symtab_index;

	/* Parse the relocation information. */
	rpnt = (ELF_RELOC *)(intptr_t)rel_addr;
	rel_size /= sizeof(ELF_RELOC);

	symtab = (ElfW(Sym) *)(intptr_t)tpnt->dynamic_info[DT_SYMTAB];
	strtab = (char *)tpnt->dynamic_info[DT_STRTAB];

	for (i = 0; i < rel_size; i++, rpnt++) {
		int res;

		symtab_index = ELF_R_SYM(rpnt->r_info);

		debug_sym(symtab, strtab, symtab_index);
		debug_reloc(symtab, strtab, rpnt);

		res = reloc_fnc(tpnt, scope, rpnt, symtab, strtab);

		if (res == 0)
			continue;

		_dl_dprintf(2, "\n%s: ", _dl_progname);

		if (symtab_index)
			_dl_dprintf(2, "symbol '%s': ",
				    strtab + symtab[symtab_index].st_name);

		if (unlikely(res < 0)) {
			int reloc_type = ELF_R_TYPE(rpnt->r_info);
			_dl_dprintf(2, "can't handle reloc type %x in lib '%s'\n",
				    reloc_type, tpnt->libname);
			return res;
		} else if (unlikely(res > 0)) {
			_dl_dprintf(2, "can't resolve symbol in lib '%s'.\n", tpnt->libname);
			return res;
		}
	}

	return 0;
}

static int
_dl_do_reloc(struct elf_resolve *tpnt, struct r_scope_elem *scope,
	     ELF_RELOC *rpnt, ElfW(Sym) *symtab, char *strtab)
{
	int reloc_type;
	int symtab_index;
	char *symname;
	struct elf_resolve *tls_tpnt = NULL;
	unsigned long *reloc_addr;
	unsigned long symbol_addr;
#if defined (__SUPPORT_LD_DEBUG__)
	unsigned long old_val;
#endif
	struct symbol_ref sym_ref;

	reloc_addr = (unsigned long *)(intptr_t)(tpnt->loadaddr + (unsigned long)rpnt->r_offset);
	reloc_type = ELF_R_TYPE(rpnt->r_info);
	symtab_index = ELF_R_SYM(rpnt->r_info);
	symbol_addr = 0;
	sym_ref.sym = &symtab[symtab_index];
	sym_ref.tpnt = NULL;
	symname = strtab + symtab[symtab_index].st_name;

	if (symtab_index) {
		symbol_addr = (unsigned long)_dl_find_hash(symname, scope, tpnt,
							   elf_machine_type_class(reloc_type), &sym_ref);

		/*
		 * We want to allow undefined references to weak symbols - this
		 * might have been intentional.  We should not be linking local
		 * symbols here, so all bases should be covered.
		 */
		if (unlikely(!symbol_addr && (ELF_ST_TYPE(symtab[symtab_index].st_info) != STT_TLS)
					&& ELF_ST_BIND(symtab[symtab_index].st_info) != STB_WEAK))
			return 1;
		if (_dl_trace_prelink) {
			_dl_debug_lookup (symname, tpnt, &symtab[symtab_index],
					&sym_ref, elf_machine_type_class(reloc_type));
		}
		tls_tpnt = sym_ref.tpnt;
	} else {
		symbol_addr = symtab[symtab_index].st_value;
		tls_tpnt = tpnt;
	}
	
#if defined (__SUPPORT_LD_DEBUG__)
	/* Start: aw11: fixed pagefault with empty relocations */
	if (reloc_type != R_386_NONE) old_val = *reloc_addr; else old_val = 0;
	/* End: aw11 */
#endif

	switch (reloc_type) {
		case R_386_NONE:
			break;
		case R_386_32:
			*reloc_addr += symbol_addr;
			break;
		case R_386_PC32:
			*reloc_addr += symbol_addr - (unsigned long)reloc_addr;
			break;
		case R_386_GLOB_DAT:
		case R_386_JMP_SLOT:
			*reloc_addr = symbol_addr;
			break;
		case R_386_RELATIVE:
			*reloc_addr += (unsigned long)tpnt->loadaddr;
			break;
		case R_386_COPY:
			if (symbol_addr) {
#if defined (__SUPPORT_LD_DEBUG__)
				if (_dl_debug_move)
					_dl_dprintf(_dl_debug_file,
						    "\n%s move %d bytes from %lx to %p",
						    symname, symtab[symtab_index].st_size,
						    symbol_addr, reloc_addr);
#endif

				_dl_memcpy((char *)reloc_addr,
					   (char *)symbol_addr,
					   symtab[symtab_index].st_size);
			}
			break;
#if defined USE_TLS && USE_TLS
		case R_386_TLS_DTPMOD32:
			*reloc_addr = tls_tpnt->l_tls_modid;
			break;
		case R_386_TLS_DTPOFF32:
			/* During relocation all TLS symbols are defined and used.
			 * Therefore the offset is already correct. */
			*reloc_addr = symbol_addr;
			break;
		case R_386_TLS_TPOFF32:
			/* The offset is positive, backward from the thread pointer. */
			CHECK_STATIC_TLS((struct link_map*) tls_tpnt);
			*reloc_addr += tls_tpnt->l_tls_offset - symbol_addr;
			break;
		case R_386_TLS_TPOFF:
			/* The offset is negative, forward from the thread pointer. */
			CHECK_STATIC_TLS((struct link_map*) tls_tpnt);
			*reloc_addr += symbol_addr - tls_tpnt->l_tls_offset;
			break;
#endif
		default:
			return -1;
	}

#if defined (__SUPPORT_LD_DEBUG__)
	if (_dl_debug_reloc && _dl_debug_detail)
		_dl_dprintf(_dl_debug_file, "\n\tpatched: %lx ==> %lx @ %p\n",
			    old_val, *reloc_addr, reloc_addr);
#endif

	return 0;
}

static int
_dl_do_lazy_reloc(struct elf_resolve *tpnt, struct r_scope_elem *scope,
		  ELF_RELOC *rpnt, ElfW(Sym) *symtab, char *strtab)
{
	int reloc_type;
	unsigned long *reloc_addr;
#if defined (__SUPPORT_LD_DEBUG__)
	unsigned long old_val;
#endif

	(void)scope;
	(void)symtab;
	(void)strtab;

	reloc_addr = (unsigned long *)(intptr_t)(tpnt->loadaddr + (unsigned long)rpnt->r_offset);
	reloc_type = ELF_R_TYPE(rpnt->r_info);

#if defined (__SUPPORT_LD_DEBUG__)
	old_val = *reloc_addr;
#endif

	switch (reloc_type) {
		case R_386_NONE:
			break;
		case R_386_JMP_SLOT:
			*reloc_addr += (unsigned long)tpnt->loadaddr;
			break;
		default:
			return -1;
	}

#if defined (__SUPPORT_LD_DEBUG__)
	if (_dl_debug_reloc && _dl_debug_detail)
		_dl_dprintf(_dl_debug_file, "\n\tpatched: %lx ==> %lx @ %p\n",
			    old_val, *reloc_addr, reloc_addr);
#endif

	return 0;
}

void
_dl_parse_lazy_relocation_information(struct dyn_elf *rpnt,
				      unsigned long rel_addr,
				      unsigned long rel_size)
{
	(void)_dl_parse(rpnt->dyn, NULL, rel_addr, rel_size, _dl_do_lazy_reloc);
}

int
_dl_parse_relocation_information(struct dyn_elf *rpnt,
				 struct r_scope_elem *scope,
				 unsigned long rel_addr,
				 unsigned long rel_size)
{
	return _dl_parse(rpnt->dyn, scope, rel_addr, rel_size, _dl_do_reloc);
}

static __always_inline void
elf_machine_setup(ElfW(Addr) load_off, unsigned long const *dynamic_info,
                  struct elf_resolve *tpnt, int lazy)
{
	(void) load_off;
	(void) lazy;
	unsigned long *lpnt = (unsigned long *) dynamic_info[DT_PLTGOT];
#ifdef ALLOW_ZERO_PLTGOT
	if (lpnt)
#endif
		INIT_GOT(lpnt, tpnt);
}

