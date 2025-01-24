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
#include <alloca.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/un.h>

#include <shmoo/cursor.h>
#include <shmoo/handle.h>
#include <shmoo/input.h>
#include <shmoo/vector.h>
#include <shmoo/buffer.h>
#include <shmoo/string.h>

/*forward*/ struct _input_file;
typedef     struct _input_file _input_file_t;
/*forward*/ struct _input_mmap;
typedef     struct _input_mmap _input_mmap_t;
/*forward*/ struct _input_pipe;
typedef     struct _input_pipe _input_pipe_t;
/*forward*/ struct _input_sock;
typedef     struct _input_sock _input_sock_t;
/*forward*/ struct _input_unix;
typedef     struct _input_unix _input_unix_t;
/*forward*/ struct _input_text;
typedef     struct _input_text _input_text_t;
/*forward*/ struct _input_buff;
typedef     struct _input_buff _input_buff_t;

struct _input_file {
    shmoo_input_t                   head;
    /* ------------------------------- */
    FILE*                           file;
};

struct _input_mmap {
    shmoo_input_t                   head;
    /* ------------------------------- */
};

struct _input_pipe {
    shmoo_input_t                   head;
    /* ------------------------------- */
    shmoo_handle_t                  desc;
};

struct _input_sock {
    shmoo_input_t                   head;
    /* ------------------------------- */
    shmoo_handle_t                  desc;
    /* ---- ^^^ _input_pipe_t ^^^ ---- */
};

struct _input_unix {
    shmoo_input_t                   head;
    /* ------------------------------- */
    shmoo_handle_t                  desc;
    /* ---- ^^^ _input_pipe_t ^^^ ---- */
    socklen_t                       plen;
    struct sockaddr_un              peer;
    socklen_t                       alen;
    struct sockaddr_un              addr;
};

struct _input_text {
    shmoo_input_t                   head;
    /* ------------------------------- */
};

struct _input_buff {
    shmoo_input_t                   head;
    /* ------------------------------- */
    const shmoo_buffer_t*           buff;
};
    

#if 0
struct _shmoo_input_ipv4 {
    shmoo_input_t                   head;
    /* ------------------------------- */
    shmoo_handle_t                  desc;
    /* ---- ^^^ _input_pipe_t ^^^ ---- */
    struct sockaddr_in              addr;
    struct sockaddr_in              peer;
    shmoo_string_t                  host;
    uint16_t                        port;
    shmoo_string_t                  serv;
};

struct _shmoo_input_ipv6 {
    shmoo_input_t                   head;
    /* ------------------------------- */
    shmoo_handle_t                  desc;
    /* ---- ^^^ _input_pipe_t ^^^ ---- */
    struct sockaddr_in6             addr;
    struct sockaddr_in6             peer;
    shmoo_string_t                  host;
    uint16_t                        port;
    shmoo_string_t                  serv;
};
#endif

static int _input_mmap_more (shmoo_input_t*);
static int _input_mmap_shut (shmoo_input_t*);
static int _input_mmap_dest (shmoo_input_t**);

static const shmoo_input_type_t _input_mmap_type = {
    .type_name = "input_mmap",
    .more      = _input_mmap_more,
    .shut      = _input_mmap_shut,
    .dest      = _input_mmap_dest,
};

static int _input_text_dest (shmoo_input_t**);

static const shmoo_input_type_t _input_text_type = {
    .type_name = "input_text",
    .more      = _input_mmap_more,
    .shut      = _input_mmap_shut,
    .dest      = _input_text_dest,
};

static const shmoo_input_type_t _input_buff_type = {
    .type_name = "input_buff",
    .more      = _input_mmap_more,
    .shut      = _input_mmap_shut,
    .dest      = _input_text_dest,
};

static int _input_pipe_dest (shmoo_input_t**);
static int _input_pipe_more (shmoo_input_t*);
static int _input_pipe_shut (shmoo_input_t*);

static const shmoo_input_type_t _input_pipe_type = {
    .type_name = "input_pipe",
    .more      = _input_pipe_more,
    .shut      = _input_pipe_shut,
    .dest      = _input_pipe_dest,
};

