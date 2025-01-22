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
#ifndef _shmoo_state_h
#define _shmoo_state_h

#include <errno.h>

#include <shmoo/compile.h>

__CDECL_BEG

/*forward*/ struct _shmoo_state_type;
typedef     struct _shmoo_state_type shmoo_state_type_t;
/*forward*/ struct _shmoo_state;
typedef     struct _shmoo_state      shmoo_state_t;

#include <shmoo/input.h>
#include <shmoo/char.h>
#include <shmoo/string.h>
#include <shmoo/buffer.h>
#include <shmoo/token.h>
#include <shmoo/vector.h>

typedef int (*shmoo_state_dest_f) (shmoo_state_t**);

struct _shmoo_state_type {
    const char*                 type_name;
    shmoo_state_dest_f          dest;
};

struct _shmoo_state {
    const shmoo_state_type_t*   type;
    const shmoo_token_t*        orig;
    /* shmoo_state can shmoo_input_more() the input layer,
     * so this cannot be a pointer to const.
     */
    shmoo_input_t*              inpu;
    uint64_t                    used;   /* Read pointer. */
    shmoo_vector_t              token_stack;
    shmoo_vector_t              token_saved;
    shmoo_vector_t              lines_saved;
    /* word expansion buffers, included makefiles, etc. */
    shmoo_vector_t              state_saved;
    shmoo_vector_t              delim_stack;
    shmoo_vector_t              conds_stack;
};

extern const shmoo_vector_type_t shmoo_state_vector_type;

extern int shmoo_state_make (
    shmoo_state_t**             __statep,
    const shmoo_state_type_t*   __type,
    shmoo_input_t*              __in,
    const shmoo_token_t*        __orig
);

extern int shmoo_state_init (
    shmoo_state_t*              __state,
    const shmoo_state_type_t*   __type,
    shmoo_input_t*              __in,
    const shmoo_token_t*        __orig
);

extern int shmoo_state_free (
    shmoo_state_t*              __state
);

inline int shmoo_state_dest (shmoo_state_t** statep) {
    return (! statep ? EINVAL : (! *statep ? 0 : (*statep)->type->dest(statep)));
}

/* Obtain the state's input */
inline int shmoo_state_input (const shmoo_state_t* state, shmoo_input_t** inp) {
    return (! (state && inp) ? EINVAL : (((*inp = state->inpu)), 1));
}

/* Obtain the amount of the input already used up */
inline int shmoo_state_used (const shmoo_state_t* state, uint64_t* usedp) {
    return (! (state && usedp) ? EINVAL : (((*usedp) = state->used), 1));
}

/* Obtain the size of the underlying input layer object. */
inline int shmoo_state_size (const shmoo_state_t* state, uint64_t* sizep) {
    return (! (state && sizep) ? EINVAL : shmoo_input_size(state->inpu, sizep));
}

/* Obtain a cursor to the current input buffer */
inline int shmoo_state_data (
    const shmoo_state_t*    state,
    uint64_t                offset,
    const uint8_t**         datap,
    size_t*                 leftp
    )
{
    return (! (state && datap && leftp)
                ? EINVAL
                : shmoo_input_data(state->inpu, offset, datap, leftp)
           );
}

/* Move input state read pointer backward */
inline int shmoo_state_back (shmoo_state_t* state, size_t bytes) {
    return (((! state) || (bytes > state->used)) ? EINVAL : ((state->used -= bytes), 1));
}

/* Peek at the next byte.  Does not advance the read counter. */
#if 0
inline int shmoo_state_peek_byte (shmoo_state_t* state, uint8_t* bytep) {
    return (! (state && bytep)
                ? EINVAL
                : shmoo_input_read_byte(state->inpu, state->used, bytep)
           );
}

/* Like shmoo_state_peek_byte(), but advance the read counter. */
inline int shmoo_state_read_byte (shmoo_state_t* state, uint8_t* bytep) {
    return (! (state && bytep)
                ? EINVAL
                : shmoo_input_read_byte(state->inpu, state->used++, bytep)
           );
}
#endif

/* Scan the current input to find the shape and
 * length of the next character; note: not byte.
 */
inline int shmoo_state_peek (
    shmoo_state_t*          state,
    shmoo_scan_f            scan_func,
    void*                   scan_state,
    void*                   valp,
    size_t*                 sizep
    )
{
    return (! (state && scan_func && valp && sizep)
                ? EINVAL
                : shmoo_input_scan(
                      state->inpu,
                      state->used,
                      scan_func,
                      scan_state,
                      valp,
                      sizep
                  )
           );
}

/* same as shmoo_state_peek_char(), but advances the input state */
inline int shmoo_state_read (
    shmoo_state_t*              state,
    const shmoo_scan_f          scan_func,
    void*                       scan_state,
    void*                       valp,
    size_t*                     sizep
    )
{
    int result = 0;
    return (! (state && scan_func && valp && sizep)
                ? EINVAL
                : ((result = shmoo_input_scan(
                      state->inpu,
                      state->used,
                      scan_func,
                      scan_state,
                      valp,
                      sizep
                      )) ? result
                         : ((state->used += *sizep), result)
                  )
           );
}

extern int shmoo_state_make_token (
    const shmoo_state_t*        __state,
    shmoo_token_t**             __tokep,
    const shmoo_token_type_t*   __toketype
);

extern int shmoo_state_init_token (
    const shmoo_state_t*        __state,
    shmoo_token_t*              __toke,
    const shmoo_token_type_t*   __toketype
);

extern int shmoo_state_curr_token (
    shmoo_state_t*              __state,
    shmoo_token_t**             __tokep
);

extern int shmoo_state_push_token (
    shmoo_state_t*              __state,
    shmoo_token_t*              __toke
);

extern int shmoo_state_back_token (
    shmoo_state_t*              __state,
    const shmoo_token_t*        __toke
);

extern int shmoo_state_save_token (
    shmoo_state_t*              __state,
    shmoo_token_t*              __toke
);

extern int shmoo_state_save_state (
    shmoo_state_t*              __state,
    shmoo_state_t*              __saved
);

extern int shmoo_state_conditional (
    shmoo_state_t*              __state,
    int                         __action
);

extern int shmoo_state_skipping (
    shmoo_state_t*              __state,
    int                         __action
);

__CDECL_END

#endif /* ! _shmoo_state_h */
