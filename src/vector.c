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
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include <shmoo/vector.h>

int
shmoo_vector_make (shmoo_vector_t** vecp, const shmoo_vector_type_t* type) {
    shmoo_vector_t* vec= NULL;
    int             err  = 0;

    if (! vecp || ! type) {
        return EINVAL;
    } else if (! (vec = calloc(1, sizeof(*vec)))) {
        return errno;
    } else if ((err = shmoo_vector_init(vec, type))) {
        return err;
    } else {
        *vecp = vec;
    }

    return 0;
}

int
shmoo_vector_init (shmoo_vector_t* vec, const shmoo_vector_type_t* type) {
    if (! vec || ! type) {
        return 0;
    } else {
        memset(vec, 0, sizeof(*vec));
        vec->type = type;
        return 1;
    }
}

int
shmoo_vector_free (shmoo_vector_t* vec) {
    if (! vec) {
        return EINVAL;
    }
    free(vec->data);
    vec->size = 0;
    vec->used = 0;
    vec->data = 0;
    return 1;
}

int
shmoo_vector_dest (shmoo_vector_t** vectp) {
    int err = 0;

    if (! vectp) {
        return EINVAL;
    } else if (! *vectp) {
        return 0;
    } else if ((err = shmoo_vector_free(*vectp))) {
        return err;
    } else {
        free(*vectp);
        *vectp = NULL;
    }

    return 0;
}

int
shmoo_vector_peek (const shmoo_vector_t* vec, size_t indx, void* datap) {
    uint8_t* data = NULL;
    if (! vec|| ! datap || (indx >= vec->used)) {
        return EINVAL;
    }
    data = vec->data;
    data += (indx * (vec->type->elem_size));
    (void) memcpy(datap, data, vec->type->elem_size);
    return 0;
}

int
shmoo_vector_insert_tail (shmoo_vector_t* vec, const void* datap) {
    uint8_t* data = NULL;
    size_t   size = 0;
    uint8_t* dest = NULL;
    size_t   copy = 0;

    if (! vec|| ! datap) {
        return EINVAL;
    }
    data = vec->data;
    size = vec->size;
    if (vec->used == vec->size) {
        size = (size ? (size * 2) : 1);
        data = realloc(data, (size * vec->type->elem_size));

        if (! data) {
            return errno;
        } else {
            dest = (data + (vec->size * vec->type->elem_size));
            copy = ((size - vec->size) * vec->type->elem_size);
            (void) memset(dest, 0, copy);
        }
        vec->data = data;
        vec->size = size;
    }
    copy = vec->type->elem_size;
    dest = (data + (vec->used * copy));
    (void) memcpy(dest, datap, copy);
    ++vec->used;

    return 0;
}

int
shmoo_vector_remove_tail (shmoo_vector_t* vec, void* datap) {
    uint8_t* data = NULL;
    uint8_t* from = NULL;
    size_t   used = 0;

    if (! vec|| ! vec->used || ! datap) {
        return EINVAL;
    }

    data = vec->data;
    used = vec->used;
    from = (data + (used * vec->type->elem_size));
    (void) memcpy(datap, from, vec->type->elem_size);
    (void) memset(data, 0, vec->type->elem_size);
    --vec->used;

    return 0;
}

int
shmoo_vector_insert_head (shmoo_vector_t* vec, const void* datap) {
    uint8_t* data = NULL;
    uint8_t* dest = NULL;
    size_t   used = 0;

    if (! vec|| ! datap) {
        return EINVAL;
    }

    data = vec->data;
    dest = (data + vec->type->elem_size);
    used = (vec->used * vec->type->elem_size);
    (void) memmove(dest, data, used);
    (void) memcpy(data, datap, vec->type->elem_size);
    ++vec->used;

    return 0;
}

int
shmoo_vector_remove_head (shmoo_vector_t* vec, void* datap) {
    uint8_t* tail = NULL;
    size_t   copy = 0;

    if (! vec|| ! datap) {
        return EINVAL;
    }

    tail = vec->data;
    tail += vec->type->elem_size;
    copy = ((vec->used - 1) * vec->type->elem_size);
    (void) memcpy(datap, vec->data, vec->type->elem_size);
    (void) memcpy(vec->data, tail, copy);
    --vec->used;

    return 0;
}