static const shmoo_input_type_t _input_unix_type = {
    .type_name = "input_unix",
    .more      = _input_pipe_more,
    .shut      = _input_pipe_shut,
    .dest      = _input_pipe_dest,
};

static int _input_file_dest (shmoo_input_t**);
static int _input_file_more (shmoo_input_t*);
static int _input_file_shut (shmoo_input_t*);

static const shmoo_input_type_t _input_file_type = {
    .type_name = "input_file",
    .more      = _input_file_more,
    .shut      = _input_file_shut,
    .dest      = _input_file_dest,
};

static const shmoo_vector_type_t _input_part_vector_type = {
    .type_name = "input_part_vector",
    .elem_size = sizeof(shmoo_cursor_t),
};

static int _input_mmap_open_path (
    shmoo_input_t**,
    shmoo_string_t,
    const struct stat*
);
static int _input_mmap_open_desc (
    shmoo_input_t**,
    shmoo_handle_t,
    const struct stat*
);
static int _input_pipe_open_path (
    shmoo_input_t**,
    shmoo_string_t,
    const struct stat*
);
static int _input_pipe_open_desc (
    shmoo_input_t**,
    shmoo_handle_t,
    const struct stat*
);
static int _input_sock_open_desc (
    shmoo_input_t**,
    shmoo_handle_t,
    const struct stat*
);
static int _input_unix_open_path (
    shmoo_input_t**,
    shmoo_string_t,
    const struct stat*
);
static int _input_unix_open_desc (
    shmoo_input_t**,
    shmoo_handle_t,
    const struct stat*
);
static int _input_more_trim (
    shmoo_input_t*
);
static int _input_more_grow (
    shmoo_input_t*,
    size_t*
);

/* public data */

const shmoo_vector_type_t shmoo_input_vector_type = {
    .type_name = "input_vector",
    .elem_size = sizeof(shmoo_input_t*),
};

/* public functions */

#define FPNAME "stdio_file: %d"
int
shmoo_input_open_file (
    shmoo_input_t**         inp,
    FILE*                   file
    )
{
    shmoo_handle_t  desc = fileno(file);
    _input_file_t*  in   = NULL;
    int             err  = 0;
    size_t          size = (sizeof(*in) + snprintf(NULL, 0, FPNAME, desc));

    if (! inp || ! file) {
        return EINVAL;
    } else if (feof(file)) {
        return EOF;
    } else if ((err = ferror(file))) {
        return err;
    } else if (! (in = calloc(1, size))) {
        return errno;
    } else if ((err = shmoo_vector_init(&in->head.part, &_input_part_vector_type))) {
        goto error;
    }

    in->head.type = &_input_file_type;
    in->head.name = (const uint8_t*) &in[1];
    snprintf((char*) &in[1], (size - sizeof(*in) + 1), FPNAME, desc);
    in->head.mtim = time(0);
    in->file      = file;
    return 0;

error:
    shmoo_vector_free(&in->head.part);
    free(in);
    return err;
}

int
shmoo_input_open_path (
    shmoo_input_t**         inp,
    const char*             _path
    )
{
    shmoo_string_t path;
    struct stat    info;

    if (! inp || ! _path) {
        return EINVAL;
    }
    path = shmoo_string(_path);

    if (-1 == stat(_path, &info)) {
        return errno;
    } else if (S_ISREG(info.st_mode)) {
        return (_input_mmap_open_path(inp, path, &info)
             || _input_pipe_open_path(inp, path, &info)
               );
    } else if (S_ISSOCK(info.st_mode)) {
        return _input_unix_open_path(inp, path, &info);
    } else if (S_ISFIFO(info.st_mode)) {
        return _input_pipe_open_path(inp, path, &info);
    } else {
        return EBADF;
    }
}

