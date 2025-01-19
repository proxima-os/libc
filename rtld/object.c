#include "object.h"
#include "elf.h"
#include "sys/auxv.h"
#include <errno.h>
#include <hydrogen/fcntl.h>
#include <hydrogen/memory.h>
#include <hydrogen/vfs.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

object_t exec_object;
object_t rtld_object;
object_t vdso_object;

// FNV-1a
static uint64_t make_hash(const char *data, size_t length) {
    uint64_t hash = 0xcbf29ce484222325;

    for (size_t i = 0; i < length; i++) {
        hash ^= (unsigned char)data[i];
        hash *= 0x00000100000001b3;
    }

    return hash;
}

static object_t **object_table;
static size_t objects_capacity;
static size_t objects_count;

static void add_to_hash_table(object_t *object) {
    if (objects_count >= (objects_capacity - (objects_capacity / 4))) {
        size_t new_cap = objects_capacity * 2; // multiplier must be a power of two
        object_t **new_table = calloc(new_cap, sizeof(*new_table));
        if (!new_table) {
            fprintf(stderr, "rtld: failed to expand object table\n");
            exit(EXIT_FAILURE);
        }

        for (size_t i = 0; i < objects_capacity; i++) {
            object_t *cur = object_table[i];

            while (cur) {
                object_t *next = cur->table_next;

                size_t bucket = cur->path.hash & (new_cap - 1);
                cur->table_prev = NULL;
                cur->table_next = new_table[bucket];
                new_table[bucket] = cur;
                if (cur->table_next) cur->table_next->table_prev = cur;

                cur = next;
            }
        }

        free(object_table);
        object_table = new_table;
        objects_capacity = new_cap;
    }

    size_t bucket = object->path.hash & (objects_capacity - 1);
    object->table_prev = NULL;
    object->table_next = object_table[bucket];
    object_table[bucket] = object;
    if (object->table_next) object->table_next->table_prev = object;
    objects_count += 1;
}

static unsigned long get_req_auxval(const char *name, unsigned long tag) {
    unsigned long value = getauxval(tag);
    if (!value) {
        fprintf(stderr, "rtld: failed to find %s\n", name);
        exit(EXIT_FAILURE);
    }
    return value;
}

#define AUXVAL(name) (get_req_auxval(#name, name))

static const char *libpath_env;

void init_objects(void) {
    libpath_env = getenv("LD_LIBRARY_PATH");

    objects_capacity = 16; // must be a power of two
    object_table = calloc(objects_capacity, sizeof(*object_table));
    if (!object_table) {
        fprintf(stderr, "rtld: failed to allocate object table\n");
        exit(EXIT_FAILURE);
    }

    const void *phdrs = (const void *)AUXVAL(AT_PHDR);
    size_t nphdr = AUXVAL(AT_PHNUM);
    size_t phdrsz = AUXVAL(AT_PHENT);

    for (size_t i = 0; i < nphdr; i++) {
        const Elf64_Phdr *phdr = phdrs + i * phdrsz;

        if (phdr->p_type == PT_DYNAMIC) exec_object.dynamic = (void *)phdr->p_vaddr;
        else if (phdr->p_type == PT_PHDR) exec_object.slide = (intptr_t)phdrs - (intptr_t)phdr->p_vaddr;
    }

    exec_object.dynamic = (void *)exec_object.dynamic + exec_object.slide;
    init_object(&exec_object);
}

void init_object(object_t *obj) {
    if (obj->path.len) {
        obj->path.hash = make_hash(obj->path.data, obj->path.len);
        add_to_hash_table(obj);
    }

    for (const Elf64_Dyn *dynamic = obj->dynamic; dynamic->d_tag != DT_NULL; dynamic++) {
        switch (dynamic->d_tag) {
        case DT_HASH:
            obj->hash.buckets = (void *)(dynamic->d_un.d_ptr + obj->slide + sizeof(*obj->hash.buckets) * 2);
            obj->hash.nbuckets = obj->hash.buckets[-2];
            obj->hash.chains = obj->hash.buckets + obj->hash.nbuckets;
            break;
        case DT_STRTAB: obj->strtab = (void *)(dynamic->d_un.d_ptr + obj->slide); break;
        case DT_SYMTAB: obj->symtab = (void *)(dynamic->d_un.d_ptr + obj->slide); break;
        case DT_SYMENT: obj->syment = dynamic->d_un.d_val; break;
        case DT_RPATH: obj->rpath = (void *)(dynamic->d_un.d_ptr + obj->slide); break;
        case DT_RUNPATH: obj->runpath = (void *)(dynamic->d_un.d_ptr + obj->slide); break;
        }
    }
}

