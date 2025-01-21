struct _makefile_subst_token {
    shmoo_token_t           head;
    const shmoo_token_t*    open;
    const shmoo_token_t*    expr;
    const shmoo_token_t*    shut;
    shmoo_vector_t          more;
};

void
_makefile_dest_subst_token (
    _makefile_token_t**     tokep
    )
{
    _makefile_subst_token_t*    toke = (_makefile_subst_token_t*) *tokep;
    _makefile_substmod_token_t* part = NULL;
    while (! shmoo_token_remove_tail(&toke->more, &part)) {
        _makefile_dest_token(&part);
    }
    shmoo_vector_free(&toke->more);
    free(toke);
    *tokep = NULL;
}

static _makefile_token_type_t _makefile_subst_token_type = {
    .name = "makefile_subst",
    .dest = _makefile_dest_subst_token,
};

int
_makefile_make_subst_token (
    _makefile_state_t*      state,
    _makefile_token_t**     tokep,
    _makefile_token_type_t* type
    )
{
    _makefile_subst_token_t* toke = NULL;
    int                      err  = 0;
    err = shmoo_token_make(
        (shmoo_state_t*) state,
        (shmoo_token_t**) &toke,
        type, sizeof(*toke)
    );
    if (! err) {
        if ((err = shmoo_vector_init(&toke->mods, &_makefile_token_vector_type))) {
            _makefile_dest_token(&toke);
        } else {
            *tokep = (_makefile_token_t*) toke;
        }
    }
    return err;
}