int
shmoo_input_open_desc (
    shmoo_input_t**         inp,
    shmoo_handle_t          desc
    )
{
    struct stat info;
    
    if (! inp || (desc < 0)) {
        return EINVAL;
    } else if (-1 == fstat(desc, &info)) {
        return errno;
    }

    if (S_ISREG(info.st_mode)) {
        return (_input_mmap_open_desc(inp, desc, &info)
             || _input_pipe_open_desc(inp, desc, &info)
               );
    } else if (S_ISSOCK(info.st_mode)) {
        return _input_sock_open_desc(inp, desc, &info);
    } else if (S_ISFIFO(info.st_mode)) {
        return _input_pipe_open_desc(inp, desc, &info);
    } else {
        return EBADF;
    }
}

int
shmoo_input_data (
    shmoo_input_t*          in,
    uint64_t                from,
    const uint8_t**         datap,
    size_t*                 leftp
    )
{
    size_t         indx = 0;
    size_t         high = 0;
    int            err  = 0;
    shmoo_cursor_t curs = {
        .size = 0,
        .data = 0,
        .from = 0,
    };

    if (! in || ! datap) {
        return EINVAL;
    }

    while (from >= in->size) {
        if ((err = in->type->more(in))) {
            break;
        }
    }
    if (from >= in->size) {
        return ERANGE;
    } else if ((err = shmoo_vector_used(&in->part, &high))) {
        return err;
    }
    
    /* Climb the vector of data buffers to find the right one. */
    for (indx = 0; indx < high; ++indx) {
        if ((err = shmoo_vector_peek(&in->part, indx, &curs))) {
            return err;
        } else if (from > (curs.from + curs.size)) {
            continue;
        } else {
            break;
        }
    }

    /* All done.  Populate the result. */
    *datap = (curs.data + (from - curs.from));
    *leftp = (curs.size - (from - curs.from));
    return 0;
}

int
shmoo_input_copy (
    shmoo_input_t*          in,
    uint64_t                from,
    uint64_t                want,
    uint8_t*                into,
    uint64_t*               usedp
    )
{
    uint32_t       indx = 0;
    size_t         high = 0;
    uint64_t       used = 0;
    const uint8_t* data = NULL;
    size_t         left = 0;
    int            err  = 0;
    shmoo_cursor_t curs = {
        .size = 0,
        .data = 0,
        .from = 0,
    };

    if (! in || ! into) {
        return EINVAL;
    } else if ((err = shmoo_input_data(in, from, &data, &left))) {
        return err;
    } else if ((err = shmoo_vector_used(&in->part, &high))) {
        return err;
    }

    /* Find the right data buffer to start the copy from. */
    for (indx = 0; indx < high; ++indx) {
        if ((err = shmoo_vector_peek(&in->part, indx, &curs))) {
            return err;
        } else if (from > (curs.from + curs.size)) {
            continue;
        } else {
            break;
        }
    }
    /* Found the right data buffer; offset within it. */
    curs.from += from;
    curs.data += from;
    curs.size -= from;

    /* Using info from the above loop, copy data out, possibly
     * spanning multiple data buffers.
     */
    for (used = 0, ++indx; indx < high; ++indx) {
        uint64_t copy = ((curs.size > want) ? want : curs.size);
        (void) memcpy((into + used), curs.data, copy);
        used += copy;
        want -= copy;
        if (! want) {
            break;
        } else if ((err = shmoo_vector_peek(&in->part, indx, &curs))) {
            return err;
        }
    }
    *usedp = used;
    return 0;
}

/* static functions */

