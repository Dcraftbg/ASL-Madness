#include <stdio.h>
#define ARENA_IMPLEMENTATION
#include "arena.h"
#undef ARENA_IMPLEMENTATION
#define FS_IMPLEMENTATION
#include "fs.h"
#include <string.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <ctype.h>

typedef struct {
    Arena* arena;
    void *head, *end;
    uint32_t depth;
} Decompiler;
Arena global_arena;

const char* shift_args(int* argc, const char*** argv) {
    if((*argc) <= 0) return NULL;
    const char* result = **argv;
    (*argv)++;
    (*argc)--;
    return result;
}
#define eprintf(...) fprintf(stderr, __VA_ARGS__)
#define eprintfln(...) (fprintf(stderr, __VA_ARGS__), fputc('\n', stderr))
#define printfln(...) (printf(__VA_ARGS__), fputc('\n', stdout))
static void ppad(size_t count, FILE* sink) {
    for(size_t i = 0; i < count*4; ++i) fputc(' ', sink);
}
#define dcprintf(dc, ...) (ppad((dc)->depth, stdout), printf(__VA_ARGS__))
#define dcprintfln(dc, ...) (dcprintf(dc, __VA_ARGS__), fputc('\n', stdout))
typedef struct {
    char     signature[4];
    uint32_t length;
    uint8_t  revision;
    uint8_t  checksum;
    char     oemid[6];
    char     oemtableid[8];
    uint32_t oemrevision;
    uint32_t creatorid;
    uint32_t creatorrevision;
} __attribute__((packed)) SDT;
static size_t dc_left(Decompiler* dc) {
    return dc->end-dc->head;
}
SDT* peek_sdt(Decompiler* dc) {
    if(dc_left(dc) < sizeof(SDT)) return NULL;
    return (SDT*)dc->head;
}
uint8_t dc_eat_byte(Decompiler* dc) {
    return ((uint8_t*)(dc->head++))[0];
}
uint8_t dc_peak_byte(Decompiler* dc) {
    return ((uint8_t*)(dc->head))[0];
}
uint32_t dc_eat_u32(Decompiler* dc) {
    uint32_t data = ((uint32_t*)dc->head)[0];
    dc->head += sizeof(uint32_t);
    return data;
}
uint16_t dc_eat_u16(Decompiler* dc) {
    uint16_t data = ((uint16_t*)dc->head)[0];
    dc->head += sizeof(uint16_t);
    return data;
}
int parse_pkg_len(Decompiler* dc) {
    if(dc->head+1 >= dc->end) {
        eprintfln("Premature EOF when parsing pkg_len");
        return -1;
    }
    uint8_t lead = dc_eat_byte(dc);
    uint8_t count = lead >> 6;
    if (count == 0) {
        return lead & 0b111111;
    } else {
        eprintfln("Unsupported pkg_len count=%d", count);
        return -1;
    }
}
int pkg_len(Decompiler* dc) {
    void* head = dc->head;
    int len = parse_pkg_len(dc);
    if(len < 0) return len;
    if(len < dc->head-head) {
        eprintfln("Invalid pkg len smaller than itself");
        return -1;
    }
    return len-(dc->head-head);
}
bool verify_checksum(uint8_t* bytes, size_t count) {
    uint8_t checksum = 0;
    for(size_t i = 0; i < count; ++i) {
        checksum += bytes[i];
    }
    return checksum == 0;
}

