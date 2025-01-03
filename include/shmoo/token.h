#ifndef _shmoo_token_h
#define _shmoo_token_h

#include <stdint.h>

#include <shmoo/compile.h>
__CDECL_BEG

/*forward*/ struct _shmoo_token;
typedef     struct _shmoo_token shmoo_token_t;

#include <shmoo/input.h>

struct _shmoo_token {
    const shmoo_input_t*    inpu;
    uint64_t                from;
    uint32_t                size;
    uint32_t                span;
};

__CDECL_END

#endif /* ! _shmoo_token_h */