int
_input_mmap_open_path (
    shmoo_input_t**         inp,
    shmoo_string_t          path,
    const struct stat*      info
    )
{
    _input_mmap_t* in   = NULL;
    int            desc = -1;
    int            err  = 0;
    shmoo_cursor_t curs = {
        .from = 0,
        .data = MAP_FAILED,
        .size = 0,
    };

    if (! inp || ! path.data || ! path.size) {
        return EINVAL;
    } else if (! (in = calloc(1, (sizeof(*in) + path.size + 1)))) {
        return errno;
    }
    in->head.type = &_input_mmap_type;
    in->head.size = info->st_size;
    in->head.name = (const uint8_t*) &in[1];
    (void) memcpy(&in[1], path.data, path.size);
    *((uint8_t*) &in[1] + path.size) = '\0';
    in->head.mtim = info->st_mtime;

    desc = open((const char*) in->head.name, O_RDONLY);
    if (-1 == desc) {
        err = errno;
        goto error;
    }
    curs.from = 0;
    curs.size = info->st_size;
    curs.data = mmap(0, info->st_size, PROT_READ, MAP_SHARED, desc, 0);
    if (MAP_FAILED == curs.data) {
        err = errno;
        goto error;
    } else if (-1 == close(desc)) {
        err = errno;
        goto error;
    } else if ((err = shmoo_vector_init(&in->head.part, &_input_part_vector_type))) {
        goto error;
    } else if ((err = shmoo_vector_insert_tail(&in->head.part, &curs))) {
        goto error;
    }
    *inp = &in->head;
    return 0;

error:
    if (-1 != desc) {
        (void) close(desc);
    }
    if (curs.data != MAP_FAILED) {
        (void) munmap((void*) curs.data, curs.size);
    }
    free(in);
    return err;
}

#define FDNAME "file_descriptor: %4d"
int
_input_mmap_open_desc (
    shmoo_input_t**         inp,
    shmoo_handle_t          desc,
    const struct stat*      info
    )
{
    _input_mmap_t* in   = NULL;
    size_t         size = (sizeof(*in) + snprintf(NULL, 0, FDNAME, desc));
    int            err  = 0;
    shmoo_cursor_t curs;

    if (! inp) {
        return EINVAL;
    }

    curs.from = 0;
    curs.size = info->st_size;
    curs.data = mmap(0, info->st_size, PROT_READ, MAP_SHARED, desc, 0);

    if (MAP_FAILED == curs.data) {
        return errno;
    } else if (! (in = calloc(1, size))) {
        err = errno;
        goto error;
    } else if ((err = shmoo_vector_init(&in->head.part, &_input_part_vector_type))) {
        goto error;
    } else if ((err = shmoo_vector_insert_tail(&in->head.part, &curs))) {
        goto error;
    }
    in->head.type = &_input_mmap_type;
    in->head.size = info->st_size;
    in->head.name = (const uint8_t*) &in[1];
    snprintf((char*) &in[1], (size - sizeof(*in) + 1), FDNAME, desc);
    in->head.mtim = info->st_mtime;
    *inp = &in->head;
    return 0;

error:
    if (MAP_FAILED != curs.data) {
        (void) munmap((void*) curs.data, curs.size);
    }
    free(in);
    return err;
}

int
_input_mmap_more (
    shmoo_input_t*          in
    )
{
    return EBADF;
    (void) in;
}

int
_input_mmap_shut (
    shmoo_input_t*          in
    )
{
    return 0;
    (void) in;
}

int
_input_mmap_dest (
    shmoo_input_t**         inp
    )
{
    shmoo_string_t data;
    _input_mmap_t* in = NULL;

    if (! inp) {
        return EINVAL;
    } else if (! *inp) {
        return 0;
    }

    in = (_input_mmap_t*) *inp;
    while (shmoo_vector_remove_tail(&in->head.part, &data)) {
        (void) munmap((void*) data.data, data.size);
    }
    shmoo_vector_free(&in->head.part);
    free(in);
    *inp = NULL;
    return 0;
}

int
_input_pipe_open_path (
    shmoo_input_t**         inp,
    shmoo_string_t          path,
    const struct stat*      info
    )
{
    _input_pipe_t* in   = NULL;
    size_t         size = (sizeof(*in) + path.size + 1);
    shmoo_handle_t desc = -1;
    int            err  = 0;

    if (! inp) {
        return EINVAL;
    } else if (! (in = calloc(1, size))) {
        return errno;
    } else if ((err = shmoo_vector_init(&in->head.part, &_input_part_vector_type))) {
        goto error;
    }
    in->head.type = &_input_pipe_type;
    in->head.size = 0;
    in->head.name = (const uint8_t*) &in[1];
    memcpy(&in[1], path.data, path.size);
    in->head.mtim = time(0);

    desc = open((const char*) in->head.name, O_RDONLY);
    if (-1 == desc) {
        err = errno;
        goto error;
    }
    in->desc = desc;
    *inp = &in->head;
    return 0;

error:
    if (-1 == desc) {
        (void) close(desc);
    }
    free(in);
    return err;

    (void) info;
}

