/* Copyright (c) 2025, coreybrenner
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 * 
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#ifndef _shmoo_vector_h
#define _shmoo_vector_h

#include <errno.h>
#include <stdint.h>
#include <string.h>

#include <shmoo/compile.h>

__CDECL_BEG

/*forward*/ struct _shmoo_vector_type;
typedef     struct _shmoo_vector_type shmoo_vector_type_t;
/*forward*/ struct _shmoo_vector;
typedef     struct _shmoo_vector      shmoo_vector_t;

typedef void (*shmoo_vector_destelem_f) (void*);

struct _shmoo_vector_type {
    const char*             type_name;
    size_t                  elem_size;
    shmoo_vector_destelem_f dest_elem;
};

struct _shmoo_vector {
    const shmoo_vector_type_t*  type;
    size_t                      size;
    size_t                      used;
    void*                       data;
};

extern int shmoo_vector_make (shmoo_vector_t**, const shmoo_vector_type_t*);
extern int shmoo_vector_init (shmoo_vector_t*, const shmoo_vector_type_t*);
extern int shmoo_vector_free (shmoo_vector_t*);
extern int shmoo_vector_dest (shmoo_vector_t**);
extern int shmoo_vector_peek (const shmoo_vector_t*, size_t, void*);

inline int shmoo_vector_used (const shmoo_vector_t* vec, size_t* usedp) {
    return (! (vec && usedp) ? 0 : ((*usedp = vec->used), 1));
}

inline int shmoo_vector_size (const shmoo_vector_t* vec, size_t* sizep) {
    return (! (vec && sizep) ? 0 : ((*sizep = vec->size), 1));
}

inline int shmoo_vector_type (const shmoo_vector_t* vec, const char** typep) {
    return (! (vec && typep) ? EINVAL : ((*typep = vec->type->type_name), 0));
}

inline void* shmoo_vector_head (const shmoo_vector_t* vec) {
    return (! (vec && vec->used) ? NULL : vec->data);
}

inline void* shmoo_vector_tail (const shmoo_vector_t* vec) {
    if (! (vec && vec->used)) {
        return NULL;
    } else {
        uint8_t* data = vec->data;
        data += ((vec->used - 1) * vec->type->elem_size);
        return data;
    }
}

inline void* shmoo_vector_slot (const shmoo_vector_t* vec, size_t slot) {
    if (! (vec && (vec->used > slot))) {
        return NULL;
    } else {
        uint8_t* data = vec->data;
        data += (slot * vec->type->elem_size);
        return data;
    }
}

extern int shmoo_vector_insert_head (shmoo_vector_t*, const void*);
extern int shmoo_vector_remove_head (shmoo_vector_t*, void*);
extern int shmoo_vector_insert_tail (shmoo_vector_t*, const void*);
extern int shmoo_vector_remove_tail (shmoo_vector_t*, void*);

inline int shmoo_vector_obtain_head (const shmoo_vector_t* vec, void* dest) {
    return (! (vec && dest)
                ? EINVAL
                : (((void) memcpy(dest, vec->data, vec->type->elem_size)), 0)
           );
}

inline int shmoo_vector_obtain_tail (const shmoo_vector_t* vec, void* dest) {
    const uint8_t* data;
    size_t         esiz;
    if (! (vec && dest)) {
        return EINVAL;
    }
    esiz = vec->type->elem_size;
    data = vec->data;
    data += ((vec->used - 1) * esiz);
    return (! (vec && dest)
                ? EINVAL
                : ((void) memcpy(dest, data, esiz), 0)
           );
}

__CDECL_END

#endif /* ! _shmoo_vector_h */
