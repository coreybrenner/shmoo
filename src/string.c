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
#include <stdint.h>         /* uint8_t */
#include <stdlib.h>         /* calloc, free */
#include <string.h>         /* memcpy */

#include <shmoo/string.h>   /* shmoo_string_t */

int
shmoo_string_data (
    const shmoo_string_t*   str,
    size_t                  offset,
    const uint8_t**         datap,
    size_t*                 leftp
    )
{
    if (! str || ! datap || ! leftp) {
        return 0;
    } else if (offset >= str->size) {
        return 0;
    }
    *datap = (str->data + offset);
    *leftp = (str->size - offset);
    return 1;
}

int
shmoo_string_size (
    const shmoo_string_t*   str,
    size_t*                 sizep
    )
{
    if (! str || ! sizep) {
        return 0;
    } else {
        *sizep = str->size;
        return 1;
    }
}

int
shmoo_string_make (
    shmoo_string_t**        strp,
    const uint8_t*          data,
    size_t                  size
    )
{
    shmoo_string_t* str;

    if (! strp || ! data) {
        return 0;
    } else if (! (str = calloc(1, sizeof(*str)))) {
        return 0;
    } else {
        str->data = data;
        str->size = size;
        *strp     = str;
        return 1;
    }
}

int
shmoo_string_dest (
    shmoo_string_t**        strp
    )
{
    if (! strp) {
        return 0;
    } else if (! *strp) {
        return 1;
    } else {
        free(*strp);
        *strp = 0;
        return 1;
    }
}

int
shmoo_string_init (
    shmoo_string_t*         str,
    const uint8_t*          data,
    size_t                  size
    )
{
    if (! str || ! data) {
        return 0;
    } else {
        str->data = data;
        str->size = size;
        return 1;
    }
}

size_t
shmoo_string_copy (
    const shmoo_string_t*   str,
    size_t                  offset,
    size_t                  length,
    uint8_t*                dest
    )
{
    if (! str || ! dest) {
        return 0;
    } else if (offset >= str->size) {
        return 0;
    } else {
        size_t copy = (
            ((offset + length) > str->size)
                ? (str->size - offset)
                : length
        );
        (void) memcpy(dest, (str->data + offset), copy);
        return 1;
    }
}