#define _gnumake_directive(ctxt,state,tokep,...) \
    (__gnumake_directive((ctxt),(state),(tokep),##__VA_ARGS__,(const char*)NULL))
#define _gnumake_lhs_word(ctxt,state,tokep) \
    (__gnumake_word((ctxt),(state),(tokep),":=#"))
#define _gnumake_rhs_word(ctxt,state,tokep) \
    (__gnumake_word((ctxt),(state),(tokep),""))
#define _gnumake_fun_word(ctxt,state,tokep) \
    (__gnumake_word((ctxt),(state),(tokep),","))
#define _gnumake_dep_word(ctxt,state,tokep) \
    (__gnumake_word((ctxt),(state),(tokep),";:|#"))

static _makefile_token_type_t _gnumake_single_quote_op_token_type = {
    .name = "gnumake_single_quote_op",
    .dest = _makefile_dest_token,
};

static _makefile_token_type_t _gnumake_double_quote_op_token_type = {
    .name = "gnumake_double_quote_op",
    .dest = _makefile_dest_token,
};

static _makefile_token_type_t _gnumake_system_quote_op_token_type = {
    .name = "gnumake_system_quote_op",
    .dest = _makefile_dest_token,
};

static _makefile_token_type_t _gnumake_cstyle_quote_op_token_type = {
    .name = "gnumake_cstyle_quote_op",
    .dest = _makefile_dest_token,
};

static _makefile_token_type_t _gnumake_locale_quote_op_token_type = {
    .name = "gnumake_locale_quote_op",
    .dest = _makefile_dest_token,
};

static _makefile_token_type_t _gnumake_escape_quote_op_token_type = {
    .name = "gnumake_escape_quote_op",
    .dest = _makefile_dest_token,
};

static _makefile_token_type_t _gnumake_pipe_op_token_type = {
    .name = "gnumake_pipe_op",
    .dest = _makefile_dest_token,
};

static _makefile_token_type_t _gnumake_semicolon_op_token_type = {
    .name = "gnumake_semicolon_op",
    .dest = _makefile_dest_token,
};

/* &: */
static _makefile_token_type_t _gnumake_ampersand_colon_op_token_type = {
    .name = "gnumake_ampersand_colon_op",
    .dest = _makefile_dest_token,
};

/* : */
static _makefile_token_type_t _gnumake_single_colon_op_token_type = {
    .name = "gnumake_single_colon_op",
    .dest = _makefile_dest_token,
};

/* :: */
static _makefile_token_type_t _gnumake_double_colon_op_token_type = {
    .name = "gnumake_double_colon_op",
    .dest = _makefile_dest_token,
};

/* = */
static _makefile_token_type_t _gnumake_delay_reset_assign_op_token_type = {
    .name = "gnumake_delay_reset_assign_op",
    .dest = _makefile_dest_token,
};

/* :=, ::= */
static _makefile_token_type_t _gnumake_immed_reset_assign_op_token_type = {
    .name = "gnumake_immed_reset_assign_op",
    .dest = _makefile_dest_token,
};

/* += */
static _makefile_token_type_t _gnumake_delay_after_assign_op_token_type = {
    .name = "gnumake_delay_after_assign_op",
    .dest = _makefile_dest_token,
};

/* +:= */
static _makefile_token_type_t _gnumake_immed_after_assign_op_token_type = {
    .name = "gnumake_immed_after_assign_op",
    .dest = _makefile_dest_token,
};

/* ?= */
static _makefile_token_type_t gnumakee_delay_ifnot_assign_op_token_type = {
    .name = "gnumake_delay_ifnot_assign_op",
    .dest = _makefile_dest_token,
};

/* ?:= */
static _makefile_token_type_t gnumakee_immed_ifnot_assign_op_token_type = {
    .name = "gnumake_immed_ifnot_assign_op",
    .dest = _makefile_dest_token,
};

/* ^= */
static _makefile_token_type_t _gnumake_delay_prior_assign_op_token_type = {
    .name = "gnumake_delay_prior_assign_op",
    .dest = _makefile_dest_token,
};

/* ^:= */
static _makefile_token_type_t _gnumake_immed_prior_assign_op_token_type = {
    .name = "gnumake_immed_prior_assign_op",
    .dest = _makefile_dest_token,
};

/* != */
static _makefile_token_type_t _gnumake_immed_shell_assign_op_token_type = {
    .name = "gnumake_immed_shell_assign_op",
    .dest = _makefile_dest_token,
};

/* @= */
static _makefile_token_type_t _gnumake_delay_macro_assign_op_token_type = {
    .name = "gnumake_delay_macro_assign_op",
    .dest = _makefile_dest_token,
};

/* ... */
static _makefile_token_type_t _gnumake_literal_token_type = {
    .name = "gnumake_literal",
    .dest = _makefile_dest_token,
};

/* '...' */
static _makefile_token_type_t _gnumake_single_quoted_token_type = {
    .name = "gnumake_single_quoted",
    .dest = _makefile_dest_token,
};

/* "..." */
static _makefile_token_type_t _gnumake_double_quoted_token_type = {
    .name = "gnumake_double_quoted",
    .dest = _makefile_dest_token,
};

/* `...` */
static _makefile_token_type_t _gnumake_system_quoted_token_type = {
    .name = "gnumake_system_quoted",
    .dest = _makefile_dest_token,
};

/* $'...' */
static _makefile_token_type_t _gnumake_cstyle_quoted_token_type = {
    .name = "gnumake_cstyle_quoted",
    .dest = _makefile_dest_token,
};

/* \... */
static _makefile_token_type_t _gnumake_escape_quoted_token_type = {
    .name = "gnumake_escape_quoted",
    .dest = _makefile_dest_token,
};

/* $"..." */
static _makefile_token_type_t _gnumake_locale_quoted_token_type = {
    .name = "gnumake_locale_quoted",
    .dest = _makefile_dest_token,
};

static _makefile_token_type_t _gnumake_assign_lhs_token_type = {
    .name = "gnumake_assign_lhs",
    .dest = _makefile_dest_token,
};

static _makefile_token_type_t _gnumake_assign_rhs_token_type = {
    .name = "gnumake_assign_rhs",
    .dest = _makefile_dest_token,
};

static _makefile_token_type_t _gnumake_orderonly_depends_token_type = {
    .name = "gnumake_orderonly_depends",
    .dest = _makefile_dest_token,
};

static _makefile_token_type_t _gnumake_depends_token_type = {
    .name = "gnumake_depends",
    .dest = _makefile_dest_token,
};

static _makefile_token_type_t _gnumake_targets_token_type = {
    .name = "gnumake_targets",
    .dest = _makefile_dest_token,
};

static _makefile_token_type_t _gnumake_if_token_type = {
    .name = "gnumake_if_directive",
    .dest = _makefile_dest_token,
};

static _makefile_token_type_t _gnumake_ifeq_token_type = {
    .name = "gnumake_ifeq_directive",
    .dest = _makefile_dest_token,
};

static _makefile_token_type_t _gnumake_ifneq_token_type = {
    .name = "gnumake_ifneq_directive",
    .dest = _makefile_dest_token,
};

static _makefile_token_type_t _gnumake_ifdef_token_type = {
    .name = "gnumake_ifdef_directive",
    .dest = _makefile_dest_token,
};

static _makefile_token_type_t _gnumake_ifndef_token_type = {
    .name = "gnumake_ifndef_directive",
    .dest = _makefile_dest_token,
};

static _makefile_token_type_t _gnumake_else_token_type = {
    .name = "gnumake_else_directive",
    .dest = _makefile_dest_token,
};

static _makefile_token_type_t _gnumake_endif_token_type = {
    .name = "gnumake_endif_directive",
    .dest = _makefile_dest_token,
};

static _makefile_token_type_t _gnumake_define_token_type = {
    .name = "gnumake_define_directive",
    .dest = _makefile_dest_token,
};

static _makefile_token_type_t _gnumake_endef_token_type = {
    .name = "gnumake_endef_directive",
    .dest = _makefile_dest_token,
};

static _makefile_token_type_t _gnumake_undefine_token_type = {
    .name = "gnumake_undefine_directive",
    .dest = _makefile_dest_token,
};

static _makefile_token_type_t _gnumake_export_token_type = {
    .name = "gnumake_export_directive",
    .dest = _makefile_dest_token,
};

static _makefile_token_type_t _gnumake_unexport_token_type = {
    .name = "gnumake_unexport_directive",
    .dest = _makefile_dest_token,
};

static _makefile_token_type_t _gnumake_readonly_token_type = {
    .name = "gnumake_readonly_directive",
    .dest = _makefile_dest_token,
};

static _makefile_token_type_t _gnumake_override_token_type = {
    .name = "gnumake_override_directive",
    .dest = _makefile_dest_token,
};

static _makefile_token_type_t _gnumake_private_token_type = {
    .name = "gnumake_private_directive",
    .dest = _makefile_dest_token,
};

static _makefile_token_type_t _gnumake_include_token_type = {
    .name = "gnumake_include_directive",
    .dest = _makefile_dest_token,
};

static _makefile_token_type_t _gnumake__include_token_type = {
    .name = "gnumake_-include_directive",
    .dest = _makefile_dest_token,
};

static _makefile_token_type_t _gnumake_load_token_type = {
    .name = "gnumake_load_directive",
    .dest = _makefile_dest_token,
};

static _makefile_token_type_t _gnumake__load_token_type = {
    .name = "gnumake_-load_directive",
    .dest = _makefile_dest_token,
};

static _makefile_token_type_t _gnumake_scope_token_type = {
    .name = "gnumake_scope_directive",
    .dest = _makefile_dest_token,
};

static _makefile_token_type_t _gnumake_endscope_token_type = {
    .name = "gnumake_endscope_directive",
    .dest = _makefile_dest_token,
};

static _makefile_token_type_t _gnumake_subdir_token_type = {
    .name = "gnumake_subdir_directive",
    .dest = _makefile_dest_token,
};

static _makefile_token_type_t _gnumake_wordlist_token_type = {
    .name = "gnumake_wordlist",
    .dest = _makefile_dest_token,
};

static _makefile_token_type_t _gnumake_linelist_token_type = {
    .name = "gnumake_linelist",
    .dest = _makefile_dest_token,
};

static _makefile_token_type_t _gnumake_loadlist_token_type = {
    .name = "gnumake_loadlist",
    .dest = _makefile_dest_token,
};

static _makefile_token_type_t _gnumake_assignment_token_type = {
    .name = "gnumake_assignment",
    .dest = _makefile_dest_token,
};

static _makefile_token_type_t _gnumake_definition_token_type = {
    .name = "gnumake_definition",
    .dest = _gnumake_dest_definition_token,
};

static _makefile_token_type_t _gnumake_delimited_token_type = {
    .name = "gnumake_delimited",
    .dest = _makefile_dest_token,
};

static _makefile_token_type_t _gnumake_loadentry_token_type = {
    .name = "gnumake_loadentry",
    .dest = _makefile_dest_token,
};

static _makefile_token_type_t _gnumake_word_token_type = {
    .name = "gnumake_word",
    .dest = _makefile_dest_token,
};

static _makefile_token_type_t _gnumake_pattern_token_type = {
    .name = "gnumake_pattern",
    .dest = _makefile_dest_token,
};

static _makefile_token_type_t _gnumake_static_pattern_rule_token_type = {
    .name = "gnumake_static_pattern_rule",
    .dest = _makefile_dest_token,
};

static _makefile_token_type_t _gnumake_single_colon_rule_token_type = {
    .name = "gnumake_single_colon_rule",
    .dest = _makefile_dest_token,
};

static _makefile_token_type_t _gnumake_double_colon_rule_token_type = {
    .name = "gnumake_double_colon_rule",
    .dest = _makefile_dest_token,
};

static _makefile_token_type_t _gnumake_charsub_token_type = {
    .name = "gnumake_charsub",
    .dest = _makefile_dest_token,
};


/*forward*/ struct _gnumake_directive_map;
typedef     struct _gnumake_directive_map _gnumake_directive_map_t;

typedef int (*_gnumake_directive_f) (
    _makefile_context_t*    __ctxt,
    _makefile_state_t*      __state,
    _makefile_token_t**     __tokep
);

struct _gnumake_directive_map {
    const char*             name;
    _gnumake_directive_f    func;
};

/*forward*/ struct _gnumake_directive_token;
typedef     struct _gnumake_directive_token _gnumake_directive_token_t;

struct _gnumake_directive_token {
    shmoo_token_t           head;
    const shmoo_token_t*    keyw;
    const shmoo_token_t*    nest;
    const shmoo_token_t*    oper;
};

int
_gnumake_make_directive_token (
    _makefile_state_t*              state,
    _makefile_token_t**             tokep,
    _makefile_token_type_t*         type
    )
{
    _gnumake_definition_token_t* toke = NULL;
    int                          err  = 0;
    err = shmoo_token_make(
        (shmoo_state_t*) state,
        (shmoo_token_t**) &toke,
        type, sizeof(*toke)
    );
    if (! err) {
        *tokep = toke;
    }
    return err;
}

struct _gnumake_assignment_token {
    shmoo_token_t               head;
    const _makefile_token_t*    name;
    const _makefile_token_t*    oper;
    const _makefile_token_t*    data;
};

int
_gnumake_make_assignment_token (
    _makefile_state_t*              state,
    _makefile_token_t**             tokep
    )
{
    _gnumake_assignment_token_t* toke = NULL;
    int                          err  = 0;
    err = shmoo_token_make(
        (shmoo_state_t*) state,
        (shmoo_token_t**) &toke,
        &_gnumake_assignment_token_type,
        sizeof(*toke)
    );
    if (! err) {
        *tokep = (_gnumake_assignment_token_t*) toke;
    }
    return err;
}

static const shmoo_vector_type_t _gnumake_line_vector_type = {
    .name = "gnumake_line_vector",
    .elem = sizeof(shmoo_state_t*),
};

struct _gnumake_definition_token {
    shmoo_token_t               head;
    const shmoo_token_t*        name;
    const shmoo_token_t*        oper;
    shmoo_vector_t              line;
};

void
_gnumake_dest_definition_token (
    _gnumake_destination_token_t**  tokep
    )
{
    if (*tokep) {
        shmoo_state_t* line = NULL;
        while (! shmoo_vector_remove_tail(&(*tokep)->line, &line)) {
            shmoo_state_dest(&line);
        }
        shmoo_vector_free(&(*tokep)->line);
        shmoo_token_dest((shmoo_token_t**) tokep);
    }
}

int
_gnumake_make_definition_token (
    _makefile_state_t*              state,
    _makefile_token_t**             tokep
    )
{
    _gnumake_definition_token_t* toke = NULL;
    int                          err  = 0;
    err = shmoo_token_make(
        (shmoo_state_t*) state,
        (shmoo_token_t**) &toke,
        &_gnumake_definition_token_type,
        sizeof(*toke)
    );
    if (! err) {
        if ((err = shmoo_vector_init(&toke->line, &_gnumake_line_vector_type))) {
            goto error;
        }
        *tokep = (_makefile_token_t*) toke;
    }

error:
    _gnumake_dest_definition_token(&toke);
    return err;
}

struct _gnumake_delimited_token {
    shmoo_token_t               head;
    const _makefile_token_t*    open;
    const _makefile_token_t*    wrap;
    const _makefile_token_t*    shut;
};

int
_gnumake_make_delimited_token (
    _makefile_state_t*              state,
    _makefile_token_t**             tokep
    )
{
    _gnumake_delimited_token_t* toke = NULL;
    int                         err  = 0;
    err = shmoo_token_make(
        (shmoo_token_t*) state,
        (shmoo_token_t**) &toke,
        &_gnumake_delimited_token_type,
        sizeof(*toke)
    );
    if (! err) {
        *tokep = (shmoo_token_t*) toke;
    }
    return err;
}

struct _gnumake_loadentry_token {
    shmoo_token_t               head;
    const _makefile_token_t*    file;
    const _makefile_token_t*    stem;
    const _makefile_token_t*    path;
    const _makefile_token_t*    bind;
};

int
_gnumake_make_loadentry_token (
    _makefile_state_t*              state,
    _makefile_token_t**             tokep
    )
{
    _gnumake_loadentry_token_t* toke = NULL;
    int                         err  = 0;

    err = shmoo_token_make(
        (shmoo_state_t*) state,
        (shmoo_token_t**) &toke,
        &_gnumake_loadentry_token_type,
        sizeof(*toke)
    );
    if (! err) {
        *tokep = (_gnumake_loadentry_token_t*) toke;
    }
    return err;
}

struct _gnumake_charsub_token = {
    shmoo_token_t           head;
    _makefile_char_t        real;
};

int
_gnumake_make_charsub_token (
    _makefile_state_t*              state,
    _makefile_token_t**             tokep
    )
{
    _makefile_charsub_token_t* toke = NULL;
    int                        err  = 0;
    err = shmoo_token_make(
        (shmoo_state_t*) state,
        (shmoo_token_t**) &toke,
        &_gnumake_charsub_token_type, 
        sizeof(*toke)
    );
    if (! err) {
        *tokep = toke;
    }
    return err;
}

struct _gnumake_charvar_token {
    shmoo_token_t       head;
    _makefile_char_t    name;
};

int
_gnumake_make_charvar_token (
    _makefile_state_t*              state,
    _makefile_token_t**             tokep
    )
{
    _gnumake_charvar_token_t* toke = NULL;
    int                       err  = 0;
    err = shmoo_token_make(
        (shmoo_state_t*) state,
        (shmoo_token_t**) &toke,
        &_gnumake_charvar_token_type, 
        sizeof(*toke)
    );
    if (! err) {
        *tokep = toke;
    }
    return err;
}

struct _gnumake_autovar_token {
    shmoo_token_t       head;
    _makefile_char_t    name;
    _gnumake_autovar_f  func;
};

int
_gnumake_make_autovar_token (
    _makefile_state_t*          state,
    _makefile_token_t**         tokep
    )
{
    _makefile_autovar_token_t* toke = NULL;
    int                        err  = 0;
    err = shmoo_token_make(
        (shmoo_state_t*) state,
        (shmoo_token_t**) &toke,
        &_gnumake_autovar_token_type,
        sizeof(*toke)
    );
    if (! err) {
        *tokep = toke;
    }
    return err;
}

static const _gnumake_directive_map_t _gnumake_directives[] = {
    { "if",         _gnumake_if       },
    { "ifeq",       _gnumake_ifeq     },
    { "ifneq",      _gnumake_ifneq    },
    { "ifdef",      _gnumake_ifdef    },
    { "ifndef",     _gnumake_ifndef   },
    { "else",       _gnumake_else     },
    { "endif",      _gnumake_endif    },
    { "define",     _gnumake_define   },
    { "endef",      _gnumake_endef    },
    { "undefine",   _gnumake_undefine },
    { "export",     _gnumake_export   },
    { "unexport",   _gnumake_export   },
    { "override",   _gnumake_override },
    { "private",    _gnumake_private  },
    { "include",    _gnumake_include  },
    { "-include",   _gnumake__include },
    { "sinclude",   _gnumake__include },
    { "load",       _gnumake_load     },
    { "-load",      _gnumake__load    },
    { "scope",      _gnumake_scope    },
    { "endscope",   _gnumake_endscope },
    { "subdir",     _gnumake_subdir   },
    { "readonly",   _gnumake_readonly },
};

_gnumake_directive_f
_gnumake_find_directive (
    _makefile_context_t*            ctxt,
    _makefile_state_t*              state,
    const char*                     name
    )
{
    _gnumake_directive_f func = NULL;
    shmoo_hash_lookup(&state->dirs, name, &func);
    return func;
}

int
_gnumake_read (
    _makefile_context_t*            ctxt
    )
{
    _makefile_state_t* file_state = NULL;
    _makefile_state_t* line_state = NULL;
    _makefile_token_t* line_token = NULL;
    int                err        = 0;

    for ( ; ; ) {
        /* Every line starts by grabbing the current input
         * state from the makefile context.  This is because
         * any line could potentially push a new input to
         * the input stack, and the parse must continue
         * with that new input.
         */
        if ((err = _makefile_cur_state(ctxt, &file_state))) {
            /* Error, or no more input states to parse from. */
            break;
        } else if ((err = _makefile_read_line(ctxt, file_state, &line_token))) {
            _makefile_state_t* next_state = NULL;
            if (err != EOF) {
                goto error;
            }
            /* EOF; pop this state and loop to go back
             * to a parse potentially already in flight.
             * Apply the current file's state to the next
             * file state on the stack, or to the context
             * if this is the final file to parse.
             */
            if ((err = _makefile_pop_state(ctxt, &file_state))) {
                goto error;
            }
            (void) _makefile_context_cur_state(ctxt, &next_state);
            if ((err = _makefile_keep_state(ctxt, next_state, &file_state))) {
                goto error;
            }
            break;
        } else if ((err = _gnumake_parse_line(ctx, file_state, &line_token))) {
            goto error;
        } else {
            file_state = NULL;
            line_token = NULL;
        }
    }

    return 0;

error:
    _makefile_dest_token(&line_token);
    _makefile_dest_state(&file_state);
    return err;
}

int
_gnumake_assign_op (
    _makefile_context_t*            ctxt,
    _makefile_state_t*              state,
    _makefile_token_t**             tokep
    )
{
    _makefile_token_t*  toke = NULL;
    _makefile_size_t    used = 0;
    _makefile_char_t    cval;
    _makefile_size_t    csiz;
    int                 type;
    int                 err;

    if (tokep) {
        if ((err = _makefile_make_token(state, &toke, NULL))) {
            return err;
        }
    }

#define _MAKEFILE_OP_ASSIGN_DELAY   0x0000
#define _MAKEFILE_OP_ASSIGN_IMMED   0x0001
#define _MAKEFILE_OP_ASSIGN_RESET   0x0000
#define _MAKEFILE_OP_ASSIGN_PRIOR   0x0002
#define _MAKEFILE_OP_ASSIGN_AFTER   0x0004
#define _MAKEFILE_OP_ASSIGN_SHELL   0x0008
#define _MAKEFILE_OP_ASSIGN_MACRO   0x0010

    do {
        if ((err = _makefile_read_char(state, &cval, &csiz))) {
            goto error;
        } else {
            used += csiz;
        }
        if (cval == '=') {
            type = (_MAKEFILE_OP_ASSIGN_DELAY|_MAKEFILE_OP_ASSIGN_RESET);
            break;
        }
        switch (cval) {
            case ':': type = (_MAKEFILE_OP_ASSIGN_IMMED|_MAKEFILE_OP_ASSIGN_RESET); break;
            case '?': type = (_MAKEFILE_OP_ASSIGN_DELAY|_MAKEFILE_OP_ASSIGN_IFNOT); break;
            case '+': type = (_MAKEFILE_OP_ASSIGN_DELAY|_MAKEFILE_OP_ASSIGN_AFTER); break;
            case '^': type = (_MAKEFILE_OP_ASSIGN_DELAY|_MAKEFILE_OP_ASSIGN_PRIOR); break;
            case '!': type = (_MAKEFILE_OP_ASSIGN_DELAY|_MAKEFILE_OP_ASSIGN_SHELL); break;
            case '@': type = (_MAKEFILE_OP_ASSIGN_DELAY|_MAKEFILE_OP_ASSIGN_MACRO); break;
            default:
                err = ENOENT;
                goto error;
        }

        if ((err = _makefile_read_char(state, &cval, &csiz))) {
            goto error;
        } else {
            used += csiz;
        }
        if (cval == '=') {
            break;
        } else if (cval == ':') {
            if (type & (_MAKEFILE_OP_ASSIGN_MACRO|_MAKEFILE_OP_ASSIGN_SHELL)) {
                err = ENOENT;
                goto error;
            }
            type |= _MAKEFILE_OP_ASSIGN_IMMED;
        } else {
            err = ENOENT;
            goto error;
        }

        if ((err = _makefile_read_char(state, &cval, &csiz))) {
            goto error;
        } else {
            used += csiz;
        }
        if (cval == '=') {
            break;
        } else {
            err = EINVAL;
            goto error;
        }
    } while (0);

    if (tokep) {
        switch (type) {
            case (_MAKEFILE_OP_ASSIGN_DELAY|_MAKEFILE_OP_ASSIGN_RESET):
                _makefile_token_set_type(toke, &_gnumake_delay_reset_assign_op_token_type);
                break;
            case (_MAKEFILE_OP_ASSIGN_IMMED|_MAKEFILE_OP_ASSIGN_RESET):
                _makefile_token_set_type(toke, &_gnumake_immed_reset_assign_op_token_type);
                break;
            case (_MAKEFILE_OP_ASSIGN_DELAY|_MAKEFILE_OP_ASSIGN_PRIOR):
                _makefile_token_set_type(toke, &_gnumake_delay_prior_assign_op_token_type);
                break;
            case (_MAKEFILE_OP_ASSIGN_IMMED|_MAKEFILE_OP_ASSIGN_PRIOR):
                _makefile_token_set_type(toke, &_gnumake_immed_prior_assign_op_token_type);
                break;
            case (_MAKEFILE_OP_ASSIGN_DELAY|_MAKEFILE_OP_ASSIGN_AFTER):
                _makefile_token_set_type(toke, &_gnumake_delay_after_assign_op_token_type);
                break;
            case (_MAKEFILE_OP_ASSIGN_IMMED|_MAKEFILE_OP_ASSIGN_AFTER):
                _makefile_token_set_type(toke, &_gnumake_immed_after_assign_op_token_type);
                break;
            case (_MAKEFILE_OP_ASSIGN_DELAY|_MAKEFILE_OP_ASSIGN_IFNOT):
                _makefile_token_set_type(toke, &_gnumake_delay_ifnot_assign_op_token_type);
                break;
            case (_MAKEFILE_OP_ASSIGN_IMMED|_MAKEFILE_OP_ASSIGN_IFNOT):
                _makefile_token_set_type(toke, &_gnumake_immed_ifnot_assign_op_token_type);
                break;
            case (_MAKEFILE_OP_ASSIGN_IMMED|_MAKEFILE_OP_ASSIGN_SHELL):
                _makefile_token_set_type(toke, &_gnumake_immed_shell_assign_op_token_type);
                break;
            case (_MAKEFILE_OP_ASSIGN_DELAY|_MAKEFILE_OP_ASSIGN_DELAY):
                _makefile_token_set_type(toke, &_gnumake_delay_macro_assign_op_token_type);
                break;
        }

        _makefile_token_add_size(toke, used, 0);
        *tokep = toke;
    }

    return 0;

error:
    _makefile_back_char(state, used);
    if (tokep) {
        _makefile_dest_token(&toke);
    }
    return err;
}

int
_gnumake_rulecolon (
    _makefile_context_t*            ctxt,
    _makefile_state_t*              state,
    _makefile_token_t**             tokep
    )
{
    _makefile_token_type_t* type = NULL;
    _makefile_token_t*      toke = NULL;
    _makefile_size_t        used = 0;
    _makefile_char_t        oval;
    _makefile_char_t        cval;
    _makefile_size_t        csiz;
    int                     err  = 0;

    if (tokep) {
        if ((err = _makefile_make_token(state, &toke, NULL))) {
            return err;
        }
    }

#define _MAKEFILE_TARGET_GROUPED
#define _MAKEFILE_TARGET_TRACED
    if ((err = _makefile_read_char(state, &cval, &csiz))) {
        goto errorssss
    } else if ((cval != ':') && (cval != '&')) {
        err = EINVAL;
        goto error;
    } else {
        type = (
            (cval == '&')
                ? &_gnumake_grouped_colon_token_type
                : &_gnumake_single_colon_token_type
        );
        oval = cval;
    }
    _makefile_token_add_size(toke, csiz, 0);
    if ((err = _makefile_read_char(state, &cval, &csiz))) {
        goto error;
    } else if (cval != ':') {
        if (oval == '&') {
            err = EINVAL;
            goto error;
        }
    } else {
        _makefile_token_add_size(toke, csiz, 0);
        if (oval == ':') {
            type = &_gnumake_double_colon_token_type;
        }
    }

    if (tokep) {
        _makefile_token_set_type(toke, type);
        *tokep = toke;
    } else if ((err = _makefile_back_char(state, used, 0))) {
        goto error;
    }
        
    return 0;

error:
    _makefile_back_char(state, used, 0);
    if (tokep) {
        _makefile_dest_token(&toke);
    }
    return err;
}

int
__gnumake_directive (
    _makefile_context_t*    ctxt,
    _makefile_state_t*      state,
    _makefile_token_t**     tokep,
    ...
    )
{
    _makefile_directive_f   func = NULL;
    _makefile_token_t*      toke = NULL;
    _makefile_token_t*      part = NULL;
    char*                   buff = NULL;
    size_t                  used = 0;
    int                     err  = 0;

    if ((err = _makefile_make_token(state, &toke, &_gnumake_literal_token_type))) {
        return err;
    }

    /* Scan characters into the token. */
    for ( ; ; ) {
        if ((err = _makefile_read_char(state, &cval, &csiz))) {
            goto error;
        } else if (_makefile_isalnum(ctxt, cval) || strchr(".-_", cval)) {
            _makefile_token_add_size(toke, csiz, 0);
        } else if ((err = _makefile_back_char(state, csiz))) {
            goto error;
        } else if (cval != '\\') {
            break;
        } else if ((err = _makefile_continuation(ctxt, state, &part))
                && (err = _makefile_continuation(ctxt, state, &part))
                  )
        {
            break;
        } else if ((err = _makefile_token_add_part(toke, &part))) {
            goto error;
        }
    }

    /* Extract and condition the text of the token. */
    if ((err = _makefile_token_text(toke, NULL, &used))) {
        goto error;
    } else if (! (buff = alloca(used + 1))) {
        err = errno;
        goto error;
    } else if ((err = _makefile_token_text(toke, buff, &used))) {
        goto error;
    } else {
        buff[used] = '\0';
    }

    /* Look up the directive handler; pluggable, so found
     * in the running context of the parse.
     */
    if (! (func = _gnumake_find_directive(ctxt, state, buff))) {
        err = ENOENT;
        goto error;
    } else {
        const char* name = NULL;
        va_list     args;
        va_start(args, tokep);
        for (used = 0; (name = va_arg(args, const char*)); ++used) {
            if (! strcmp(name, buff)) {
                break;
            }
        }
        va_end(args);
        if (used && ! name) {
            err = ENOENT;
            goto error;
        }
    }

    /* Gobble whitespace after possible directive. */
    (void) _makefile_hspace(ctxt, state, NULL);
    /* Look for the next piece to be an assignment
     * operator or a rule colon operator.  If either
     * of these is found, then the word is NOT a
     * directive, but is instead a variable name or
     * maybe a phony target name.
     *
     * NOTE: This is an extension/correction of GNU
     *       Make syntax, which disallows that kind
     *       of usage without some workaround.
     */
    if (! (err = _gnumake_assign_op(ctxt, state, NULL))
     || ! (err = _gnumake_rulecolon(ctxt, state, NULL))
       )
    {
        goto error;
    }

    if (tokep) {
        /* If we sent a pointer to be filled, fill it
         * and keep the parser advanced past the word.
         */
        *tokep = toke;
    } else if ((err = _makefile_back_token(line, toke))) {
        /* Otherwise, this was a token look-ahead, so
         * move the line state back to where we started.
         * If we failed to do that, it is an error.
         */
        goto error;
    } else {
        /* Nuke the token, if we are not returning it. */
        _makefile_dest_token(&toke);
    }
    /* Finally, we found a real directive and it matched
     * any of the names given (if any).  Whether or not we
     * want to keep the parser moved ahead and return the
     * token, we still want to return the handler function.
     */
    return func;

error:
    /* Encountered an error, or did not find a directive,
     * or the directive found was not what we wanted.
     */
    _makefile_back_token(state, toke);
    _makefile_dest_token(&part);
    _makefile_dest_token(&toke);
    
    return NULL;
}

/* GNU Make-compatible line parser */
int
_gnumake_parse_line (
    _makefile_context_t*    ctxt,
    _makefile_state_t*      file_state,
    const shmoo_token_t*    line_token
    )
{
    _makefile_directive_f   func       = NULL;
    _makefile_state_t*      line_state = NULL;
    shmoo_token_t*          action     = NULL;
    int                     err        = 0;

    /* Parse states follow the state of the current top of the
     * context's input state stack.
     */
    if (! (_makefile_skipping(file_state) || _makefile_defining(file_state))) {
        /* If not skipping or defining, try to squirrel this line
         * away as part of a recipe.
         */
        if (! (err = _makefile_recipe(ctxt, file_state, &line_state))) {
            /* Success! */
            return err;
        }
    }

    /* Strip leading whitespace. */
    (void) _makefile_hspace(line_state, NULL, 0);
    /* Given the different states the build configuration
     * could be in currently, look for the right avenues
     * to continue parsing.
     */
    if (_makefile_skipping(file_state, 0)) {
        func = _gnumake_directive(
            ctxt, line_state, NULL,
            "if", "ifeq", "ifneq", "ifdef", "ifndef",
            "else",
            "endif"
        );
        if (func) {
            if ((err = (*func)(ctxt, file_state, &action))) {
                goto error;
            } else if ((err = _makefile_state_apply_token(file_state, action))) {
                goto error;
            } else if ((err = _makefile_state_save_token(file_state, &action))) {
                goto error;
            }
        }
    } else if (_makefile_defining(file_state, 0)) {
        func = _gnumake_directive(
            ctxt, line_state, NULL,
            "define", "endef", "override"
        );
        if (func) {
            if ((err = (*func)(ctxt, file_state, &action))) {
                goto error;
            } else if ((err = _makefile_apply_token(ctxt, file_state, action))) {
                goto error;
            } else if ((err = _makefile_state_save_token(file_state, &action))) {
                goto error;
            }
        } else {
            if ((err = _makefile_define_line(ctxt, file_state, line_state))) {
                goto error;
            }
        }
    } else if ((func = _gnumake_directive(ctxt, line_state, NULL))) {
        if ((err = (*func)(ctxt, file_state, &action))) {
            goto error;
        } else if ((err = _makefile_apply_token(ctxt, file_state, action))) {
            goto error;
        } else if ((err = _makefile_state_save_token(file_state, &action))) {
            goto error;
        }
    } else if (! (err = _gnumake_assignment(ctxt, line_state, &action))) {
    } else if (! (err = _gnumake_rule(ctxt, line_state, &action))) {
    } else {
        /* Should never be here. */
    }

    /* Apply the action token generated by parsing, then
     * save the token so the whole schmear can be dumped
     * out to a more structured log, then picked up and
     * examined postmortem to figure out why the darned
     * build failed...
     */
    if ((err = _makefile_apply_token(ctxt, file_state, action))
     || (err = _makefile_state_save_token(file_state, action))
       )
    {
        goto error;
    }
    action = NULL;
    return 0;

error:
    shmoo_token_dest(&action);
    return err;
}

int
_gnumake_assignment (
    _makefile_context_t*        ctxt,
    _makefile_state_t*          state,
    _makefile_token_t**         tokep
    )
{
    _gnumake_assignment_token_t* toke = NULL;
    _makefile_token_t* part = NULL;
    _makefile_token_t* name = NULL;
    _makefile_token_t* hspc = NULL;
    _makefile_token_t* oper = NULL;
    _makefile_token_t* data = NULL;
    _makefile_token_t* hspc = NULL;
    int                err  = 0;

    if ((err = _gnumake_make_assignment_token(state, &toke))) {
        return err;
    } else if ((err = _makefile_make_token(state, &name, &_makefile_assign_lhs_token))) {
        goto error;
    }

    for ( ; ; ) {
        if ((err = _gnumake_lhs_word(ctxt, state, &part))) {
            break;
        } else if (hspc && (err = _makefile_token_add_part(name, &hspc))) {
            goto error;
        } else if ((err = _makefile_token_add_part(name, &part))) {
            goto error;
        } else if ((err = _makefile_hspace(ctxt, state, &hspc))) {
            break;
        }
    }
    _makefile_dest_token(&hspc);
    toke->name = name;
    if ((err = _makefile_token_add_part(toke, &name))) {
        goto error;
    }

    if ((err = _gnumake_assign_op(ctxt, state, &part))) {
        goto error;
    }
    toke->oper = part;
    if ((err = _makefile_token_add_part((_makefile_token_t*) toke, &part))) {
        goto error;
    }

    if ((err = _makefile_make_token(state, &data, &_gnumake_assign_rhs_token_type))) {
        goto error;
    }

    for ( ; ; ) {
        if ((err = _makefile_hspace(ctxt, state, NULL))) {
            break;
        } else if ((err = _gnumake_lhs_word(ctxt, state, &part))) {
            break;
        } else if ((err = _makefile_token_add_part(data, &part))) {
            goto error;
        }
    }
    toke->data = part;
    if ((err = _makefile_token_add_part((_makefile_token_t*) toke, &data))) {
        goto error;
    }

    *tokep = (_makefile_token_t*) toke;
    return 0;

error:
    _makefile_back_token(state, (const _makefile_token_t*) toke);
    _makefile_dest_token(&hspc);
    _makefile_dest_token(&part);
    _makefile_dest_token(&data);
    _makefile_dest_token(&name);
    _makefile_dest_token((_makefile_token_t**) &toke);
    return 0;
}

int
_gnumake_double_colon_op (
    _makefile_context_t*    ctxt,
    _makefile_state_t*      state,
    _makefile_token_t**     tokep
    )
{
    _makefile_token_t*  toke = NULL;
    _makefile_char_t    cval;
    _makefile_size_t    csiz;
    int                 err  = 0;

    if ((err = _makefile_make_token(state, &toke, &_gnumake_double_colon_op_token_type))) {
        return err;
    }

    if ((err = _makefile_read_char(state, &cval, &csiz))) {
        goto error;
    } else if (cval != ':') {
        err = EINVAL;
        goto error;
    } else if (tokep) {
        _makefile_token_add_size(toke, csiz, 0);
    }
    if ((err = _makefile_read_char(state, &cval, &csiz))) {
        goto error;
    } else if (cval != ':') {
        err = EINVAL;
        goto error;
    } else if (tokep) {
        _makefile_token_add_size(toke, csiz, 0);
    }

    *tokep = toke;
    return 0;

error:
    _makefile_back_token(state, toke);
    _makefile_dest_token(&toke);
    return err;
}

int
_gnumake_single_colon_op (
    _makefile_context_t*        ctxt,
    _makefile_state_t*          state,
    _makefile_token_t**         tokep
    )
{
    _makefile_token_t* toke = NULL;
    _makefile_char_t   cval;
    _makefile_size_t   csiz;
    int                err  = 0;

    if ((err = _makefile_make_token(state, &toke, &_gnumake_single_colon_op_token_type))) {
        return err;
    }

    if ((err = _makefile_read_char(state, &cval, &csiz))) {
        goto error;
    } else if (cval != ':') {
        err = EINVAL;
        goto error;
    } else if (tokep) {
        _makefile_token_add_size(toke, csiz, 0);
    }

    *tokep = toke;
    return 0;

error:
    _makefile_back_token(state, toke);
    _makefile_dest_token(&toke);
    return err;
}

int
_gnumake_pipe_op (
    _makefile_context_t*        ctxt,
    _makefile_state_t*          state,
    _makefile_token_t**         tokep
    )
{
    _makefile_token_t* toke = NULL;
    _makefile_char_t   cval;
    _makefile_size_t   csiz;
    int                err  = 0;

    if (tokep) {
        if ((err = _makefile_make_token(state, &toke, &_gnumake_pipe_op_token_type))) {
            return err;
        }
    }

    if ((err = _makefile_read_char(state, &cval, &csiz))) {
        goto error;
    } else if (cval != '|') {
        err = ENOENT;
        goto error;
    } else if (tokep) {
        _makefile_token_add_size(toke, csiz, 0);
        *tokep = tokek;
    }

    return 0;

error:
    _makefile_back_char(state, csiz, 0);
    _makefile_dest_token(&toke);
    return err;
}

int
_gnumake_semicolon_op (
    _makefile_context_t*        ctxt,
    _makefile_state_t*          state,
    _makefile_token_t**         tokep
    )
{
    _makefile_token_t* toke = NULL;
    _makefile_char_t   cval;
    _makefile_size_t   csiz;
    int                err  = 0;

    if (tokep) {
        if ((err = _makefile_make_token(state, &toke, &_gnumake_semicolon_op_token_type))) {
            return err;
        }
    }

    if ((err = _makefile_read_char(state, &cval, &csiz))) {
        goto error;
    } else if (cval != ';') {
        err = ENOENT;
        goto error;
    } else if (tokep) {
        _makefile_token_add_size(toke, csiz, 0);
        *tokep = toke;
    }

    return 0;

error:
    _makefile_back_char(state, csiz, 0);
    _makefile_dest_token(&toke);
    return err;
}

int
_gnumake_nested (
    _makefile_context_t*        ctxt,
    _makefile_state_t*          state,
    _makefile_token_t**         tokep
    )
{
    _makefile_token_type_t* redo = NULL;
    _makefile_token_t*      toke = NULL;
    _makefile_token_t*      open = NULL;
    _makefile_token_t*      shut = NULL;
    _makefile_token_t*      part = NULL;
    _makefile_token_t       back;
    _makefile_char_t        cval;
    _makefile_size_t        csiz;
    const char*             tail;
    int                     type;
    int                     mask;
    int                     done;
    int                     err  = 0;

    if ((err = _makefile_make_token(state, &toke, &_gnumake_nested_token_type))) {
        return err;
    } else if ((err = _makefile_make_token(state, &open, NULL))) {
        goto error;
    } else if ((err = _makefile_read_char(state, &cval, &csiz))) {
        goto error;
    }

    switch (cval) {
        default:
            /* First character must be one of the nesting operators. */
            if (! (err = _makefile_back_char(state, csiz, 0))) {
                err = EINVAL;
            }
            goto error;

        case '(':
            type = &_gnumake_open_single_paren_op_token_type;
            tail = ")";
            break;
        case '[':
            type = &_gnumake_open_single_brack_op_token_type;
            tail = "]";
            break;
        case '{':
            type = &_gnumake_open_single_brace_op_token_type;
            tail = "}";
            break;
        case '<':
            type = &_gnumake_open_single_angle_op_token_type;
            tail = ">";
            break;
    }
    _makefile_token_add_size(open, csiz, 0);

    /* Push the desired delimiter, to pick up later. */
    if ((err = _makefile_push_delim(ctxt, state, open, tail))) {
        goto error;
    } else if ((err = _makefile_token_add_part(toke, &open))) {
        goto error;
    }

    for ( ; ; ) {
        if ((err = _makefile_read_char(state, &cval, &csiz))) {
            goto error;
        }

        switch (cval) {
            default:
                _makefile_token_add_size(toke, csiz, 0);
                continue;

            case '(':
            case '[':
            case '{':
            case '<':
                if ((err = _makefile_back_char(state, csiz, 0))) {
                    goto error;
                } else if (! (err = _gnumake_nested(ctxt, state, &part))) {
                    if ((err = _makefile_token_add_part(toke, &part))) {
                        goto error;
                    }
                } else {
                    _makefile_token_add_size(toke, csiz, 0);
                }
                continue;

            case ')':
            case '}':
            case ']':
            case '>':
                if ((err = _makefile_back_char(state, csiz, 0))) {
                    goto error;
                } else if (! _gnumake_iscloser(ctxt, state)) {
                    if ((err = _makefile_read_char(state, &cval, &csiz))) {
                        goto error;
                    }
                    _makefile_token_add_size(toke, csiz, 0);
                } else {
                    break;
                }
                continue;

            case '\\':
                if ((err = _makefile_back_char(state, csiz, 0))) {
                    goto error;
                } else if (! (err = _makefile_continuation(ctxt, state, &part))
                        || ! (err = _gnumake_escape_quoted(ctxt, state, &part, "\\\"'`(){}[]<>"))
                          )
                {
                    if ((err = _makefile_token_add_part(toke, &part))) {
                        goto error;
                    }
                } else if ((err = _makefile_read_char(state, &cval, &csiz))) {
                    goto error;
                } else {
                    _makefile_token_add_size(toke, csiz, 0);
                }
                continue;

            case '\r':
            case '\n':
                if ((err = _makefile_back_char(state, csiz, 0))) {
                    goto error;
                } else if ((err = _makefile_endline(ctxt, state, &part))) {
                    goto error;
                } else if ((err = _makefile_token_add_part(toke, &part))) {
                    goto error;
                }
                continue;

            case '\'':
                if ((err = _makefile_back_char(state, csiz, 0))) {
                    goto error;
                } else if (! (err = _gnumake_single_quoted(ctxt, state, &part))) {
                    if ((err = _makefile_token_add_part(toke, &part))) {
                        goto error;
                    }
                } else if ((err = _makefile_read_char(state, &cval, &csiz))) {
                    goto error;
                } else {
                    _makefile_token_add_size(toke, csiz, 0);
                }
                continue;

            case '"':
                if ((err = _makefile_back_char(state, csiz, 0))) {
                    goto error;
                } else if (! (err = _gnumake_double_quoted(ctxt, state, &part))) {
                    if ((err = _makefile_token_add_part(toke, &part))) {
                        goto error;
                    }
                } else if ((err = _makefile_read_char(state, &cval, &csiz))) {
                    goto error;
                } else {
                    _makefile_token_add_size(toke, csiz, 0);
                }
                continue;

            case '`':
                if ((err = _makefile_back_char(state, csiz, 0))) {
                    goto error;
                } else if (! (err = _gnumake_system_quoted(ctxt, state, &part))) {
                    if ((err = _makefile_token_add_part(toke, &part))) {
                        goto error;
                    }
                } else if ((err = _makefile_read_char(state, &cval, &csiz))) {
                    goto error;
                } else {
                    _makefile_token_add_size(toke, csiz, 0);
                }
                continue;

            case '$':
                if ((err = _makefile_back_char(state, csiz, 0))) {
                    goto error;
                } else if (! (err = _gnumake_dollar(ctxt, state, &part))) {
                    if ((err = _makefile_token_add_part(toke, &part))) {
                        goto error;
                    }
                } else {
                    _makefile_token_add_size(toke, csiz, 0);
                }
                continue;
        }
        break;
    }

    *tokep = toke;
    return 0;

error:
    _makefile_back_token(state, toke);
    _makefile_dest_token(&shut);
    _makefile_dest_token(&open);
    _makefile_dest_token(&part);
    _makefile_dest_token(&toke);
    return err;
}

int
_gnumake_single_quoted (
    _makefile_context_t*        ctxt,
    _makefile_state_t*          state,
    _makefile_token_t**         tokep
    )
{
    _makefile_token_t* toke = NULL;
    _makefile_token_t* part = NULL;
    _makefile_char_t   cval;
    _makefile_size_t   csiz;
    int                err  = 0;

    if ((err = _makefile_make_token(state, &toke, &_gnumake_single_quoted_token_type))) {
        return err;
    } else if ((err = _makefile_make_token(state, &part, &_gnumake_single_quote_op_token_type))) {
        goto error;
    } else if ((err = _makefile_read_char(state, &cval, &csiz))) {
        goto error;
    } else if (cval != '\'') {
        goto error;
    } else {
        _makefile_token_add_size(part, csiz, 0);
        if ((err = _makefile_token_add_part(toke, &part))) {
            goto error;
        }
    }

    for ( ; ; ) {
        if ((err = _makefile_read_char(state, &cval, &csiz))) {
            goto error;
        }

        switch (cval) {
            default:
                _makefile_token_add_size(toke, csiz, 0);
                continue;

            case '\'':
                if ((err = _makefile_back_char(state, csiz, 0))) {
                    goto error;
                } else if ((err = _makefile_make_token(state, &part, &_gnumake_single_quote_op_token_type))) {
                    goto error;
                } else {
                    _makefile_token_add_size(part, csiz, 0);
                    if ((err = _makefile_token_add_part(toke, &part))) {
                        goto error;
                    }
                }
                continue;

            case '\\':
                if ((err = _makefile_back_char(state, csiz, 0))) {
                    goto error;
                } else if (! (err = _makefile_continuation(ctxt, state, &part))
                        || ! (err = _gnumake_escape_quoted(ctxt, state, &part, "'"))
                          )
                {
                    if ((err = _makefile_token_add_part(toke, &part))) {
                        goto error;
                    }
                } else if ((err = _makefile_read_char(state, &cval, &csiz))) {
                    goto error;
                } else {
                    _makefile_token_add_size(toke, csiz);
                }
                continue;

            case '\r':
            case '\n':
                if ((err = _makefile_back_char(state, csiz, 0))) {
                    goto error;
                } else if ((err = _makefile_endline(ctxt, state, &part))) {
                    goto error;
                } else if ((err = _makefile_token_add_part(toke, &part))) {
                    goto error;
                }
                continue;
        }
    }

    *tokep = toke;
    return 0;

error:
    _makefile_back_token(state, toke);
    _makefile_dest_token(&part);
    _makefile_dest_token(&toke);
    return err;
}

int
_gnumake_double_quoted (
    _makefile_context_t*    ctxt,
    _makefile_state_t*      state,
    _makefile_token_t**     tokep
    )
{
    _makefile_token_t*  toke = NULL;
    _makefile_token_t*  part = NULL;
    _makefile_char_t    cval;
    _makefile_size_t    csiz;
    int                 err  = 0;


    if ((err = _makefile_make_token(state, &toke, &_gnumake_double_quoted_token_type))) {
        return err;
    } else if ((err = _makefile_make_token(state, &part, &_gnumake_double_quote_op_token_type))) {
        goto error;
    } else if ((err = _makefile_read_char(state, &cval, &csiz))) {
        goto error;
    } else if (cval != '"') {
        goto error;
    } else {
        _makefile_token_add_size(part, csiz, 0);
        if ((err = _makefile_token_add_part(toke, &part))) {
            goto error;
        }
    }

    for ( ; ; ) {
        if ((err = _makefile_read_char(state, &cval, &csiz))) {
            goto error;
        }

        switch (cval) {
            default:
                _makefile_token_add_size(toke, csiz, 0);
                continue;

            case '"':
                if ((err = _makefile_back_char(state, csiz, 0))) {
                    goto error;
                } else if ((err = _makefile_make_token(state, &part, &_gnumake_double_quote_token_type))) {
                    goto error;
                } else {
                    _makefile_token_add_size(part, csiz, 0);
                    toke->shut = part;
                    if ((err = _makefile_token_add_part(toke, &part))) {
                        goto error;
                    }
                }
                break;

            case '\\':
                if ((err = _makefile_back_char(state, csiz, 0))) {
                    goto error;
                } else if (! (err = _makefile_continuation(ctxt, state, &part))
                        || ! (err = _gnumake_escape_quoted(ctxt, state, &part, "\"`$"))
                          )
                {
                    if ((err = _makefile_token_add_part(toke, &part))) {
                        goto error;
                    }
                } else if ((err = _makefile_read_char(state, &cval, &csiz))) {
                    goto error;
                } else {
                    _makefile_token_add_size(toke, csiz);
                }
                continue;

            case '\r':
            case '\n':
                if ((err = _makefile_back_char(state, csiz, 0))) {
                    goto error;
                } else if ((err = _makefile_endline(ctxt, state, &part))) {
                    goto error;
                } else if ((err = _makefile_token_add_part(toke, &part))) {
                    goto error;
                }
                continue;

            case '$':
                if ((err = _makefile_back_char(state, csiz, 0))) {
                    goto error;
                } else if (! (err = _gnumake_dollar(ctxt, state, &part))) {
                    if ((err = _makefile_token_add_part(toke, &part))) {
                        goto error;
                    }
                } else if ((err = _makefile_read_char(state, &cval, &csiz))) {
                    goto error;
                } else {
                    _makefile_token_add_size(toke, csiz, 0);
                }
                continue;

            case '(':
            case '{':
            case '[':
            case '<':
                if ((err = _makefile_back_char(state, csiz, 0))) {
                    goto error;
                } else if (! (err = _gnumake_nested(ctxt, state, &part))) {
                    if ((err = _makefile_token_add_part(toke &part))) {
                        goto error;
                    }
                } else if ((err = _makefile_read_char(state, &cval, &csiz))) {
                    goto error;
                } else {
                    _makefile_token_add_size(toke, csiz, 0);
                }
                continue;
        }
    }

    *tokep = toke;
    return 0;

error:
    _makefile_back_token(state, toke);
    _makefile_dest_token(&part);
    _makefile_dest_token(&toke);
    return err;
}

int
_gnumake_escape_quoted (
    _makefile_context_t*        ctxt,
    _makefile_state_t*          state,
    _makefile_token_t**         tokep,
    const char*                 which
    )
{
    _makefile_token_t*      toke = NULL;
    _makefile_token_t*      part;
    _makefile_char_t        cval;
    _makefile_size_t        csiz;
    int                     err  = 0;

    if ((err = _makefile_make_token(state, &toke, &_gnumake_escape_quoted_token_type))) {
        return err;
    } else if ((err = _makefile_make_token(state, &part, &_gnumake_escape_quote_op_token_type))) {
        goto error;
    } else if ((err = _makefile_read_char(state, &cval, &csiz))) {
        goto error;
    } else if (cval != '\\') {
        err = EINVAL;
        goto error;
    } else {
        _makefile_token_add_size(part, csiz, 0);
        if ((err = _makefile_token_add_part(toke, &part))) {
            goto error;
        }
    }

    if ((err = _makefile_make_token(state, &part, &_gnumake_literal_token_type))) {
        goto error;
    } else if ((err = _makefile_read_char(state, &cval, &csiz))) {
        goto error;
    } else if (! strchr(which, cval)) {
        err = EINVAL;
        goto error;
    } else {
        _makefile_token_add_size(part, csiz, 0);
        if ((err = _makefile_token_add_part(toke, &part))) {
            goto error;
        }
    }

    *tokep = toke;
    return 0;

error:
    _makefile_back_token(state, toke);
    _makefile_dest_token(&part);
    _makefile_dest_token(&toke);
    return err;
}

int
_gnumake_system_quoted (
    _makefile_context_t*        ctxt,
    _makefile_state_t*          state,
    _makefile_token_t**         tokep,
    )
{
    _makefile_token_t*      toke = NULL;
    _makefile_token_t*      part = NULL;
    _makefile_char_t        cval;
    _makefile_size_t        csiz;
    int                     err  = 0;

    if ((err = _makefile_make_token(state, &toke, &_gnumake_system_quoted_token_type))) {
        return err;
    } else if ((err = _makefile_make_token(state, &part, &_gnumake_system_quote_op_token_type))) {
        goto error;
    } else if ((err = _makefile_read_char(state, &cval, &csiz))) {
        goto error;
    } else if (cval != '`') {
        err = EINVAL;
        goto error;
    } else if (_makefile_token_add_size(part, csiz, 0), 0) {
    } else if ((err = _makefile_push_delim(ctxt, state, part, "`"))) {
        goto error;
    } else if ((err = _makefile_token_add_part(toke, &part))) {
        goto error;
    }

    for ( ; ; ) {
        if ((err = _makefile_read_char(state, &cval, &csiz))) {
            goto error;
        }

        switch (cval) {
            default:
                _makefile_token_add_size(toke, csiz, 0);
                continue;

            case '`':
                if ((err = _makefile_back_char(state, csiz, 0))) {
                    goto error;
                } else if ((err = _makefile_make_token(state, &part, &_gnumake_system_quote_op_token_type))) {
                    goto error;
                } else if ((err = _makefile_read_delim(ctxt, state, &csiz))) {
                    goto error;
                } else if (_makefile_token_add_size(part, csiz, 0), 0) {
                } else if ((err = _makefile_token_add_part(toke, &part))) {
                    goto error;
                }
                break;

            case '\\':
                if ((err = _makefile_back_char(state, csiz, 0))) {
                    goto error;
                } else if (! (err = _makefile_continuation(ctxt, state, &part))
                        || ! (err = _gnumake_escape_quoted(ctxt, state, &part, "\\\"'`$#"))
                          )
                {
                    if ((err = _makefile_token_add_part(toke, &part))) {
                        goto error;
                    }
                } else if ((err = _makefile_read_char(state, &cval, &csiz))) {
                    goto error;
                } else {
                    _makefile_token_add_size(toke, csiz, 0);
                }
                continue;

            case '\r':
            case '\n':
                if ((err = _makefile_back_char(state, csiz, 0))) {
                    goto error;
                } else if ((err = _makefile_endline(ctxt, state, &part))) {
                    goto error;
                } else if ((err = _makefile_token_add_part(toke, &part))) {
                    goto error;
                } else {
                    /* Should never be here. */
                }
                continue;
                    
            case '\'':
                if ((err = _makefile_back_char(state, csiz, 0))) {
                    goto error;
                } else if (! (err = _gnumake_single_quoted(ctxt, state, &part))) {
                    if ((err = _makefile_token_add_part(toke, &part))) {
                        goto error;
                    }
                } else if ((err = _makefile_read_char(state, &cval, &csiz))) {
                    goto error;
                } else {
                    _makefile_token_add_size(toke, csiz, 0);
                }
                continue;

            case '"':
                if ((err = _makefile_back_char(state, csiz, 0))) {
                    goto error;
                } else if (! (err = _gnumake_double_quoted(ctxt, state, &part))) {
                    if ((err = _makefile_token_add_part(toke, &part))) {
                        goto error;
                    }
                } else if ((err = _makefile_read_char(state, &cval, &csiz))) {
                    goto error;
                } else {
                    _makefile_token_add_size(toke, csiz, 0);
                }
                continue;

            case '$':
                if ((err = _makefile_back_char(state, csiz, 0))) {
                    goto error;
                } else if (! (err = _gnumake_dollar(ctxt, state, &part))) {
                    if ((err = _makefile_token_add_part(toke, &part))) {
                        goto error;
                    }
                } else if ((err = _makefile_read_char(state, &cval, &csiz))) {
                    goto error;
                } else {
                    _makefile_token_add_size(toke, csiz, 0);
                }
                continue;

            case '(':
            case '{':
            case '[':
            case '<':
                if ((err = _makefile_back_char(state, csiz, 0))) {
                    goto error;
                } else if (! (err = _gnumake_nested(ctxt, state, &part))) {
                    if ((err = _makefile_token_add_part(toke &part))) {
                        goto error;
                    }
                } else if ((err = _makefile_read_char(state, &cval, &csiz))) {
                    goto error;
                } else {
                    _makefile_token_add_size(toke, csiz, 0);
                }
                continue;

            case ')':
            case '}':
            case ']':
            case '>':
        }
    }

    *tokep = toke;
    return 0;

error:
    _makefile_back_token(state, toke);
    _makefile_dest_token(&part);
    _makefile_dest_token(&toke);
    return err;
}

int
_gnumake_cstyle_quoted (
    _makefile_context_t*        ctxt,
    _makefile_state_t*          state,
    _makefile_token_t**         tokep
    )
{
    _makefile_token_t* toke = NULL;
    _makefile_token_t* part = NULL;
    _makefile_char_t   cval;
    _makefile_size_t   csiz;
    _makefile_char_t   real;
    _makefile_size_t   size;
    int                err  = 0;

    if ((err = _makefile_make_token(state, &toke, &_gnumake_cstyle_quoted_token_type))) {
        return err;
    } else if ((err = _makefile_make_token(state, &part, &_gnumake_cstyle_quote_op_token_type))) {
        goto error;
    } else if ((err = _makefile_read_char(state, &cval, &csiz))) {
        goto error;
    } else if (cval != '$') {
        err = EINVAL;
        goto error;
    } else {
        _makefile_token_add_size(part, csiz, 0);
    }
    if ((err = _makefile_read_char(state, &cval, &csiz))) {
        goto error;
    } else if (cval != '\'') {
        err = EINVAL;
        goto error;
    } else {
        _makefile_token_add_size(part, csiz, 0);
        if ((err = _makefile_token_add_part(toke, &part))) {
            goto error;
        }
    }

    for ( ; ; ) {
        if ((err = _makefile_read_char(state, &cval, &csiz))) {
            goto error;
        }

        switch (cval) {
            default:
                _makefile_token_add_size(toke, csiz, 0);
                continue;

            case '\'':
                if ((err = _makefile_back_char(state, csiz, 0))) {
                    goto error;
                } else if ((err = _makefile_make_token(state, &part, &_gnumake_single_quote_op_token_type)))) {
                    goto error;
                } else {
                    _makefile_token_add_size(part, csiz, 0);
                    if ((err = _makefile_token_add_part(toke, &part))) {
                        goto error;
                    }
                }
                break;

            case '\\':
                used = 0;
                if ((err = _makefile_back_char(state, csiz, 0))) {
                    goto error;
                } else if (! (err = _makefile_continuation(ctxt, state, &part))
                        || ! (err = _makefile_escape_quoted(ctxt, state, &part, "'"))
                          )
                {
                    if ((err = _makefile_token_add_part(toke, &part))) {
                        goto error;
                    }
                    continue;
                } else if ((err = _gnumake_make_charsub_token(state, &part))) {
                    goto error;
                } else if ((err = _makefile_read_char(state, &cval, &csiz))) {
                    goto error;
                } else {
                    used += csiz;
                    _makefile_token_add_size(part, csiz, 0);
                    if ((err = _makefile_read_char(state, &cval, &csiz))) {
                        goto error;
                    }
                }

                real = 0;
                switch (cval) {
                    default:
                        /* Axe the charsub token. */
                        _makefile_dest_token(&part);
                        /* Rewind back to just after \\. */
                        if ((err = _makefile_back_char(state, csiz, 0))) {
                            goto error;
                        }
                        /* Add \\ to the outer token. */
                        _makefile_token_add_size(toke, used, 0);
                        continue;

                    case 'a':
                    case 'b': /*FALLTHRU*/
                    case 'e': /*FALLTHRU*/
                    case 'E': /*FALLTHRU*/
                    case 'f': /*FALLTHRU*/
                    case 'n': /*FALLTHRU*/
                    case 'r': /*FALLTHRU*/
                    case 't': /*FALLTHRU*/
                    case 'v': /*FALLTHRU*/
                        /* Add the escaped char to the charsub token. */
                        _makefile_token_add_size(toke, csiz, 0);
                        switch (cval) {
                            case 'a': real = '\a';   break;
                            case 'b': real = '\b';   break;
                            case 'e': real = '\x1B'; break;
                            case 'E': real = '\x1B'; break;
                            case 'f': real = '\f';   break;
                            case 'n': real = '\n';   break;
                            case 'r': real = '\r';   break;
                            case 't': real = '\t';   break;
                            case 'v': real = '\v';   break;
                        }
                        break;

#define _HEX {                                                  \
    if ((err = _makefile_read_char(state, &cval, &csiz))) {     \
        break;                                                  \
    } else if (_makefile_isxdigit(ctxt, state, cval)) {         \
        _makefile_char_t curr = (                               \
            _makefile_isdigit(ctxt, state, cval)                \
                ? (cval - '0') /* ASCII-compatible */           \
                : (_makefile_isupper(ctxt, state, cval)         \
                    ? (10 + (cval - 'A'))                       \
                    : (10 + (cval - 'a'))                       \
                  )                                             \
        );                                                      \
        real = ((real << 4) | curr);                            \
    } else if ((err = _makefile_back_char(state, csiz, 0))) {   \
        goto error;                                             \
    } else {                                                    \
        break;                                                  \
    }
                    case 'U':
                        _HEX;
                        _HEX;
                        _HEX;
                        _HEX;
                    case 'u': /*FALLTHRU*/
                        _HEX;
                        _HEX;
                    case 'x': /*FALLTHRU*/
                        _HEX;
                        _HEX;
                        break;

#define _OCT                                                    \
    real = ((real << 3) | (cval - '0'));                        \
    _makefile_token_add_size(part, csiz, 0);                    \
    if ((err = _makefile_read_char(state, &cval, &csiz))) {     \
        break;                                                  \
    } else if (! _makefile_isodigit(ctxt, state, cval)) {       \
        if ((err = _makefile_back_char(state, csiz, 0))) {      \
            goto error;                                         \
        }                                                       \
        break;                                                  \
    }
                    case '0': case '1': case '2': case '3':
                        _OCT;
                    case '4': case '5': case '6': case '7': /*FALLTHRU*/
                        _OCT;
                        _OCT;
                        break;
                }
                _makefile_token_add_size(part, used, 0);
                ((_gnumake_charsub_token_t*) part)->real = real;
                if ((err = _makefile_token_add_part(toke, &part))) {
                    goto error;
                }
                continue;

            case '\r':
            case '\n':
                if ((err = _makefile_back_char(state, csiz, 0))) {
                    goto error;
                } else if ((err = _makefile_endline(ctxt, state, &part))) {
                    goto error;
                } else if ((err = _makefile_token_add_part(toke, &part))) {
                    goto error;
                }
                continue;
        }

        break;
    }

    *tokep = toke;
    return 0;

error:
    _makefile_back_token(state, toke);
    _makefile_dest_token(&part);
    _makefile_dest_token(&toke);
    return err;
}

int
_gnumake_locale_quoted (
    _makefile_context_t*        ctxt,
    _makefile_state_t*          state,
    _makefile_token_t**         tokep
    )
{
    _makefile_token_t* toke = NULL;
    _makefile_token_t* part = NULL;
    _makefile_char_t   cval;
    _makefile_size_t   csiz;
    _makefile_char_t   real;
    _makefile_size_t   size;
    int                err  = 0;

    if ((err = _makefile_make_token(state, &toke, &_gnumake_cstyle_quoted_token_type))) {
        return err;
    } else if ((err = _makefile_make_token(state, &part, &_gnumake_cstyle_quote_op_token_type))) {
        goto error;
    } else if ((err = _makefile_read_char(state, &cval, &csiz))) {
        goto error;
    } else if (cval != '$') {
        err = EINVAL;
        goto error;
    } else {
        _makefile_token_add_size(part, csiz, 0);
    }
    if ((err = _makefile_read_char(state, &cval, &csiz))) {
        goto error;
    } else if (cval != '"') {
        err = EINVAL;
        goto error;
    } else {
        _makefile_token_add_size(part, csiz, 0);
        if ((err = _makefile_token_add_part(toke, &part))) {
            goto error;
        }
    }

    for ( ; ; ) {
        if ((err = _makefile_read_char(state, &cval, &csiz))) {
            goto error;
        }

        switch (cval) {
            default:
                _makefile_token_add_size(toke, csiz, 0);
                continue;

            case '"':
                if ((err = _makefile_back_char(state, csiz, 0))) {
                    goto error;
                } else if ((err = _makefile_make_token(state, &part, &_gnumake_single_quote_op_token_type)))) {
                    goto error;
                } else {
                    _makefile_token_add_size(part, csiz, 0);
                    if ((err = _makefile_token_add_part(toke, &part))) {
                        goto error;
                    }
                }
                break;

            case '\\':
                used = 0;
                if ((err = _makefile_back_char(state, csiz, 0))) {
                    goto error;
                } else if (! (err = _makefile_continuation(ctxt, state, &part))
                        || ! (err = _makefile_escape_quoted(ctxt, state, &part, "\"`$"))
                          )
                {
                    if ((err = _makefile_token_add_part(toke, &part))) {
                        goto error;
                    }
                } else if ((err = _makefile_read_char(state, &cval, &csiz))) {
                    goto error;
                } else {
                    _makefile_token_add_size(toke, csiz, 0);
                }
                continue;

            case '\r':
            case '\n':
                if ((err = _makefile_back_char(state, csiz, 0))) {
                    goto error;
                } else if ((err = _makefile_endline(ctxt, state, &part))) {
                    goto error;
                } else if ((err = _makefile_token_add_part(toke, &part))) {
                    goto error;
                }
                continue;

            case '`':
                if ((err = _makefile_back_char(state, csiz, 0))) {
                    goto error;
                } else if (! (err = _gnumake_system_quoted(ctxt, state, &part))) {
                    if ((err = _makefile_token_add_part(toke, &part))) {
                        goto error;
                    }
                } else if ((err = _makefile_read_char(state, &cval, &csiz))) {
                    goto error;
                } else {
                    _makefile_token_add_size(toke, csiz, 0);
                }
                continue;

            case '$':
                if ((err = _makefile_back_char(state, csiz, 0))) {
                    goto error;
                } else if (! (err = _gnumake_dollar(ctxt, state, &part))) {
                    if ((err = _makefile_token_add_part(toke, &part))) {
                        goto error;
                    }
                } else if ((err = _makefile_read_char(state, csiz, 0))) {
                    goto error;
                } else {
                    _makefile_token_add_size(toke, csiz, 0);
                }
                continue;
        }

        break;
    }

    *tokep = toke;
    return 0;

error:
    _makefile_back_token(state, toke);
    _makefile_dest_token(&part);
    _makefile_dest_token(&toke);
    return err;
}