int
_input_pipe_open_desc (
    shmoo_input_t**         inp,
    shmoo_handle_t          desc,
    const struct stat*      info
    )
{
    _input_pipe_t* in   = NULL;
    size_t         size = (sizeof(*in) + snprintf(NULL, 0, FDNAME, desc));
    int            err  = 0;

    if (! inp || (desc < 0)) {
        return EINVAL;
    } else if (! (in = calloc(1, size))) {
        return errno;
    } else if ((err = shmoo_vector_init(&in->head.part, &_input_part_vector_type))) {
        goto error;
    }
    in->head.type = &_input_pipe_type;
    in->head.size = 0;
    in->head.name = (const uint8_t*) &in[1];
    snprintf((char*) &in[1], size, FDNAME, desc);
    in->head.mtim = time(0);
    in->desc      = desc;
    *inp = &in->head;
    return 0;

error:
    free(in);
    return err;

    (void) info;
}

int
_input_pipe_shut (
    shmoo_input_t*          _in
    )
{
    _input_pipe_t* in = (_input_pipe_t*) _in;

    if (! in) {
        return EINVAL;
    } else if (in->desc < 0) {
        return 0;
    } else if (-1 == close(in->desc)) {
        return errno;
    } else {
        return 0;
    }
}

int
_input_pipe_dest (
    shmoo_input_t**         inp
    )
{
    _input_pipe_t* in   = NULL;
    shmoo_cursor_t curs = {
        .from = 0,
        .data = 0,
        .size = 0,
    };

    if (! inp) {
        return EINVAL;
    } else if (! *inp) {
        return 0;
    } else if (in->desc >= 0) {
        (void) close(in->desc);
    }
    while (! shmoo_vector_remove_tail(&in->head.part, &curs)) {
        (void) munmap((void*) curs.data, curs.size);
    }
    shmoo_vector_free(&in->head.part);
    free(in);
    *inp = NULL;

    return 0;
}

int
_input_more_grow (
    shmoo_input_t*          in,
    size_t*                 leftp
    )
{
    shmoo_cursor_t* last = NULL;
    uint8_t*        mold = NULL;
    uint8_t*        mnew = MAP_FAILED;
    size_t          page = sysconf(_SC_PAGE_SIZE);
    size_t          left = 0;

    if ((last = (shmoo_cursor_t*) shmoo_vector_tail(&in->part))) {
        if (in->size != (last->from + last->size)) {
            /* There is empty memory at the end of the last memory map. */
            left = (last->size - (in->size - last->from));
            mnew = 0;
        } else {
            /* Try to join this memory blob directly to the last one. */
#ifdef _USE_GNU
            mnew = mremap(last->data, last->size,
                          (last->size + page),
                          MREMAP_MAYMOVE
                         );
#else
            mnew = mmap((mold + last->size), page,
                        (PROT_READ|PROT_WRITE),
                        MAP_ANONYMOUS, -1, 0
                       );
#endif
            if (MAP_FAILED == mnew) {
                /* Could not join the old and new memory maps.
                 * setting 'last' to NULL will cause a new cursor
                 * to be inserted into the input's part vector.
                 */
                last = 0;
            } else {
                /* Successfully joined the memory maps.  Rewrite
                 * the necessary bits of the relevant cursor in
                 * the input part vector.
                 */
                last->data  = mnew;
                last->size += page;
            }
            /* The amount of data left to fill. */
            left = page;
        }
    }
    if (MAP_FAILED == mnew) {
        int err = 0;
        shmoo_cursor_t curs = {
            .from = in->size,
            .data = mmap(0, page, (PROT_READ|PROT_WRITE),
                         MAP_ANONYMOUS, -1, 0
                        ),
            .size = page,
        };
        if (MAP_FAILED == curs.data) {
            return errno;
        } else if ((err = shmoo_vector_insert_tail(&in->part, &curs))) {
            (void) munmap((void*) curs.data, curs.size);
            return err;
        }
        left = page;
    }

    *leftp = left;
    return 0;
}

