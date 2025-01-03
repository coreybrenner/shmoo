#ifndef _shmoo_cursor_h
#define _shmoo_cursor_h

#include <stdint.h>

#include <shmoo/compile.h>

__CDECL_BEG

/*forward*/ struct _shmoo_cursor;
typedef     struct _shmoo_cursor shmoo_cursor_t;

struct _shmoo_cursor {
    uint64_t        from;
    uint64_t        size;
    const uint8_t*  data;
};

extern int shmoo_cursor_make    (shmoo_cursor_t**, const uint8_t*, uint64_t);
extern int shmoo_cursor_dest    (shmoo_cursor_t**);
extern int shmoo_cursor_init    (shmoo_cursor_t*, uint64_t, uint64_t, const uint8_t*);
extern int shmoo_cursor_from    (const shmoo_cursor_t*, uint64_t*);
extern int shmoo_cursor_data    (const shmoo_cursor_t*, uint64_t, const uint8_t**, uint64_t*);
extern int shmoo_cursor_size    (const shmoo_cursor_t*, uint64_t*);
extern int shmoo_cursor_copy    (const shmoo_cursor_t*, uint64_t, uint64_t, uint8_t*);

__CDECL_END

#endif /* ! _shmoo_cursor_h */
