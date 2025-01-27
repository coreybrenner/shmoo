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
#ifndef _shmoo_token_h
#define _shmoo_token_h

#include <stdint.h>

#include <shmoo/compile.h>

__CDECL_BEG

/*forward*/ struct _shmoo_token_type;
typedef     struct _shmoo_token_type shmoo_token_type_t;
/*forward*/ struct _shmoo_token;
typedef     struct _shmoo_token      shmoo_token_t;

#include <shmoo/vector.h>

extern const shmoo_vector_type_t shmoo_token_vector_type;

#include <shmoo/state.h>
#include <shmoo/context.h>

typedef int (*shmoo_token_dest_f) (shmoo_token_t**);
typedef int (*shmoo_token_eval_f) (const shmoo_token_t*, shmoo_context_t*);

struct _shmoo_token_type {
    const char*                 type_name;
    shmoo_token_dest_f          dest;
    shmoo_token_eval_f          eval;
};

struct _shmoo_token {
    const shmoo_token_type_t*   type;
    shmoo_state_t*              inst;       /* Input state from which this token originates */
    uint64_t                    from;       /* byte offset of this token within the input */
    size_t                      size;       /* byte count occupied by this token */
    size_t                      span;       /* lines spanned by this token */
    size_t                      last_size;  /* number of bytes lately added to this token */
    size_t                      last_span;  /* number of lines lately added to this token */
    shmoo_vector_t              part;
};

static inline int shmoo_token_dest (shmoo_token_t** tokep) {
    return (! tokep ? EINVAL : (! *tokep ? 0 : (*tokep)->type->dest(tokep)));
}

static inline int shmoo_token_type (const shmoo_token_t* toke, const shmoo_token_type_t** typep) {
    return (! (toke && typep) ? EINVAL : ((*typep = toke->type), 0));
}

static inline int shmoo_token_inst (const shmoo_token_t* toke, const shmoo_state_t** statep) {
    return (! (toke && statep) ? EINVAL : ((*statep = toke->inst), 0));
}

static inline int shmoo_token_from (const shmoo_token_t* toke, uint64_t* fromp) {
    return (! (toke && fromp) ? EINVAL : ((*fromp = toke->from), 0));
}

static inline int shmoo_token_size (const shmoo_token_t* toke, size_t* sizep) {
    return (! (toke && sizep) ? EINVAL : ((*sizep = toke->size), 0));
}

static inline int shmoo_token_span (const shmoo_token_t* toke, size_t* spanp) {
    return (! (toke && spanp) ? EINVAL : ((*spanp = toke->span), 0));
}

static inline int shmoo_token_more (shmoo_token_t* toke, size_t size, size_t span) {
    return (! toke ? EINVAL : (
            toke->last_size += size,
            toke->last_span += span,
            toke->size      += size,
            toke->span      += span,
            0
           ));
}

extern int shmoo_token_init (
    shmoo_token_t*              __toke,
    const shmoo_token_type_t*   __type,
    shmoo_state_t*              __inst
);

extern int shmoo_token_free (
    shmoo_token_t*              __toke
);

extern int shmoo_token_make (
    shmoo_token_t**             __tokep,
    const shmoo_token_type_t*   __type,
    const shmoo_state_t*        __inst
);

extern int shmoo_token_part (
    shmoo_token_t*              __toke,
    shmoo_token_t*              __part
);

extern int shmoo_token_text (
    const shmoo_token_t*        __toke,
    uint8_t*                    __buff, /* 0: count only */
    size_t*                     __sizep,
    size_t*                     __spanp
);

extern int shmoo_token_show     (const shmoo_token_t*, shmoo_buffer_t*);
extern int shmoo_token_eval     (const shmoo_token_t*, shmoo_buffer_t*, shmoo_context_t*);

__CDECL_END

#endif /* ! _shmoo_token_h */