int
_input_more_trim (
    shmoo_input_t*          in
    )
{
    shmoo_cursor_t* last = NULL;
    size_t          page = sysconf(_SC_PAGE_SIZE);
    int             err  = 0;

    if ((err = shmoo_vector_obtain_tail(&in->part, &last))) {
        return err;
    } else if (in->size == last->from) {
        /* entire last cursor is a new allocation */
        shmoo_cursor_t curs;
        if (! (err = shmoo_vector_remove_tail(&in->part, &curs))) {
            (void) munmap((void*) curs.data, curs.size);
        }
        return err;
    } else if ((in->size - last->from) < page) {
        /* did not allocate new memory */
        return 0;
    } else {
        /* last cursor had allocation joined */
#ifdef _USE_GNU
        uint8_t* mnew = mremap(last->data, last->size,
                               (last->size - page),
                               MREMAP_MAYMOVE
                              );
        if (MAP_FAILED == mnew) {
            return errno;
        } else {
            last->data  = mnew;
        }
#else
        (void) munmap((void*) (last->data + (last->size - page)), page);
#endif
        last->size -= page;
        return 0;
    }
}

/* Read a pipe a page at a time, trying to merge mappings. */
int
_input_pipe_more (
    shmoo_input_t*          _in
    )
{
    _input_pipe_t*  in   = (_input_pipe_t*) _in;
    shmoo_cursor_t  last = { 0 };
    uint8_t*        more = NULL;
    size_t          used = 0;
    size_t          left = 0;
    int             got  = 0;
    int             err  = 0;

    if (! in || (in->desc < 0)) {
        return EINVAL;
    } else if ((err = _input_more_grow(_in, &left))) {
        return err;
    } else if ((err = shmoo_vector_obtain_tail(&in->head.part, &last))) {
        return 0;
    }
    more = (uint8_t*) (last.data + (in->head.size - last.from));

    /* Read into the newly-allocated buffer. */
    for (used = 0; used < left; ) {
        got = read(in->desc, (more + used), (left - used));
        if ((got < 0) && (errno == EINTR)) {
        } else if (got > 0) {
            used += got;
        } else {
            err = (got ? errno : EOF);
            break;
        }
    }

    /* Last read() got a 0 result, meaning EOF. */
    if (! got) {
        /* Close the file descriptor. */
        (void) close(in->desc);
        in->desc = -1;
    }

    /* If we read no data, unmap the new allocations. */
    if (used) {
        in->head.size += used;
    } else if (left != (size_t) sysconf(_SC_PAGE_SIZE)) {
    } else if ((err = _input_more_trim(_in))) {
        return err;
    }
    return 0;
}

int
_input_unix_open_path (
    shmoo_input_t**         inp,
    shmoo_string_t          path,
    const struct stat*      info
    )
{
    const struct sockaddr* addr = NULL;
    size_t                 size = 0;
    socklen_t              alen = 0;
    socklen_t              plen = 0;
    int                    desc = -1;
    _input_unix_t*         in   = NULL;
    int                    err  = 0;

    if ((path.size + 1) >= sizeof(in->addr.sun_path)) {
        size = (sizeof(*in) + ((path.size + 1) - sizeof(in->addr.sun_path)));
    } else {
        size = sizeof(*in);
    }
    if (! (in = calloc(1, size))) {
        return errno;
    } else if ((err = shmoo_vector_init(&in->head.part, &_input_part_vector_type))) {
        goto error;
    }
    in->head.type = &_input_unix_type;
    in->head.size = 0;
    in->head.name = (const uint8_t*) in->addr.sun_path;
    in->addr.sun_family = AF_UNIX;
    memcpy(in->addr.sun_path, path.data, path.size);
    in->head.mtim = time(0);
    addr = (const struct sockaddr*) &in->addr;

    alen = (offsetof(struct sockaddr_un, sun_path) + path.size);
    plen = sizeof(in->peer);
    desc = socket(AF_UNIX, SOCK_STREAM, 0);
    if (-1 == desc) {
        err = errno;
        goto error;
    } else if (-1 == connect(desc, addr, alen)) {
        err = errno;
        goto error;
    } else if (-1 == shutdown(desc, SHUT_WR)) {
        err = errno;
        goto error;
    } else if (-1 == getpeername(desc, (struct sockaddr*) &in->peer, &plen)) {
        err = errno;
        goto error;
    } else if (plen > sizeof(in->peer)) {
        plen = sizeof(in->peer);
    }
    in->alen = alen;
    in->plen = plen;
    in->desc = desc;
    *inp     = &in->head;
    return 0;

error:
    if (-1 != desc) {
        (void) close(desc);
    }
    free(in);
    return err;

    (void) info;
}