int
_gnumake_dollar (
    _makefile_context_t*        ctxt,
    _makefile_state_t*          state,
    _makefile_token_t**         tokep
    )
{
    _makefile_token_type_t* type = NULL;
    _gnumake_autovar_f      func = NULL;
    _makefile_token_t*      toke = NULL;
    _makefile_token_t       back = { 0 };
    _makefile_char_t        cval;
    _makefile_size_t        csiz;
    _makefile_size_t        open;
    int                     err  = 0;

    if ((err = _makefile_init_token(state, &back, NULL))) {
        return err;
    } else if ((err = _makefile_read_char(state, &cval, &csiz))) {
        goto error;
    } else if (cval != '$') {
        err = EINVAL;
        goto error;
    } else if (! (err = _makefile_read_char(state, &cval, &csiz))) {
        type = &_gnumake_charsub_token_type;
    } else if (_makefile_isspace(ctxt, cval)) {
        type = &_gnumake_charsub_token_type;
    } else if ((err = _makefile_back_token(state, &back))) {
        goto error;
    }

    if (type) {
        if ((err = _makefile_make_token(state, &toke, type))) {
            goto error;
        } else if ((err = _makefile_read_char(state, &cval, &csiz))) {
            goto error;
        }
        _makefile_token_add_size(toke, csiz, 0);
        *tokep = toke;
        return 0;
    }

    switch (cval) {
        default:
            if ((err = _gnumake_make_charvar_token(state, (_gnumake_charvar_token_t**) &toke))) {
                goto error;
            } else if ((err = _makefile_read_char(state, &cval, &csiz))) {
                goto error;
            }
            _makefile_token_add_size(toke, csiz, 0);
            if ((err = _makefile_read_char(state, &cval, &csiz))) {
                goto error;
            }
            _makefile_token_add_size(toke, csiz, 0);
            ((_gnumake_charvar_token_t*) toke)->name = cval;
            *tokep = toke;
            return 0;

        case '$':
            if ((err = _gnumake_make_charsub_token(state, (_gnumake_charsub_token_t**) &toke))) {
                goto error;
            } else if ((err = _makefile_read_char(state, &cval, &csiz))) {
                goto error;
            }
            _makefile_token_add_size(toke, csiz, 0);
            if ((err = _makefile_read_char(state, &cval, &csiz))) {
                goto error;
            }
            _makefile_token_add_size(toke, csiz, 0);
            ((_gnumake_charsub_token_t*) toke)->real = cval;
            *tokep = toke;
            return 0;

        case '<':
        case '>':
        case '^':
        case '+':
        case '@':
        case '|':
        case '?':
        case '*':
        case '%':
        case '#':
        case '!':
            if ((err = _gnumake_make_autovar_token(state, &toke))) {
                goto error;
            } else if ((err = _makefile_read_char(state, &cval, &csiz))) {
                goto error;
            } else {
                _makefile_token_add_size(toke, csiz, 0);
                if ((err = _makefile_read_char(state, &cval, &csiz))) {
                    goto error;
                }
                _makefile_token_add_size(toke, csiz, 0);
                ((_gnumake_autovar_token_t*) toke)->name = cval;
            }
            switch (cval) {
                case '<': func = _gnumake_autovar_lessthan;     break;
                case '>': func = _gnumake_autovar_greaterthan;  break;
                case '^': func = _gnumake_autovar_caret;        break;
                case '+': func = _gnumake_autovar_plus;         break;
                case '@': func = _gnumake_autovar_at;           break;
                case '|': func = _gnumake_autovar_pipe;         break;
                case '?': func = _gnumake_autovar_question;     break;
                case '*': func = _gnumake_autovar_star;         break;
                case '%': func = _gnumake_autovar_percent;      break;
                case '#': func = _gnumake_autovar_pound;        break;
                case '!': func = _gnumake_autovar_bang;         break;
            }
            ((_gnumake_autovar_token_t*) toke)->name = cval;
            ((_gnumake_autovar_token_t*) toke)->func = func;
            *tokep = toke;
            return 0;

        case '(':
            if (! (err = _gnumake_subst_math(ctxt, state, &toke))
             || ! (err = _gnumake_subst_name(ctxt, state, &toke))
               )
            {
                *tokep = toke;
                return 0;
            }
            break;

        case '{':
            if (! (err = _gnumake_subst_lang(ctxt, state, &toke))
             || ! (err = _gnumake_subst_name(ctxt, state, &toke))
               )
            {
                *tokep = toke;
                return 0;
            }
            break;

        case '[':
            if (! (err = _gnumake_subst_test(ctxt, state, &toke))
             || ! (err = _gnumake_subst_name(ctxt, state, &toke))
               )
            {
                *tokep = toke;
                return 0;
            }
            break;

        case '"':
            if (! (err = _gnumake_locale_quoted(ctxt, state, &toke))) {
                *tokep = toke;
                return 0;
            }
            break;

        case '\'':
            if (! (err = _gnumake_cstyle_quoted(ctxt, state, &toke))) {
                *tokep = toke;
                return 0;
            }
            break;
    }

    if ((err = _gnumake_make_charvar_token(state, &toke))) {
        goto error;
    } else if ((err = _makefile_read_char(state, &cval, &csiz))) {
        goto error;
    } else {
        _makefile_token_add_size(toke, csiz, 0);
    }
    if ((err = _makefile_read_char(state, &cval, &csiz))) {
        goto error;
    } else {
        _makefile_token_add_size(toke, csiz, 0);
        ((_gnumake_charvar_token_t*) toke)->name = cval;
    }

    *tokep = toke;
    return 0;

error:
    _makefile_back_token(state, &here);
    _makefile_free_token(&here);
    _makefile_dest_token(&toke);
    return err;
}

