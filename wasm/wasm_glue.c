/*
 * wasm_glue.c — Bare-metal WASM C runtime and module initialization.
 *
 * Provides the minimal C runtime needed for -nostdlib builds:
 *   - Memory functions: memset, memcpy, memmove, memcmp
 *   - Heap allocator:   malloc, calloc, free (simple bump allocator)
 *   - Module init:      initialize() — resets the heap, called from JS
 *
 * The bump allocator is intentionally simple: free() is a no-op.
 * This is appropriate for the typical JS usage pattern where the page
 * lifecycle bounds the allocator lifetime.  Call initialize() to reset
 * the heap (frees all previous allocations at once).
 */

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── Memory functions ─────────────────────────────────────────────────── */

void *memset(void *s, int c, size_t n) {
    unsigned char *p = (unsigned char *)s;
    unsigned char  v = (unsigned char)c;
    while (n--) *p++ = v;
    return s;
}

void *memcpy(void *dst, const void *src, size_t n) {
    unsigned char       *d = (unsigned char *)dst;
    const unsigned char *s = (const unsigned char *)src;
    while (n--) *d++ = *s++;
    return dst;
}

void *memmove(void *dst, const void *src, size_t n) {
    unsigned char       *d = (unsigned char *)dst;
    const unsigned char *s = (const unsigned char *)src;
    if (d < s) {
        while (n--) *d++ = *s++;
    } else {
        d += n; s += n;
        while (n--) *--d = *--s;
    }
    return dst;
}

int memcmp(const void *s1, const void *s2, size_t n) {
    const unsigned char *a = (const unsigned char *)s1;
    const unsigned char *b = (const unsigned char *)s2;
    while (n--) {
        if (*a != *b) return (int)*a - (int)*b;
        a++; b++;
    }
    return 0;
}

/* ── Bump allocator ───────────────────────────────────────────────────── */
/*
 * Reserve enough heap for several cortex_emu_t instances.
 * Each instance is ~800 KB; 4 MB covers ~5 simultaneous instances.
 * Increase HEAP_SIZE if more instances are needed.
 */
#define HEAP_SIZE (4 * 1024 * 1024)

static unsigned char s_heap[HEAP_SIZE] __attribute__((aligned(8)));
static size_t        s_heap_top = 0;

void *malloc(size_t size) {
    size = (size + 7u) & ~(size_t)7u;   /* align to 8 bytes */
    if (s_heap_top + size > HEAP_SIZE) return 0;
    void *ptr = s_heap + s_heap_top;
    s_heap_top += size;
    return ptr;
}

void *calloc(size_t n, size_t size) {
    size_t total = n * size;
    void  *ptr   = malloc(total);
    if (ptr) memset(ptr, 0, total);
    return ptr;
}

void free(void *ptr) {
    (void)ptr; /* bump allocator: no individual frees */
}

/* ── Module initialization ────────────────────────────────────────────── */

/*
 * initialize() — must be called once from JS before using the module.
 * Resets the heap allocator (effectively frees all previous instances).
 */
__attribute__((visibility("default")))
void initialize(void) {
    s_heap_top = 0;
}

#ifdef __cplusplus
} /* extern "C" */
#endif
