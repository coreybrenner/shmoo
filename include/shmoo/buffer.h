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
#ifndef _shmoo_buffer_h
#define _shmoo_buffer_h

#include <stdint.h>         /* uint8_t, uint64_t */

#include <shmoo/compile.h>

__CDECL_BEG

/* This is a simple linear buffer, useful as a shmoo_string_t.
 * Data starts at offset 0, always.  Not useful for a ring buffer.
 */

/*forward*/ struct _shmoo_buffer_type;
typedef     struct _shmoo_buffer_type shmoo_buffer_type_t;
/*forward*/ struct _shmoo_buffer;
typedef     struct _shmoo_buffer      shmoo_buffer_t;


typedef void (*shmoo_buffer_dest_f) (shmoo_buffer_t**);
typedef void (*shmoo_buffer_free_f) (shmoo_buffer_t*);
typedef int  (*shmoo_buffer_grow_f) (shmoo_buffer_t*, size_t);

struct _shmoo_buffer_type {
    const char*                 type_name;
    const shmoo_buffer_dest_f   dest;
    const shmoo_buffer_free_f   free;
    const shmoo_buffer_grow_f   grow;
};

extern const shmoo_buffer_type_t shmoo_static_buffer_type;
extern const shmoo_buffer_type_t shmoo_dynamic_buffer_type;

/* shmoo_buffer_t is like a shmoo_string_t, and can be
 * cast-and-passed as one, but the 'size' field of the
 * string portin is used for tracking the number of bytes
 * used by the data in the buffer, while the actual size
 * of the data allocated to the buffer is tracked by the
 * buffer's 'size' field.  Like shmoo_string_t, this should
 * model iovec on UNIX platforms, and whatever Windows
 * calls their similar structure.
 */
struct _shmoo_buffer {
#if defined(_Win32) || defined(_Win64)
    size_t                      used;
    uint8_t*                    data;
#else
    uint8_t*                    data;
    size_t                      used;
#endif
    const shmoo_buffer_type_t*  type;
    size_t                      size;
};

#define shmoo_buffer_make(buffp,data,size,...) \
    (_shmoo_buffer_make((buffp),(data),(size),##__VA_ARGS__,(const shmoo_buffer_t*)0))
extern int _shmoo_buffer_make (
    shmoo_buffer_t**            __buffp,
    uint8_t*                    __data,
    size_t                      __size,
    ...
);

#define shmoo_buffer_init(buff,data,size,...) \
    (_shmoo_buffer_make((buff),(data),(size),##__VA_ARGS__,(const shmoo_buffer_t*)0))
extern int _shmoo_buffer_init (
    shmoo_buffer_t*             __buff,
    uint8_t*                    __data,
    size_t                      __size,
    ...
);

inline int shmoo_buffer_free (shmoo_buffer_t* buff) {
    return (! buff ? EINVAL : (buff->type->free(buff), 0));
}

inline int shmoo_buffer_dest (shmoo_buffer_t** buffp) {
    return (! buffp ? EINVAL : (! *buffp ? 0 : (((*buffp)->type->dest(buffp)), 0)));
}

inline int shmoo_buffer_data (
    const shmoo_buffer_t*       buff,
    size_t                      offset,
    uint8_t**                   datap,
    size_t*                     leftp
    )
{
    return (! (buff && datap && leftp)
                ? EINVAL
                : ((offset >= buff->used)
                    ? ERANGE 
                    : (  (*datap = (buff->data + offset))
                       , (*leftp = (buff->used - offset))
                       , 0
                      )
                  )
           );
}

inline int shmoo_buffer_size (const shmoo_buffer_t* buff, size_t* sizep) {
    return (! (buff && sizep) ? EINVAL : ((*sizep = buff->size), 0));
}

inline int shmoo_buffer_used (const shmoo_buffer_t* buff, size_t* usedp) {
    return (! (buff && usedp) ? EINVAL : ((*usedp = buff->used), 0));
}

inline int shmoo_buffer_type (const shmoo_buffer_t* buff, const char** typep) {
    return (! (buff && typep) ? EINVAL : ((*typep = buff->type->type_name), 0));
}

inline int shmoo_buffer_grow (shmoo_buffer_t* buff, size_t newsize) {
    return (! buff ? EINVAL : (buff->type->grow(buff, newsize), 0));
}

extern size_t shmoo_buffer_copy (
    const shmoo_buffer_t*       __buff,
    size_t                      __offset,
    size_t                      __length,
    uint8_t*                    __dest
);

__CDECL_END

#endif /* ! _shmoo_cursor_h */
