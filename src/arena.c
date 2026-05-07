#include "../include/arena.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

Arena* global_arena = NULL;

Arena* arena_new() {
    Arena* a = malloc(sizeof(Arena));
    if (!a) return NULL;
    a->blocks = NULL;
    return a;
}

void* arena_alloc(Arena* a, size_t size) {
    if (!a) return malloc(size);
    
    void* ptr = malloc(size);
    if (!ptr) return NULL;
    
    ArenaBlock* block = malloc(sizeof(ArenaBlock));
    if (!block) {
        free(ptr);
        return NULL;
    }
    
    block->ptr = ptr;
    block->size = size;
    block->next = a->blocks;
    a->blocks = block;
    
    return ptr;
}

void arena_free_all(Arena* a) {
    if (!a) return;
    ArenaBlock* curr = a->blocks;
    while (curr) {
        ArenaBlock* next = curr->next;
        free(curr->ptr);
        free(curr);
        curr = next;
    }
    a->blocks = NULL;
}

void arena_destroy(Arena* a) {
    if (!a) return;
    arena_free_all(a);
    free(a);
}

void nr_init_memory() {
    if (!global_arena) global_arena = arena_new();
}

void* nr_malloc(size_t size) {
    return arena_alloc(global_arena, size);
}

void* nr_realloc(void* ptr, size_t old_size, size_t new_size) {
    void* new_ptr = nr_malloc(new_size);
    if (ptr && new_ptr) {
        memcpy(new_ptr, ptr, old_size < new_size ? old_size : new_size);
    }
    return new_ptr;
}

char* nr_strdup(const char* s) {
    if (!s) return NULL;
    size_t len = strlen(s);
    char* res = nr_malloc(len + 1);
    if (res) memcpy(res, s, len + 1);
    return res;
}

void nr_free_all() {
    arena_free_all(global_arena);
}

ArenaBlock* arena_checkpoint(Arena* a) {
    return a ? a->blocks : NULL;
}

void arena_rollback(Arena* a, ArenaBlock* checkpoint) {
    if (!a) return;
    ArenaBlock* curr = a->blocks;
    while (curr && curr != checkpoint) {
        ArenaBlock* next = curr->next;
        free(curr->ptr);
        free(curr);
        curr = next;
    }
    a->blocks = checkpoint;
}

ArenaBlock* nr_arena_checkpoint() {
    return arena_checkpoint(global_arena);
}

void nr_arena_rollback(ArenaBlock* checkpoint) {
    arena_rollback(global_arena, checkpoint);
}
