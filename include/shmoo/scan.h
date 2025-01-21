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
#ifndef _shmoo_scan_h
#define _shmoo_scan_h

#include <stdint.h>

#include <shmoo/compile.h>

__CDECL_BEG

#include <shmoo/cursor.h>

typedef int (*shmoo_scan_f) (
    /***
     * Scan customizer.  Use this to pluck, e.g., Unicode data points,
     * fixed-length binary blobs, nested-delimited "words", and so on.
     *
     * ARGUMENTS:
     *   scan_state:
     *      Given by caller, a state buffer that keeps track of the
     *      state of a scan, so that the function can be called again
     *      (and again..., see: scan_iteration) at the top of a new
     *      buffer, to continue an in-progress scan for a multi-byte
     *      token.  This can also carry information like code page
     *      number or mapping, so it is possible to turn weird files
     *      into something legible.
     *   cursor:
     *      Pointer to data, giving the byte offset (from the top of
     *      the shmoo_input), a pointer into the relevant data buffer,
     *      and a count of bytes left in the buffer.
     *   sizep:
     *      Pointer to a size_t, which will be set to the number of
     *      bytes consumed by reading the sequence.  The same pointer
     *      will be handed to each call of the function, so it is
     *      possible to use this pointer to keep track of the number
     *      of bytes consumed at each successive call of the callback.
     *
     * RETURNS:
     *  <0: Encountered the end of a read buffer before an entire code
     *      point was scanned.  This will cause the scanner driver to
     *      provide the next buffer in sequence, if any, so the scan
     *      may be continued.  The state of the scan (for instance, a
     *      small array of bytes) is managed by this function.
     *      Suggested use: Return the negative of the number of bytes
     *      so far consumed
     *  =0: Could not complete the scan for a complete sequence.
     *      The scan state information may be read from the state
     *      buffer and the number of bytes consumed by inspecting
     *      the data under *sizep.  Suggested use: *sizep should give
     *      the number of bytes required to skip over the invalid
     *      sequence.
     *  >0: Success.  Suggested use: Return the number of bytes
     *      required to copy the sequence (i.e., the number of
     *      bytes that "reading" -- moving the input state read
     *      offset counter -- would consume).
     */
    void*                   __scan_state,
    const shmoo_cursor_t*   __cursor,
    size_t*                 __sizep
);

__CDECL_END

#endif /* ! _shmoo_scan_h */
