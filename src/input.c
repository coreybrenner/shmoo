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
#include <alloca.h>         /* alloca */
#include <fcntl.h>          /* open, O_... */
#include <stdarg.h>         /* va_list, va_start, va_arg, va_end */
#include <stdint.h>         /* uint(8|16|32|64)_t */
#include <stdio.h>          /* snprintf */
#include <stdlib.h>         /* calloc, free */
#include <string.h>         /* memcpy */
#include <time.h>           /* timespec */
#include <unistd.h>         /* close */
#include <sys/mman.h>       /* mmap, munmap, mremap, MAP_..., PROT_... */
#include <sys/socket.h>     /* socket, connect, SOCK_..., AF_... */
#include <sys/stat.h>       /* stat, fstat, S_IS... */
#include <sys/types.h>      /* size_t, socklen_t, ... */
#include <sys/un.h>         /* sockaddr_un */

#include <shmoo/cursor.h>
#include <shmoo/handle.h>   /* shmoo_handle_t */
#include <shmoo/input.h>    /* shmoo_input_t */
#include <shmoo/vector.h>   /* shmoo_vector_t */

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
    const shmoo_origin_t*           orig;
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

static int      _input_mmap_dest (shmoo_input_t**);
static uint64_t _input_mmap_more (shmoo_input_t*, uint64_t);
static int      _input_mmap_shut (shmoo_input_t*);

const shmoo_input_type_t _input_mmap_type = {
    .name = "input_mmap",
    .dest = _input_mmap_dest,
    .more = _input_mmap_more,
    .shut = _input_mmap_shut,
};

const shmoo_input_type_t _input_text_type = {
    .name = "input_text",
    .dest = _input_mmap_dest,
    .more = _input_mmap_more,
    .shut = _input_mmap_shut,
};

static int      _input_pipe_dest (shmoo_input_t**);
static uint64_t _input_pipe_more (shmoo_input_t*, uint64_t);
static int      _input_pipe_shut (shmoo_input_t*);

const shmoo_input_type_t _input_pipe_type = {
    .name = "input_pipe",
    .dest = _input_pipe_dest,
    .more = _input_pipe_more,
    .shut = _input_pipe_shut,
};

const shmoo_input_type_t _input_unix_type = {
    .name = "input_unix",
    .dest = _input_pipe_dest,
    .more = _input_pipe_more,
    .shut = _input_pipe_shut,
};

static int      _input_file_dest (shmoo_input_t**);
static uint64_t _input_file_more (shmoo_input_t*, uint64_t);
static int      _input_file_shut (shmoo_input_t*);

const shmoo_input_type_t _input_file_type = {
    .name = "input_file",
    .dest = _input_file_dest,
    .more = _input_file_more,
    .shut = _input_file_shut,
};

static const shmoo_vector_type_t _input_part_vector_type = {
    .name = "input_part_vector",
    .elem = sizeof(shmoo_cursor_t),
};

static int _input_mmap_open_path (shmoo_input_t**, shmoo_string_t, const struct stat*);
static int _input_mmap_open_desc (shmoo_input_t**, shmoo_handle_t, const struct stat*);
static int _input_pipe_open_path (shmoo_input_t**, shmoo_string_t, const struct stat*);
static int _input_pipe_open_desc (shmoo_input_t**, shmoo_handle_t, const struct stat*);
static int _input_sock_open_desc (shmoo_input_t**, shmoo_handle_t, const struct stat*);
static int _input_unix_open_path (shmoo_input_t**, shmoo_string_t, const struct stat*);
static int _input_unix_open_desc (shmoo_input_t**, shmoo_handle_t, const struct stat*);

/* public functions */

