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

#include <stdint.h>
#include <string.h>     /* memcpy */

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
    size_t                      size;
    size_t                      used;
    void*                       data;
};

extern int      shmoo_vector_init (shmoo_vector_t*, const shmoo_vector_type_t*);
extern int      shmoo_vector_free (shmoo_vector_t*);
/*
extern int      shmoo_vector_used (const shmoo_vector_t*, uint32_t*);
*/
inline int shmoo_vector_used (const shmoo_vector_t* vec, size_t* usedp) {
    return (! (vec && usedp) ? 0 : ((*usedp = vec->used), 1));
}

inline int shmoo_vector_size (const shmoo_vector_t* vec, size_t* sizep) {
    return (! (vec && sizep) ? 0 : ((*sizep = vec->size), 1));
}

inline int shmoo_vector_type (const shmoo_vector_t* vec, const char** typep) {
    return (! (vec && typep) ? 0 : ((*typep = vec->type->name), 1));
}

/*
extern int      shmoo_vector_peek (const shmoo_vector_t*, uint32_t, void*);
extern void*    shmoo_vector_tail (const shmoo_vector_t*);
*/
inline int shmoo_vector_peek (const shmoo_vector_t* vec, size_t indx, void* outp) {
    return (! (vec && outp && (indx >= vec->used)) ? 0 : (
            (memcpy(outp,
                    ((uint8_t*) vec->data + (indx * vec->type->elem)),
                    vec->type->elem
                   ),
             1
            )
           );
}

inline void* shmoo_vector_tail (const shmoo_vector_t* vec) {
    return (! (vec && vec->used) ? 0 : (
            ((uint8_t*) vec->data + (vec->type->elem * (vec->used - 1)))
           );
}

extern int      shmoo_vector_insert_head (shmoo_vector_t*, const void*);
extern int      shmoo_vector_insert_tail (shmoo_vector_t*, const void*);
/*
extern int      shmoo_vector_remove_tail (shmoo_vector_t*, void*);
*/
inline int shmoo_vector_remove_tail (shmoo_vector_t* vec, void* outp) {
    return (! (vec && vec->used && outp) ? 0 : (
            (memcpy(outp,
                    ((uint8_t*) vec->data + (vec->type->elem * --vec->used)), 
                    vec->data->elem
                   ),
             memset(((uint8_t*) vec->data + (vec->type->elem * vec->used)),
                    0, vec->type->elem),
             1
            )
           );
}

__CDECL_END

#endif /* ! _shmoo_vector_h */
