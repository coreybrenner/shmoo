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
#include <string.h>         /* strchr */
#include <stddef.h>

#include <shmoo/char.h>

#include <shmoo/char.h>
#include <shmoo/scan.h>
#include <shmoo/cursor.h>

/* shmoo_scan_f API */
int
shmoo_char_scan_ascii (
    void*                   _state,
    const shmoo_cursor_t*   cursor,
    size_t*                 sizep
    )
{
    shmoo_char_scan_ascii_state_t* state = (shmoo_char_scan_ascii_state_t*) _state;

    if (! (state && cursor && sizep)) {
        return  0;
    } else if (! cursor->size) {
        state->cval = SHMOO_CHAR_INVALID;
        *sizep      = 0;
        return -1;
    } else {
        state->cval = (shmoo_char_t) *cursor->data;
        *sizep      = sizeof(*cursor->data);
        return  1;
    }

    { shmoo_scan_f __apicheck__ = shmoo_char_scan_ascii; (void) __apicheck__; }
}

/* shmoo_scan_f API */
int
shmoo_char_scan_utf8 (
    void*                   _state,
    const shmoo_cursor_t*   cursor,
    size_t*                 usedp
    )
{
    shmoo_char_scan_utf8_state_t* state = (shmoo_char_scan_utf8_state_t*) _state;
    size_t         used  = 0;
    size_t         left  = 0;
    size_t         want  = 0;
    const uint8_t* next  = 0;
    uint8_t        byte  = 0;
    shmoo_char_t   cerr  = 0;
    shmoo_char_t   cval  = 0;
    size_t         csiz  = 0;
    size_t         nshl  = 0;
    uint32_t       errs  = 0;
    int            rslt  = 0;

    if (! (state && cursor && usedp)) {
        /* Error. */
        return 0;
    } else if (! cursor->size) {
        /* No bytes left in buffer; give back number
         * of bytes already read for this character,
         * Return -1 to signify this buffer ended before
         * a whole character could be scanned.
         */
        return -(*usedp = state->used);
    }

    left = cursor->size;
    next = cursor->data;
    switch (state->used) {
        case 3:
            byte  = state->data.byte[++used];
            cval  = ((cval << 6) | (byte & 0x3F));
            cerr  = ((cerr << 6) | (byte & 0xC0));
            nshl += 6;
            /*FALLTHRU*/
        case 2:
            byte  = state->data.byte[++used];
            cval  = ((cval << 6) | (byte & 0x3F));
            cerr  = ((cerr << 6) | (byte & 0xC0));
            nshl += 6;
            /*FALLTHRU*/
        case 1:
            byte  = state->data.byte[0];
            used += 1;
            break;
        case 0: 
            byte = state->data.byte[0] = *cursor->data;
            break;
    }

    switch (byte & 0xF8) {
        case 0xF0:  /* 1111 0 */    /* ---- -xxx */
            cval |= (((shmoo_char_t) (byte & 0x07)) << nshl);
            csiz  = 4;
            break;
        case 0xE0:  /* 1110 x */    /* ---- xxxx */
        case 0xE8:  /* 1110 x */    /* ---- xxxx */
            cval |= (((shmoo_char_t) (byte & 0x0F)) << nshl);
            csiz  = 3;
            break;
        case 0xC0:  /* 110x x */    /* ---x xxxx */
        case 0xC8:  /* 110x x */    /* ---x xxxx */
        case 0xD0:  /* 110x x */    /* ---x xxxx */
        case 0xD8:  /* 110x x */    /* ---x xxxx */
            cval |= (((shmoo_char_t) (byte & 0x1F)) << nshl);
            csiz  = 2;
            break;
        default:    /* 0xxx x */    /* -xxx xxxx */
            cval  = byte;
            csiz  = 1;
            break;

        /* Invalid starting sequences. */
        case 0x80:  /* 10xx x */    /* --xx xxxx */
        case 0x88:  /* 10xx x */    /* --xx xxxx */
        case 0x90:  /* 10xx x */    /* --xx xxxx */
        case 0x98:  /* 10xx x */    /* --xx xxxx */
        case 0xA0:  /* 10xx x */    /* --xx xxxx */
        case 0xA8:  /* 10xx x */    /* --xx xxxx */
        case 0xB0:  /* 10xx x */    /* --xx xxxx */
        case 0xB8:  /* 10xx x */    /* --xx xxxx */ {
            /* Scan ahead until we encounter a valid starting character.
             * Return the number of invalid bytes in the sequence.
             */
            while (left) {
                if ((*next & 0xC0) != 0x80) {
                    break;
                }
                left -= 1;
                used += 1;
                next += 1;
            }
            state->cval = SHMOO_CHAR_INVALID;
            state->used = used;
            *usedp      = used;
            return 0;
        }
    }

    want = (csiz - used);
    switch ((want < left) ? want : left) {
        case 3:
            byte = *next++;
            cval = ((cval << 6) | (byte & 0x3F));
            cerr = ((cerr << 6) | (byte & 0xC0));
            state->data.byte[used++] = byte;
            /*FALLTHRU*/
        case 2:
            byte = *next++;
            cval = ((cval << 6) | (byte & 0x3F));
            cerr = ((cerr << 6) | (byte & 0xC0));
            state->data.byte[used++] = byte;
            /*FALLTHRU*/
        case 1:
            byte = *next++;
            cval = ((cval << 6) | (byte & 0x3F));
            cerr = ((cerr << 6) | (byte & 0xC0));
            state->data.byte[used++] = byte;
            /*FALLTHRU*/
        case 0:
            switch (csiz) {
                case 4: errs += ((cerr & 0x00C00000) != 0x00800000);
                    /*FALLTHRU*/
                case 3: errs += ((cerr & 0x0000C000) != 0x00008000);
                    /*FALLTHRU*/
                case 2: errs += ((cerr & 0x000000C0) != 0x00000080);
                    /*FALLTHRU*/
                case 1: break;
            }
            if (errs) {
                cval = SHMOO_CHAR_INVALID;
                rslt = 0;
            } else {
                rslt = used;
            }
    }

    state->cval = cval;
    state->used = used;
    *usedp      = used;
    return rslt;

    { shmoo_scan_f __apicheck__ = shmoo_char_scan_ascii; (void) __apicheck__; }
}

