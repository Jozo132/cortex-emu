/*
 * Minimal stdlib.h for bare-metal WASM builds (-nostdlib).
 * Implementations are provided in wasm_glue.c.
 */
#ifndef _WASM_STDLIB_H
#define _WASM_STDLIB_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

void *malloc(size_t size);
void *calloc(size_t n, size_t size);
void  free(void *ptr);

#ifdef __cplusplus
}
#endif

#endif /* _WASM_STDLIB_H */