int
_shell_math_expr (
    _makefile_context_t*    ctxt,
    _makefile_state_t*      state,
    _makefile_token_t**     tokep
    )
{
}

int
_shell_test_expr (
    _makefile_context_t*    ctxt,
    _makefile_state_t*      state,
    _makefile_token_t**     tokep
    )
{
}

int
_other_lang_expr (
    _makefile_context_t*    ctxt,
    _makefile_state_t*      state,
    _makefile_token_t**     tokep
    )
{
}

/* $((...)) */
int
_gnumake_subst_math (
    _makefile_context_t*    ctxt,
    _makefile_state_t*      state,
    _makefile_token_t**     tokep
    )
{
    _makefile_subst_token_t* toke = NULL;
    _makefile_token_t*       part = NULL;
    _makefile_char_t         cval;
    _makefile_size_t         csiz;
    int                      err  = 0;

    if ((err = _makefile_make_subst_token(state, &toke, &_gnumake_subst_math_token_type))) {
        return err;
    } else if ((err = _makefile_make_token(state, &part, &_gnumake_subst_math_open_op_token_type))) {
        return err;
    } else if ((err = _makefile_read_char(state, &cval, &csiz))) {
        goto error;
    } else if (cval != '$') {
        err = EINVAL;
        goto error;
    } else if (_makefile_token_add_size(part, csiz, 0), 0) {
    } else if ((err = _makefile_read_char(state, &cval, &csiz))) {
        goto error;
    } else if (cval != '(') {
        err = EINVAL;
        goto error;
    } else if (_makefile_token_add_size(part, csiz, 0), 0) {
    } else if ((err = _makefile_read_char(state, &cval, &csiz))) {
        goto error;
    } else if (cval != '(') {
        err = EINVAL;
        goto error;
    } else if ((err = _makefile_push_delim(ctxt, state, part, "))"))) {
        goto error;
    } else if ((toke->open = part), (err = _makefile_token_add_part(toke, &part))) {
        goto error;
    } else if ((err = _makefile_token_add_part(toke, &part))) {
        goto error;
    } else if ((err = _shell_math_expr(ctxt, state, &part))) {
        goto error;
    } else if ((toke->expr = part), (err = _makefile_token_add_part(toke, &part))) {
        goto error;
    } else if ((err = _makefile_make_token(state, &part, &_gnumake_subst_math_shut_op_token_type))) {
        goto error;
    } else if ((err = _makefile_read_delim(ctxt, state, &csiz))) {
        goto error;
    } else if (_makefile_token_add_size(part, csiz, 0)) {
    } else if ((toke->shut = part), (err = _makefile_token_add_part(toke, &part))) {
        goto error;
    }

    *tokep = toke;
    return 0;

error:
    _makefile_back_token(state, toke);
    _makefile_dest_token(&part);
    _makefile_dest_token(&toke);
    return err;
}

