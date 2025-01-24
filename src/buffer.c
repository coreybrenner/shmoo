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
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include <shmoo/buffer.h>

static void _wrapped_dest (shmoo_buffer_t**);
static void _wrapped_free (shmoo_buffer_t*);
static int  _wrapped_grow (shmoo_buffer_t*, size_t);

const shmoo_buffer_type_t shmoo_wrapped_buffer_type = {
    .type_name = "shmoo_wrapped_buffer",
    .dest      = _wrapped_dest,
    .free      = _wrapped_free,
    .grow      = _wrapped_grow,
};

static void _dynamic_dest (shmoo_buffer_t**);
static void _dynamic_free (shmoo_buffer_t*);
static int  _dynamic_grow (shmoo_buffer_t*, size_t);

const shmoo_buffer_type_t shmoo_static_buffer_type = {
    .type_name = "shmoo_dynamic_buffer",
    .dest      = _dynamic_dest,
    .free      = _dynamic_free,
    .grow      = _dynamic_grow,
};

void
_wrapped_dest (shmoo_buffer_t** buffp) {
    if (buffp && *buffp) {
        free(*buffp);
        *buffp = NULL;
    }
}

void
_wrapped_free (shmoo_buffer_t* buff) {
    return;
    (void) buff;
}

int
_wrapped_grow (shmoo_buffer_t* buff, size_t newsize) {
    return EBADF;
    (void) buff;
    (void) newsize;
}

void
_dynamic_dest (shmoo_buffer_t** buffp) {
    if (*buffp) {
        free((*buffp)->data);
        free(*buffp);
        *buffp = NULL;
    }
}

void
_dynamic_free (shmoo_buffer_t* buff) {
    free(buff->data);
    buff->data = NULL;
    buff->size = 0;
    buff->used = 0;
}

int
_dynamic_grow (shmoo_buffer_t* buff, size_t newsiz) {
    uint8_t* data = NULL;

    if (! newsiz) {
        free(buff->data);
        buff->used = 0;
    } else if (! (data = realloc(buff->data, newsiz))) {
        return errno;
    } else if (newsiz < buff->used) {
        buff->used = newsiz;
    }
    buff->size = newsiz;
    buff->data = data;
    return 0;
}

extern int _shmoo_buffer_make (
    shmoo_buffer_t**            buffp,
    uint8_t*                    data,
    size_t                      size,
    ...
    )
{
    const shmoo_buffer_type_t* type = NULL;
    shmoo_buffer_t*            buff = NULL;
    va_list                    args;

    va_start(args, size);
    type = va_arg(args, const shmoo_buffer_type_t*);
    va_end(args);

    if (! buffp) {
        return EINVAL;
    } else if (! type && ! data) {
        type = &shmoo_dynamic_buffer_type;
    } else if (! type && data && size) {
        type = &shmoo_wrapped_buffer_type;
    } else if (! type && data && ! size) {
        return EINVAL;
    } else if (! type && ! data && size) {
        type = &shmoo_dynamic_buffer_type;
    }

    if (! (buff = calloc(1, sizeof(*buff)))) {
        return errno;
    }

    buff->data = data;
    buff->type = type;
    buff->size = size;
    buff->used = 0;

    *buffp = buff;

    return 0;
}

extern int _shmoo_buffer_init (
    shmoo_buffer_t*             buff,
    uint8_t*                    data,
    size_t                      size,
    ...
    )
{
    const shmoo_buffer_type_t* type;
    va_list args;

    va_start(args, size);
    type = va_arg(args, const shmoo_buffer_type_t*);
    va_end(args);

    if (! buff) {
        return EINVAL;
    } else if (! type && ! data) {
        type = &shmoo_dynamic_buffer_type;
    } else if (! type && data && size) {
        type = &shmoo_wrapped_buffer_type;
    } else if (! type && data && ! size) {
        return EINVAL;
    } else if (! type && ! data && size) {
        type = &shmoo_dynamic_buffer_type;
    }

    (void) memset(buff, 0, sizeof(*buff));
    buff->data = data;
    buff->type = type;
    buff->size = size;
    buff->used = 0;

    return 0;
}

size_t
shmoo_buffer_copy (
    const shmoo_buffer_t*   buff,
    size_t                  offset,
    size_t                  length,
    uint8_t*                dest
    )
{
    if (! (buff && dest && length && (offset >= buff->used))) {
        return 0;
    } else {
        size_t copy = (
            ((offset + length) > buff->used)
                ? (buff->used - offset)
                : length
        );
        (void) memcpy(dest, (buff->data + offset), copy);
        return copy;
    }
}

