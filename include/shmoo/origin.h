#ifndef _shmoo_origin_h
#define _shmoo_origin_h

#include <shmoo/compile.h>

__CDECL_BEG

/*forward*/ struct _shmoo_origin;
typedef     struct _shmoo_origin shmoo_origin_t;

#include <shmoo/input.h>
#include <shmoo/token.h>

struct _shmoo_origin {
    const shmoo_input_t*    inpu;
    uint64_t                from;   /* starting byte offset */
    uint32_t                line;
};

__CDECL_END

#endif /* ! _shmoo_origin_h */
