#include "elf.h"
#include <stddef.h>
#include <stdint.h>
#include <string.h>

static const void *vdso;
static const void *vdso_strtab;
static const Elf64_Sym *vdso_symtab;
static const Elf64_Word *vdso_buckets;
static const Elf64_Word *vdso_chains;
static Elf64_Word vdso_nbuckets;
static Elf64_Word vdso_nchains;

static void setup_vdso(Elf64_auxv_t *auxv) {
    for (Elf64_auxv_t *cur = auxv; cur->a_type != AT_NULL; cur++) {
        if (cur->a_type == AT_SYSINFO_EHDR) {
            vdso = (const void *)cur->a_un.a_val;
            break;
        }
    }

    const Elf64_Ehdr *header = vdso;
    const Elf64_Dyn *dynamic = NULL;

    for (int i = 0; i < header->e_phnum; i++) {
        const Elf64_Phdr *segment = vdso + header->e_phoff + (size_t)i * header->e_phentsize;

        if (segment->p_type == PT_DYNAMIC) {
            dynamic = vdso + segment->p_offset;
            break;
        }
    }

    const Elf64_Word *hash = NULL;

    for (const Elf64_Dyn *cur = dynamic; cur->d_tag != DT_NULL; cur++) {
        switch (cur->d_tag) {
        case DT_HASH: hash = vdso + cur->d_un.d_ptr; break;
        case DT_STRTAB: vdso_strtab = vdso + cur->d_un.d_ptr; break;
        case DT_SYMTAB: vdso_symtab = vdso + cur->d_un.d_ptr; break;
        }
    }

    vdso_nbuckets = hash[0];
    vdso_nchains = hash[1];
    vdso_buckets = &hash[2];
    vdso_chains = &hash[2 + vdso_nbuckets];
}

static Elf64_Word elf_hash(const unsigned char *name) {
    Elf64_Word hash = 0;

    for (unsigned char c = *name; c != 0; c = *++name) {
        hash = (hash << 4) + c;

        Elf64_Word top = hash & 0xf0000000;

        if (top) {
            hash ^= top >> 24;
            hash &= ~top;
        }
    }

    return hash;
}

static const Elf64_Sym *get_vdso_sym(const void *name) {
    Elf64_Word hash = elf_hash(name);
    Elf64_Word index = vdso_buckets[hash % vdso_nbuckets];

    while (index != STN_UNDEF) {
        const Elf64_Sym *sym = &vdso_symtab[index];
        const unsigned char *s1 = vdso_strtab + sym->st_name;
        const unsigned char *s2 = name;

        for (;;) {
            unsigned char c1 = *s1++;
            unsigned char c2 = *s2++;

            if (c1 != c2) break;
            if (c1 == 0) return sym;
        }

        index = vdso_chains[index];
    }

    return NULL;
}

typedef struct {
    intptr_t slide;
    const void *strtab;
    const Elf64_Sym *symtab;
} relocation_ctx_t;

static uintptr_t get_symbol(relocation_ctx_t *ctx, Elf64_Xword info) {
    Elf64_Word index = ELF64_R_SYM(info);
    if (index == STN_UNDEF) return 0;

    const Elf64_Sym *sym = &ctx->symtab[index];

    if (sym->st_shndx == SHN_UNDEF) {
        return (uintptr_t)vdso + get_vdso_sym(ctx->strtab + sym->st_name)->st_value;
    } else {
        return sym->st_value + ctx->slide;
    }
}

static void do_relocation(relocation_ctx_t *ctx, const Elf64_Rela *rel) {
    uintptr_t addr = rel->r_offset + ctx->slide;

    switch (ELF64_R_TYPE(rel->r_info)) {
    case R_X86_64_NONE:
    case R_X86_64_COPY: break;
    case R_X86_64_64: *(uint64_t *)addr = get_symbol(ctx, rel->r_info) + rel->r_addend; break;
    case R_X86_64_GLOB_DAT: *(uint64_t *)addr = get_symbol(ctx, rel->r_info); break;
    case R_X86_64_JUMP_SLOT: *(uint64_t *)addr = get_symbol(ctx, rel->r_info); break;
    case R_X86_64_RELATIVE: *(uint64_t *)addr = ctx->slide + rel->r_addend; break;
    case R_X86_64_IRELATIVE: {
        uintptr_t fnaddr = ctx->slide + rel->r_addend;
        *(void **)addr = ((void *(*)(void))fnaddr)();
        break;
    }
    default: __builtin_trap();
    }
}

static void do_relocations(relocation_ctx_t *ctx, const void *table, size_t entsize, size_t size) {
    for (size_t i = 0; i < size; i += entsize) {
        do_relocation(ctx, table + i);
    }
}

static void relocate_self(Elf64_auxv_t *auxv, Elf64_Dyn *dynamic) {
    relocation_ctx_t ctx = {};

    for (Elf64_auxv_t *cur = auxv; cur->a_type != AT_NULL; cur++) {
        if (cur->a_type == AT_BASE) {
            ctx.slide = cur->a_un.a_val;
            break;
        }
    }

    const void *rela = NULL;
    size_t relasz = 0;
    size_t relaent = 0;
    size_t pltrelsz = 0;
    const void *jmprel = NULL;

    for (Elf64_Dyn *cur = dynamic; cur->d_tag != DT_NULL; cur++) {
        switch (cur->d_tag) {
        case DT_RELA: rela = (const void *)(cur->d_un.d_ptr + ctx.slide); break;
        case DT_RELASZ: relasz = cur->d_un.d_val; break;
        case DT_RELAENT: relaent = cur->d_un.d_val; break;
        case DT_STRTAB: ctx.strtab = (const void *)(cur->d_un.d_ptr + ctx.slide); break;
        case DT_SYMTAB: ctx.symtab = (const void *)(cur->d_un.d_ptr + ctx.slide); break;
        case DT_PLTRELSZ: pltrelsz = cur->d_un.d_val; break;
        case DT_JMPREL: jmprel = (const void *)(cur->d_un.d_ptr + ctx.slide); break;
        }
    }

    if (rela) do_relocations(&ctx, rela, relaent, relasz);
    if (jmprel) do_relocations(&ctx, jmprel, sizeof(Elf64_Rela), pltrelsz);
}

// This runs before libc's _start. Its purpose is to ensure the dynamic linker itself has been relocated correctly.
// This code must not contain any relocations.
__attribute__((used)) void rtld_init(void **start_info, Elf64_Dyn *dynamic) {
    // Find auxiliary vector
    start_info += (uintptr_t)start_info[0] + 2;  // Skip over argc and argv
    while (start_info[0]) start_info++;          // Skip over envp
    Elf64_auxv_t *auxv = (void *)&start_info[1]; // Skip over the envp terminator

    setup_vdso(auxv);
    relocate_self(auxv, dynamic);
}

// these are necessary because rtld can't be linked with crti.o and crtn.o

__attribute__((used)) void _init(void) {
}

__attribute__((used)) void _fini(void) {
}
