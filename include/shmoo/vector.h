#ifndef _shmoo_vector_h
#define _shmoo_vector_h

#include <stdint.h>

#include <shmoo/compile.h>

__CDECL_BEG

/*forward*/ struct _shmoo_vector_type;
typedef     struct _shmoo_vector_type shmoo_vector_type_t;
/*forward*/ struct _shmoo_vector;
typedef     struct _shmoo_vector      shmoo_vector_t;

struct _shmoo_vector_type {
    const char* name;
    uint32_t    elem;
};

struct _shmoo_vector {
    const shmoo_vector_type_t*  type;
    uint32_t                    size;
    uint32_t                    used;
    void*                       data;
};

extern int      shmoo_vector_init (shmoo_vector_t*, const shmoo_vector_type_t*);
extern int      shmoo_vector_free (shmoo_vector_t*);
extern int      shmoo_vector_used (const shmoo_vector_t*, uint32_t*);
extern int      shmoo_vector_peek (const shmoo_vector_t*, uint32_t, void*);
extern void*    shmoo_vector_tail (const shmoo_vector_t*);
extern int      shmoo_vector_insert_tail (shmoo_vector_t*, const void*);
extern int      shmoo_vector_remove_tail (shmoo_vector_t*, void*);

__CDECL_END

#endif /* ! _shmoo_vector_h */
