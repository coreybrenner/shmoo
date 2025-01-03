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
#ifndef _shmoo_cursor_h
#define _shmoo_cursor_h

#include <stdint.h>         /* uint8_t, uint64_t */

#include <shmoo/compile.h>

__CDECL_BEG

/*forward*/ struct _shmoo_cursor;
typedef     struct _shmoo_cursor shmoo_cursor_t;

/* shmoo_cursor_t is like a shmoo_string_t, and can be
 * cast-and-passed as one, but the cursor marks the
 * starting offset within a larger buffer (like a file).
 * Like shmoo_string_t, this should model iovec on UNIX
 * platforms, and whatever Windows calls their similar
 * structure.
 */
struct _shmoo_cursor {
#if defined(_Win32) || defined(_Win64)
    size_t          size;
    const uint8_t*  data;
#else
    const uint8_t*  data;
    size_t          size;
#endif
    uint64_t        from;
};

extern int shmoo_cursor_make (
    shmoo_cursor_t**        __cur,
    const uint8_t*          __data,
    size_t                  __size,
    uint64_t                __offset
);

extern int shmoo_cursor_dest (
    shmoo_cursor_t**        __curp
);

extern int shmoo_cursor_init (
    shmoo_cursor_t*         __cur,
    const uint8_t*          __data,
    size_t                  __size,
    uint64_t                __offset
);

extern int shmoo_cursor_from (
    const shmoo_cursor_t*   __cur,
    uint64_t*               __fromp
);

extern int shmoo_cursor_data (
    const shmoo_cursor_t*   __cur,
    size_t                  __offset,
    const uint8_t**         __datap,
    size_t*                 __leftp
);

extern int shmoo_cursor_size (
    const shmoo_cursor_t*   __cur,
    size_t*                 __sizep
);

extern size_t shmoo_cursor_copy (
    const shmoo_cursor_t*   __cur,
    size_t                  __offset,
    size_t                  __length,
    uint8_t*                __dest
);

inline shmoo_cursor_t shmoo_cursor (const uint8_t* data, size_t length, uint64_t from) {
    shmoo_cursor_t cur = { .data = data, .size = length, .from = from };
    return cur;
}

__CDECL_END

#endif /* ! _shmoo_cursor_h */