/* ${{...}} */
int
_gnumake_subst_lang (
    _makefile_context_t*    ctxt,
    _makefile_state_t*      state,
    _makefile_token_t**     tokep
    )
{
    _makefile_subst_token_t* toke = NULL;
    _makefile_token_t*       part = NULL;
    _makefile_char_t         cval;
    _makefile_size_t         csiz;
    int                      err  = 0;

    if ((err = _makefile_make_subst_token(state, &toke, &_gnumake_subst_math_token_type))) {
        return err;
    } else if ((err = _makefile_make_token(state, &part, &_gnumake_subst_lang_open_op_token_type))) {
        return err;
    } else if ((err = _makefile_read_char(state, &cval, &csiz))) {
        goto error;
    } else if (cval != '$') {
        err = EINVAL;
        goto error;
    } else if (_makefile_token_add_size(part, csiz, 0), 0) {
    } else if ((err = _makefile_read_char(state, &cval, &csiz))) {
        goto error;
    } else if (cval != '{') {
        err = EINVAL;
        goto error;
    } else if (_makefile_token_add_size(part, csiz, 0), 0) {
    } else if ((err = _makefile_read_char(state, &cval, &csiz))) {
        goto error;
    } else if (cval != '{') {
        err = EINVAL;
        goto error;
    } else if ((err = _makefile_push_delim(ctxt, state, part, "}}"))) {
        goto error;
    } else if ((toke->open = part), (err = _makefile_token_add_part(toke, &part))) {
        goto error;
    } else if ((err = _other_lang_expr(ctxt, state, &part))) {
        goto error;
    } else if ((toke->expr = part), (err = _makefile_token_add_part(toke, &part))) {
        goto error;
    } else if ((err = _makefile_make_token(state, &part, &_gnumake_subst_lang_shut_op_token_type))) {
        goto error;
    } else if ((err = _makefile_read_delim(ctxt, state, &csiz))) {
        goto error;
    } else if (_makefile_token_add_size(part, csiz, 0)) {
    } else if ((toke->shut = part), (err = _makefile_token_add_part(toke, &part))) {
        goto error;
    }

    *tokep = toke;
    return 0;

error:
    _makefile_back_token(state, toke);
    _makefile_dest_token(&part);
    _makefile_dest_token(&toke);
    return err;
}

/* $[[...]] */
int
_gnumake_subst_test (
    _makefile_context_t*    ctxt,
    _makefile_state_t*      state,
    _makefile_token_t**     tokep
    )
{
    _makefile_subst_token_t* toke = NULL;
    _makefile_token_t*       part = NULL;
    _makefile_char_t         cval;
    _makefile_size_t         csiz;
    int                      err  = 0;

    if ((err = _makefile_make_subst_token(state, &toke, &_gnumake_subst_math_token_type))) {
        return err;
    } else if ((err = _makefile_make_token(state, &part, &_gnumake_subst_test_open_op_token_type))) {
        return err;
    } else if ((err = _makefile_read_char(state, &cval, &csiz))) {
        goto error;
    } else if (cval != '$') {
        err = EINVAL;
        goto error;
    } else if (_makefile_token_add_size(part, csiz, 0), 0) {
    } else if ((err = _makefile_read_char(state, &cval, &csiz))) {
        goto error;
    } else if (cval != '[') {
        err = EINVAL;
        goto error;
    } else if (_makefile_token_add_size(part, csiz, 0), 0) {
    } else if ((err = _makefile_read_char(state, &cval, &csiz))) {
        goto error;
    } else if (cval != '[') {
        err = EINVAL;
        goto error;
    } else if ((err = _makefile_push_delim(ctxt, state, part, "]]"))) {
        goto error;
    } else if ((toke->open = part), (err = _makefile_token_add_part(toke, &part))) {
        goto error;
    } else if ((err = _shell_test_expr(ctxt, state, &part))) {
        goto error;
    } else if ((toke->expr = part), (err = _makefile_token_add_part(toke, &part))) {
        goto error;
    } else if ((err = _makefile_make_token(state, &part, &_gnumake_subst_test_shut_op_token_type))) {
        goto error;
    } else if ((err = _makefile_read_delim(ctxt, state, &csiz))) {
        goto error;
    } else if (_makefile_token_add_size(shut, csiz, 0), 0) {
    } else if ((toke->shut = part), (err = _makefile_token_add_part(toke, &part))) {
        goto error;
    }

    *tokep = toke;
    return 0;

error:
    _makefile_back_token(state, toke);
    _makefile_dest_token(&part);
    _makefile_dest_token(&toke);
    return err;
}

struct _gnumake_modifier_token {
    shmoo_token_t           head;
    _makefile_char_t        xtra;
    _makefile_token_t*      xlhs;
    _makefile_token_t*      xrhs;
};

int
_gnumake_make_modifier_token (
    _makefile_state_t*      state,
    _makefile_token_t**     tokep,
    )
{
    _gnumake_modifier_token_t* toke = NULL;

    int err = shmoo_token_make(
        (shmoo_state_t*) state,
        (shmoo_token_t**) &toke,
        &_gnumake_modifier_token_type,
        sizeof(*toke)
    );
    if (! err) {
        *tokep = (_makefile_token_t*) toke;
    }
    return err;
}

int
_gnumake_modifier (
    _makefile_context_t*    ctxt,
    _makefile_state_t*      state,
    _makefile_token_t**     tokep
    )
{
    _makefile_token_t*  toke = NULL;
    _makefile_char_t    oval;
    _makefile_size_t    used;
    _makefile_char_t    cval;
    _makefile_size_t    csiz;
    int                 err  = 0;

    if ((err = _gnumake_make_modifier_token(state, &toke))) {
        return err;
    } else if ((err = _makefile_read_char(state, &cval, &csiz))) {
        goto error;
    } else if (cval != ':') {
        if (! (err = _makefile_back_char(state, csiz, 0))) {
            err = EINVAL;
        }
        goto error;
    }

    _makefile_token_add_size(part, csiz, 0);
    if ((err = _makefile_read_char(state, &cval, &csiz))) {
        goto error;
    }
    switch (cval) {
        case 'E':
        case 'H':
        case 'M':
        case 'N':
        case 'Q':
        case 'q':
        case 'T':
        case 'u':
        case 'L':
        case 'P':
            oval = cval;
            used = csiz;
            if ((err = _makefile_read_char(state, &cval, &csiz))
             || (err = _makefile_back_char(state, csiz, 0))
               )
            {
                goto error;
            } else if ((cval != ':') && ! _makefile_iscloser(ctxt, state)) {
                break;
            }
            switch (oval) {
                case 'E': func = _gnumake_modifier_E; break;
                case 'H': func = _gnumake_modifier_H; break;
                case 'M': func = _gnumake_modifier_M; break;
                case 'N': func = _gnumake_modifier_N; break;
                case 'Q': func = _gnumake_modifier_Q; break;
                case 'q': func = _gnumake_modifier_q; break;
                case 'T': func = _gnumake_modifier_T; break;
                case 'u': func = _gnumake_modifier_u; break;
                case 'L': func = _gnumake_modifier_L; break;
                case 'P': func = _gnumake_modifier_P; break;
            }
            toke->func = func;
            *tokep = (_makefile_token_t*) toke;
            return 0;

        case 'O':
            oval = cval;
            used = csiz;
            if ((err = _makefile_read_char(state, &cval, &csiz))) {
                goto error;
            }
            if ((cval == 'r') || (cval == 'x')) {
                oval  = cval;
                used += csiz;
                if ((err = _makefile_read_char(state, &cval, &csiz))) {
                    goto error;
                }
            }
            if ((err = _makefile_back_char(state, csiz, 0))) {
                goto error;
            }
            if ((cval == ':') || _makefile_iscloser(ctxt, state)) {
                break;
            }
            switch (oval) {
                case 'O': func = _gnumake_modifier_O;  break;
                case 'r': func = _gnumake_modifier_Or; break;
                case 'x': func = _gnumake_modifier_Ox; break;
            }
            _makefile_token_add_size(toke, used, 0);
            toke->func = func;
            *tokep = (_makefile_token_t*) toke;
            return 0;

        case 't':
            used = csiz; // sizeof(t)
            if ((err = _makefile_read_char(state, &cval, &csiz))) {
                goto error;
            } else if (strchr("AiuWws", cval)) {
                break;
            }
            oval  = cval;
            used += csiz;
            if ((err = _makefile_read_char(state, &cval, &csiz))
             || (err = _makefile_back_char(state, csiz, 0))
               )
            {
                goto error;
            }
            switch (oval) {
                case 'A': func = _gnumake_modifier_tA; break;
                case 'i': func = _gnumake_modifier_ti; break;
                case 'u': func = _gnumake_modifier_tu; break;
                case 'W': func = _gnumake_modifier_tW; break;
                case 'w': func = _gnumake_modifier_tw; break;
                case 's': func = _gnumake_modifier_ts; break;
            }
            oval = '\0';
            if ((oval == 's') && ! ((cval == ':') || _makefile_iscloser(ctxt, state))) {
                oval  = cval;
                used += csiz;
                if ((err = _makefile_read_char(state, &cval, &csiz))
                 || (err = _makefile_back_char(state, csiz, 0))
                   )
                {
                    goto error;
                }
            }
            if ((cval != ':') && ! _makefile_iscloser(ctxt, state)) {
                break;
            }
            toke->func = func;
            toke->xtra = oval;
            *tokep = (_makefile_token_t*) toke;
            return 0;

        case 's':
            used = csiz;
            if ((err = _makefile_read_char(state, &cval, &csiz))) {
                goto error;
            } else if (cval != 'h') {
                break;
            }
            used += csiz;
            if ((err = _makefile_read_char(state, &cval, &csiz))
             || (err = _makefile_back_char(state, csiz, 0))
               )
            {
                goto error;
            } else if ((cval != ':') && ! _makefile_iscloser(ctxt, state)) {
                break;
            }
            _makefile_token_add_size(toke, used, 0);
            toke->func = func;
            *tokep = (_makefile_token_t*) toke;
            return 0;

        case ':':
            used = csiz;
            if ((err = _makefile_read_char(state, &cval, &csiz))) {
                goto error;
            } else if (cval != ':') {
                break;
            }
            used += csiz;
            if ((err = _makefile_read_char(state, &cval, &csiz))) {
                goto error;
            }
            switch (cval) {
                case '=': func = _gnumake_modifier_colon_equal;          break;
                case ':': func = _gnumake_modifier_colon_colon_equal;    break;
                case '!': func = _gnumake_modifier_colon_bang_equal;     break;
                case '?': func = _gnumake_modifier_colon_question_equal; break;
                case '+': func = _gnumake_modifier_colon_plus_equal;     break;
                case '^': func = _gnumake_modifier_colon_caret_equal;    break;
                case '@': func = _gnumake_modifier_colon_at_equal;       break;
            }
            used += csiz;
            if (cval != '=') {
                if ((err = _makefile_read_char(state, &cval, &csiz))) {
                    goto error;
                } else if (cval != '=') {
                    break;
                }
                used += csiz;
            }
            if ((err = _makefile_read_char(state, &cval, &csiz))
             || (err = _makefile_back_char(state, csiz, 0))
               )
            {
                goto error;
            } else if ((cval != ':') && ! _makefile_iscloser(ctxt, state)) {
                break;
            }
            _makefile_token_add_size(toke, used, 0);
            toke->func = func;
            *tokep = (_makefile_token_t*) toke;
            return 0;

#define _LETTER(L)                                              \
    if ((err = _makefile_read_char(state, &csiz, &cval))) {     \
        goto error;                                             \
    } else if (cval != (L)) {                                   \
        break;                                                  \
    }                                                           \
    used += csiz;

        case 'h':
            used = csiz;
            _LETTER('a');
            _LETTER('s');
            _LETTER('h');
            if ((err = _makefile_read_char(state, &cval, &csiz))
             || (err = _makefile_back_char(state, csiz, 0))
               )
            {
                goto error;
            } else if ((csiz != ':') && ! _makefile_iscloser(ctxt, state)) {
                break;
            }
            toke->func = _gnumake_modifier_hash;
            *tokep = (_makefile_token_t*) toke;
            return 0;

        case '_':
            used = csiz;
            if ((err = _makefile_read_char(state, &cval, &csiz))
             || (err = _makefile_back_char(state, csiz, 0))
               )
            {
                goto error;
            } else if ((cval == ':') || _makefile_iscloser(ctxt, state)) {
                _makefile_token_add_size(toke, used, 0);
                toke->func = _gnumake_modifier__;
                *tokep = (_makefile_token_t*) toke;
                return 0;
            } else if (cval != '=') {
                break;
            }
            used += csiz;
            if ((err = _makefile_read_char(state, &cval, &csiz))
             || (err = _gnumake_lhs_word(ctxt, state, &part))
             || (err = _makefile_token_add_part(toke, &part))
             || (err = _makefile_read_char(state, &cval, &csiz))
             || (err = _makefile_back_char(state, csiz, 0))
               )
            {
                goto error;
            } else if ((cval != ':') && ! _makefile_iscloser(ctxt, state)) {
                break;
            }
            toke->func = _gnumake_modifier__VAR;
            *tokep = (_makefile_token_t*) toke;
            return 0;

        case 'g':
            used = csiz;
            _LETTER('m');
            _LETTER('t');
            _LETTER('i');
            _LETTER('m');
            _LETTER('e');
            if ((err = _makefile_read_char(state, &cval, &csiz))) {
                goto error;
            } else if (cval != '=') {
                if ((err = _makefile_back_char(state, csiz, 0))) {
                    goto error;
                } else if ((cval == ':') || _makefile_iscloser(ctxt, state)) {
                    _makefile_token_add_size(toke, used, 0);
                    toke->func = _gnumake_modifier_gmtime;
                    *toke = (_makefile_token_t*) toke;
                    return 0;
                } else {
                    break;
                }
            }
            used += csiz;
            _LETTER('u');
            _LETTER('t');
            _LETTER('c');
            if ((err = _makefile_read_char(state, &cval, &csiz))
             || (err = _makefile_back_char(state, csiz, 0))
               )
            {
                goto error;
            } else if ((cval != ':') && ! _makefile_iscloser(ctxt, state)) {
                break;
            }
            _makefile_token_add_size(toke, used, 0);
            toke->func = _gnumake_modifier_gmtime_utc;
            *toke = (_makefile_token_t*) toke;
            return 0;

        case 'l':
            used = csiz;
            _LETTER('o');
            _LETTER('c');
            _LETTER('a');
            _LETTER('l');
            _LETTER('t');
            _LETTER('i');
            _LETTER('m');
            _LETTER('e');
            if ((err = _makefile_read_char(state, &cval, &csiz))) {
                goto error;
            } else if (cval != '=') {
                if ((err = _makefile_back_char(state, csiz, 0))) {
                    goto error;
                } else if ((cval == ':') || _makefile_iscloser(ctxt, state)) {
                    _makefile_token_add_size(toke, used, 0);
                    toke->func = _gnumake_modifier_localtime;
                    *toke = (_makefile_token_t*) toke;
                    return 0;
                } else {
                    break;
                }
            }
            used += csiz;
            _LETTER('u');
            _LETTER('t');
            _LETTER('c');
            if ((err = _makefile_read_char(state, &cval, &csiz))
             || (err = _makefile_back_char(state, csiz, 0))
               )
            {
                goto error;
            } else if ((cval != ':') && ! _makefile_iscloser(ctxt, state)) {
                break;
            }
            _makefile_token_add_size(toke, used, 0);
            toke->func = _gnumake_modifier_localtime_utc;
            *toke = (_makefile_token_t*) toke;
            return 0;

        case 'D':
        case 'U':
            _makefile_token_add_size(toke, csiz, 0);
            for ( ; ; ) {
                if (! (err = _makefile_hspace(ctxt, state, &part))) {
                    if ((err = _makefile_token_add_part(toke, &part))) {
                        goto error;
                    }
                }
                if (! (err = _gnumake_lhs_word(ctxt, state, &part))) {
                    if ((err = _makefile_token_add_part(toke, &part))) {
                        goto error;
                    }
                } else {
                    break;
                }
            }
            toke->func = (
                (cval == 'D')
                    ? _gnumake_modifier_D
                    : _gnumake_modifier_U
            );
            *tokep = (_makefile_token_t*) toke;
            return 0;

        case '[':
            used = csiz;
            if ((err = _makefile_read_char(state, &cval, &csiz))) {
                goto error;
            } else if ((cval == '*') || (cval == '#')) {
                _makefile_token_add_size(toke, (used + csiz), 0);
                toke->xtra = cval;
            } else if ((err = _shell_number(ctxt, state, &part))) {
                goto error;
            } else if ((toke->xlhs = part), 0) {
                goto error;
            } else if ((err = _makefile_token_add_part(toke, &part))) {
                goto error;
            } else if ((err = _makefile_read_char(state, &cval, &csiz))) {
                goto error;
            } else if ((cval != '.') && (cval != ']')) {
                break;
            } else if (cval == '.') {
                used = csiz;
                if ((err = _makefile_read_char(state, &cval, &csiz))) {
                    goto error;
                } else if (cval != '.') {
                    break;
                }
                _makefile_token_add_size(toke, (used + csiz), 0);
                if ((err = _shell_number(ctxt, state, &part))) {
                    goto error;
                } else if ((toke->xrhs = part), 0) {
                } else if ((err = _makefile_token_add_part(toke, &part))) {
                    goto error;
                }
            }
            if ((err = _makefile_read_char(state, &cval, &csiz))) {
                goto error;
            } else if (err != ']') {
                break;
            } else {
                _makefile_token_add_size(toke, csiz, 0);
            }
            if ((err = _makefile_read_char(state, &cval, &csiz))
             || (err = _makefile_back_char(state, csiz, 0))
               )
            {
                goto error;
            } else if ((cval != ':') && ! _makefile_iscloser(ctxt, state)) {
                break;
            }
            toke->func = _gnumake_modifier_indices;
            *tokep = (_makefile_token_t*) toke;
            return 0;

        case '!':
        case '?':
            oval = cval;
            _makefile_token_add_size(toke, csiz, 0);
            if ((err = _gnumake_make_modifier_token(state, &word))) {
                goto error;
            }
            for ( ; ; ) {
                if ((err = _makefile_read_char(state, &cval, &csiz))) {
                    goto error;
                }
                switch (cval) {
                    case ':':
                    case '!':
                        if ((cval == ':') || ((oval == '!') && (cval == '!'))) {
                            break;
                        }
                        _makefile_token_add_size(word, csiz, 0);
                        continue;

                    case '\\':
                        if ((err = _makefile_back_char(state, csiz, 0))) {
                            goto error;
                        } else if (! (err = _makefile_continuation(ctxt, state, &part))
                                || ! (err = _gnumake_escape_quoted(ctxt, state, &part, "\\\"'`$!(){}[]:"))
                                  )
                        {
                            if ((err = _makefile_token_add_part(word, &part))) {
                                goto error;
                            }
                        } else if ((err = _makefile_read_char(state, &cval, &csiz))) {
                            goto error;
                        } else {
                            _makefile_token_add_size(word, csiz, 0);
                        }
                        continue;

                    case '\r':
                    case '\n':
                        if ((err = _makefile_back_char(state, csiz, 0))) {
                            goto error;
                        } else if ((err = _makefile_endline(ctxt, state, &part))) {
                            goto error;
                        } else if ((err = _makefile_token_add_part(word, &part))) {
                            goto error;
                        }
                        continue;

                    case '\'':
                        if ((err = _makefile_back_char(state, csiz, 0))) {
                            goto error;
                        } else if (! (err = _gnumake_single_quoted(ctxt, state, &part))) {
                            if ((err = _makefile_token_add_part(word, &part))) {
                                goto error;
                            }
                        } else if ((err = _makefile_read_char(state, &cval, &csiz))) {
                            goto error;
                        } else {
                            _makefile_token_add_size(word, csiz, 0);
                        }
                        continue;

                    case '"':
                        if ((err = _makefile_back_char(state, csiz, 0))) {
                            goto error;
                        } else if (! (err = _gnumake_double_quoted(ctxt, state, &part))) {
                            if ((err = _makefile_token_add_part(word, &part))) {
                                goto error;
                            }
                        } else if ((err = _makefile_read_char(state, &cval, &csiz))) {
                            goto error;
                        } else {
                            _makefile_token_add_size(word, csiz, 0);
                        }
                        continue;

                    case '`':
                        if ((err = _makefile_back_char(state, csiz, 0))) {
                            goto error;
                        } else if (! (err = _gnumake_system_quoted(ctxt, state, &part))) {
                            if ((err = _makefile_token_add_part(word, &part))) {
                                goto error;
                            }
                        } else if ((err = _makefile_read_char(state, &cval, &csiz))) {
                            goto error;
                        } else {
                            _makefile_token_add_size(word, csiz, 0);
                        }
                        continue;

                    case '$':
                        if ((err = _makefile_back_char(state, csiz, 0))) {
                            goto error;
                        } else if ((err = _gnumake_dollar(ctxt, state, &part))) {
                            goto error;
                        } else if ((err = _makefile_token_add_part(word, &part))) {
                            goto error;
                        }
                        continue;
                }
                if (! toke->xlhs) {
                    toke->xlhs = word;
                } else {
                    toke->xrhs = word;
                }
                if ((err = _makefile_token_add_part(toke, &word))) {
                    goto error;
                } else if ((oval == '?') && (cval == ':') && ! toke->xrhs) {
                    _makefile_token_add_size(toke, csiz, 0);
                    continue;
                } else if (cval == ':') {
                    if ((err = _makefile_back_char(state, csiz, 0))) {
                        goto error;
                    }
                }
                break;
            }
        
            if ((err = _makefile_read_char(ctxt, state, &part))
             || (err = _makefile_back_char(ctxt, csiz, 0))
               )
            {
                goto error;
            } else if ((cval != ':') && ! _makefile_iscloser(ctxt, state)) {
                break;
            }
            toke->func = (
                (oval == '?')
                    ? _gnumake_modifier_question_colon
                    : _gnumake_modifier_bang
            );
            *tokep = (_makefile_token_t*) toke;
            return 0;

        case 'r':
            used = csiz;
            _LETTER('a');
            _LETTER('n');
            _LETTER('g');
            _LETTER('e');
            if ((err = _makefile_read_char(state, &cval, &csiz))) {
                goto error;
            } else if (cval == '=') {
                _makefile_token_add_size(toke, csiz, 0);
                if ((err = _shell_number(ctxt, state, &part))) {
                    goto error;
                }
                toke->xlhs = part;
                if ((err = _makefile_token_add_part(toke, &part))) {
                    goto error;
                }
            }
            if ((err = _makefile_read_char(state, &cval, &csiz))
             || (err = _makefile_back_char(state, csiz, 0))
               )
            {
                goto error;
            } else if ((csiz != cval) && ! _makefile_iscloser(ctxt, state)) {
                break;
            }
            _makefile_token_add_size(toke, csiz, 0);
            toke->func = _gnumake_modifier_range;
            *tokep = (_makefile_token_t*) toke;
            return 0;

        case '@':
        case '@temp@string@'

        case 'S':
        case 'C':
        case 'S/old/new/1gW'
        case 'C/old/new/1gW'

        case 'old=new'
    }

}

