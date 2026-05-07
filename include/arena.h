#ifndef ARENA_H
#define ARENA_H

#include <stddef.h>

typedef struct ArenaBlock {
    char* heap;
    struct ArenaBlock* next;
} ArenaBlock;

typedef struct {
    char* heap_start;
    char* heap_end;
    char* current;
    ArenaBlock* old_blocks;
} Arena;

Arena* arena_new();
void* arena_alloc(Arena* a, size_t size);
void arena_free_all(Arena* a);
void arena_destroy(Arena* a);

// Global arena for general use
extern Arena* global_arena;
void nr_init_memory();
void* nr_malloc(size_t size);
void* nr_realloc(void* ptr, size_t old_size, size_t new_size);
char* nr_strdup(const char* s);
void nr_free_all();
ArenaBlock* arena_checkpoint(Arena* a);
void arena_rollback(Arena* a, ArenaBlock* checkpoint);
ArenaBlock* nr_arena_checkpoint();
void nr_arena_rollback(ArenaBlock* checkpoint);

#endif
