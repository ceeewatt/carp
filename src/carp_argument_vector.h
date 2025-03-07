#pragma once

struct CarpArgumentVector {
    const char** buf;
    int capacity;
    int size;
};

int carp_vector_init(
    struct CarpArgumentVector* vec,
    int capacity);

void carp_vector_cleanup(
    struct CarpArgumentVector* vec);

void carp_vector_push(
    struct CarpArgumentVector* vec,
    const char* elem);

const char* carp_vector_pop(
    struct CarpArgumentVector* vec);

const char* carp_vector_at(
    struct CarpArgumentVector* vec,
    int index);

int carp_vector_get_size(
    struct CarpArgumentVector* vec);