#define FPNAME "stdio_file: %d"
int
shmoo_input_open_file (shmoo_input_t** inp, FILE* file) {
    shmoo_handle_t  desc = fileno(file);
    _input_file_t*  in   = 0;
    size_t          size = (sizeof(*in) + snprintf(0, 0, FPNAME, desc));

    if (! inp || ! file) {
        return 0;
    } else if (feof(file) || ferror(file)) {
        return 0;
    } else if (! (in = calloc(1, size))) {
        return 0;
    }

    in->head.type = &_input_file_type;
    in->head.name = (const uint8_t*) &in[1];
    in->head.mtim = time(0);
    snprintf((char*) &in[1], (size - sizeof(*in)), FPNAME, desc);
    in->file = file;
    return 1;
}

int
shmoo_input_open_path (shmoo_input_t** inp, shmoo_string_t path) {
    struct stat info;
    char*       buff = alloca(path.size + 1);

    if (! inp || ! buff) {
        return 0;
    }
    memcpy(buff, path.data, path.size);
    buff[path.size] = '\0';

    if (-1 == stat(buff, &info)) {
        return 0;
    } else if (S_ISREG(info.st_mode)) {
        return (_input_mmap_open_path(inp, path, &info)
             || _input_pipe_open_path(inp, path, &info)
               );
    } else if (S_ISSOCK(info.st_mode)) {
        return _input_unix_open_path(inp, path, &info);
    } else if (S_ISFIFO(info.st_mode)) {
        return _input_pipe_open_path(inp, path, &info);
    } else {
        return 0;
    }
}

int
shmoo_input_open_desc (shmoo_input_t** inp, shmoo_handle_t desc) {
    struct stat info;
    
    if (! inp || (-1 == desc)) {
        return 0;
    } else if (-1 == fstat(desc, &info)) {
        return 0;
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
        return 0;
    }
}

int
shmoo_input_dest (shmoo_input_t** inp) {
    if (! inp) {
        return 0;
    } else if (! *inp) {
        return 1;   /* already freed... */
    }
    return (*inp)->type->dest(inp);
}

int
_shmoo_input_data (shmoo_input_t* in, uint64_t from, shmoo_cursor_t* datap, ...) {
    uint32_t       indx;
    uint32_t       high;
    shmoo_cursor_t curs = {
        .size = 0,
        .data = 0,
        .from = 0,
    };

    if (! in || ! datap) {
        return 0;
    }

    if (from >= in->size) {
        /* If we are asking for data beyond the bounds of what the
         * input source currently has, try to read more data and
         * procure a pointer and amount from the new buffer.
         */
        uint64_t want;
        va_list  args;

        /* Allow a default; if no amount of data is specified,
         * round up the size of the last data buffer to the
         * next page boundary so there might be some data
         * available to copy out.
         */
        va_start(args, datap);
        want = va_arg(args, uint64_t);
        va_end(args);
        if (! want) {
            uint64_t page = sysconf(_SC_PAGE_SIZE);
            uint64_t mask = (page - 1);
            want = ((from + page) & ~mask);
        }
        /* Try to slurp up more data. */
        if (! in->type->more(in, want)) {
            return 0;
        }
    }

    /* Find out how many slots the data buffer vector has. */
    if (! shmoo_vector_used(&in->part, &high)) {
        return 0;
    }
    
    /* Climb the vector of data buffers to find the right one. */
    for (indx = 0; indx < high; ++indx) {
        if (! shmoo_vector_peek(&in->part, indx, &curs)) {
            return 0;
        } else if (from > (curs.from + curs.size)) {
            continue;
        } else {
            break;
        }
    }

    /* All done.  Populate the result. */
    curs.data += (from - curs.from);
    curs.size -= (from - curs.from);
    curs.from  = from;
    *datap = curs;

    return 1;
}