int
_input_sock_open_desc (
    shmoo_input_t**         inp,
    shmoo_handle_t          desc,
    const struct stat*      info
    )
{
    struct sockaddr     addr = { 0 };
    socklen_t           alen = 0;

    alen = sizeof(addr);
    if (-1 == getsockname(desc, &addr, &alen)) {
        return errno;
    }

    if (addr.sa_family == AF_UNIX) {
        return _input_unix_open_desc(inp, desc, info);
//    } else if (addr.sa_family == AF_INET) {
//    } else if (addr.sa_family == AF_INET6) {
    }

    return 0;
}

int
_input_unix_open_desc (
    shmoo_input_t**         inp,
    shmoo_handle_t          desc,
    const struct stat*      info
    )
{
    _input_unix_t*     in   = NULL;
    size_t             size = 0;
    struct sockaddr_un abuf;
    socklen_t          alen = sizeof(abuf);
    socklen_t          plen = sizeof(abuf);
    int                err  = 0;

    if (-1 == getsockname(desc, (struct sockaddr*) &abuf, &alen)) {
        return errno;
    } else if (alen >= sizeof(abuf)) {
        size = (sizeof(*in) + (alen - sizeof(abuf) + 1));
    } else {
        size = sizeof(*in);
    }

    if (! (in = calloc(1, size))) {
        return errno;
    } else if (-1 == getsockname(desc, (struct sockaddr*) &in->addr, &alen)) {
        err = errno;
        goto error;
    } else if (-1 == getpeername(desc, (struct sockaddr*) &in->peer, &plen)) {
        err = errno;
        goto error;
    } else if ((err = shmoo_vector_init(&in->head.part, &_input_part_vector_type))) {
        goto error;
    } else if (plen > sizeof(in->peer)) {
        plen = sizeof(in->peer);
    }
    in->head.type = &_input_unix_type;
    in->head.size = 0;
    in->head.name = (const uint8_t*) in->addr.sun_path;
    in->head.mtim = time(0);
    in->alen = alen;
    in->plen = plen;
    *inp = &in->head;
    return 0;

error:
    free(in);
    return err;

    (void) info;
}

int
_input_file_more (
    shmoo_input_t*          _in
    )
{
    _input_file_t*  in   = (_input_file_t*) _in;
    uint8_t*        more = NULL;
    shmoo_cursor_t  last = { 0 };
    size_t          left = 0;
    size_t          used = 0;
    size_t          got  = 0;
    int             err  = 0;

    if (! in || ! in->file) {
        return EINVAL;
    } else if ((err = _input_more_grow(_in, &left))) {
        return err;
    } else if ((err = shmoo_vector_obtain_tail(&in->head.part, &last))) {
        return err;
    }
    more = (uint8_t*) (last.data + (in->head.size - last.from));

    /* Read into the newly-allocated buffer. */
    for (used = 0; used < left; ) {
        got = fread((more + used), 1, (left - used), in->file);
        if (got > 0) {
            used += got;
            continue;
        } else if (feof(in->file)) {
            err = (used ? 0 : EOF);
        } else if (ferror(in->file)) {
            if ((err = errno == EINTR)) {
                continue;
            }
        }
        break;
    }

    /* Last fread() got no data; close the descriptor. */
    if (feof(in->file)) {
        (void) fclose(in->file);
        in->file = NULL;
    }

    if (used) {
        in->head.size += used;
    } else if (left == (size_t) sysconf(_SC_PAGE_SIZE)) {
        err = _input_more_trim(_in);
    }

    return err;
}