const char* dc_nameseg(Decompiler* dc) {
    if(dc_left(dc) == 0) return NULL;
    uint8_t lead = dc_peak_byte(dc);
    if((lead >= 'A' && lead <= 'Z') || lead == '_') {
        if(dc_left(dc) < 4) return NULL;
        char* str = arena_alloc(dc->arena, 5);
        for(size_t i = 0; i < 4; ++i) {
            uint8_t byte = dc_eat_byte(dc);
            if(byte == '_' && i > 0) {
                str[i] = '\0';
                continue;
            }
            str[i] = byte;
        }
        str[4] = '\0';
        return str;
    }
    eprintf("Unhandled leading character: `");
    if(isprint(lead)) eprintf("%c", lead);
    else eprintf("\\x%02X", lead);
    eprintfln("` (%02X)", lead);
    return NULL;
}
int decompile_dataref(Decompiler* dc) {
    if(dc_left(dc) == 0) 
        return -1;
    int e;
    uint8_t op = dc_eat_byte(dc); 
    switch(op) {
    case 0x0B:
        if(dc_left(dc) < 2)
            return -1;
        dcprintfln(dc, "0x%04X", dc_eat_u16(dc));
        return 0;
    case 0x0C:
        if(dc_left(dc) < 4)
            return -1;
        dcprintfln(dc, "0x%08X", dc_eat_u32(dc));
        return 0;
    default:
        eprintfln("Unknown dataref op: 0x%02X", op);
        return -1;
    }
    return 0;
}
int decompile_obj(Decompiler* dc) {
    if(dc_left(dc) == 0) 
        return -1;
    int e;
    uint8_t op = dc_eat_byte(dc); 
    switch(op) {
        case 0x08: {
            const char* name = dc_nameseg(dc);
            if(!name) {
                eprintfln("Failed to parse name");
                return -1;
            }
            dcprintfln(dc, "Name(%s, ", name); 
            dc->depth++;
            if((e=decompile_dataref(dc)) < 0)
                return e;
            dc->depth--;
            dcprintfln(dc, ")");
            return 0;
        } break;
        case 0x5B: {
            if(dc_left(dc) == 0)
                return -1;
            op = dc_eat_byte(dc);
            switch(op) {
                case 0x82: {
                    int len = pkg_len(dc);
                    if(len < 0) return -1;
                    if(dc_left(dc) < (size_t)len)
                        return -1;
                    void* next = dc->head+len;
                    void* end = dc->end; 

                    dc->end = next;
                    const char* name = dc_nameseg(dc);
                    if(!name) {
                        eprintfln("Failed to parse name");
                        return -1;
                    }
                    dcprintfln(dc, "Device(%s)", name);
                    dcprintfln(dc, "{");
                    dc->depth++;
                    while(dc_left(dc) > 0) {
                        // TODO: Remove recursion using a stack
                        if((e=decompile_obj(dc)) < 0) { 
                            eprintfln("decompile_obj failed");
                            return e;
                        }
                    }
                    dc->depth--;
                    dcprintfln(dc, "}");
                    dc->head = next;
                    dc->end = end;
                } break;
                default:
                    eprintfln("Unknown extended opcode: 0x%02X", op);
                    return -1;
            }
        } break;
        default:
            eprintfln("Unknown object op: 0x%02X", op);
            return -1;
    }
    return 0;
}
int decompile(Decompiler* dc) {
    int e;
    SDT* sdt = peek_sdt(dc);
    if(!sdt) {
        eprintfln("Failed to parse SDT");
        return -1;
    }
    if(dc_left(dc) > sdt->length) {
        eprintfln("Multiple SDTs in one file aren't supported. This file is invalid");
        return -1;
    }
    if(!verify_checksum(dc->head, sdt->length)) {
        eprintfln("Checksum failed");
        return -1;
    }

    dc->head += sizeof(SDT);
    dcprintfln(dc, "DefinitionBlock (\"\", \"%.4s\", %d, \"%.6s\", \"%.8s\", 0x%08X)", sdt->signature, sdt->revision, sdt->oemid, sdt->oemtableid, sdt->oemrevision);
    dcprintfln(dc, "{");
    dc->depth++;
    while(dc->head < dc->end) {
        uint8_t op = dc_eat_byte(dc); 
        switch(op) {
            case 0x10: {
                int len = pkg_len(dc);
                if(len < 0) return -1;
                if(dc_left(dc) < (size_t)len) 
                    return -1;
                void* next = dc->head+len;
                void* end = dc->end; 

                dc->end = next;
                const char* name = dc_nameseg(dc);
                if(!name) {
                    eprintfln("Failed to parse name");
                    return -1;
                }
                dcprintfln(dc, "Scope(%s)", name);
                dcprintfln(dc, "{");
                dc->depth++;
                if((e=decompile_obj(dc)) < 0) 
                    return e;
                dc->depth--;
                dcprintfln(dc, "}");
                dc->head = next;
                dc->end = end;
            } break;
            default:
                eprintfln("Unknown opcode: 0x%02X", op);
                return -1;
        }
    }
    dc->depth--;
    printfln("}");
    return 0;
}
int main(int argc, const char** argv) {
    const char* exe = shift_args(&argc, &argv);
    assert(exe);
    const char* ipath = NULL;
    const char* arg;
    while((arg=shift_args(&argc, &argv))) {
        if(!ipath) ipath = arg;
        else {
            eprintfln("Unknown argument: `%s`", arg);
            exit(1);
        }
    }
    if(!ipath) {
        eprintfln("Missing input path!");
        exit(1);
    }
    void* data;
    size_t size;
    int e;
    if((e=fs_read(&global_arena, ipath, &data, &size)) < 0) {
        eprintfln("Failed to read `%s`: %s", ipath, strerror(-e));
        goto read_err;
    }
    Decompiler decompiler={0};
    decompiler.arena = &global_arena;
    decompiler.head  = data;
    decompiler.end   = data+size;
    if(decompile(&decompiler) < 0) {
        eprintfln("Failed to decompile");
        goto decomp_err;
    }

    arena_free(&global_arena);
    return 0;
decomp_err:
read_err:
    arena_free(&global_arena);
    return 1;
}
