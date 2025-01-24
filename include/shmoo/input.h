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

#include <shmoo/vector.h>

/* Make visible as soon as possible, for other headers' use. */
extern const shmoo_vector_type_t shmoo_input_vector_type;

#include <shmoo/scan.h>
#include <shmoo/cursor.h>

typedef int (*shmoo_input_more_f) (shmoo_input_t*);
typedef int (*shmoo_input_shut_f) (shmoo_input_t*);
typedef int (*shmoo_input_dest_f) (shmoo_input_t**);

/* These need to be visible ahead of the inline functions. */
struct _shmoo_input_type {
    const char*                 type_name;
    shmoo_input_more_f          more;
    shmoo_input_shut_f          shut;
    shmoo_input_dest_f          dest;
};

struct _shmoo_input {
    const shmoo_input_type_t*   type;
    const uint8_t*              name;
    uint64_t                    size;
    int64_t                     mtim;
    size_t                      refs;
    shmoo_vector_t              part;
};

/* These functions are needed inline by other headers. */

static inline int shmoo_input_dest (shmoo_input_t** inp) {
    return (! inp ? 0 : (! *inp ? 1 : (*inp)->type->dest(inp)));
}

static inline int shmoo_input_more (shmoo_input_t* in) {
    return (! in ? 0 : in->type->more(in));
}

static inline int shmoo_input_shut (shmoo_input_t* in) {
    return (! (in && in->type) ? 0 : in->type->more(in));
}

static inline int shmoo_input_size (const shmoo_input_t* in, uint64_t* sizep) {
    return (! (in && sizep) ? 0 : ((*sizep = in->size), 1));
}

static inline int shmoo_input_type (const shmoo_input_t* in, const char** typep) {
    return (! (in && typep) ? 0 : ((*typep = in->type->type_name), 1));
}

static inline int shmoo_input_refs (const shmoo_input_t* in, size_t* refsp) {
    return (! (in && refsp) ? 0 : ((*refsp = in->refs), 1));
}

extern int shmoo_input_data (
    shmoo_input_t*                  __in,
    uint64_t                        __offset,
    const uint8_t**                 __datap,
    size_t*                         __leftp
);

extern int shmoo_input_scan (
    /***
     * Inspect a sequence of bytes, given a function and some
     * bookkeeping info, to obtain the next desired sequence.
     * Consider a function which scans a UTF8 stream for character
     * code points, or which matches an entire delimited token
     * from the buffered stream.
     *
     * ARGUMENTS:
     *   in:
     *      The input layer object in which to scan, to match the
     *      next sequence of bytes.
     *   offset:
     *      The offset at which to begin scanning.
     *   scan_func:
     *      The function used to scan the input buffers for the
     *      expected sequence.
     *   scan_state:
     *      Pointer to A buffer which can hold the current state
     *      of the scan in progress.  This buffer is managed by
     *      scan_func.
     *   scan_result:
     *      Pointer to a shmoo_cursor which will, on success,
     *      contain the offset of the beginning of the match,
     *      the length of the match and a pointer to the start
     *      of the relevant data.
     *
     * RETURNS:
     *  >0: Failed to match the expected sequence.  Look for
     *      information in the scan state buffer and in the
     *      data at scan_result->size.
     *  =0: Successfully matched the expected sequence,  This
     *      is the return value from the last call to scan_func.
     */
    shmoo_input_t*                  __in,
    uint64_t                        __offset,
    shmoo_scan_f                    __scan_func,
    void*                           __scan_state,
    shmoo_cursor_t*                 __scan_result
);

extern int shmoo_input_copy (
    shmoo_input_t*                  __in,
    uint64_t                        __offset,
    size_t                          __length,
    uint8_t*                        __dest,
    uint64_t*                       __usedp
);

/* Needed for shmoo_input_open_... */
#include <shmoo/handle.h>
#include <shmoo/string.h>
#include <shmoo/buffer.h>
#include <shmoo/token.h>

extern int shmoo_input_open_file (
    shmoo_input_t**                 __inp,
    FILE*                           __file
);

extern int shmoo_input_open_path (
    shmoo_input_t**                 __inp,
    const char*                     __path
);

extern int shmoo_input_open_desc (
    shmoo_input_t**                 __inp,
    shmoo_handle_t                  __desc
);

extern int shmoo_input_open_text (
    shmoo_input_t**                 __inp,
    shmoo_string_t                  __text
);

extern int shmoo_input_open_buff (
    shmoo_input_t**                 __inp,
    const shmoo_buffer_t*           __buff
);

extern int shmoo_input_open_toke (
    shmoo_input_t**                 __inp,
    const shmoo_token_t*            __toke
);

__CDECL_END

#endif /* ! __shmoo_input_h */