static object_t *get_object(const char *name) {
    size_t length = strlen(name);
    uint64_t hash = make_hash(name, length);
    size_t bucket = hash & (objects_capacity - 1);

    object_t *cur = object_table[bucket];
    while (cur && (cur->path.hash != hash || cur->path.len != length || memcmp(cur->path.data, name, length))) {
        cur = cur->table_next;
    }
    return cur;
}

static object_t *search_first;
static object_t *search_last;

#define DEP_OPEN_FLAGS (O_RDONLY | O_NODIR)

static unsigned char wanted_ident[] = {ELFMAG0, ELFMAG1, ELFMAG2, ELFMAG3, ELFCLASS64, ELFDATA2LSB, EV_CURRENT};

static int read_fully(int fd, void *buffer, size_t size, uint64_t position) {
    while (size > 0) {
        hydrogen_io_res_t res = hydrogen_pread(fd, buffer, size, position);
        if (res.error) return res.error;
        if (!res.transferred) return EOVERFLOW;
        buffer += res.transferred;
        size -= res.transferred;
        position += res.transferred;
    }

    return 0;
}

static bool verify_object(int fd, Elf64_Ehdr *hdr) {
    int error = read_fully(fd, hdr, sizeof(*hdr), 0);
    if (error) return false;
    return memcmp(hdr->e_ident, wanted_ident, sizeof(wanted_ident)) == 0 && hdr->e_type == ET_DYN &&
           hdr->e_machine == EM_X86_64 && hdr->e_version == EV_CURRENT;
}

static int find_object(const char *paths, const char *name, Elf64_Ehdr *hdr, bool semisep) {
    size_t nlen = strlen(name);

    for (;;) {
        const char *end = paths;
        for (;;) {
            char c = *end;
            if (!c) break;
            if (c == ':') break;
            if (semisep && c == ';') break;
            end++;
        }
        size_t prlen = end - paths;

        int fd;

        if (prlen != 0) {
            if (end[-1] != '/') prlen++;
            size_t plen = prlen + nlen;
            char *buf = malloc(plen);
            if (!buf) {
                fprintf(stderr, "rtld: failed to allocate path buffer\n");
                exit(EXIT_FAILURE);
            }
            memcpy(buf, paths, prlen);
            buf[prlen - 1] = '/';
            memcpy(buf + prlen, name, nlen);
            fd = hydrogen_open(-1, buf, plen, DEP_OPEN_FLAGS, 0);
            free(buf);
        } else {
            fd = hydrogen_open(-1, name, nlen, DEP_OPEN_FLAGS, 0);
        }

        if (fd >= 0 && verify_object(fd, hdr)) return fd;

        if (!*end) return -1;
        paths = end + 1;
    }
}

static int open_object(object_t *owner, const char *name, Elf64_Ehdr *hdr) {
    if (strchr(name, '/')) {
        int fd = hydrogen_open(-1, name, strlen(name), DEP_OPEN_FLAGS, 0);
        if (fd < 0) {
            fprintf(stderr, "rtld: failed to open %s: %s\n", name, strerror(-fd));
            exit(EXIT_FAILURE);
        }
        int error = read_fully(fd, hdr, sizeof(*hdr), 0);
        if (error) {
            fprintf(stderr, "rtld: %s: failed to read header: %s\n", name, strerror(error));
            exit(EXIT_FAILURE);
        }
        return fd;
    }

    if (owner->rpath && !owner->runpath) {
        int fd = find_object(owner->rpath, name, hdr, false);
        if (fd >= 0) return fd;
    }

    if (libpath_env) {
        int fd = find_object(libpath_env, name, hdr, true);
        if (fd >= 0) return fd;
    }

    if (owner->runpath) {
        int fd = find_object(owner->runpath, name, hdr, false);
        if (fd >= 0) return fd;
    }

    int fd = find_object("/usr/lib", name, hdr, false);
    if (fd >= 0) return fd;

    fprintf(stderr, "rtld: failed to find shared object '%s'\n", name);
    exit(EXIT_FAILURE);
}

