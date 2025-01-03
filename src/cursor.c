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
#include <stdlib.h>         /* calloc, free */
#include <string.h>         /* memcpy */

#include <shmoo/cursor.h>

int
shmoo_cursor_make (
    shmoo_cursor_t**        curp,
    const uint8_t*          data,
    size_t                  size,
    uint64_t                offset
    )
{
    shmoo_cursor_t* cur = 0;

    if (! curp || ! data) {
        return 0;
    } else if (! (cur = calloc(1, sizeof(*cur)))) {
        return 0;
    } else {
        cur->data = data;
        cur->size = size;
        cur->from = offset;
        *curp = cur;
        return 1;
    }
}

int
shmoo_cursor_dest (
    shmoo_cursor_t**        curp
    )
{
    if (! curp) {
        return 0;
    } else if (! *curp) {
        return 1;
    } else {
        free(*curp);
        *curp = 0;
        return 1;
    }
}

int
shmoo_cursor_init (
    shmoo_cursor_t*         cur,
    const uint8_t*          data,
    size_t                  size,
    uint64_t                offset
    )
{
    if (! cur || ! data) {
        return 0;
    } else {
        cur->data = data;
        cur->size = size;
        cur->from = offset;
        return 1;
    }
}

int
shmoo_cursor_from (
    const shmoo_cursor_t*   cur,
    uint64_t*               fromp
    )
{
    if (! cur || ! fromp) {
        return 0;
    } else {
        *fromp = cur->from;
        return 1;
    }
}

int
shmoo_cursor_data (
    const shmoo_cursor_t*   cur,
    size_t                  offset,
    const uint8_t**         datap,
    size_t*                 leftp
    )
{
    if (! cur || ! datap || ! leftp) {
        return 0;
    } else if (offset >= cur->size) {
        return 0;
    } else {
        *datap = (cur->data + offset);
        *leftp = (cur->size - offset);
        return 1;
    }
}

int
shmoo_cursor_size (
    const shmoo_cursor_t*   cur,
    size_t*                 sizep
    )
{
    if (! cur || ! sizep) {
        return 0;
    } else {
        *sizep = cur->size;
        return 0;
    }
}

size_t
shmoo_cursor_copy (
    const shmoo_cursor_t*   cur,
    size_t                  offset,
    size_t                  length,
    uint8_t*                dest
    )
{
    if (! cur || ! dest) {
        return 0;
    } else if (offset >= cur->size) {
        return 0;
    } else {
        size_t copy = (
            ((offset + length) > cur->size)
                ? (cur->size - offset)
                : length
        );
        (void) memcpy(dest, (cur->data + offset), copy);
        return 1;
    }
}

