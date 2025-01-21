#ifndef _shmoo_parser_h
#define _shmoo_parser_h

#include <shmoo/compile.h>

__CDECL_BEG

/*forward*/ struct _shmoo_parser_type;
typedef     struct _shmoo_parser_type shmoo_parser_type_t;
/*forward*/ struct _shmoo_parser;
typedef     struct _shmoo_parser      shmoo_parser_t;

#include <shmoo/char.h>
#include <shmoo/line.h>
#include <shmoo/vector.h>
#include <shmoo/token.h>
#include <shmoo/input.h>

typedef int (*shmoo_read_char_f) (shmoo_parser_t*, shmoo_char_t*, size_t*);
typedef int (*shmoo_read_line_f) (shmoo_parser_t*, shmoo_line_t**);

struct _shmoo_parser_type {
    const shmoo_read_char_f     read_char;
    const shmoo_read_line_f     read_line;
};

struct _shmoo_parser {
    const shmoo_parser_type_t*  type;
    shmoo_vector_t              token_stack;
    shmoo_vector_t              token_saved;
    shmoo_vector_t              state_stack;
    shmoo_vector_t              state_saved;
    shmoo_vector_t              input_saved;
    shmoo_vector_t              lines_saved;
};

extern int shmoo_parser_read_char (
    shmoo_parser_t*             par, 
    shmoo_char_t*               charp,
    size_t*                     sizep
);

extern int shmoo_parser_back_char (
    shmoo_parser_t*             par,
    size_t                      size
);

extern int shmoo_parser_make_token (
    const shmoo_parser_t*       par,
    shmoo_token_t**             tokp
);

extern int shmoo_parser_push_token (
    shmoo_parser_t*             par,
    shmoo_token_t*              tok
);

extern int shmoo_parser_drop_token (
    shmoo_parser_t*             par,
    shmoo_token_t**             tokp
);

extern int shmoo_parser_save_token (
    shmoo_parser_t*             par,
    shmoo_token_t*              tok
);

extern int shmoo_parser_back_token (
    shmoo_parser_t*             par,
    shmoo_token_t*              tok
);

extern int shmoo_parser_push_input (
    shmoo_parser_t*             par,
    shmoo_input_t*              inp
);

extern int shmoo_parser_drop_input (
    shmoo_parser_t*             par,
    shmoo_input_t**             inpp
);

extern int shmoo_parser_save_input (
    shmoo_parser_t*             par,
    shmoo_input_t*              inp
);

__CDECL_END

#endif /* ! _shmoo_parser_h */
