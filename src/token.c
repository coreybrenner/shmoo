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
#include <stdlib.h>

#include <shmoo/token.h>

const shmoo_vector_type_t shmoo_token_vector_type = {
    .name = "token_vector",
    .size = sizeof(shmoo_token_t*),
};

int
shmoo_token_init (
    shmoo_token_t*            toke,
    const shmoo_token_type_t* type,
    shmoo_state_t*            inst
    )
{
    shmoo_token_t*  toke = 0;
    uint64_t        from = 0;

    if (! toke || ! type) {
        return 0;
    }
    (void) memset(toke, 0, sizeof(*toke));

    if (! shmoo_vector_init(&toke->part, &shmoo_token_vector_type)) {
        return 0;
    } else if (inst && ! shmoo_state_used(inst, &from)) {
        return 0;
    } else {
        toke->type = type;
        toke->inst = inst;
        toke->from = from;
        return 1;
    }
}

int
shmoo_token_free (
    shmoo_token_t*  toke
    )
{
    shmoo_token_t* part;

    if (! toke) {
        return 0;
    }
    while (shmoo_vector_remove_tail(&toke->part, &part)) {
        if (! shmoo_token_dest(&part)) {
            return 0;
        }
    }
    if (! shmoo_vector_free(&toke->part)) {
        return 0;
    }
    (void) memset(toke, 0, sizeof(*toke));
    return 1;
}

int
shmoo_token_make (
    shmoo_token_t**           tokep,
    const shmoo_token_type_t* type,
    shmoo_state_t*            inst
    )
{
    shmoo_token_t*  toke = 0;

    if (! tokep || ! type) {
        return 0;
    } else if (! (toke = calloc(1, sizeof(*toke)))) {
        return 0;
    } else if (! shmoo_token_init(toke, type, inst)) {
        free(toke);
        return 0;
    } else {
        *tokep = toke;
        return 1;
    }
}

/*
int
shmoo_token_dest (
    shmoo_token_t**     tokep,
    )
{
    if (! tokep) {
        return 0;
    } else if (! *tokep) {
        return 1;
    } else if (! shmoo_token_free(*tokep)) {
        return 0;
    } else {
        free(*tokep);
        *tokep = 0;
        return 1;
    }
}
*/

int
shmoo_token_part (
    shmoo_token_t*      toke,
    shmoo_token_t*      part
    )
{
    shmoo_token_t* last = 0;

    if (! toke || ! part) {
        return 0;
    } else if (! toke->last_size) {
        return shmoo_vector_insert_tail(&toke->part, part);
    } else if (! shmoo_state_back(toke->inst, toke->last_size, toke->last_span)) {
        return 0;
    } else if (! shmoo_state_token(toke->inst, &last, toke->type)) {
        return 0;
    } else if (! shmoo_vector_insert_tail(&toke->part, last)) {
        shmoo_token_dest(&last);
        return 0;
    }
    /* toke->size and toke->span already hold this last_... info. */
    last->size = toke->last_size;
    last->span = toke->last_span;
    toke->last_size = 0;
    toke->last_span = 0;
    return shmoo_vector_insert_tail(&toke->part, part);
}