/* $(...), ${...}, $[...] */
int
_gnumake_subst_name (
    _makefile_context_t*    ctxt,
    _makefile_state_t*      state,
    _makefile_token_t**     tokep
    )
{
    _makefile_varref_token_t* toke = NULL;
    _makefile_token_t*        part = NULL;
    _makefile_token_t*        farg = NULL;
    _makefile_token_t*        hspc = NULL;
    _makefile_token_type_t*   type = NULL;
    _makefile_char_t          cval;
    _makefile_size_t          csiz;
    const char*               shut;
    int                       err  = 0;

    if ((err = _makefile_make_token(state, &toke, NULL))) {
        return err;
    } else if ((err = _makefile_make_token(state, &part, NULL))) {
        return err;
    } else if ((err = _makefile_read_char(state, &cval, &csiz))) {
        goto error;
    } else if (cval != '$') {
        err = EINVAL;
        goto error;
    } else if (_makefile_token_add_size(part, csiz, 0), 0) {
    } else if ((err = _makefile_read_char(state, &cval, &csiz))) {
        goto error;
    }

    switch (cval) {
        case '(':
        case '{':
            shut = ((cval == '(') ? ")" : "}");
            type = &_gnumake_delay_subst_name_shut_op_token_type;
            _makefile_token_set_type(part, &_gnumake_delay_subst_name_open_op_token_type);
            break;
        case '[':
            shut = "]";
            type = &_gnumake_immed_subst_name_shut_op_token_type;
            _makefile_token_set_type(part, &_gnumake_immed_subst_name_open_op_token_type);
            break;
        default:
            err = EINVAL;
            goto error;
    }

    if ((err = _makefile_push_delim(ctxt, state, part, shut))) {
        goto error;
    } else if ((toke->open = part), (err = _makefile_token_add_part((_makefile_token_t*) toke, &part))) {
        goto error;
    } else if ((err = _gnumake_lhs_word(ctxt, state, &part))) {
        goto error;
    } else if ((toke->name = part), (err = _makefile_token_add_part((_makefile_token_t*) toke, &part))) {
        goto error;
    }

    if (! (err = _makefile_hspace(ctxt, state, NULL))) {
        if (type == &_gnumake_delay_subst_name_shut_op_token_type) {
            _makefile_token_set_type(toke, &_gnumake_delay_funcall_token_type);
        } else {
            _makefile_token_set_type(toke, &_gnumake_immed_funcall_token_type);
        }
        for ( ; ; ) {
            if ((err = _makefile_make_token(state, &farg, &_gnumake_funarg_token_type))) {
                goto error;
            }
            for ( ; ; ) {
                if ((err = _gnumake_fun_word(ctxt, state, &part))) {
                    break;
                } else if (hspc && (err = _makefile_token_add_part(farg, &hspc))) {
                    goto error;
                } else if ((err = shmoo_vector_insert_tail(farg, &part))) {
                    goto error;
                } else if ((err = _makefile_hspace(ctxt, state, &hspc))) {
                    break;
                }
            }
            _makefile_dest_token(&hspc);
            if ((err = shmoo_vector_insert_tail(&toke->more, farg))) {
                goto error;
            } else if ((err = _makefile_read_char(state, &cval, &csiz))) {
                goto error;
            } else if (cval == ',') {
                (void) _makefile_hspace(ctxt, state, NULL);
                continue;
            } else if ((err = _makefile_back_char(state, csiz, 0))) {
                goto error;
            } else {
                break;
            }
        }
    } else {
        if (type == &_gnumake_delay_subst_name_shut_op_token_type) {
            _makefile_token_set_type(toke, &_gnumake_delay_varsubst_token_type);
        } else {
            _makefile_token_set_type(toke, &_gnumake_immed_varsubst_token_type);
        }
        for ( ; ; ) {
            if ((err = _gnumake_modifier(ctxt, state, &part))) {
                break;
            } else if ((err = shmoo_vector_insert_tail(&toke->more, part))) {
                goto error;
            } else {
                part = NULL;
            }
        }
    }

    
    if ((err = _makefile_make_token(state, &part, type))) {
        goto error;
    } else if ((err = _makefile_read_delim(ctxt, state, &csiz))) {
        goto error;
    } else if (_makefile_token_add_size(part, csiz, 0), 0) {
    } else if ((toke->shut = part), (err = _makefile_token_add_part(toke, &part))) {
        goto error;
    }

    *tokep = toke;
    return 0;

error:
    _makefile_back_token(state, toke);
    _makefile_dest_token(&hspc);
    _makefile_dest_token(&part);
    _makefile_dest_token(&farg);
    _makefile_dest_token(&toke);
    return err;
}

int
_gnumake_autovar_lessthan (
    )
{
}

int
_gnumake_autovar_greaterthan (
    )
{
}

int
_gnumake_autovar_caret (
    )
{
}

int
_gnumake_autovar_plus (
    )
{
}

int
_gnumake_autovar_at (
    )
{
}

int
_gnumake_autovar_pipe (
    )
{
}

int
_gnumake_autovar_question (
    )
{
}

int
_gnumake_autovar_star (
    )
{
}

int
_gnumake_autovar_percent (
    )
{
}

int
_gnumake_autovar_pound (
    )
{
}

int
_gnumake_autovar_bang (
    )
{
}

int
_gnumake_iscloser (
    _makefile_context_t*        ctxt,
    _makefile_state_t*          state
    )
{
}

int
__gnumake_word (
    _makefile_context_t*        ctxt,
    _makefile_state_t*          state,
    _makefile_token_t**         tokep,
    const char*                 term
    )
{
    _makefile_token_t* toke = NULL;
    _makefile_token_t* part = NULL;
    int                err  = 0;

    if ((err = _makefile_make_token(state, &toke, &_gnumake_word_token_type))) {
        return err;
    } else if ((err = _makefile_read_char(state, &cval, &csiz))) {
        goto error;
    } else if (_makefile_isspace(ctxt, cval) || (cval == '#') || strchr(term, cval)) {
        /* Beginning of word cannot be a terminator, # or space */
        err = EINVAL;
        goto error;
    } else {
        _makefile_token_add_size(toke, csiz, 0);
    }

    for ( ; ; ) {
        /* Allows '#' in the middle of a word, shell-compatible. */
        if ((err = _makefile_read_char(state, &cval, &csiz))) {
            break;
        } else if (_makefile_isspace(ctxt, cval) || strchr(term, cval)) {
            if ((err = _makefile_back_char(state, csiz, 0))) {
                goto error;
            }
            break;
        }

        switch (cval) {
            case '\\':
                if ((err = _makefile_back_char(state, csiz, 0))) {
                    goto error;
                } else if (! (err = _makefile_continuation(ctxt, state, &part))
                        || ! (err = _gnumake_escape_quoted(ctxt, state, &part, "\\'`\":\t ()[]{}<>="))
                          )
                {
                    if ((err = _makefile_token_add_part(toke, &part))) {
                        goto error;
                    }
                } else if ((err = _makefile_read_char(state, &cval, &csiz))) {
                    goto error;
                } else {
                    _makefile_token_add_size(toke, csiz, 0);
                }
                continue;

            case '\'':
                if ((err = _makefile_back_char(state, csiz, 0))) {
                    goto error;
                } else if ((err = _gnumake_single_quoted(ctxt, state, &part))) {
                    if ((err = _makefile_read_char(state, csiz, 0))) {
                        goto error;
                    }
                    _makefile_token_add_size(toke, csiz, 0);
                }
                continue;

            case '"':
                if ((err = _makefile_back_char(state, csiz, 0))) {
                    goto error;
                } else if ((err = _gnumake_double_quoted(ctxt, state, &part))) {
                    if ((err = _makefile_read_char(state, csiz, 0))) {
                        goto error;
                    }
                    _makefile_token_add_size(toke, csiz, 0);
                }
                continue;

            case '`':
                if ((err = _makefile_back_char(state, csiz, 0))) {
                    goto error;
                } else if ((err = _gnumake_system_quoted(ctxt, state, &part))) {
                    if ((err = _makefile_read_char(state, csiz, 0))) {
                        goto error;
                    }
                    _makefile_token_add_size(toke, csiz, 0);
                }
                continue;

            case '%':
                if (_makefile_token_get_type(toke) == &_gnumake_pattern_token_type) {
                    _makefile_token_add_size(toke, csiz, 0);
                } else if ((err = _makefile_back_char(state, csiz, 0))) {
                    goto error;
                } else if ((err = _gnumake_percent(ctxt, state, &part))) {
                    goto error;
                } else if ((err = _makefile_token_add_part(toke, &part))) {
                    goto error;
                } else {
                    _makefile_token_set_type(toke, &_gnumake_pattern_token_type);
                }
                continue;

            case '$':
                if ((err = _makefile_back_char(state, csiz, 0))) {
                    goto error;
                } else if ((err = _gnumake_dollar(ctxt, state, &part))) {
                    goto error;
                } else if ((err = _makefile_token_add_part(toke, &part))) {
                    goto error;
                }
                continue;

            case '[':
            case '{':
            case '(':
            case '<':
                if ((err = _makefile_back_char(state, csiz, 0))) {
                    goto error;
                } else if (! (err = _gnumake_nested(ctxt, state, &part))) {
                    if ((err = _makefile_token_add_part(toke, &part))) {
                        goto error;
                    }
                } else if ((err = _makefile_read_char(state, &cval, &csiz))) {
                    goto error;
                } else {
                    _makefile_token_add_size(toke, csiz, 0);
                }
                continue;

            case ']':
            case '}':
            case ')':
            case '>':
                if ((err = _makefile_back_car(state, csiz, 0))) {
                    goto error;
                } else if (_gnumake_iscloser(ctxt, state)) {
                    break;
                } else if ((err = _makefile_read_char(state, &cval, &csiz))) {
                    goto error;
                } else {
                    _makefile_token_add_size(toke, csiz, 0);
                }
                continue;
        }
    }

    *tokep = toke;
    return 0;

error:
    _makefile_back_token(state, toke);
    _makefile_dest_token(&part);
    _makefile_dest_token(&toke);
    return err;
}

int
_gnumake_depends (
    _makefile_context_t*        ctxt,
    _makefile_state_t*          state,
    _makefile_token_t**         tokep
    )
{
    _makefile_token_t* toke = NULL;
    _makefile_token_t* part = NULL;
    int                err  = 0;

    if ((err = _makefile_make_token(state, &toke, &_gnumake_depends_token_type))) {
        goto error;
    }
    for ( ; ; ) {
        if ((err = _gnumake_dep_word(ctxt, state, &part))) {
            break;
        } else if (_makefile_token_add_part(deps, &part)) {
            goto error;
        }
        (void) _makefile_hspace(ctxt, state, NULL);
    }

    *tokep = toke;
    return 0;

error:
    _makefile_back_token(state, toke);
    _makefile_dest_token(&part);
    _makefile_dest_token(&toke);
    return err;
}

int
_gnumake_orderonly_depends (
    _makefile_context_t*        ctxt,
    _makefile_state_t*          state,
    _makefile_token_t**         tokep
    )
{
    _makefile_token_t* toke = NULL;
    _makefile_token_t* part = NULL;
    int                err  = 0;

    if ((err = _makefile_make_token(state, &toke, &_gnumake_orderonly_depends_token_type))) {
        goto error;
    } else if ((err = _gnumake_pipe_op(ctxt, state, &part))) {
        goto error;
    } else if ((err = _makefile_token_add_part(toke, &part))) {
        goto error;
    }

    for ( ; ; ) {
        (void) _makefile_hspace(ctxt, state, NULL);
        if ((err = _gnumake_dep_word(ctxt, state, &part))) {
            if (err != ENOENT) {
                goto error;
            }
            break;
        } else if ((err = _makefile_token_add_part(toke, &part))) {
            goto error;
        }
    }

    *tokep = toke;
    return 0;

error:
    _makefile_back_token(state, toke);
    _makefile_dest_token(&part);
    _makefile_dest_token(&toke);
    return err;
}

