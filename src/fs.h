#pragma once
#include "arena.h"
#include <errno.h>
int fs_read(Arena* arena, const char* path, void** result, size_t* size);
#ifdef FS_IMPLEMENTATION
int fs_read(Arena* arena, const char* path, void** result, size_t* size) {
    FILE* f = fopen(path, "rb");
    if(!f) 
        goto fopen_err;
    if(fseek(f, 0, SEEK_END) < 0)
        goto ferr;
    long m = ftell(f);
    if (m < 0)
        goto ferr;
    if (fseek(f, 0, SEEK_SET) < 0)
        goto ferr;
    Arena_Mark mark = arena_snapshot(arena);
    void* data = arena_alloc(arena, m);
    fread(data, m, 1, f);
    if(ferror(f)) {
        arena_rewind(arena, mark);
        goto ferr;
    }
    *result = data;
    *size = m;
    return 0;
ferr:
    fclose(f);
fopen_err:
    return -errno;
}
#endif
