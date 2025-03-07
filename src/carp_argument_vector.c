#include "carp_argument_vector.h"

#include <stdlib.h>

static int resize(
    struct CarpArgumentVector* vec,
    int capacity)
{
    void* mem = realloc(vec->buf, capacity * sizeof(char*));

    if (mem) {
        vec->buf = (const char**)mem;
        vec->capacity = capacity;
        return 0;
    }
    else {
        return 1;
    }
}

int carp_vector_init(
    struct CarpArgumentVector* vec,
    int capacity)
{
    void* mem = malloc(capacity * (sizeof(char*)));

    if (mem) {
        vec->buf = (const char**)mem;
        vec->capacity = capacity;
        vec->size = 0;
        return 0;
    }
    else {
        return 1;
    }
}

void carp_vector_cleanup(
    struct CarpArgumentVector* vec)
{
    free(vec->buf);
    vec->buf = NULL;
    vec->capacity = 0;
    vec->size = 0;
}

void carp_vector_push(
    struct CarpArgumentVector* vec,
    const char* elem)
{
    if (vec->size == vec->capacity) {
        resize(vec, vec->capacity * 2);
    }

    vec->buf[vec->size++] = elem;
}

const char* carp_vector_pop(
    struct CarpArgumentVector* vec)
{
    if (vec->size == 0) {
        return NULL;
    }

    const char* elem = vec->buf[--vec->size];

    if (vec->size <= vec->capacity / 4) {
        resize(vec, vec->capacity / 2);
    }

    return elem;
}

const char* carp_vector_at(
    struct CarpArgumentVector* vec,
    int index)
{
    if (index < vec->size) {
        return vec->buf[index];
    }
    else {
        return NULL;
    }
}

int carp_vector_get_size(
    struct CarpArgumentVector* vec)
{
    return vec->size;
}