static void load_into_object(object_t *object, int fd, Elf64_Ehdr *hdr) {
    size_t phdrs_size = (size_t)hdr->e_phnum * hdr->e_phentsize;
    void *phdrs = malloc(phdrs_size);
    if (!phdrs) {
        fprintf(stderr, "rtld: failed to allocate program header buffer\n");
        exit(EXIT_FAILURE);
    }
    int error = read_fully(fd, phdrs, phdrs_size, hdr->e_phoff);
    if (error) {
        fprintf(stderr, "rtld: failed to read program headers: %s\n", strerror(error));
        exit(EXIT_FAILURE);
    }

    uintptr_t min_vaddr = UINTPTR_MAX;
    uintptr_t max_vaddr = 0;

    for (size_t i = 0; i < hdr->e_phnum; i++) {
        Elf64_Phdr *seg = phdrs + i * hdr->e_phentsize;
        if (seg->p_type != PT_LOAD || seg->p_memsz == 0) continue;

        if (seg->p_vaddr < min_vaddr) min_vaddr = seg->p_vaddr;
        if (seg->p_vaddr + seg->p_memsz > max_vaddr) max_vaddr = seg->p_vaddr + seg->p_memsz;
    }

    if (min_vaddr >= max_vaddr) {
        fprintf(stderr, "rtld: dynamic object is empty\n");
        exit(EXIT_FAILURE);
    }

    intptr_t addr = hydrogen_map_memory(0, max_vaddr - min_vaddr, 0, -1, 0);
    if (addr < 0) {
        fprintf(stderr, "rtld: failed to allocate memory area: %s\n", strerror(-addr));
        exit(EXIT_FAILURE);
    }
    addr |= min_vaddr & 0xfff;
    intptr_t slide = addr - min_vaddr;

    for (size_t i = 0; i < hdr->e_phnum; i++) {
        Elf64_Phdr *seg = phdrs + i * hdr->e_phentsize;

        if (seg->p_type != PT_LOAD || seg->p_memsz == 0) {
            if (seg->p_type == PT_DYNAMIC) object->dynamic = (void *)(seg->p_vaddr + slide);
            continue;
        }

        int flags = VMM_EXACT | VMM_PRIVATE;
        if (seg->p_flags & PF_R) flags |= VMM_READ;
        if (seg->p_flags & PF_W) flags |= VMM_WRITE;
        if (seg->p_flags & PF_X) flags |= VMM_EXEC;
        if (flags == (VMM_EXACT | VMM_PRIVATE)) continue;

        intptr_t vaddr = seg->p_vaddr + slide;
        intptr_t file_end = vaddr;
        intptr_t mem_end = vaddr + seg->p_memsz;

        if (seg->p_filesz) {
            file_end = (file_end + seg->p_filesz + 0xfff) & ~0xfff;
            addr = hydrogen_map_memory(vaddr, seg->p_filesz, flags, fd, seg->p_offset);
            if (addr != vaddr) {
                fprintf(stderr, "rtld: failed to map segment: %s\n", strerror(-addr));
                exit(EXIT_FAILURE);
            }
        }

        if (file_end < mem_end) {
            addr = hydrogen_map_memory(file_end, mem_end - file_end, flags, -1, 0);
            if (addr != file_end) {
                fprintf(stderr, "rtld: failed to map segment: %s\n", strerror(-addr));
                exit(EXIT_FAILURE);
            }
        }

        if (seg->p_filesz != seg->p_memsz && (flags & VMM_WRITE) != 0) {
            memset((void *)(seg->p_vaddr + slide + seg->p_filesz), 0, seg->p_memsz - seg->p_filesz);
        }
    }

    object->slide = slide;
}

static object_t *load_object(object_t *owner, const char *name) {
    object_t *object = calloc(1, sizeof(*object));
    if (!object) {
        fprintf(stderr, "rtld: failed to allocate object for %s\n", name);
        exit(EXIT_FAILURE);
    }

    object->path.len = strlen(name);
    object->path.data = malloc(object->path.len);
    if (!object->path.data) {
        fprintf(stderr, "rtld: failed to allocate name for %s\n", name);
        exit(EXIT_FAILURE);
    }
    memcpy(object->path.data, name, object->path.len);

    Elf64_Ehdr hdr;
    int fd = open_object(owner, name, &hdr);
    load_into_object(object, fd, &hdr);
    hydrogen_close(fd);

    init_object(object);
    return object;
}

static uintptr_t get_symbol(object_t *obj, Elf64_Xword info) {
    Elf64_Word idx = ELF64_R_SYM(info);
    if (idx == STN_UNDEF) return 0;

    const Elf64_Sym *sym = (void *)obj->symtab + idx * obj->syment;
    const Elf64_Sym *found = search_for_symbol(obj->strtab + sym->st_name, &obj);

    if (found) return found->st_value + obj->slide;
    if (ELF64_ST_BIND(sym->st_info) == STB_WEAK) return 0;

    fprintf(stderr, "rtld: failed to find symbol '%s'\n", obj->strtab + sym->st_name);
    exit(EXIT_FAILURE);
}

