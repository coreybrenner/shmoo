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
#ifndef _shmoo_string_h
#define _shmoo_string_h

#include <stdint.h>

#include <shmoo/compile.h>

__CDECL_BEG

/*forward*/ struct _shmoo_string;
typedef     struct _shmoo_string shmoo_string_t;

/* Align this structure as much as possible with struct iovec.
 * Windows has a similar structure used for similar things, but
 * its members are backwards of iovec.  Strings are read-only
 * views on a buffer, where it might be useful to readv().
 */
struct _shmoo_string {
#if defined(_Win32) || defined(_Win64)
    size_t         size;
    const uint8_t* data;
#else
    const uint8_t* data;
    size_t         size;
#endif
};

/*
extern int shmoo_string_data (
    const shmoo_string_t*   __str,
    size_t                  __offset,
    const uint8_t**         __datap,
    size_t*                 __leftp
);
*/
inline int shmoo_string_data (
    const shmoo_string_t*   str,
    size_t                  off,
    const uint8_t**         datap,
    size_t*                 leftp
    )
{
    return (! (str && datap && leftp) ? 0 : (
            ((off >= str->size) ? 0 : (
             *datap = (str->data + off),
             *leftp = (str->size - off),
             1
            ))
           ));
}

/*
extern int shmoo_string_size (
    const shmoo_string_t*   __str,
    size_t*                 __sizep
);
*/
inline int shmoo_string_size (const shmoo_string_t* str, size_t* sizep) {
    return (! (str && sizep) ? 0 : ((*sizep = str->size), 1));
}

extern int shmoo_string_make (
    shmoo_string_t**        __strp,
    const uint8_t*          __data,
    size_t                  __size
);

extern int shmoo_string_dest (
    shmoo_string_t**        __strp  /* pointer set to 0 after free() */
);

/*
extern int shmoo_string_init (
    shmoo_string_t*         __str,
    const uint8_t*          __data,
    size_t                  __size
);
*/
inline int shmoo_string_init (shmoo_string_t* str, const uint8_t* data, size_t size) {
    return (! str ? 0 : (
            str->data = data,
            str->size = size,
            1
           ));
}

/*
extern size_t shmoo_string_copy (
    const shmoo_string_t*   __str,
    size_t                  __offset,
    size_t                  __length,
    uint8_t*                __dest
);
*/
inline size_t shmoo_string_copy (
    const shmoo_string_t* str,
    size_t                off,
    size_t                len,
    uint8_t*              dest
    )
{
    size_t copy = (((off + len) > str->size) ? (str->size - off) : len);
    return (! (str && dest) ? 0 : (
            ((off >= str->size) ? 0 : (
             (memcpy(dest, (str->data + off), copy),
              copy
             )
            ))
           ));
}

inline shmoo_string_t shmoo_string (const char* data) {
    shmoo_string_t str = { .data = (const uint8_t*) data, .size = strlen(data) };
    return str;
}

__CDECL_END

#endif /* ! _shmoo_string_h */
