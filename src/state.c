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
#include <shmoo/state.h>

const shmoo_vector_type_t shmoo_state_vector_type = {
    .name = "state_vector",
    .size = sizeof(shmoo_state_t*),
};

int
shmoo_state_make (
    shmoo_state_t**             statep,
    const shmoo_state_type_t*   type,
    shmoo_input_t*              in
    )
{
    shmoo_state_t* state = 0;

    if (! statep || ! type || ! in) {
        return 0;
    } else if (! (state = calloc(1, sizeof(*state)))) {
        return 0;
    } else if (! shmoo_vector_init(&state->lines_saved, &shmoo_token_vector_type)
            || ! shmoo_vector_init(&state->token_stack, &shmoo_token_vector_type)
            || ! shmoo_vector_init(&state->token_saved, &shmoo_token_vector_type)
            || ! shmoo_vector_init(&state->input_saved, &shmoo_input_vector_type)
              )
    {
        return 0;
    }
    state->type = type;
    state->inpu = in;
    return 1;
}

int
shmoo_state_dest (
    shmoo_state_t**             statep
    )
{
    shmoo_state_t* state;
    shmoo_token_t* toke;

    if (! statep) {
        return 0;
    } else if (! (state = *statep)) {
        return 1;
    } else if (! shmoo_input_dest(&state->inpu)) {
        return 0;
    } else if (! shmoo_line_dest(&state->line)) {
        return 0;
    }

    while (shmoo_vector_remove_tail(&state->lines_saved, &line)) {
        shmoo_line_dest(&line);
    }
    shmoo_vector_free(&state->lines_saved);

    while (shmoo_vector_remove_tail(&state->token_stack, &toke)) {
        shmoo_token_dest(&toke);
    }
    shmoo_vector_free(&state->token_stack);

    while (shmoo_vector_remove_tail(&state->token_saved, &toke)) {
        shmoo_token_dest(&toke);
    }
    shmoo_vector_free(&state->token_saved);

    while (shmoo_vector_remove_tail(&state->input_saved, &inpu)) {
        shmoo_input_dest(&inpu);
    }
    shmoo_vector_free(&state->input_saved);

    free(state);
    *statep = 0;
    return 1;
}

struct _shmoo_state {
    const shmoo_state_type_t*   type;
    shmoo_input_t*              inpu;
    shmoo_line_t*               line;
    uint64_t                    used;
    shmoo_vector_t              lines_saved;
    shmoo_vector_t              token_stack;
    shmoo_vector_t              token_saved;
};


#if 0
int
shmoo_state_input (
    const shmoo_state_t*        state,
    shmoo_input_t**             inp
    )
{
    if (! state || ! inp) {
        return 0;
    } else {
        *inp = state->inpu;
        return 1;
    }
}

int
shmoo_state_line (
    const shmoo_state_t*        state,
    const shmoo_line_t**        linep
    )
{
    if (! state || ! linep) {
        return 0;
    } else {
        *linep = state->line;
        return 1;
    }
}

int
shmoo_state_used (
    const shmoo_state_t*        state,
    uint64_t*                   usedp
    )
{
    if (! state || ! usedp) {
        return 0;
    } else {
        *usedp = state->used;
        return 1;
    }
}
#endif

int
_shmoo_state_data (
    shmoo_state_t*              state,
    shmoo_cursor_t*             curp,
    ...
    )
{
    uint64_t want;
    va_list  args;
    if (! state || ! curp) {
        return 0;
    }
    va_start(args, curp);
    want = va_arg(args, uint64_t);
    va_end(args);

    return shmoo_input_data(state->inpu, state->used, curp, want);
}

int
shmoo_state_back (
    shmoo_state_t*              state,
    size_t                      bytes
    )
{
}

