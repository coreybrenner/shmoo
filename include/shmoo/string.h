#ifndef _shmoo_string_h
#define _shmoo_string_h

#include <stdint.h>

#include <shmoo/compile.h>

__CDECL_BEG

/*forward*/ struct _shmoo_string;
typedef     struct _shmoo_string shmoo_string_t;

struct _shmoo_string {
    uint64_t       size;
    const uint8_t* data;
};

extern int shmoo_string_data    (const shmoo_string_t*, uint64_t, uint8_t**, uint64_t*);
extern int shmoo_string_size    (const shmoo_string_t*, uint64_t*);
extern int shmoo_string_make    (shmoo_string_t**, const uint8_t*, uint64_t);
extern int shmoo_string_dest    (shmoo_string_t**);
extern int shmoo_string_init    (shmoo_string_t*, const uint8_t*, uint64_t);
extern int shmoo_string_copy    (const shmoo_string_t*, uint64_t, uint64_t, uint8_t*);

__CDECL_END

#endif /* ! _shmoo_string_h */
