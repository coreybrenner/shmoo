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
#include <string.h>

#include <shmoo/vector.h>

int
shmoo_vector_init (shmoo_vector_t* vect, const shmoo_vector_type_t* type) {
    if (! vect || ! type) {
        return 0;
    } else {
        memset(vect, 0, sizeof(*vect));
        vect->type = type;
        return 1;
    }
}

int
shmoo_vector_free (shmoo_vector_t* vect) {
    if (! vect) {
        return 0;
    }
    free(vect->data);
    vect->size = 0;
    vect->used = 0;
    vect->data = 0;
    return 1;
}

int
shmoo_vector_used (const shmoo_vector_t* vect, uint32_t* usedp) {
    if (! vect || ! usedp) {
        return 0;
    } else {
        *usedp = vect->used;
        return 1;
    }
}

int
shmoo_vector_size (const shmoo_vector_t* vect, uint32_t* sizep) {
    if (! vect || ! sizep) {
        return 0;
    } else {
        *sizep = vect->size;
        return 1;
    }
}

int
shmoo_vector_peek (const shmoo_vector_t* vect, uint32_t indx, void* datap) {
    uint8_t* data = 0;
    if (! vect || ! datap || (indx >= vect->used)) {
        return 0;
    }
    memcpy(datap, (data + (indx * vect->type->elem)), vect->type->elem);
    return 1;
}

void*
shmoo_vector_tail (const shmoo_vector_t* vect) {
    if (! vect || ! vect->used) {
        return 0;
    } else {
        uint8_t* data = vect->data;
        return (data + ((vect->used - 1) * vect->type->elem));
    }
}

int
shmoo_vector_insert_tail (shmoo_vector_t* vect, const void* datap) {
    uint8_t* data = 0;
    uint32_t size = 0;
    uint32_t used = 0;

    if (! vect || ! datap) {
        return 0;
    }
    data = vect->data;
    size = vect->size;
    if (vect->used == vect->size) {
        size = (size ? (size * 2) : 4);
        data = realloc(data, (size * vect->type->elem));

        if (! data) {
            return 0;
        }
        memset((data + (vect->size * vect->type->elem)), 0,
               ((size - vect->size) * vect->type->elem)
              );
        vect->data = data;
        vect->size = size;
    }
    memcpy((data + (used * vect->type->elem)), datap, vect->type->elem);
    ++vect->used;
    return 1;
}

int
shmoo_vector_remove_tail (shmoo_vector_t* vect, void* datap) {
    uint8_t*  data = 0;
    uint32_t  used = 0;
    if (! vect || ! vect->used || ! datap) {
        return 0;
    }
    data = vect->data;
    used = vect->used;
    memcpy(datap, (data + (used * vect->type->elem)), vect->type->elem);
    memset((data + (used * vect->type->elem)), 0, vect->type->elem);
    --vect->used;
    return 1;
}