int
_shmoo_state_peek_byte (
    shmoo_state_t*              state,
    uint8_t*                    bytep,
    ...
    )
{
    shmoo_cursor_t* curp;
    va_list         args;

    if (! state || ! bytep) {
        return 0;
    }
    va_start(args, bytep);
    curp = va_arg(args, shmoo_cursor_t*);
    va_end(args);

    return shmoo_input_byte(state->inpu, state->used, bytep, curp);
}

int
_shmoo_state_read_byte (
    shmoo_state_t*              state,
    uint8_t*                    bytep,
    ...
    )
{
    shmoo_cursor_t* curp;
    va_list         args;

    if (! state || ! bytep) {
        return 0;
    }
    va_start(args, bytep);
    curp = va_arg(args, shmoo_cursor_t*);
    va_end(args);

    if (! shmoo_input_byte(state->inpu, state->used, bytep, curp)) {
        return 0;
    } else {
        /* Successfully scanned a byte in the input, and maybe populated
         * a cursor describing the rest of the input buffer.  Bump up the
         * read position in the input state.  The cursor returned will
         * start with the byte just read, not one past.
         */
        ++state->used;
        return 1;
    }
}

int
shmoo_state_peek_char (
    shmoo_state_t*              state,
    const shmoo_scan_f          scanfunc,
    void*                       scanstate, /* must be zeroed, etc., before calling. */
    shmoo_char_t*               charp,
    size_t*                     sizep
    )
{
    int            rslt = 0;
    shmoo_char_t   cval = 0;
    size_t         csiz = 0;
    uint32_t       scno = 0;
    shmoo_cursor_t curs = {
        .data = 0,
        .size = 0,
        .from = 0,
    };

    if (! state || ! scanfunc || ! charp || ! sizep) {
        return 0;
    }

    for (csiz = scno = 0; ; ++scno) {
        /* csiz will carry the number of bytes scanned so far, to make
         * up the character being peeked.  We want to track this so the
         * input layer can provide us a cursor to the right buffer.
         */
        if (! shmoo_input_data(state->inpu, (state->used + csiz), &curs)) {
            return 0;
        }
        rslt = (*scanfunc)(scno, scanstate, &curs, &cval, &csiz);
        if (! rslt) {
            /* Could not scan */
            return 0;
        } else if (rslt > 0) {
            /* Scanned a whole (possibly multi-byte) character. */
            break;
        }
        /* Otherwise, scanfunc will have stored in its matching
         * scanstate, the information needed to continue the scan
         * for the character starting at the state's current
         * input byte offset.  Loop until the whole character is
         * read, or we find an intractable error.
         */
    }

    *charp = cval;
    *sizep = csiz;
    return 1;
}

int
shmoo_state_read_char (
    shmoo_state_t*              state,
    const shmoo_scan_f          scanfunc,
    void*                       scanstate,
    shmoo_char_t*               charp,
    size_t*                     sizep
    )
{
    if (! shmoo_state_scan_char(state, scanfunc, scanstate, charp, sizep)) {
        return 0;
    } else {
        state->used += *sizep;
        return 1;
    }
}

int
shmoo_state_make_token (
    shmoo_state_t*              state,
    shmoo_token_t**             tokep,
    const shmoo_token_type_t*   type
    )
{
    toke->orig = state->orig;
}

int
shmoo_state_curr_token (
    shmoo_state_t*              state,
    shmoo_token_t**             tokep
    )
{
}

int
shmoo_state_push_token (
    shmoo_state_t*              state,
    shmoo_token_t*              toke
    )
{
}

int
shmoo_state_back_token (
    shmoo_state_t*              state,
    const shmoo_token_t*        toke
    )
{
}

int
shmoo_state_save_token (
    shmoo_state_t*              state,
    const shmoo_token_t*        toke
    )
{
}

int
shmoo_state_save_state (
    shmoo_state_t*              state,
    shmoo_state_t*              saved
    )
{
}

int
shmoo_state_conditional (
    shmoo_state_t*              state,
    int                         action
    )
{
}

int
shmoo_state_skipping (
    shmoo_state_t*              state,
    int                         action
    )
{
}

