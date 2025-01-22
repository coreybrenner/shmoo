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


typedef int (*shmoo_buffer_dest_f) (shmoo_buffer_t**);
typedef int (*shmoo_buffer_free_f) (shmoo_buffer_t**);
typedef int (*shmoo_buffer_grow_f) (shmoo_buffer_t*, size_t*);
typedef int (*shmoo_buffer_trim_f) (shmoo_buffer_t*, size_t*);

struct _shmoo_buffer_type {
    const char*                 name;
    const shmoo_buffer_dest_f   dest;
    const shmoo_buffer_grow_f   grow;
    const shmoo_buffer_trim_f   trim;
    const shmoo_buffer_free_f   free;
};

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
    const uint8_t*              data;
#else
    const uint8_t*              data;
    size_t                      used;
#endif
    const shmoo_buffer_type_t*  type;
    size_t                      size;
};

extern int shmoo_buffer_make (
    shmoo_buffer_t**            __bufp,
    const shmoo_buffer_type_t*  __type,
    uint8_t**                   __data,
    size_t                      __size
);

extern int shmoo_buffer_init (
    shmoo_buffer_t*             __buf,
    const shmoo_buffer_type_t*  __type,
    uint8_t*                    __data,
    size_t                      __size
);

extern int shmoo_buffer_free (
    shmoo_buffer_t*             __buf
);

extern int shmoo_buffer_dest (
    shmoo_buffer_t**            __bufp
);

extern int shmoo_buffer_data (
    const shmoo_buffer_t*       __buf,
    size_t                      __offset,
    uint8_t**                   __data,
    size_t*                     __leftp
);

extern int shmoo_buffer_size (
    const shmoo_buffer_t*       __buf,
    size_t*                     __sizep
);

extern int shmoo_buffer_used (
    const shmoo_buffer_t*       __buf,
    size_t*                     __usedp
);

extern size_t shmoo_buffer_copy (
    const shmoo_buffer_t*       __buf,
    size_t                      __offset,
    size_t                      __length,
    uint8_t*                    __dest
);

extern int shmoo_buffer_grow (
    shmoo_buffer_t*             __buf,
    size_t                      __newsize
);

extern int shmoo_buffer_trim (
    shmoo_buffer_t*             __buf,
    size_t                      __newsize
);

__CDECL_END

#endif /* ! _shmoo_cursor_h */
