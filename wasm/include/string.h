/*
 * Minimal string.h for bare-metal WASM builds (-nostdlib).
 * Implementations are provided in wasm_glue.c.
 */
#ifndef _WASM_STRING_H
#define _WASM_STRING_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

void *memset(void *s, int c, size_t n);
void *memcpy(void *dst, const void *src, size_t n);
void *memmove(void *dst, const void *src, size_t n);
int   memcmp(const void *s1, const void *s2, size_t n);

#ifdef __cplusplus
}
#endif

#endif /* _WASM_STRING_H */
