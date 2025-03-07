#pragma once

typedef void (*CARP_CALLBACK)(void*, const char**, int);

struct CarpOption {
    const char* name;
    struct CarpOptionSpec {
        int arguments;
        CARP_CALLBACK callback;
    } spec;
};

struct CarpOptionSpec* carp_backend_search(
    const char* name,
    int len);
