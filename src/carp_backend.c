#include "carp_backend.h"

#include <stdlib.h>

#ifdef CARP_IMPLEMENTATION_HASH
extern struct CarpOptionSpec* carp_hash(const char* name, int len);
#else
extern struct CarpOptionSpec* carp_search(const char* name, int len);
#endif

struct CarpOptionSpec* carp_backend_search(
    const char* name,
    int len)
{
    struct CarpOptionSpec* spec = NULL;
#ifdef CARP_IMPLEMENTATION_HASH
    spec = carp_hash(name, len);
#else
    spec = carp_search(name, len);
#endif

    return spec;
}
