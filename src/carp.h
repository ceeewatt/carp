#pragma once

struct Carp {
    const char** argv;
    int argc;
};

void carp_parse(
    struct Carp* carp,
    int argc,
    char* argv[],
    void* callback_param);

void carp_cleanup(
    struct Carp* carp);