int
_input_file_shut (
    shmoo_input_t*          _in
    )
{
    _input_file_t* in = (_input_file_t*) _in;

    if (! in) {
        return EINVAL;
    } else if (in->file) {
        fclose(in->file);
        in->file = NULL;
    }
    return 0;
}

int
_input_file_dest (
    shmoo_input_t**         inp
    )
{
    _input_file_t*  in   = NULL;
    shmoo_cursor_t  curs = {
        .from = 0,
        .size = 0,
        .data = NULL,
    };

    if (! inp) {
        return EINVAL;
    } else if (! *inp) {
        return 0;
    }
    in = (_input_file_t*) *inp;
    if (in->file) {
        (void) fclose(in->file);
    }
    while (! (shmoo_vector_remove_tail(&in->head.part, &curs))) {
        (void) munmap((void*) curs.data, curs.size);
    }
    shmoo_vector_free(&in->head.part);
    free(in);
    *inp = NULL;
    return 0;
}

int
_input_text_dest (
    shmoo_input_t**         inp
    )
{
    if (! inp) {
        return EINVAL;
    } else if (*inp) {
        (void) shmoo_vector_free(&(*inp)->part);
        free(*inp);
        *inp = NULL;
    }
    return 0;
}

#define ITNAME "input_text"
int
_shmoo_input_open_text (
    shmoo_input_t**         inp,
    shmoo_string_t          text
    )
{
    _input_text_t* in   = NULL;
    int            err  = 0;
    shmoo_cursor_t curs = {
        .from = 0,
        .data = 0,
        .size = 0,
    };

    if (! inp || ! text.data) {
        return EINVAL;
    } else if (! (in = calloc(1, (sizeof(*in) + sizeof(ITNAME))))) {
        return errno;
    } else if ((err = shmoo_string_data(&text, 0, &curs.data, &curs.size))) {
        goto error;
    } else if ((err = shmoo_vector_init(&in->head.part, &_input_part_vector_type))) {
        goto error;
    } else if ((err = shmoo_vector_insert_tail(&in->head.part, &curs))) {
        goto error;
    }
    in->head.type = &_input_text_type;
    in->head.size = text.size;
    in->head.name = (const uint8_t*) &in[1];
    (void) memcpy(&in[1], ITNAME, sizeof(ITNAME));
    in->head.mtim = time(0);
    *inp = &in->head;
    return 0;
    
error:
    shmoo_vector_free(&in->head.part);
    free(in);
    return err;
}

#define IBNAME "input_buff"
int
_shmoo_input_open_buff (
    shmoo_input_t**         inp,
    const shmoo_buffer_t*   buff
    )
{
    _input_buff_t* in   = NULL;
    int            err  = 0;
    uint8_t*       data = NULL;
    size_t         left = 0;

    if (! inp || ! buff) {
        return EINVAL;
    } else if (! (in = calloc(1, (sizeof(*in) + sizeof(IBNAME))))) {
        return errno;
    } else if ((err = shmoo_buffer_data(buff, 0, &data, &left))) {
        goto error;
    } else if ((err = shmoo_vector_init(&in->head.part, &_input_part_vector_type))) {
        goto error;
    } else {
        shmoo_cursor_t curs = {
            .from = 0,
            .data = data,
            .size = left,
        };
        if ((err = shmoo_vector_insert_tail(&in->head.part, &curs))) {
            goto error;
        }
    }

    in->head.type = &_input_buff_type;
    in->head.size = left;
    in->head.name = (const uint8_t*) &in[1];
    (void) memcpy(&in[1], IBNAME, sizeof(IBNAME));
    in->head.mtim = time(0);
    *inp = &in->head;
    return 0;

error:
    shmoo_vector_free(&in->head.part);
    free(in);
    return err;
}


