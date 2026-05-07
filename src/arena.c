#include "../include/arena.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

Arena* global_arena = NULL;

Arena* arena_new() {
    Arena* a = malloc(sizeof(Arena));
    if (!a) return NULL;
    // Allocate 1GB continuous heap for Nira runtime
    size_t heap_size = 1024 * 1024 * 1024;
    a->heap_start = malloc(heap_size);
    if (!a->heap_start) {
        printf("Nira: Failed to allocate 1GB heap\n");
        exit(1);
    }
    a->heap_end = a->heap_start + heap_size;
    a->current = a->heap_start;
    return a;
}

void* arena_alloc(Arena* a, size_t size) {
    if (!a) return malloc(size);
    // Align to 8 bytes
    size = (size + 7) & ~7;
    if (a->current + size > a->heap_end) {
        printf("Nira: Out of Memory (Arena Exhausted)\n");
        exit(1);
    }
    void* ptr = a->current;
    a->current += size;
    return ptr;
}

void arena_free_all(Arena* a) {
    if (!a) return;
    a->current = a->heap_start;
}

void arena_destroy(Arena* a) {
    if (!a) return;
    free(a->heap_start);
    free(a);
}

void arena_gc(Arena* a) {
    // Simple reset for benchmarks. A real GC would be needed for complex apps.
    // For now, bump allocator is enough for speed tests.
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
    if (!a) return NULL;
    return (ArenaBlock*)a->current;
}

void arena_rollback(Arena* a, ArenaBlock* checkpoint) {
    if (!a || !checkpoint) return;
    a->current = (char*)checkpoint;
}

ArenaBlock* nr_arena_checkpoint() {
    return arena_checkpoint(global_arena);
}

void nr_arena_rollback(ArenaBlock* checkpoint) {
    arena_rollback(global_arena, checkpoint);
}