int
_gnumake_static_pattern_rule_tail (
    _makefile_context_t*        ctxt,
    _makefile_state_t*          state,
    _makefile_rule_token_t*     rule
    )
{
    _makefile_token_t* patt = NULL;
    _makefile_token_t* part = NULL;
    int                err  = 0;

    if ((err = _makefile_make_token(state, &patt, &_gnumake_static_pattern_rule_token_type))) {
        return err;
    }

    /* Read a single pattern. */
    if (! (err = _gnumake_dep_word(ctxt, state, &part))) {
        if ((err = _makefile_token_add_part(patt, &part))) {
            goto error;
        }
    }
    rule->patt = patt;
    if ((err = _makefile_token_add_part((_makefile_token_t*) rule, &patt))) {
        goto error;
    }
    (void) _makefile_hspace(ctxt, state, NULL);

    /* Require a single colon after the pattern. */
    if ((err = _gnumake_single_colon(ctxt, state, &part))) {
        goto error;
    }
    (void) _makefile_hspace(ctxt, state, NULL);

    if ((err = _gnumake_depends(ctxt, state, &part))) {
        goto error;
    }
    rule->deps = part;
    if ((err = _makefile_token_add_part((_makefile_token_t*) rule, &part))) {
        goto error;
    }
    (void) _makefile_hspace(ctxt, state, NULL);

    if ((err = _gnumake_pipe_op(ctxt, state, &part))) {
        goto error;
    }
    rule->oron = part;
    if ((err = _makefile_token_add_part((_makefile_token_t*) rule, &part))) {
        goto error;
    }

    *rulep = (_makefile_token_t*) rule;
    return 0;

error:
    _makefile_back_token(state, patt);
    _makefile_dest_token(&part);
    _makefile_dest_token(&patt);
    return err;
}

int
_gnumake_normal_rule_tail (
    _makefile_context_t*        ctxt,
    _makefile_state_t*          state,
    _makefile_rule_token_t*     rule
    )
{
    _makefile_directive_f func = NULL;
    _makefile_token_t*    part = NULL;
    _makefile_token_t     here = { 0 };
    int                   err  = 0;

    func = _gnumake_directive(
        ctxt, state, NULL,
        "override", "private",
        "export", "unexport",
        "define", "undefine"
    );
    if (func) {
        if ((err = (*func)(ctxt, state, &part))) {
            goto error;
        }
        rule->assi = part;
        if ((err = _makefile_token_add_part(rule, &part))) {
            goto error;
        }
        return 0;
    } else if (! (err = _gnumake_assignment(ctxt, state, &part))) {
        rule->assi = part;
        if ((err = _makefile_token_add_part(rule, &part))) {
            goto error;
        }
        return 0;
    }

    /* Mark the spot to which we want to return, if
     * we fail to match the rest of a rule.
     */
    if ((err = _makefile_init_token(state, &here, NULL))) {
        goto error;
    }

    if (! (err = _gnumake_depends(ctxt, state, &part))) {
        rule->deps = part;
        if ((err = _makefile_token_add_part((_makefile_token_t*) rule, &part))) {
            goto error;
        }
    }
    (void) _makefile_hspace(ctxt, state, NULL);

    if (! (err = _gnumake_orderonly_depends(ctxt, state, &part))) {
        rule->oron = part;
        if ((err = _makefile_token_add_part((_makefile_token_t*) rule, &part))) {
            goto error;
        }
    }
    (void) _makefile_hspace(ctxt, state, NULL);

    if (! (err = _gnumake_semicolon_op(ctxt, state, &part))) {
        if ((err = _makefile_token_add_part((_makefile_token_t*) rule, &part))) {
            goto error;
        }
        (void) _makefile_hspace(ctxt, state, NULL);
        if ((err = _makefile_rule_add_line(rule, state))) {
            goto error;
        }
    }

    return 0;

error:
    _makefile_back_token(state, &here);
    _makefile_dest_token(&part);
    _makefile_free_token(&here);
    return err;
}

int
_gnumake_targets (
    _makefile_context_t*    ctxt,
    _makefile_state_t*      state,
    _makefile_token_t**     tokep
    )
{
    _makefile_token_t* toke = NULL;
    _makefile_token_t* hspc = NULL;
    _makefile_token_t* part = NULL;
    int                err  = 0;

    if ((err = _makefile_make_token(state, &toke, &_gnumake_targets_token_type))) {
        goto error;
    }

    for ( ; ; ) {
        if (! (err = _gnumake_tgt_patt(ctxt, state, &part))) {
            if (hspc && (err = _makefile_token_add_part(toke, &hspc))) {
                goto error;
            } else if ((err = _makefile_token_add_part(toke, &part))) {
                goto error;
            }
        } else {
            break;
        }

        if ((err = _makefile_hspace(ctxt, state, &hspc))) {
            break;
        }
    }
    _makefile_dest_token(&hspc);

    *tokep = toke;
    return 0;

error:
    _makefile_back_token(state, toke);
    _makefile_dest_token(&part);
    _makefile_dest_token(&hspc);
    _makefile_dest_token(&toke);
    return err;
}

int
_gnumake_parse_rule (
    _makefile_context_t*        ctxt,
    _makefile_state_t*          state,
    _makefile_token_t**         rulep
    )
{
    _makefile_directive_f   func = NULL;
    _makefile_rule_token_t* rule = NULL;
    _makefile_token_t*      part = NULL;
    int                     err  = 0;

    if ((err = _makefile_state_make_rule_token(state, &rule))) {
        return err;
    }

    if ((err = _gnumake_targets(ctxt, state, &part))) {
        goto error;
    }
    toke->tgts = part;
    if ((err = _makefile_token_add_part(toke, &part))) {
        goto error;
    }

    if (! (err = _gnumake_double_colon(ctxt, state, &part))) {
        rule->oper = part;
        if ((err = _makefile_token_add_part((_makefile_token_t*) rule, &part))) {
            goto error;
        } else if ((err = _gnumake_normal_rule_tail(ctxt, state, rule))) {
            goto error;
        } else {
            _makefile_token_set_type(rule, &_gnumake_double_colon_rule_token_type);
        }
    } else if (! (err = _gnumake_single_colon(ctxt, state, &part))) {
        rule->oper = part;
        if ((err = _makefile_token_add_part((_makefile_token_t*) rule, &part))) {
            goto error;
        } else if (! (err = _gnumake_static_pattern_rule_tail(ctxt, state, rule))) {
            _makefile_token_set_type(rule, &_gnumake_static_pattern_rule_token_type);
        } else if (! (err = _gnumake_normal_rule_tail(ctxt, state, rule))) {
            _makefile_token_set_type(rule, &_gnumake_single_colon_rule_token_type);
        } else {
            goto error;
        }
    }

    *rulep = (_makefile_token_t*) rule;
    return 0;

error:
    _makefile_back_token(state, rule);
    _makefile_dest_token(&part);
    _makefile_dest_rule_token(&rule);
    return err;
}

int
_gnumake_if_expr (
    _makefile_context_t*    ctxt,
    _makefile_state_t*      state,
    _makefile_token_t**     tokep
    )
{
}

