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
#ifndef _shmoo_input_h
#define _shmoo_input_h

#include <stdint.h>
#include <stdio.h>
#include <time.h>

#include <shmoo/compile.h>

__CDECL_BEG

/*forward*/ struct _shmoo_input_type;
typedef     struct _shmoo_input_type shmoo_input_type_t;
/*forward*/ struct _shmoo_input;
typedef     struct _shmoo_input      shmoo_input_t;

#include <shmoo/handle.h>
#include <shmoo/origin.h>
#include <shmoo/vector.h>
#include <shmoo/cursor.h>
#include <shmoo/string.h>

struct _shmoo_input_type {
    const char*         name;
    int               (*dest) (shmoo_input_t**);
    uint64_t          (*more) (shmoo_input_t*, uint64_t);
    int               (*shut) (shmoo_input_t*);
};

struct _shmoo_input {
    const shmoo_input_type_t*       type;
    const uint8_t*                  name;
    uint64_t                        size;
    int64_t                         mtim;
    shmoo_vector_t                  part;
};

extern int      shmoo_input_open_file   (shmoo_input_t**, FILE*);
extern int      shmoo_input_open_path   (shmoo_input_t**, shmoo_string_t);
extern int      shmoo_input_open_desc   (shmoo_input_t**, shmoo_handle_t);
#define shmoo_input_open_text(inputp,text,...) \
    (_shmoo_input_open_text((inputp),(text),##__VA_ARGS__,(const shmoo_origin_t*)0))
extern int      _shmoo_input_open_text  (shmoo_input_t**, ...);

extern int      shmoo_input_dest        (shmoo_input_t**);
#define shmoo_input_data(input,from,datap,...) \
    (_shmoo_input_data((input),(from),(datap),##__VA_ARGS__,(uint64_t)0))
extern int      _shmoo_input_data       (shmoo_input_t*, uint64_t, shmoo_cursor_t*, ...);
extern uint64_t shmoo_input_read        (shmoo_input_t*, uint64_t, uint64_t, uint8_t*);
#define shmoo_input_more(input,...) \
    (_shmoo_input_more((input),##__VA_ARGS__,(uint64_t)0))
extern uint64_t _shmoo_input_more       (shmoo_input_t*, ...);
extern int      shmoo_input_shut        (shmoo_input_t*);
extern int      shmoo_input_size        (const shmoo_input_t*, uint64_t*);
extern int      shmoo_input_type        (const shmoo_input_t*, const char**);
extern int      shmoo_input_orig        (const shmoo_input_t*, const shmoo_origin_t**);

__CDECL_END

#endif /* ! __shmoo_input_h */