uint64_t
shmoo_input_read (shmoo_input_t* in, uint64_t from, uint64_t want, uint8_t* into) {
    uint32_t       indx = 0;
    uint32_t       high = 0;
    uint64_t       used = 0;
    shmoo_cursor_t curs = {
        .size = 0,
        .data = 0,
        .from = 0,
    };

    if (! in || ! into) {
        return 0;
    } else if (! shmoo_input_data(in, from, &curs, want)) {
        return 0;
    } else if (! shmoo_vector_used(&in->part, &high)) {
        return 0;
    }

    /* Find the right data buffer to start the copy from. */
    for (indx = 0; indx < high; ++indx) {
        if (! shmoo_vector_peek(&in->part, indx, &curs)) {
            return 0;
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
        memcpy((into + used), curs.data, copy);
        used += copy;
        want -= copy;
        if (0 == want) {
            break;
        } else if (! shmoo_vector_peek(&in->part, indx, &curs)) {
            break;
        }
    }

    return used;
}

uint64_t
_shmoo_input_more (shmoo_input_t* in, ...) {
    uint64_t want;
    va_list  args;

    va_start(args, in);
    want = va_arg(args, uint64_t);
    va_end(args);

    if (! in) {
        return 0;
    }
    if (want == 0) {
        shmoo_cursor_t* last = 0;
        long int        page = sysconf(_SC_PAGE_SIZE);
        if (-1 == page) {
            return 0;
        } else if (! (last = shmoo_vector_tail(&in->part))) {
            return 0;
        }
        want = ((last->from + last->size + page) & ~(page - 1));
    }

    return in->type->more(in, want);
}

int
shmoo_input_shut (shmoo_input_t* in) {
    if (! in) {
        return 0;
    } else {
        return in->type->shut(in);
    }
}

int
shmoo_input_size (const shmoo_input_t* in, uint64_t* sizep) {
    if (! in || ! sizep) {
        return 0;
    } else {
        *sizep = in->size;
        return 1;
    }
}

int
shmoo_input_type (const shmoo_input_t* in, const char** typep) {
    if (! in || ! typep) {
        return 0;
    } else {
        *typep = in->type->name;
        return 1;
    }
}

int
shmoo_input_orig (const shmoo_input_t* in, const shmoo_origin_t** origp) {
    if (! in || ! origp) {
        return 0;
    } else if (in->type != &_input_text_type) {
        return 0;
    } else {
        *origp = ((const _input_text_t*) in)->orig;
        return 1;
    }
}

/* static functions */

int
_input_mmap_open_path (shmoo_input_t** inp, shmoo_string_t path, const struct stat* info) {
    shmoo_cursor_t data = {
        .data = MAP_FAILED,
        .size = 0,
    };
    _input_mmap_t* in   = 0;
    size_t         size = (sizeof(*in) + path.size + 1);
    int            desc = -1;

    if (! (in = calloc(1, size))) {
        goto error;
    }
    in->head.type = &_input_mmap_type;
    in->head.size = info->st_size;
    in->head.mtim = info->st_mtime;
    in->head.name = (const uint8_t*) &in[1];
    memcpy(&in[1], path.data, path.size);

    desc = open((const char*) in->head.name, O_RDONLY);
    if (-1 == desc) {
        goto error;
    }
    data.from = 0;
    data.size = info->st_size;
    data.data = mmap(0, info->st_size, PROT_READ, MAP_SHARED, desc, 0);
    if (-1 == close(desc)) {
        goto error;
    } else if (desc = -1, data.data == MAP_FAILED) {
        goto error;
    } else if (! shmoo_vector_init(&in->head.part, &_input_part_vector_type)) {
        goto error;
    } else if (! shmoo_vector_insert_tail(&in->head.part, &data)) {
        goto error;
    }
    *inp = &in->head;
    return 1;

error:
    if (-1 != desc) {
        (void) close(desc);
    }
    if (data.data != MAP_FAILED) {
        (void) munmap((void*) data.data, data.size);
    }
    free(in);
    return 0;
}

#define FDNAME "file_descriptor: %d"
int
_input_mmap_open_desc (shmoo_input_t** inp, shmoo_handle_t desc, const struct stat* info) {
    _input_mmap_t* in   = 0;
    size_t         size = (sizeof(*in) + snprintf(0, 0, FDNAME, desc));
    shmoo_cursor_t data = {
        .from = 0,
        .size = info->st_size,
        .data = mmap(0, info->st_size, PROT_READ, MAP_SHARED, desc, 0),
    };

    if (MAP_FAILED == data.data) {
        return 0;
    } else if (! (in = calloc(1, size))) {
        goto error;
    } else if (! shmoo_vector_init(&in->head.part, &_input_part_vector_type)) {
        goto error;
    } else if (! shmoo_vector_insert_tail(&in->head.part, &data)) {
        goto error;
    }
    in->head.type = &_input_mmap_type;
    in->head.size = info->st_size;
    in->head.mtim = info->st_mtime;
    in->head.name = (const uint8_t*) &in[1];
    snprintf((char*) &in[1], (size - sizeof(*in)), FDNAME, desc);
    *inp = &in->head;
    return 1;

error:
    if (MAP_FAILED != data.data) {
        (void) munmap((void*) data.data, data.size);
    }
    free(in);
    return 0;
}

uint64_t
_input_mmap_more (shmoo_input_t* in, uint64_t want) {
    return 0;
    (void) in;
    (void) want;
}

int
_input_mmap_shut (shmoo_input_t* in) {
    return 1;
    (void) in;
}

int
_input_mmap_dest (shmoo_input_t** inp) {
    shmoo_string_t data;
    _input_mmap_t* in = 0;

    if (! inp) {
        return 0;
    } else if (! (in = *((_input_mmap_t**) inp))) {
        return 0;
    }

    while (shmoo_vector_remove_tail(&in->head.part, &data)) {
        (void) munmap((void*) data.data, data.size);
    }
    shmoo_vector_free(&in->head.part);
    free(in);
    *inp = 0;

    return 1;
}

int
_input_pipe_open_path (shmoo_input_t** inp, shmoo_string_t path, const struct stat* info) {
    _input_pipe_t* in   = 0;
    size_t         size = (sizeof(*in) + path.size + 1);
    shmoo_handle_t desc = -1;

    if (! (in = calloc(1, size))) {
        return 0;
    } else if (! shmoo_vector_init(&in->head.part, &_input_part_vector_type)) {
        goto error;
    }
    in->head.type = &_input_pipe_type;
    in->head.mtim = time(0);
    in->head.name = (const uint8_t*) &in[1];
    memcpy(&in[1], path.data, path.size);

    desc = open((const char*) in->head.name, O_RDONLY);
    if (-1 == desc) {
        goto error;
    }
    in->desc = desc;
    *inp = &in->head;
    return 1;

error:
    if (-1 == desc) {
        (void) close(desc);
    }
    free(in);
    return 0;

    (void) info;
}

int
_input_pipe_open_desc (shmoo_input_t** inp, shmoo_handle_t desc, const struct stat* info) {
    _input_pipe_t* in   = 0;
    size_t         size = (sizeof(*in) + snprintf(0, 0, FDNAME, desc));

    if (! (in = calloc(1, size))) {
        return 0;
    } else if (! shmoo_vector_init(&in->head.part, &_input_part_vector_type)) {
        goto error;
    }
    in->head.type = &_input_pipe_type;
    in->head.mtim = time(0);
    in->head.name = (const uint8_t*) &in[1];
    snprintf((char*) &in[1], size, FDNAME, desc);
    in->desc = desc;
    *inp = &in->head;
    return 1;

error:
    free(in);
    return 0;

    (void) info;
}

int
_input_pipe_shut (shmoo_input_t* _in) {
    _input_pipe_t* in = (_input_pipe_t*) _in;
    if ((-1 != in->desc) && (-1 == close(in->desc))) {
        in->desc = -1;
        return 0;
    } else {
        return 1;
    }
}

int
_input_pipe_dest (shmoo_input_t** inp) {
    shmoo_string_t data;
    _input_pipe_t* in = 0;

    if (! inp) {
        return 0;
    } else if (! (in = *((_input_pipe_t**) inp))) {
        return 0;
    }
    if (-1 != in->desc) {
        (void) close(in->desc);
    }
    while (shmoo_vector_remove_tail(&in->head.part, &data)) {
        (void) munmap((void*) data.data, data.size);
    }
    shmoo_vector_free(&in->head.part);
    free(in);
    *inp = 0;

    return 1;
}

uint64_t
_input_pipe_more (shmoo_input_t* _in, uint64_t want) {
    _input_pipe_t*  in   = (_input_pipe_t*) _in;
    uint8_t*        mnew = MAP_FAILED;
    uint8_t*        more = 0;
    uint64_t        used = 0;
    uint64_t        done = 0;
    uint64_t        size = 0;
    int             got  = -1;

#ifdef _USE_GNU
    uint8_t*        mold = 0;
    shmoo_cursor_t* last = 0;
    shmoo_cursor_t  curs = {
        .from = 0,
        .size = 0,
        .data = 0,
    };
    if ((last = shmoo_vector_tail(&in->head.part))) {
        /* Try to join this memory blob directly to the last one. */
        mold = (uint8_t*) last->data;
        mnew = mremap(mold, last->size, (last->size + want), MREMAP_MAYMOVE);
        if (MAP_FAILED == mnew) {
            last = 0;
        } else {
            last->data = mnew;
            more = (mnew + last->size);
            size = (last->size + want);
        }
    }
#endif
    if (MAP_FAILED == mnew) {
        /* If this is the first allocation, or we did not join a
         * new allocation to the last one taken, make a new one.
         */
        mnew = mmap(0, want, (PROT_READ|PROT_WRITE), MAP_ANONYMOUS, -1, 0);
//        mnew = mmap(0, want, (PROT_READ|PROT_WRITE), MAP_ANON, -1, 0);
        more = mnew;
        size = want;
    }
    if (MAP_FAILED == mnew) {
        return 0;
    }

    /* Read into the newly-allocated buffer. */
    for (used = 0; used < want; ) {
        got = read(in->desc, (more + used), (want - used));
        if (got > 0) {
            used += got;
        } else {
            break;
        }
    }

    if (! used) {
        /* Therefore: last; we remapped to get an extra page, then
         * didn't read any bytes from the descriptor.  Unmap the
         * added memory chunk.
         */
        size = (size + want);
        done = (size + used);
    } else {
        /* THerefore: used; chop the end off the mapping, as needed. */
        size = want;
        done = used;
    }

    /* Chop the remapping down to size; presumably this is all we get. */
#ifdef _USE_GNU
    if (used < want) {
        uint8_t* mrem = mremap(mnew, size, done, MREMAP_MAYMOVE);
        if (MAP_FAILED != mrem) {
            mnew = mrem;
        }
    }

    if (last) {
        last->data = mnew;
        last->size = done;
    } else
#endif
    {
        shmoo_cursor_t made = {
            .from = in->head.size,
            .data = mnew,
            .size = done,
        };
        if (! shmoo_vector_insert_tail(&in->head.part, &made)) {
            goto error;
        }
    }
    in->head.size += used;

    return used;

error:
    (void) munmap(mnew, want);
    return 0;
}

int
_input_unix_open_path (shmoo_input_t** inp, shmoo_string_t path, const struct stat* info) {
    const struct sockaddr* addr = 0;
    size_t                 size = 0;
    socklen_t              alen = 0;
    socklen_t              plen = 0;
    int                    desc = -1;
    _input_unix_t*         in   = 0;

    if ((path.size + 1) >= sizeof(in->addr.sun_path)) {
        size = (sizeof(*in) + ((path.size + 1) - sizeof(in->addr.sun_path)));
    } else {
        size = sizeof(*in);
    }
    if (! (in = calloc(1, size))) {
        return 0;
    } else if (! shmoo_vector_init(&in->head.part, &_input_part_vector_type)) {
        goto error;
    }
    in->head.mtim = time(0);
    in->head.name = (const uint8_t*) in->addr.sun_path;
    in->addr.sun_family = AF_UNIX;
    memcpy(in->addr.sun_path, path.data, path.size);
    addr = (const struct sockaddr*) &in->addr;

    alen = (offsetof(struct sockaddr_un, sun_path) + path.size + 1);
    plen = sizeof(in->peer);
    desc = socket(AF_UNIX, SOCK_STREAM, 0);
    if (-1 == desc) {
        goto error;
    } else if (-1 == connect(desc, addr, alen)) {
        goto error;
    } else if (-1 == shutdown(desc, SHUT_WR)) {
        goto error;
    } else if (-1 == getpeername(desc, (struct sockaddr*) &in->peer, &plen)) {
        goto error;
    }
    in->alen = alen;
    in->plen = plen;
    in->desc = desc;
    *inp     = &in->head;
    return 1;

error:
    if (-1 != desc) {
        (void) close(desc);
    }
    free(in);
    return 0;

    (void) info;
}

int
_input_sock_open_desc (shmoo_input_t** inp, shmoo_handle_t desc, const struct stat* info) {
    _input_sock_t*      in = 0;
    struct sockaddr     addr;
    socklen_t           alen;

    alen = sizeof(addr);
    if (-1 == getsockname(desc, &addr, &alen)) {
        return 0;
    }

    if (addr.sa_family == AF_UNIX) {
        return _input_unix_open_desc(inp, desc, info);
//    } else if (addr.sa_family == AF_INET) {
//    } else if (addr.sa_family == AF_INET6) {
    }

    return 1;

    (void) in;
}

int
_input_unix_open_desc (shmoo_input_t** inp, shmoo_handle_t desc, const struct stat* info) {
    _input_unix_t*     in   = 0;
    size_t             size = 0;
    struct sockaddr_un abuf;
    socklen_t          alen = sizeof(abuf);
    socklen_t          plen = sizeof(abuf);

    if (-1 == getsockname(desc, (struct sockaddr*) &abuf, &alen)) {
        return 0;
    } else if (alen >= sizeof(abuf)) {
        size = (sizeof(*in) + (alen - sizeof(abuf) + 1));
    } else {
        size = sizeof(*in);
    }

    if (! (in = calloc(1, size))) {
        return 0;
    } else if (-1 == getsockname(desc, (struct sockaddr*) &in->addr, &alen)) {
        goto error;
    } else if (-1 == getpeername(desc, (struct sockaddr*) &in->peer, &plen)) {
        goto error;
    } else if (! shmoo_vector_init(&in->head.part, &_input_part_vector_type)) {
        goto error;
    }
    in->head.type = &_input_unix_type;
    in->head.mtim = time(0);
    in->head.name = (const uint8_t*) in->addr.sun_path;
    in->alen = alen;
    in->plen = plen;
    *inp = &in->head;
    return 1;

error:
    free(in);
    return 0;

    (void) info;
}

#define TBNAME "text_buffer"
int
_input_text_open_text (shmoo_input_t** inp, shmoo_string_t text, ...) {
    const shmoo_origin_t* orig = 0;
    _input_text_t*        in   = 0;
    size_t                size = (sizeof(*in) + sizeof(TBNAME));
    uint8_t*              data = MAP_FAILED;
    va_list               args;

    if (! inp) {
        return 0;
    } else if (! (in = calloc(1, size))) {
        return 0;
    } else if (! shmoo_vector_init(&in->head.part, &_input_part_vector_type)) {
        goto error;
    }
    in->head.type = &_input_text_type;
    in->head.size = text.size;
    in->head.mtim = time(0);
    in->head.name = (const uint8_t*) &in[1];
    memcpy(&in[1], TBNAME, sizeof(TBNAME));

    data = mmap(0, (text.size + 1), (PROT_READ|PROT_WRITE), MAP_ANONYMOUS, -1, 0);
//    data = mmap(0, (text.size + 1), (PROT_READ|PROT_WRITE), MAP_ANON, -1, 0);
    if (MAP_FAILED == data) {
        goto error;
    }
    memcpy(data, text.data, text.size);
    data[text.size] = '\0';
    text.data = data;

    if (! shmoo_vector_insert_tail(&in->head.part, &text)) {
        goto error;
    }

    va_start(args, text);
    orig = va_arg(args, const shmoo_origin_t*);
    va_end(args);
    in->orig = orig;

    *inp = &in->head;

    return 1;

error:
    if (MAP_FAILED != data) {
        (void) munmap((void*) data, (text.size + 1));
    }
    free(in);
    return 0;
}

uint64_t
_input_file_more (shmoo_input_t* _in, uint64_t want) {
    _input_file_t*  in   = (_input_file_t*) _in;
    uint8_t*        mnew = MAP_FAILED;
    uint8_t*        more = 0;
    uint64_t        used = 0;
    uint64_t        done = 0;
    uint64_t        size = 0;
    int             got  = -1;

#ifdef _USE_GNU
    uint8_t*        mold = 0;
    shmoo_cursor_t* last = 0;
    shmoo_cursor_t  curs = {
        .from = 0,
        .size = 0,
        .data = 0,
    };
    if ((last = shmoo_vector_tail(&in->head.part))) {
        /* Try to join this memory blob directly to the last one. */
        mold = (uint8_t*) last->data;
        mnew = mremap(mold, last->size, (last->size + want), MREMAP_MAYMOVE);
        if (MAP_FAILED == mnew) {
            last = 0;
        } else {
            last->data = mnew;
            more = (mnew + last->size);
            size = (last->size + want);
        }
    }
#endif
    if (MAP_FAILED == mnew) {
        /* If this is the first allocation, or we did not join a
         * new allocation to the last one taken, make a new one.
         */
        mnew = mmap(0, want, (PROT_READ|PROT_WRITE), MAP_ANONYMOUS, -1, 0);
//        mnew = mmap(0, want, (PROT_READ|PROT_WRITE), MAP_ANON, -1, 0);
        more = mnew;
        size = want;
    }
    if (MAP_FAILED == mnew) {
        return 0;
    }

    /* Read into the newly-allocated buffer. */
    for (used = 0; used < want; ) {
        got = fread((more + used), (want - used), 1, in->file);
        if (got > 0) {
            used += got;
        } else {
            break;
        }
    }

    if (! used) {
        /* Therefore: last; we remapped to get an extra page, then
         * didn't read any bytes from the descriptor.  Unmap the
         * added memory chunk.
         */
        size = (size + want);
        done = (size + used);
    } else {
        /* THerefore: used; chop the end off the mapping, as needed. */
        size = want;
        done = used;
    }

#ifdef _USE_GNU
    /* Chop the remapping down to size; presumably this is all we get. */
    if (used < want) {
        uint8_t* mrem = mremap(mnew, size, done, MREMAP_MAYMOVE);
        if (MAP_FAILED != mrem) {
            mnew = mrem;
        }
    }

    if (last) {
        last->data = mnew;
        last->size = done;
    } else
#endif
    {
        shmoo_cursor_t made = {
            .from = in->head.size,
            .data = mnew,
            .size = done,
        };
        if (! shmoo_vector_insert_tail(&in->head.part, &made)) {
            goto error;
        }
    }
    in->head.size += used;

    return used;

error:
    (void) munmap(mnew, want);
    return 0;
}

int
_input_file_shut (shmoo_input_t* _in) {
    _input_file_t*  in = (_input_file_t*) _in;
    if (! in) {
        return 0;
    } else if (in->file) {
        fclose(in->file);
        in->file = 0;
    }
    return 1;
}

int
_input_file_dest (shmoo_input_t** inp) {
    _input_file_t*  in;
    shmoo_cursor_t  curs;

    if (! inp || ! *inp) {
        return 0;
    }
    in = *((_input_file_t**) inp);
    if (in->file) {
        (void) fclose(in->file);
    }
    while ((shmoo_vector_remove_tail(&in->head.part, &curs))) {
        (void) munmap((void*) curs.data, curs.size);
    }
    shmoo_vector_free(&in->head.part);
    free(in);
    *inp = 0;
    return 1;
}