int
_gnumake_if (
    _makefile_context_t*    ctxt,
    _makefile_state_t*      state,
    _makefile_token_t**     tokep
    )
{
    _makefile_token_t* toke = NULL;
    _makefile_token_t* part = NULL;
    int                err = 0;

    if ((err = _gnumake_make_directive_token(state, &toke, &_gnumake_if_token_type))) {
        return err;
    } else if ((err = _gnumake_directive(ctxt, state, &part, "if"))) {
        goto error;
    }
    toke->keyw = part;
    if ((err = _makefile_token_add_part((_makefile_token_t*) toke, &part))) {
        goto error;
    } else if ((err = _makefile_hspace(ctxt, state, NULL))) {
        goto error;
    } else if ((err = _gnumake_if_expr(ctxt, state, &part))) {
        goto error;
    }
    toke->nest = part;
    if ((err = _makefile_token_add_part((_makefile_token_t*) toke, &part))) {
        goto error;
    }

    *tokep = (_makefile_token_t*) toke;
    return 0;

error:
    _makefile_back_token(state, (const _makefile_token_t*) toke);
    _makefile_dest_token(&part);
    _gnumake_dest_token((_makefile_token_t*) dest_directive_token(&toke);
    return err;
}

int
_ifeq_expr (
    _makefile_context_t*    ctxt,
    _makefile_state_t*      state,
    _makefile_token_t**     tokep
    )
{
}

int
_gnumake_ifeq (
    _makefile_context_t*    ctxt,
    _makefile_state_t*      state,
    _makefile_token_t**     tokep
    )
{
    _makefile_token_t* toke = NULL;
    _makefile_token_t* part = NULL;
    int                err = 0;

    if ((err = _gnumake_make_directive_token(state, &toke, &_gnumake_ifeq_token_type))) {
        return err;
    } else if ((err = _gnumake_directive(ctxt, state, &part, "ifeq"))) {
        goto error;
    }
    toke->keyw = part;
    if ((err = _makefile_token_add_part((_makefile_token_t*) toke, &part))) {
        goto error;
    } else if ((err = _makefile_hspace(ctxt, state, NULL))) {
        goto error;
    } else if ((err = _gnumake_ifeq_expr(ctxt, state, &part))) {
        goto error;
    }
    toke->nest = part;
    if ((err = _makefile_token_add_part((_makefile_token_t*) toke, &part))) {
        goto error;
    }

    *tokep = (_makefile_token_t*) toke;
    return 0;

error:
    _makefile_back_token(state, (const _makefile_token_t*) toke);
    _makefile_dest_token(&part);
    _makefile_dest_token((_makefile_token_t*) dest_directive_token(&toke);
    return err;
}

int
_gnumake_ifneq (
    _makefile_context_t*    ctxt,
    _makefile_state_t*      state,
    _makefile_token_t**     tokep
    )
{
    _makefile_token_t* toke = NULL;
    _makefile_token_t* part = NULL;
    int                err = 0;

    if ((err = _gnumake_make_directive_token(state, &toke, &_gnumake_ifneq_token_type))) {
        return err;
    } else if ((err = _gnumake_directive(ctxt, state, &part, "ifneq"))) {
        goto error;
    }
    toke->keyw = part;
    if ((err = _makefile_token_add_part((_makefile_token_t*) toke, &part))) {
        goto error;
    } else if ((err = _makefile_hspace(ctxt, state, NULL))) {
        goto error;
    } else if ((err = _gnumake_ifeq_expr(ctxt, state, &part))) {
        goto error;
    }
    toke->nest = part;
    if ((err = _makefile_token_add_part((_makefile_token_t*) toke, &part))) {
        goto error;
    }

    *tokep = toke;
    return 0;

error:
    _makefile_back_token(state, (const _makefile_token_t*) toke);
    _makefile_dest_token(&part);
    _makefile_dest_token((_makefile_token_t*) dest_directive_token(&toke);
    return err;
}

int
_gnumake_ifdef (
    _makefile_context_t*    ctxt,
    _makefile_state_t*      state,
    _makefile_token_t**     tokep
    )
{
    _makefile_token_t* toke = NULL;
    _makefile_token_t* part = NULL;
    int                err = 0;

    if ((err = _gnumake_make_directive_token(state, &toke, &_gnumake_ifdef_token_type))) {
        return err;
    } else if ((err = _gnumake_directive(ctxt, state, &part, "ifdef"))) {
        goto error;
    }
    toke->keyw = part;
    if ((err = _makefile_token_add_part((_makefile_token_t*) toke, &part))) {
        goto error;
    } else if ((err = _makefile_hspace(ctxt, state, NULL))) {
        goto error;
    } else if ((err = _gnumake_rhs_word(ctxt, state, &part))) {
        goto error;
    }
    toke->nest = part;
    if ((err = _makefile_token_add_part((_makefile_token_t*) toke, &part))) {
        goto error;
    }

    *tokep = (_makefile_token_t*) toke;
    return 0;

error:
    _makefile_back_token(state, (const _makefile_token_t*) toke);
    _makefile_dest_token(&part);
    _makefile_dest_token((_makefile_token_t*) dest_directive_token(&toke);
    return err;
}

int
_gnumake_ifndef (
    _makefile_context_t*    ctxt,
    _makefile_state_t*      state,
    _makefile_token_t**     tokep
    )
{
    _makefile_token_t* toke = NULL;
    _makefile_token_t* part = NULL;
    int                err = 0;

    if ((err = _gnumake_make_directive_token(state, &toke, &_gnumake_ifndef_token_type))) {
        return err;
    } else if ((err = _gnumake_directive(ctxt, state, &part, "ifndef"))) {
        goto error;
    }
    toke->keyw = part;
    if ((err = _makefile_token_add_part((_makefile_token_t*) toke, &part))) {
        goto error;
    } else if ((err = _makefile_hspace(ctxt, state, NULL))) {
        goto error;
    } else if ((err = _gnumake_rhs_word(ctxt, state, &part))) {
        goto error;
    }
    toke->nest = part;
    if ((err = _makefile_token_add_part((_makefile_token_t*) toke, &part))) {
        goto error;
    }

    *tokep = (_makefile_token_t*) toke;
    return 0;

error:
    _makefile_back_token(state, (const _makefile_token_t*) toke);
    _makefile_dest_token(&part);
    _makefile_dest_token((_makefile_token_t*) dest_directive_token(&toke);
    return err;
}

int
_gnumake_else (
    _makefile_context_t*    ctxt,
    _makefile_state_t*      state,
    _makefile_token_t**     tokep
    )
{
    _gnumake_directive_f        func = NULL;
    _gnumake_directive_token_t* toke = NULL;
    _makefile_token_t*          part = NULL;
    int                         err  = 0;

    if ((err = _gnumake_make_directive_token(state, &toke, &_gnumake_else_token_type))) {
        return err;
    } else if ((err = _gnumake_directive(ctxt, state, &part, "else"))) {
        goto error;
    }
    toke->keyw = part;
    if ((err = _makefile_token_add_part((_makefile_token_t*) toke, &part))) {
        goto error;
    } else if ((err = _makefile_hspace(ctxt, state, NULL))) {
        goto error;
    }

    func = _gnumake_directive(
        ctxt, state, NULL, "if", "ifeq", "ifneq", "ifdef", "ifndef"
    );
    if (func) {
        if ((err = (*func)(ctxt, state, &part))) {
            goto error;
        }
        toke->nest = part;
        if ((err = _makefile_token_add_part((_makefile_token_t*) toke, &part))) {
            goto error;
        }
    }

    *tokep = (_makefile_token_t*) toke;
    return 0;

error:
    _makefile_back_token(state, (const _makefile_token_t*) toke);
    _makefile_dest_token(&part);
    _makefile_dest_token((_makefile_token_t*) dest_directive_token(&toke);
    return err;
}

int
_gnumake_endif (
    _makefile_context_t*    ctxt,
    _makefile_state_t*      state,
    _makefile_token_t**     tokep
    )
{
    _gnumake_directive_token_t* toke = NULL;
    _makefile_token_t*          part = NULL;
    int                         err  = 0;

    if ((err = _gnumake_make_directive_token(state, &toke, &_gnumake_endif_token_type))) {
        return err;
    } else if ((err = _gnumake_directive(ctxt, state, &part, "endif"))) {
        goto error;
    }
    toke->keyw = part;
    if ((err = _makefile_token_add_part((_makefile_token_t*) toke, &part))) {
        goto error;
    }

    *tokep = (_makefile_token_t*) toke;
    return 0;

error:
    _makefile_back_token(state, (const _makefile_token_t*) toke);
    _makefile_dest_token(&part);
    _gnumake_dest_token((_makefile_token_t*) dest_directive_token(&toke);
    return err;
}

int
_gnumake_define (
    _makefile_context_t*    ctxt,
    _makefile_state_t*      state,
    _makefile_token_t**     tokep
    )
{
    _gnumake_definition_token_t* toke = NULL;
    _makefile_token_t*           part = NULL;
    int                          err  = 0;

    if ((err = _gnumake_make_definition_token(state, &toke, &_gnumake_define_token_type))) {
        return err;
    } else if ((err = _gnumake_directive(ctxt, state, &part, "define"))) {
        goto error;
    }
    toke->keyw = part;
    if ((err = _makefile_token_add_part((_makefile_token_t*) toke, &part))) {
        goto error;
    } else if ((err = _makefile_hspace(ctxt, state, NULL))) {
        goto error;
    } else if ((err = _gnumake_lhs_word(ctxt, state, &part))) {
        goto error;
    }
    toke->name = part;
    if ((err = _makefile_token_add_part((_makefile_token_t*) toke, &part))) {
        goto error;
    } else if ((err = _makefile_hspace(ctxt, state, NULL))) {
        goto error;
    }

    if (! (err = _gnumake_assign_op(ctxt, state, &part))) {
        toke->oper = part;
        if ((err = _makefile_token_add_part((_makefile_token_t*) toke, &part))) {
            goto error;
        }
    }

    if ((err = _makefile_make_token(ctxt, state, &part, &_gnumake_linelist_token_type))) {
        goto error;
    }
    toke->line = part;
    if ((err = _makefile_token_add_part((_makefile_token_t*) toke, &part))) {
        goto error;
    }

    *tokep = (_makefile_token_t*) toke;
    return 0;

error:
    _makefile_back_token(state, (const _makefile_token_t*) toke);
    _makefile_dest_token(&part);
    _gnumake_dest_definition_token(&toke);
    return err;
}

int
_gnumake_endef (
    _makefile_context_t*    ctxt,
    _makefile_state_t*      state,
    _makefile_token_t**     tokep
    )
{
    _gnumake_directive_token_t* toke = NULL;
    _makefile_token_t*          part = NULL;
    int                         err  = 0;

    if ((err = _gnumake_make_directive_token(state, &toke, &_gnumake_endef_token_type))) {
        return err;
    } else if ((err = _gnumake_directive(ctxt, state, &part, "endef"))) {
        goto error;
    }
    toke->keyw = part;
    if ((err = _makefile_token_add_part((_makefile_token_t*) toke, &part))) {
        goto error;
    }

    *tokep = (_makefile_token_t*) toke;
    return 0;

error:
    _makefile_back_token(state, (const _makefile_token_t*) toke);
    _makefile_dest_token(&part);
    _gnumake_dest_token((_makefile_token_t*) dest_directive_token(&toke);
    return err;
}

int
_gnumake_undefine (
    _makefile_context_t*    ctxt,
    _makefile_state_t*      state,
    _makefile_token_t**     tokep
    )
{
    _gnumake_directive_token_t* toke = NULL;
    _makefile_token_t*          list = NULL;
    _makefile_token_t*          part = NULL;
    int                         err  = 0;

    if ((err = _gnumake_make_directive_token(state, &toke, &_gnumake_undefine_token_type))) {
        return err;
    } else if ((err = _gnumake_directive(ctxt, state, &part, "undefine"))) {
        goto error;
    }
    toke->keyw = part;
    if ((err = _makefile_token_add_part((_makefile_token_t*) toke, &part))) {
        goto error;
    } else if ((err = _makefile_hspace(ctxt, state, NULL))) {
        goto error;
    } else if ((err = _makefile_state_make_token(state, &list, &_gnumake_wordlist_token_type))) {
        goto error;
    }

    for ( ; ; ) {
        if ((err = _gnumake_lhs_word(ctxt, state, &part))) {
            break;
        } else if ((err = _makefile_token_add_part(list, &part))) {
            goto error;
        } else if ((err = _makefile_hspace(ctxt, state, NULL))) {
            break;
        }
    }
    toke->nest = list;
    if ((err = _makefile_token_add_part((_makefile_token_t*) part, &list))) {
        goto error;
    }

    *tokep = (_makefile_token_t*) toke;
    return 0;

error:
    _makefile_back_token(state, (const _makefile_token_t*) toke);
    _makefile_dest_token(&part);
    _makefile_dest_token(&list);
    _gnumake_dest_token((_makefile_token_t*) dest_directive_token(&toke);
    return err;
}

int
_gnumake_export (
    _makefile_context_t*    ctxt,
    _makefile_state_t*      state,
    _makefile_token_t**     tokep
    )
{
    _gnumake_directive_token_t* toke = NULL;
    _makefile_token_t*          part = NULL;
    _makefile_token_t*          list = NULL;
    int                         err  = 0;

    if ((err = _gnumake_make_directive_token(state, &toke, &_gnumake_export_token_type))) {
        return err;
    } else if ((err = _gnumake_directive(ctxt, state, &part, "export"))) {
        goto error;
    }
    toke->keyw = part;
    if ((err = _makefile_token_add_part((_makefile_token_t*) toke, &part))) {
        goto error;
    } else if ((err = _makefile_hspace(ctxt, state, NULL))) {
        goto error;
    }

    if (! (err = _gnumake_assignment(ctxt, state, &part))) {
        toke->nest = part;
        if ((err = _makefile_token_add_part((_makefile_token_t*) toke, &part))) {
            goto error;
        }
    } else {
        if ((err = _makefile_state_make_token(state, &list, &_gnumake_wordlist_token_type))) {
            goto error;
        }
        for ( ; ; ) {
            if ((err = _gnumake_rhs_word(ctxt, state, &part))) {
                break;
            } else if ((err = _makefile_token_add_part(list, &part))) {
                goto error;
            } else if ((err = _makefile_hspace(ctxt, state, NULL))) {
                break;
            }
        }
        toke->nest = list;
        if ((err = _makefile_token_add_part((_makefile_token_t*) toke, &list))) {
            goto error;
        }
    }

    *tokep = (_makefile_token_t*) toke;
    return 0;

error:
    _makefile_back_token(state, (const _makefile_token_t*) toke);
    _makefile_dest_token(&part);
    _makefile_dest_token(&list);
    _gnumake_dest_token((_makefile_token_t*) dest_directive_token(&toke);
    return err;
}

int
_gnumake_unexport (
    _makefile_context_t*    ctxt,
    _makefile_state_t*      state,
    _makefile_token_t**     tokep
    )
{
    _gnumake_directive_token_t* toke = NULL;
    _makefile_token_t*          part = NULL;
    int                         err  = 0;

    if ((err = _gnumake_make_directive_token(state, &toke, &_gnumake_unexport_token_type))) {
        return err;
    } else if ((err = _gnumake_directive(ctxt, state, &part, "unexport"))) {
        goto error;
    }
    toke->keyw = part;
    if ((err = _makefile_token_add_part((_makefile_token_t*) toke, &part))) {
        goto error;
    } else if ((err = _makefile_hspace(ctxt, state, NULL))) {
        goto error;
    }

    if (! (err = _gnumake_assignment(ctxt, state, &part))) {
        toke->nest = part;
        if ((err = _makefile_token_add_part((_makefile_token_t*) toke, &part))) {
            goto error;
        }
    } else {
        if ((err = _makefile_state_make_token(state, &list, &_gnumake_wordlist_token_type))) {
            goto error;
        }
        for ( ; ; ) {
            if ((err = _gnumake_rhs_word(ctxt, state, &part))) {
                break;
            } else if ((err = _makefile_token_add_part(list, &part))) {
                goto error;
            } else if ((err = _makefile_hspace(ctxt, state, NULL))) {
                break;
            }
        }
        toke->nest = list;
        if ((err = _makefile_token_add_part((_makefile_token_t*) toke, &list))) {
            goto error;
        }
    }

    *tokep = (_makefile_token_t*) toke;
    return 0;

error:
    _makefile_back_token(state, (const _makefile_token_t*) toke);
    _makefile_dest_token(&part);
    _makefile_dest_token(&list);
    _gnumake_dest_token((_makefile_token_t*) dest_directive_token(&toke);
    return err;
}

int
_gnumake_override (
    _makefile_context_t*    ctxt,
    _makefile_state_t*      state,
    _makefile_token_t**     tokep
    )
{
    _gnumake_directive_token_t* toke = NULL;
    _makefile_token_t*          part = NULL;
    int                         err  = 0;

    if ((err = _gnumake_make_directive_token(state, &toke, &_gnumake_override_token_type))) {
        return err;
    } else if ((err = _gnumake_directive(ctxt, state, &part, "override"))) {
        goto error;
    }
    toke->keyw = part;
    if ((err = _makefile_token_add_part((_makefile_token_t*) toke, &part))) {
        goto error;
    } else if ((err = _makefile_hspace(ctxt, state, NULL))) {
        goto error;
    }

    func = _gnumake_directive(
        ctxt, state, &part,
        "define", "undefine",
        "export", "unexport",
        "private", "readonly"
    );
    if (func) {
        if ((err = (*func)(ctxt, state, &part))) {
            goto error;
        }
    } else if ((err = _gnumake_assignment(ctxt, state, &part))) {
        goto error;
    }
    toke->nest = part;
    if ((err = _makefile_token_add_part((_makefile_token_t*) toke, &part))) {
        goto error;
    }

    *tokep = (_makefile_token_t*) toke;
    return 0;

error:
    _makefile_back_token(state, (const _makefile_token_t*) toke);
    _makefile_dest_token(&part);
    _gnumake_dest_token((_makefile_token_t*) dest_directive_token(&toke);
    return err;
}

int
_gnumake_private (
    _makefile_context_t*    ctxt,
    _makefile_state_t*      state,
    _makefile_token_t**     tokep
    )
{
    _gnumake_directive_f        func = NULL;
    _gnumake_directive_token_t* toke = NULL;
    _makefile_token_t*          part = NULL;
    int                         err  = 0;

    if ((err = _gnumake_make_directive_token(state, &toke, &_gnumake_private_token_type))) {
        return err;
    } else if ((err = _gnumake_directive(ctxt, state, &part, "private"))) {
        goto error;
    }
    toke->keyw = part;
    if ((err = _makefile_token_add_part((_makefile_token_t*) toke, &part))) {
        goto error;
    } else if ((err = _makefile_hspace(ctxt, state, NULL))) {
        goto error;
    }

    func = _gnumake_directive(
        ctxt, state, &part,
        "define", "undefine",
        "export", "unexport"
    );
    if (func) {
        if ((err = (*func)(ctxt, state, &part))) {
            goto error;
        }
    } else if ((err = _gnumake_assignment(ctxt, state, &part))) {
        goto error;
    }
    toke->nest = part;
    if ((err = _makefile_token_add_part((_makefile_token_t*) toke, &part))) {
        goto error;
    }

    *tokep = (_makefile_token_t*) toke;
    return 0;

error:
    _makefile_back_token(state, (const _makefile_token_t*) toke);
    _makefile_dest_token(&part);
    _gnumake_dest_token((_makefile_token_t*) dest_directive_token(&toke);
    return err;
}

int
_gnumake_include (
    _makefile_context_t*    ctxt,
    _makefile_state_t*      state,
    _makefile_token_t**     tokep
    )
{
    _gnumake_directive_token_t* toke = NULL;
    _makefile_token_t*          part = NULL;
    _makefile_token_t*          list = NULL;
    int                         err  = 0;

    if ((err = _gnumake_make_directive_token(state, &toke, &_gnumake_include_token_type))) {
        return err;
    } else if ((err = _gnumake_directive(ctxt, state, &part, "include"))) {
        goto error;
    }
    toke->keyw = part;
    if ((err = _makefile_token_add_part(toke, &part))) {
        goto error;
    } else if ((err = _makefile_state_make_token(state, &list, &_gnumake_wordlist_token_type))) {
        goto error;
    }

    for ( ; ; ) {
        if ((err = _makefile_hspace(ctxt, state, NULL))) {
            break;
        } else if ((err = _gnumake_lhs_word(ctxt, state, &part))) {
            break;
        } else if ((err = _makefile_token_add_part(list, &part))) {
            goto error;
        }
    }
    toke->nest = list;
    if ((err = _makefile_token_add_part((_makefile_token_t*) toke, &list))) {
        goto error;
    }

    *tokep = (_makefile_token_t*) toke;
    return 0;

error:
    _makefile_back_token(state, (const _makefile_token_t*) toke);
    _makefile_dest_token(&part);
    _makefile_dest_token(&list);
    _gnumake_dest_token((_makefile_token_t*) dest_directive_token(&toke);
    return err;
}

int
_gnumake__include (
    _makefile_context_t*    ctxt,
    _makefile_state_t*      state,
    _makefile_token_t**     tokep
    )
{
    _gnumake_directive_token_t* toke = NULL;
    _makefile_token_t*          part = NULL;
    _makefile_token_t*          list = NULL;
    int                         err  = 0;

    if ((err = _gnumake_make_directive_token(state, &toke, &_gnumake__include_token_type))) {
        return err;
    } else if ((err = _gnumake_directive(ctxt, state, &part, "-include", "sinclude"))) {
        goto error;
    }
    toke->keyw = part;
    if ((err = _makefile_token_add_part(toke, &part))) {
        goto error;
    } else if ((err = _makefile_state_make_token(state, &list, &_gnumake_wordlist_token_type))) {
        goto error;
    }

    for ( ; ; ) {
        if ((err = _makefile_hspace(ctxt, state, NULL))) {
            break;
        } else if ((err = _gnumake_rhs_word(ctxt, state, &part))) {
            break;
        } else if ((err = _makefile_token_add_part(list, &part))) {
            goto error;
        }
    }
    toke->nest = list;
    if ((err = _makefile_token_add_part((_makefile_token_t*) toke, &list))) {
        goto error;
    }

    *tokep = (_makefile_token_t*) toke;
    return 0;

error:
    _makefile_back_token(state, (const _makefile_token_t*) toke);
    _makefile_dest_token(&part);
    _makefile_dest_token(&list);
    _gnumake_dest_token((_makefile_token_t*) dest_directive_token(&toke);
    return err;
}

int
_gnumake_load (
    _makefile_context_t*    ctxt,
    _makefile_state_t*      state,
    _makefile_token_t**     tokep
    )
{
    _gnumake_directive_token_t* toke = NULL;
    _makefile_token_t*          part = NULL;
    _makefile_token_t*          list = NULL;
    int                         err  = 0;

    if ((err = _gnumake_make_directive_token(state, &toke, &_gnumake_load_token_type))) {
        return err;
    } else if ((err = _gnumake_directive(ctxt, state, &part, "load"))) {
        goto error;
    }
    toke->keyw = part;
    if ((err = _makefile_token_add_part(toke, &part))) {
        goto error;
    } else if ((err = _makefile_state_make_token(state, &list, &_gnumake_loadlist_token_type))) {
        goto error;
    }

    for ( ; ; ) {
        if ((err = _makefile_hspace(ctxt, state, NULL))) {
            break;
        } else if ((err = _gnumake_rhs_word(ctxt, state, &part))) {
            break;
        } else if ((err = _makefile_token_add_part(list, &part))) {
            goto error;
        }
    }
    toke->nest = list;
    if ((err = _makefile_token_add_part((_makefile_token_t*) toke, &list))) {
        goto error;
    }

    *tokep = (_makefile_token_t*) toke;
    return 0;

error:
    _makefile_back_token(state, (const _makefile_token_t*) toke);
    _makefile_dest_token(&part);
    _makefile_dest_token(&list);
    _gnumake_dest_token((_makefile_token_t*) dest_directive_token(&toke);
    return err;
}

int
_gnumake__load (
    _makefile_context_t*    ctxt,
    _makefile_state_t*      state,
    _makefile_token_t**     tokep
    )
{
    _gnumake_directive_token_t* toke = NULL;
    _makefile_token_t*          part = NULL;
    _makefile_token_t*          list = NULL;
    int                         err  = 0;

    if ((err = _gnumake_make_directive_token(state, &toke, &_gnumake__load_token_type))) {
        return err;
    } else if ((err = _gnumake_directive(ctxt, state, &part, "-load"))) {
        goto error;
    }
    toke->keyw = part;
    if ((err = _makefile_token_add_part(toke, &part))) {
        goto error;
    } else if ((err = _makefile_state_make_token(state, &list, &_gnumake_loadlist_token_type))) {
        goto error;
    }

    for ( ; ; ) {
        if ((err = _makefile_hspace(ctxt, state, NULL))) {
            break;
        } else if ((err = _gnumake_rhs_word(ctxt, state, &part))) {
            break;
        } else if ((err = _makefile_token_add_part(list, &part))) {
            goto error;
        }
    }
    toke->nest = list;
    if ((err = _makefile_token_add_part((_makefile_token_t*) toke, &list))) {
        goto error;
    }

    *tokep = (_makefile_token_t*) toke;
    return 0;

error:
    _makefile_back_token(state, (const _makefile_token_t*) toke);
    _makefile_dest_token(&part);
    _makefile_dest_token(&list);
    _gnumake_dest_token((_makefile_token_t*) dest_directive_token(&toke);
    return err;
}

int
_gnumake_scope (
    _makefile_context_t*    ctxt,
    _makefile_state_t*      state,
    _makefile_token_t**     tokep
    )
{
    _gnumake_directive_f        func = NULL;
    _gnumake_directive_token_t* toke = NULL;
    _makefile_token_t*          part = NULL;
    int                         err  = 0;

    if ((err = _gnumake_make_directive_token(state, &toke, &_gnumake_scope_token_type))) {
        return err;
    } else if ((err = _gnumake_directive(ctxt, state, &part, "scope"))) {
        goto error;
    }
    toke->keyw = part;
    if ((err = _makefile_token_add_part(toke, &part))) {
        goto error;
    }
    (void) _makefile_hspace(ctxt, state, NULL);

    if ((func = _gnumake_directive(ctxt, state, NULL, "include", "-include", "sinclude"))) {
        if ((err = (*func)(ctxt, state, &part))) {
            goto error;
        }
        toke->nest = part;
        if ((err = _makefile_token_add_part((_makefile_token_t*) toke, &part))) {
            goto error;
        }
    }

    *tokep = (_makefile_token_t*) toke;
    return 0;

error:
    _makefile_back_token(state, (const _makefile_token_t*) toke);
    _makefile_dest_token(&part);
    _gnumake_dest_token((_makefile_token_t*) dest_directive_token(&toke);
    return err;
}

int
_gnumake_endscope (
    _makefile_context_t*    ctxt,
    _makefile_state_t*      state,
    _makefile_token_t**     tokep
    )
{
    _gnumake_directive_token_t* toke = NULL;
    _makefile_token_t*          part = NULL;
    int                         err  = 0;

    if ((err = _gnumake_make_directive_token(state, &toke, &_gnumake_endscope_token_type))) {
        return err;
    } else if ((err = _gnumake_directive(ctxt, state, &part, "endscope"))) {
        goto error;
    }
    toke->keyw = part;
    if ((err = _makefile_token_add_part(toke, &part))) {
        goto error;
    }

    *tokep = (_makefile_token_t*) toke;
    return 0;

error:
    _makefile_back_token(state, (const _makefile_token_t*) toke);
    _makefile_dest_token(&part);
    _gnumake_dest_token((_makefile_token_t*) dest_directive_token(&toke);
    return err;
}

int
_gnumake_subdir (
    _makefile_context_t*    ctxt,
    _makefile_state_t*      state,
    _makefile_token_t**     tokep
    )
{
    _gnumake_directive_token_t* toke = NULL;
    _makefile_token_t*          part = NULL;
    _makefile_token_t*          list = NULL;
    int                         err  = 0;

    if ((err = _gnumake_make_directive_token(state, &toke, &_gnumake_subdir_token_type))) {
        return err;
    } else if ((err = _gnumake_directive(ctxt, state, &part, "subdir"))) {
        goto error;
    }
    toke->keyw = part;
    if ((err = _makefile_token_add_part(toke, &part))) {
        goto error;
    } else if ((err = _makefile_state_make_token(state, &list, &_gnumake_wordlist_token_type))) {
        goto error;
    }

    for ( ; ; ) {
        if ((err = _makefile_hspace(ctxt, state, NULL))) {
            break;
        } else if ((err = _gnumake_rhs_word(ctxt, state, &part))) {
            break;
        } else if ((err = _makefile_token_add_part(list, &part))) {
            goto error;
        }
    }
    toke->nest = list;
    if ((err = _makefile_token_add_part((_makefile_token_t*) toke, &list))) {
        goto error;
    }

    *tokep = (_makefile_token_t*) toke;
    return 0;

error:
    _makefile_back_token(state, (const _makefile_token_t*) toke);
    _makefile_dest_token(&part);
    _makefile_dest_token(&list);
    _makefile_dest_token((_makefile_token_t**) dest_directive_token(&toke);
    return err;
}


int
_gnumake_readonly (
    _makefile_context_t*    ctxt,
    _makefile_state_t*      state,
    _makefile_token_t**     tokep
    )
{
    _gnumake_directive_token_t* toke = NULL;
    _makefile_token_t*          part = NULL;
    _makefile_token_t*          list = NULL;
    int                         err  = 0;

    if ((err = _gnumake_make_directive_token(state, &toke, &_gnumake_readonly_token_type))) {
        return err;
    } else if ((err = _gnumake_directive(ctxt, state, &part, "export"))) {
        goto error;
    }
    toke->keyw = part;
    if ((err = _makefile_token_add_part((_makefile_token_t*) toke, &part))) {
        goto error;
    } else if ((err = _makefile_hspace(ctxt, state, NULL))) {
        goto error;
    }

    if (! (err = _gnumake_assignment(ctxt, state, &part))) {
        toke->nest = part;
        if ((err = _makefile_token_add_part((_makefile_token_t*) toke, &part))) {
            goto error;
        }
    } else {
        if ((err = _makefile_state_make_token(state, &list, &_gnumake_wordlist_token_type))) {
            goto error;
        }
        for ( ; ; ) {
            if ((err = _gnumake_rhs_word(ctxt, state, &part))) {
                break;
            } else if ((err = _makefile_token_add_part(list, &part))) {
                goto error;
            } else if ((err = _makefile_hspace(ctxt, state, NULL))) {
                break;
            }
        }
        toke->nest = list;
        if ((err = _makefile_token_add_part((_makefile_token_t*) toke, &list))) {
            goto error;
        }
    }

    *tokep = (_makefile_token_t*) toke;
    return 0;

error:
    _makefile_back_token(state, (const _makefile_token_t*) toke);
    _makefile_dest_token(&part);
    _makefile_dest_token(&list);
    _gnumake_dest_token((_makefile_token_t*) dest_directive_token(&toke);
    return err;
}
