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