static void do_relocation(object_t *obj, const Elf64_Rela *rel) {
    uintptr_t addr = rel->r_offset + obj->slide;

    switch (ELF64_R_TYPE(rel->r_info)) {
    case R_X86_64_NONE:
    case R_X86_64_COPY: break;
    case R_X86_64_64: *(uint64_t *)addr = get_symbol(obj, rel->r_info) + rel->r_addend; break;
    case R_X86_64_GLOB_DAT: *(uint64_t *)addr = get_symbol(obj, rel->r_info); break;
    case R_X86_64_JUMP_SLOT: *(uint64_t *)addr = get_symbol(obj, rel->r_info); break;
    case R_X86_64_RELATIVE: *(uint64_t *)addr = obj->slide + rel->r_addend; break;
    case R_X86_64_IRELATIVE: {
        uintptr_t fnaddr = obj->slide + rel->r_addend;
        *(void **)addr = ((void *(*)(void))fnaddr)();
        break;
    }
    default: fprintf(stderr, "rtld: unknown relocation type %#lx\n", ELF64_R_TYPE(rel->r_info)); exit(EXIT_FAILURE);
    }
}

static void do_relocations(object_t *obj, const void *table, size_t entsize, size_t size) {
    for (size_t i = 0; i < size; i += entsize) {
        do_relocation(obj, table + i);
    }
}

static void relocate_object(object_t *obj) {
    if (obj == &rtld_object || obj == &vdso_object) return;

    const void *rela = NULL;
    size_t relasz = 0;
    size_t relaent = 0;
    size_t pltrelsz = 0;
    const void *jmprel = NULL;

    for (const Elf64_Dyn *cur = obj->dynamic; cur->d_tag != DT_NULL; cur++) {
        switch (cur->d_tag) {
        case DT_RELA: rela = (const void *)(cur->d_un.d_ptr + obj->slide); break;
        case DT_RELASZ: relasz = cur->d_un.d_val; break;
        case DT_RELAENT: relaent = cur->d_un.d_val; break;
        case DT_PLTRELSZ: pltrelsz = cur->d_un.d_val; break;
        case DT_JMPREL: jmprel = (const void *)(cur->d_un.d_ptr + obj->slide); break;
        }
    }

    if (rela) do_relocations(obj, rela, relaent, relasz);
    if (jmprel) do_relocations(obj, jmprel, sizeof(Elf64_Rela), pltrelsz);
}

void process_dependencies(object_t *root) {
    search_first = search_last = root;

    for (object_t *obj = search_first; obj != NULL; obj = obj->search_next) {
        for (const Elf64_Dyn *cur = obj->dynamic; cur->d_tag != DT_NULL; cur++) {
            if (cur->d_tag == DT_NEEDED) {
                const char *name = obj->strtab + cur->d_un.d_val;

                object_t *object = get_object(name);
                if (!object) object = load_object(obj, name);

                if (object->search_next == NULL && object != search_last) {
                    search_last->search_next = object;
                    search_last = object;
                }
            }
        }
    }

    for (object_t *obj = search_first; obj != NULL; obj = obj->search_next) {
        relocate_object(obj);
    }
}

static uint32_t get_elf_hash(const char *name) {
    uint32_t hash = 0;
    for (;;) {
        char c = *name++;
        if (!c) break;
        hash = (hash << 4) + c;
        uint32_t top = hash & 0xf0000000;
        if (top) hash ^= top >> 24;
        hash &= ~top;
    }
    return hash;
}

const Elf64_Sym *search_for_symbol(const char *name, object_t **owner_out) {
    uint32_t hash = get_elf_hash(name);
    const Elf64_Sym *sym = NULL;
    object_t *owner = NULL;

    for (object_t *cur_obj = search_first; cur_obj != NULL; cur_obj = cur_obj->search_next) {
        Elf64_Word idx = cur_obj->hash.buckets[hash % cur_obj->hash.nbuckets];

        while (idx != STN_UNDEF) {
            const Elf64_Sym *cur = (void *)cur_obj->symtab + idx * cur_obj->syment;

            if (!sym || ELF64_ST_BIND(cur->st_info) != STB_WEAK) {
                if (cur->st_value != 0 && strcmp(cur_obj->strtab + cur->st_name, name) == 0) {
                    sym = cur;
                    owner = cur_obj;

                    if (ELF64_ST_BIND(cur->st_info) != STB_WEAK) goto done;
                }
            }

            idx = cur_obj->hash.chains[idx];
        }
    }

done:
    *owner_out = owner;
    return sym;
}
