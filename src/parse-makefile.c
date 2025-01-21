#include <alloca.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include <shmoo/char.h>
#include <shmoo/context.h>
#include <shmoo/state.h>
#include <shmoo/token.h>

#define _MAKEFILE_FLAG_PRELOAD      0x0001
#define _MAKEFILE_FLAG_REGULAR      0x0002
#define _MAKEFILE_FLAG_OVERRIDE     0x0004
#define _MAKEFILE_FLAG_OPTIONAL     0x0008
#define _MAKEFILE_FLAG_SCOPE        0x0010
#define _MAKEFILE_FLAG_EXPORT       0x0020
#define _MAKEFILE_FLAG_UNEXPORT     0x0040

struct shmoo_token {
    const shmoo_input_t*    inpu;
    const shmoo_token_t*    orig;
    uint64_t                from;
    size_t                  used;
    size_t                  span;
};

/*forward*/ struct _makefile_context;
typedef     struct _makefile_contex             _makefile_context_t;
/*forward*/ struct _makefile_state;
typedef     struct _makefile_state              _makefile_state_t;
/*forward*/ struct _makefile_rule;
typedef     struct _makefile_rule               _makefile_rule_t;

typedef shmoo_size_t                            _makefile_size_t;
typedef shmoo_char_t                            _makefile_char_t;
typedef shmoo_token_t                           _makefile_token_t;
typedef const shmoo_token_type_t                _makefile_token_type_t;

/*forward*/ struct _makefile_assignment_token;
            struct _makefile_assignment_token   _makefile_assignment_token_t;
/*forward*/ struct _makefile_rule_token;
            struct _makefile_rule_token         _makefile_assignment_token_t;

struct _makefile_assignment_token {
    shmoo_token_t       head;
    shmoo_token_t*      name;
    shmoo_token_t*      oper;
    shmoo_token_t*      data;
};

struct _makefile_rule_token {
    shmoo_token_t       head;
    shmoo_token_t*      tgts;
    shmoo_token_t*      oper;
    shmoo_token_t*      pats;
    shmoo_token_t*      deps;
    shmoo_token_t*      oodp;
    shmoo_token_t*      assi;
};

struct _makefile_context {
    shmoo_context_t     head;
    shmoo_vector_t      inst;
    shmoo_vector_t      bran;
    shmoo_vector_t      toke;
    shmoo_vector_t      rule;
    shmoo_hash_t        dire;
    shmoo_hash_t        vars;
    shmoo_char_t        rpfx;   /* \t, by default. */
};

struct _makefile_state {
    shmoo_state_t       head;
    int                 flag;
    shmoo_vector_t      rule;
    _makefile_rule_t*   orul;
    shmoo_hash_t        vars;
    shmoo_token_t*      defn_token;
    shmoo_size_t        defn_depth;
};

struct _makefile_rule {
    shmoo_vector_t      rcpe;
};

typedef int (*_makefile_directive_f) (_makefile_context_t*, _makefile_state_t*, _makefile_token_t**, ...);
typedef int (*_makefile_function_f)  (_makefile_context_t*, _makefile_state_t*, ...);

#define _gnumake_lhs_word(ctxt,state,tokep) \
    (__gnumake_word((ctxt),(state),(tokep),":=#"))
#define _gnumake_tgt_patt(ctxt,state,tokep) \
    (__gnumake_word((ctxt),(state),(tokep),":=#"))
#define _gnumake_dep_patt(ctxt,state,tokep) \
    (__gnumake_word((ctxt),(state),(tokep),";:|#"))

void
_makefile_token_add_part (
    _makefile_token_t*              into,
    const _makefile_token_t**       partp
    )
{
    (void) shmoo_token_part((shmoo_token_t*) into, (const shmoo_token_t*) part);
}

void
_makefile_token_add_size (
    _makefile_token_t*              into,
    _makefile_size_t                used,
    _makefile_size_t                span
    )
{
    (void) shmoo_token_more((shmoo_token_t*) into, used, span);
}

int
_makefile_read_char (
    _makefile_state_t*              state,
    _makefile_char_t*               cvalp,
    _makefile_size_t*               csizp
    )
{
    shmoo_char_scan_utf8_state_t utf8 = SHMOO_SCAN_CHAR_UTF8_INITIALIZER;
    err = shmoo_state_read(
        (shmoo_state_t*) state,
        shmoo_char_scan_utf8,
        &utf8
    );
    if (! err) {
        *cvalp = utf8.cval;
        *csizp = utf8.csiz;
    }
    return err;
}

int
_makefile_back_char (
    _makefile_state_t*              state,
    _makefile_size_t                used
    )
{
    return shmoo_state_back(&state->head, used);
}

int
_makefile_make_token (
    _makefile_state_t*              state,
    _makefile_token_t**             tokep,
    const _makefile_token_type_t*   type
    )
{
    return shmoo_state_make_token(
        (shmoo_state_t*) &state,
        (shmoo_token_t**) tokep,
        (const shmoo_token_type_t*) type
    );
}

int
_makefile_context_cur_state (
    _makefile_context_t*            ctxt,
    _makefile_state_t**             statep
    )
{
    return shmoo_vector_obtain_tail(&ctxt->inpu, statep);
}

int
_makefile_context_pop_state (
    _makefile_context_t*            ctxt,
    _makefile_state_t**             statep
    )
{
    return shmoo_vector_remove_tail(&ctxt->inpu, statep);
}

int
_makefile_context_apply_state (
    _makefile_context_t*            ctxt,
    _makefile_state_t*              state
    )
{
}

int
_makefile_context_save_state (
    _makefile_context_t*            ctxt,
    _makefile_state_t*              state
    )
{
}

int
_makefile_state_apply_state (
    _makefile_state_t*              into,
    _makefile_state_t*              from
    )
{
}

int
_makefile_state_save_line (
    _makefile_state_t*              state,
    _makefile_token_t*              line
    )
{
}

void
_makefile_dest_token (
    _makefile_token_t**             tokep
    )
{
    (void) shmoo_token_dest((shmoo_token_t**) tokep);
}

int
_gnumake_read (
    _makefile_context_t*            ctxt
    )
{
    _makefile_state_t*   file_state = NULL;
    _makefile_token_t*   line_token = NULL;
    int                  err        = 0;

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
        } else if ((err = _makefile_read_line(file_state, &line_token))) {
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
            if (! (err = _makefile_context_cur_state(ctxt, &next_state))) {
                err = _makefile_state_apply_state(next_state, file_state);
            } else {
                err = _makefile_context_apply_state(ctxt, file_state);
            }
            if (! err) {
                err = _makefile_context_save_state(ctxt, &file_state);
            }
            if (err) {
                goto error;
            } else {
                file_state = NULL;
            }
            break;
        } else if ((err = _gnumake_parse_line(ctx, file_state, line_token))) {
            goto error;
        } else if ((err = _makefile_state_save_line(file_state, line_token))) {
            goto error;
        } else {
            file_state = NULL;
            line_token = NULL;
        }
    }

    return 0;

error:
    _makefile_dest_token(&line_token);
    return err;
}

int
_makefile_curr_rule (
    _makefile_state_t*              state,
    _makefile_rule_token_t**        rulep
    )
{
    int err = ENOENT;
    if (state->orul) {
        *rulep = state->orul;
        err = 0;
    }
    return err;
}

int
_makefile_rule_add_line (
    _makefile_rule_token_t*         rule,
    _makefile_state_t*              state
    )
{
    return shmoo_vector_insert_tail(&rule->rcpe, state);
}

int
_makefile_recipe (
    _makefile_context_t*            ctxt,
    _makefile_state_t*              file_state,
    _makefile_state_t**             line_statep
    )
{
    _makefile_rule_token_t* rule = NULL;
    _makefile_char_t        cval;
    _makefile_size_t        csiz;
    int                     err  = 0;

    if ((err = _makefile_read_char(*line_statep, &cval, &csiz))) {
        return err;
    } else if (cval != ctxt->rpfx) {
        if ((err = _makefile_back_char(*line_statep, csiz))) {
            return err;
        } else {
            return EINVAL;
        }
    } else if ((err = _makefile_curr_rule(file_state, &rule))) {
        return err;
    } else if ((err = _makefile_rule_add_line(rule, *line_statep))) {
        return err;
    } else {
        *line_statep = NULL; /* belongs to the recipe now */
    }

    return err;
}

static _makefile_token_type_t _makefile_continuation_token_type = {
    .name = "makefile_continuation_token",
};

int
_makefile_continuation (
    _makefile_state_t*              state,
    _makefile_token_t**             tokep
    )
{
    _makefile_token_t*  toke = 0;
    _makefile_size_t    used = 0;
    _makefile_char_t    cval;
    _makefile_size_t    csiz;
    int                 err  = 0;

    if (tokep) {
        if ((err = _makefile_make_token(state, &toke, &_makefile_continuation_token_type))) {
            return err;
        }
    }

    do {
        if ((err = _makefile_read_char(state, &cval, &csiz))) {
            goto error;
        } else {
            used += csiz;
        }
        if (cval != '\\') {
            err = EINVAL;
            goto error;
        }
        if ((err = _maekfile_state_read_char(state, &cval, &csiz))) {
            goto error;
        } else {
            used += csiz;
        }
        if ((cval != '\r') && (cval != '\n')) {
            err = EINVAL;
            goto error;
        } else if (cval == '\n') {
            break;
        }
        if ((err = _makefile_read_char(state, &cval, &csiz))) {
            break;
        } else if (cval != '\n') {
            if ((err = _makefile_back_char(state, csiz))) {
                goto error;
            }
        } else {
            used += csiz;
            break;
        }
    } while (0);

    if (tokep) {
        _makefile_token_add_size(toke, used, 1);
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

static _makefile_token_type_t _makefile_endline_token_type = {
    .name = "makefile_endline_token",
};

int
_makefile_endline (
    _makefile_state_t*              state,
    _makefile_token_t**             tokep
    )
{
    _makefile_token_t*  toke = 0;
    _makefile_size_t    used = 0;
    _makefile_char_t    cval;
    _makefile_size_t    csiz;
    int                 err  = 0;

    if (tokep) {
        if ((err = _makefile_make_token(state, &toke, &_makefile_endline_token_type))) {
            return err;
        }
    }

    do {
        if ((err = _makefile_read_char(state, &cval, &csiz))) {
            goto error;
        } else {
            used += csiz;
        }
        if ((cval != '\r') && (cval != '\n')) {
            err = EINVAL;
            goto error;
        } else if (cval == '\n') {
            break;
        }
        if ((err = _makefile_read_char(state, &cval, &csiz))) {
            break;
        } else if (cval != '\n') {
            if ((err = _makefile_back_char(state, csiz))) {
                goto error;
            }
        } else {
            used += csiz;
            break;
        }
    } while (0);

    if (tokep) {
        _makefile_token_add_size(toke, used, 1);
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
_makefile_branches (
    _makefile_state_t*              state,
    int                             adjust
    )
{
}

int
_makefile_skipping (
    _makefile_state_t*              state,
    int                             adjust
    )
{
}

int
_makefile_defining (
    _makefile_state_t*              state,
    int                             adjust
    )
{
}

/* = */
static _makefile_token_type_t _makefile_assign_delay_token_type = {
    .name = "makefile_delay_assign_reset_token",
};

/* :=, ::= */
static _makefile_token_type_t _makefile_immed_assign_reset_token_type = {
    .name = "makefile_immed_assign_reset_token",
};

/* += */
static _makefile_token_type_t _makefile_delay_assign_after_token_type = {
    .name = "makefile_delay_assign_after_token",
};

/* +:= */
static _makefile_token_type_t _gnumake_immed_assign_after_token_type = {
    .name = "gnumake_immed_assign_after_token",
};

/* ?= */
static _makefile_token_type_t _makefile_delay_assign_ifnot_token_type = {
    .name = "makefile_delay_assign_ifnot_token",
};

/* ?:= */
static _makefile_token_type_t gnumakee_immed_assign_ifnot_token_type = {
    .name = "gnumake_immed_assign_ifnot_token",
};

/* ^= */
static _makefile_token_type_t _gnumake_delay_assign_prior_token_type = {
    .name = "gnumake_delay_assign_prior_token",
};

/* ^:= */
static _makefile_token_type_t _gnumake_immed_assign_prior_token_type = {
    .name = "gnumake_immed_assign_prior_token",
};

/* != */
static _makefile_token_type_t _makefile_delay_assign_shell_token_type = {
    .name = "makefile_delay_assign_shell_token",
};

/* !:= */
static _makefile_token_type_t _gnumake_immed_assign_shell_token_type = {
    .name = "gnumake_immed_assign_shell_token",
};

int
_gnumake_assignop (
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
            default:
                err = EINVAL;
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
            type |= _MAKEFILE_OP_ASSIGN_IMMED;
        } else {
            err = EINVAL;
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
                _makefile_token_set_type(toke, &_makefile_delay_assign_reset_token_type);
                break;
            case (_MAKEFILE_OP_ASSIGN_IMMED|_MAKEFILE_OP_ASSIGN_RESET):
                _makefile_token_set_type(toke, &_makefile_immed_assign_reset_token_type);
                break;
            case (_MAKEFILE_OP_ASSIGN_DELAY|_MAKEFILE_OP_ASSIGN_PRIOR):
                _makefile_token_set_type(toke, &_makefile_delay_assign_prior_token_type);
                break;
            case (_MAKEFILE_OP_ASSIGN_IMMED|_MAKEFILE_OP_ASSIGN_PRIOR):
                _makefile_token_set_type(toke, &_makefile_immed_assign_prior_token_type);
                break;
            case (_MAKEFILE_OP_ASSIGN_DELAY|_MAKEFILE_OP_ASSIGN_AFTER):
                _makefile_token_set_type(toke, &_makefile_delay_assign_after_token_type);
                break;
            case (_MAKEFILE_OP_ASSIGN_IMMED|_MAKEFILE_OP_ASSIGN_AFTER):
                _makefile_token_set_type(toke, &_makefile_immed_assign_after_token_type);
                break;
            case (_MAKEFILE_OP_ASSIGN_DELAY|_MAKEFILE_OP_ASSIGN_IFNOT):
                _makefile_token_set_type(toke, &_makefile_delay_assign_ifnot_token_type);
                break;
            case (_MAKEFILE_OP_ASSIGN_IMMED|_MAKEFILE_OP_ASSIGN_IFNOT):
                _makefile_token_set_type(toke, &_makefile_immed_assign_ifnot_token_type);
                break;
            case (_MAKEFILE_OP_ASSIGN_DELAY|_MAKEFILE_OP_ASSIGN_SHELL):
                _makefile_token_set_type(toke, &_makefile_delay_assign_shell_token_type);
                break;
            case (_MAKEFILE_OP_ASSIGN_IMMED|_MAKEFILE_OP_ASSIGN_SHELL):
                _makefile_token_set_type(toke, &_makefile_immed_assign_shell_token_type);
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

static _makefile_token_type_t _makefile_single_colon_rule_token_type = {
    .name = "makefile_single_colon_rule_token",
};

static _makefile_token_type_t _makefile_double_colon_rule_token_type = {
    .name = "makefile_double_colon_rule_token",
};

int
_gnumake_rulecolon (
    _makefile_state_t*              state,
    _makefile_token_t**             tokep
    )
{
    _makefile_token_type_t* type = NULL;
    _makefile_token_t*      toke = NULL;
    _makefile_size_t        used = 0;
    _makefile_char_t        cval;
    _makefile_size_t        csiz;
    int                     err  = 0;

    if (tokep) {
        if ((err = _makefile_make_token(state, &toke, NULL))) {
            return err;
        }
    }

    do {
        if ((err = _makefile_read_char(state, &cval, &csiz))) {
            goto error;
        } else {
            used += csiz;
        }
        if (cval != ':') {
            err = EINVAL;
            goto error;
        }
        type = &_makefile_single_colon_rule_token_type;
        if ((err = _makefile_read_char(state, &cval, &csiz))) {
            break;
        } else {
            used += csiz;
        }
        if (cval != ':') {
            if ((err = _makefile_back_char(state, csiz))) {
                goto error;
            } else {
                break;
            }
        }
        type = &_makefile_double_colon_rule_token_type;
    } while (0);

    if (tokep) {
        _makefile_token_set_type(toke, type);
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

_makefile_token_type_t _makefile_bareword_token_type = {
    .name = "makefile_bareword_token",
};

#define _gnumake_directive(context,file_state,token_pointer,...) \
    (__gnumake_directive((context),(file_state),(token_pointer),##__VA_ARGS__,(const char*)NULL))
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
    const char*             name = NULL;
    char*                   buff = NULL;
    size_t                  used = 0;
    int                     err  = 0;

    if ((err = _makefile_make_token(state, &toke, &_makefile_bareword_token_type))) {
        return err;
    }

    /* Scan characters into the token. */
    for ( ; ; ) {
        if ((err = _makefile_read_char(line_state, &cval, &csiz))) {
            goto error;
        } else if (_makefile_isalnum(ctxt, cval) || strchr(".-_", cval)) {
            _makefile_token_add_size(toke, csiz, 0);
        } else if ((err = _makefile_back_char(line_state, csiz))) {
            goto error;
        } else if (cval != '\\') {
            break;
        } else if ((err = _makefile_continuation(line_state, &part))) {
            break;
        } else {
            _makefile_token_add_part(toke, &part);
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
    if (! (func = _makefile_find_directive(ctxt, buff))) {
        err = EOPNOTSUPP;
        goto error;
    } else {
        va_list args;
        va_start(args, tokep);
        for (used = 0; (name = va_arg(args, const char*)); ++used) {
            if (! strcmp(name, buff)) {
                break;
            }
        }
        va_end(args);
        if (used && ! name) {
            err  = ENFILE;
            goto error;
        }
    }

    /* Gobble whitespace after possible directive. */
    (void) _makefile_hspace(line_state, NULL, 0)
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
    if (! _gnumake_assignop(line_state, NULL, 0)
     || ! _gnumake_rulecolon(line_state, NULL, 0)
       )
    {
        goto error;
    }

    if (tokep) {
        /* If we sent a pointer to be filled, fill it
         * and keep the parser advanced past the word.
         */
        *tokep = toke;
    } else if ((err = _makefile_back_token(line_state, toke))) {
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
    _makefile_back_token(line_state, toke);
    _makefile_dest_token(&part);
    _makefile_dest_token(&toke);
    
    return NULL;
}

/* Allow for the possibility of locale or character set
 * to be switched by parsing, which might cause differences
 * in the way some characters are interpreted.  Probably
 * stupid.
 */
int
_makefile_ishspace (
    _makefile_context_t*    ctxt,
    _makefile_state_t*      state,
    shmoo_char_t            cval
    )
{
    return isblank(cval);
    (void) ctxt;
    (void) state;
}

int
_makefile_isspace (
    _makefile_context_t*    ctxt,
    _makefile_state_t*      state,
    shmoo_char_t            cval
    )
{
    return isspace(cval);
    (void) ctxt;
    (void) state;
}

int
_makefile_isalnum (
    _makefile_context_t*    ctxt,
    _makefile_state_t*      state,
    shmoo_char_t            cval
    )
{
    return isalnum(cval);
    (void) ctxt;
    (void) state;
}

const shmoo_token_type_t _makefile_hspace_token_type = {
    .name = "makefile_hspace_token",
};

int
_makefile_hspace (
    _makefile_state_t*      line_state,
    shmoo_token_t**         tokep,
    int                     flags
    )
{
    _makefile_token_t*  toke = NULL;
    _makefile_token_t*  part = NULL;
    size_t              used = 0;
    int                 err  = 0;
    shmoo_char_t        cval;
    size_t              csiz;

    if (tokep) {
        if ((err = _makefile_state_token(line_state, &toke, &_makefile_hspace_token_type))) {
            return err;
        }
    }

    for (used = 0; ; ++used) {
        if ((err = _makefile_read_char(line_state, &cval, &csiz))) {
            if (err == EOF) {
                break;
            } else {
                goto error;
            }
        } else if (cval == '\\') {
            if ((err = _makefile_back_char(line_state, csiz))) {
                goto error;
            } else if ((err = _makefile_continuation(line_state, (tokep ? &part : NULL)))) {
                break;
            } else if (tokep) {
                if ((err = shmoo_token_part(toke, part))) {
                    goto error;
                }
            }
        } else if (_makefile_ishspace(cval)) {
            if (tokep) {
                if ((err = shmoo_token_more(toke, csiz, 0))) {
                    goto error;
                }
            }
        }
    }

    if (! used) {
        err = ENOSPC;
        goto error;
    } else if (tokep) {
        *tokep = toke;
    }
    return 1;

error:
    if (tokep) {
        shmoo_token_dest(&toke);
        shmoo_token_dest(&part);
    }
    return err;
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
            }
        }
    } else if ((func = _gnumake_directive(ctxt, line_state, NULL))) {
        if ((err = (*func)(ctxt, file_state, &action))) {
            goto error;
        }
    } else if (! (err = _gnumake_parse_assignment(ctxt, line_state, &action))) {
    } else if (! (err = _gnumake_parse_rule(ctxt, line_state, &action))) {
    } else {
        /* Should never be here. */
    }

    /* Apply the action token generated by parsing, then
     * save the token so the whole schmear can be dumped
     * out to a more structured log, then picked up and
     * examined postmortem to figure out why the darned
     * build failed...
     */
    if ((err = _makefile_state_apply_token(ctxt, file_state, action, 0))
     || (err = _makefile_save_token(file_state, action))
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
_makefile_read_line (
    _makefile_state_t*      state,
    _makefile_token_t**     tokep
    )
{
    _makefile_char_t    this_cval;
    _makefile_size_t    this_csiz;
    _makefile_char_t    next_cval;
    _makefile_size_t    next_csiz;
    _makefile_token_t*  toke = 0;
    _makefile_token_t*  part = 0;
    int                 err  = 0;

    if ((err = _makefile_make_token(state, &toke, &_makefile_line_token_type))) {
        return err;
    }

    for ( ; ; ) {
        if ((err = _makefile_make_token(state, &toke, &_makefile_line_token_type))) {
            if (! used) {
                goto error;
            } else {
                break;
            }
        }
        switch (this_cval) {
            default:
                _makefile_token_add_size(toke, this_csiz, 0);
                continue;
            case '\\':
                if ((err = _makefile_read_char(state, &next_cval, &next_csiz))) {
                    break;
                } else if ((err = _makefile_back_char(state, next_csiz))) {
                    goto error;
                } else if ((next_cval != '\r') && (next_cval != '\n')) {
                    _makefile_token_add_size(toke, this_csiz, 0);
                } else if ((err = _makefile_back_char(state, this_csiz))) {
                    goto error;
                } else if ((err = _makefile_continuation(state, &part))) {
                    goto error;
                } else {
                    _makefile_token_add_part(toke, &part);
                }
                continue;
            case '\r':
            case '\n':
                if ((err = _makefile_back_char(state, this_csiz))) {
                    goto error;
                } else if ((err = _makefile_endline(state, &part))) {
                    /* This shouldn't happen. */
                    goto error;
                } else {
                    _makefile_token_add_part(toke, &part);
                }
                break;
        }

        if ((err = _makefile_read_char(state, &this_cval, &this_csiz)) > 0) {
            goto error;
        }
    }
    *tokep = toke;
    return 0;

error:
    _makefile_dest_token(&part);
    _makefile_dest_token(&toke);
    return rslt;
}

static _makefile_token_type_t _makefile_assignment_token_type = {
    .name = "makefile_assignment_token",
};

void
_makefile_state_dest_assignment_token (
    _makefile_assignment_token_t**  tokep
    )
{
    if (*tokep) {
        shmoo_token_free((shmoo_token_t*) *tokep);
        free(*tokep);
        *tokep = NULL;
    }
}

int
_makefile_state_make_assignment_token (
    _makefile_state_t*              state,
    _makefile_assignment_token_t** tokep
    )
{
    _maekfile_assignment_token_t*   toke = NULL;
    int                             err  = 0;

    if (! (toke = calloc(1, sizeof(*toke)))) {
        return errno;
    }
    err = shmoo_state_init_token(
        state, (shmoo_token_t*) toke, &_makefile_assignment_token_type
    );
    if (! err) {
        *tokep = toke;
        return 0;
    }

    _makefile_dest_assignment_token(&toke);
    return err;
}

int
_gnumake_parse_assignment (
    _makefile_context_t*        ctxt,
    _makefile_state_t*          state,
    _makefile_token_t**         tokep
    )
{
    _makefile_assignment_token_t*   toke = NULL;
    _makefile_token_t*              part = NULL;
    _makefile_token_t*              name = NULL;
    _makefile_token_t*              oper = NULL;
    _makefile_token_t*              data = NULL;
    _makefile_token_t*              hspc = NULL;
    int                             err  = 0;

    if ((err = _makefile_state_make_assignment_token(state, &toke))) {
        return err;
    }
    err = _makefile_make_token(state, &name, &_makefile_assignment_name_token);
    if (err) {
        goto error;
    }

    for ( ; ; ) {
        if ((err = _gnumake_lhs_word(state, &part))) {
            goto error;
        } else if ((err = _makefile_token_add_part(name, &part))) {
            goto error;
        }
        if ((err = _makefile_hspace(state, &part))) {
            if (err == ENOSPC) {
                break;
            } else {
                goto error;
            }
        } else if ((err = _makefile_token_add_part(name, &part))) {
            goto error;
        }
    }

    if ((err = _gnumake_assignop(state, &oper))) {
        goto error;
    }
    (void) _makefile_hspace(state, NULL);

    err = _makefile_make_token(state, &data, &_makefile_assignment_data_token_type);
    if (err) {
        goto error;
    }

    for ( ; ; ) {
        if ((err = _gnumake_lhs_word(state, &part))) {
            break;
        } else if (hspc && (err = _makefile_token_add_part(data, &hspc))) {
            goto error;
        } else if ((err = _makefile_token_add_part(data, &part))) {
            goto error;
        } else if ((err = _makefile_hspace(state, &hspc))) {
            break;
        }
    }

    if (hspc) {
        _makefile_dest_token(&hspc);
    }
    toke->name = name;
    toke->oper = oper;
    toke->data = data;
    if ((err = _makefile_token_add_part(toke, &name))
     || (err = _makefile_token_add_part(toke, &oper))
     || (err = _makefile_token_add_part(toke, &data))
       )
    {
        goto error;
    }

    *tokep = toke;
    return 0;

error:
    _makefile_dest_token(&hspc);
    _makefile_dest_token(&part);
    _makefile_dest_token(&data);
    _makefile_dest_token(&oper);
    _makefile_dest_token(&name);
    _makefile_dest_token(&toke);
    return 0;
}

static _makefile_token_type_t _makefile_rule_token_type = {
    .name = "makefile_rule_token",
};

void
_makefilee_state_dest_rule_token (
    _makefile_rule_token_t**    tokep
    )
{
    if (*tokep) {
        shmoo_token_free((shmoo_token_t*) *tokep);
        free(*tokep);
        *tokep = NULL;
    }
}

int
_makefile_state_make_rule_token (
    _makefile_state_t*          state,
    _makefile_rule_token_t**    tokep
    )
{
    _maekfile_rule_token_t* toke = NULL;
    int                     err  = 0;

    if (! (toke = calloc(1, sizeof(*toke)))) {
        return errno;
    } else if (! (err = shmoo_state_init_token(&state->head, toke, &_makefile_rule_token_type))) {
        free(toke);
        return err;
    } else {
        *tokep = toke;
        return 0;
    }
}

static _makefile_token_type_t _makefile_rule_targets_node_type = {
    .name = "makefile_rule_targets_token",
};

static _makefile_token_type_t _makefile_rule_depends_node_type = {
    .name = "makefile_rule_depends_token",
};

int
_gnumake_parse_rule (
    _makefile_context_t*        ctxt,
    _makefile_state_t*          state,
    _makefile_token_t**         rulep
    )
{
    _makefile_directive_f       func = NULL;
    _makefile_rule_token_t*     rule = NULL;
    _makefile_token_t*          part = NULL;
    _makefile_token_t*          hspc = NULL;
    _makefile_token_t*          tgts = NULL;
    int                         nwrd = 0;
    int                         err  = 0;

    if ((err = _makefile_state_make_rule_token(state, &rule))) {
        return err;
    } else if ((err = _makefile_make_token(state, &tgts, &_makefile_rule_targets_token_type))) {
        goto error;
    }

    for ( ; ; ) {
        if (! (err = _gnumake_tgt_patt(ctxt, state, &part))) {
            if (hspc && (err = _makefile_token_add_part(tgts, &hspc))) {
                goto error;
            } else if ((err = _makefile_token_add_part(tgts, &part))) {
                goto error;
            }
            ++nwrd;
        } else if (! nwrd) {
            goto error;
        } else {
            break;
        }

        if ((err = _makefile_hspace(ctxt, state, &hspc))) {
            break;
        }
    }
    _makefile_dest_token(&hspc);
    toke->tgts = tgts;
    if ((err = _makefile_token_add_part(toke, &tgts))) {
        goto error;
    }

    if (! (err = _gnumake_double_colon(ctxt, state, &part))) {
        _makefile_token_set_type(rule, &_gnumake_double_colon_rule_token_type);
        rule->oper = part;
        if ((err = _makefile_token_add_part((_makefile_token_t*) rule, &part))) {
            goto error;
        } else if ((err = _gnumake_normal_rule_tail(ctxt, state, rule))) {
            goto error;
        }
    } else if (! (err = _gnumake_single_colon(ctxt, state, &part))) {
        if ((err = _makefile_token_add_part((_makefile_token_t*) rule, &part))) {
            goto error;
        } else if (! (err = _gnumake_static_rule_tail(ctxt, state, rule))) {
            _makefile_token_set_type(rule, &_gnumake_static_pattern_rule_token_type);
        } else if (! (err = _gnumake_normal_rule_tail(ctxt, state, rule))) {
            _makefile_token_set_type(rule, &_gnumake_single_colon_rule_token_type);
        } else {
            goto error;
        }
    }

    *rulep = rule;
    return 0;

error:
    _makefile_back_token(state, rule);
    _makefile_dest_token(&part);
    _makefile_dest_token(&hspc);
    _makefile_dest_token(&tgts);
    _makefile_dest_rule_token(&rule);
    return err;
}

int
_gnumake_double_colon (
    _makefile_context_t*    ctxt,
    _makefile_state_t*      state,
    _makefile_token_t**     tokep
    )
{
    _makefile_token_t*  toke = NULL;
    _makefile_char_t    cval;
    _makefile_size_t    csiz;
    int                 err  = 0;

    if ((err = _makefile_make_token(state, &toke, &_gnumake_double_colon_token_type))) {
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
_gnumake_single_colon (
    _makefile_context_t*        ctxt,
    _makefile_state_t*          state,
    _makefile_token_t**         tokep
    )
{
    _makefile_token_t*  toke = NULL;
    _makefile_char_t    cval;
    _makefile_size_t    csiz;
    int                 err  = 0;

    if ((err = _makefile_make_token(state, &toke, &_gnumake_single_colon_token_type))) {
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
_gnumake_normal_rule_tail (
    _makefile_context_t*        ctxt,
    _makefile_state_t*          state,
    _makefile_rule_token_t*     rule
    )
{
    _makefile_directive_f           func = NULL;
    _makefile_token_t*              assi = NULL;
    _makefile_token_t*              deps = NULL;
    _makefile_token_t*              oron = NULL;
    _makefile_token_t*              part = NULL;
    _makefile_size_t                nwrd = 0;
    int                             err  = 0;

    func = _gnumake_directive(
        ctxt, state, NULL,
        "override", "private",
        "export", "unexport",
        "define", "undefine"
    );
    if (func) {
        if ((err = (*func)(ctxt, state, &assi))) {
            goto error;
        }
        rule->assi = assi;
        if ((err = _makefile_token_add_part(rule, &assi))) {
            goto error;
        }
        return 0;
    } else if (! (err = _gnumake_parse_assignment(ctxt, state, &assi))) {
        rule->assi = assi;
        if ((err = _makefile_token_add_part(rule, &assi))) {
            goto error;
        }
        return 0;
    }

    if ((err = _makefile_make_token(state, &here, NULL))) {
        goto error;
    }

    if (! (err = _gnumake_depends(ctxt, state, &deps))) {
        rule->deps = deps;
        if ((err = _makefile_token_add_part((_makefile_token_t*) rule, &deps))) {
            goto error;
        }
    }
    (void) _makefile_hspace(ctxt, state, NULL);

    if (! (err = _gnumake_orderonly(ctxt, state, &oron))) {
        rule->oron = oron;
        if ((err = _makefile_token_add_part((_makefile_token_t*) rule, &oron))) {
            goto error;
        }
    }
    (void) _makefile_hspace(ctxt, state, NULL);

    if (! (err = _gnumake_semicolon(ctxt, state, &part))) {
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
    _makefile_back_token(state, here);
    _makefile_dest_token(&assi);
    _makefile_dest_token(&part);
    _makefile_dest_token(&oron);
    _makefile_dest_token(&deps);
    _makefile_dest_token(&here);
    return err;
}

int
_gnumake_orderonly (
    _makefile_context_t*        ctxt,
    _makefile_state_t*          state,
    _makefile_token_t**         tokep
    )
{
    _makefile_token_t*  toke = NULL;
    _makefile_token_t*  part = NULL;
    int                 err  = 0;

    if ((err = _makefile_make_token(state, &toke, &_gnumake_orderonly_token_type))) {
        goto error;
    } else if ((err = _gnumake_pipe(ctxt, state, &part))) {
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
    _makefile_dest_token(&part);
    _makefile_dest_token(&oron);
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
    _makefile_size_t   nwrd = 0;
    int                err  = 0;

    if ((err = _makefile_make_token(state, &toke, &_makefile_depends_token_type))) {
        goto error;
    }
    for (nwrd = 0; ; ) {
        if ((err = _gnumake_dep_word(ctxt, state, &part))) {
            break;
        } else if (_makefile_token_add_part(deps, &part)) {
            goto error;
        }
        ++nwrd;
        (void) _makefile_hspace(state, NULL);
    }
    if (! nwrd) {
        err = ENOENT;
        goto error;
    }

    *tokep = toke;
    return 0;

error:
    _makefile_dest_token(&part);
    _makefile_dest_token(&toke);
    return err;
}

int
_gnumake_pipe (
    _makefile_context_t*        ctxt,
    _makefile_state_t*          state,
    _makefile_token_t**         tokep
    )
{
    _makefile_token_t*  toke = NULL;
    _makefile_char_t    cval;
    _makefile_size_t    csiz;
    int                 err  = 0;

    if (tokep) {
        if ((err = _makefile_make_token(state, &toke, &_gnumake_pipe_token_type))) {
            return err;
        }
    }

    if ((err = _makefile_read_char(state, &cval, &csiz))) {
        goto error;
    } else if (cval != '|') {
        err = EINVAL;
        goto error;
    } else if (tokep) {
        _makefile_token_add_size(toke, csiz, 0);
    }

    if (tokep) {
        *tokep = toke;
    }
    return 0;

error:
    if (tokep) {
        _makefile_dest_token(&toke);
    }
    return err;
}

int
_gnumake_semicolon (
    _makefile_state_t*          state,
    _makefile_token_t**         tokep
    )
{
    _makefile_token_t*  toke = NULL;
    _makefile_char_t    cval;
    _makefile_size_t    csiz;
    int                 err  = 0;

    if (tokep) {
        if ((err = _makefile_make_token(state, &toke, &_gnumake_semicolon_token_type))) {
            return err;
        }
    }

    if ((err = _makefile_read_char(state, &cval, &csiz))) {
        goto error;
    } else if (cval != ';') {
        err = EINVAL;
        goto error;
    } else if (tokep) {
        _makefile_token_add_size(toke, csiz, 0);
    }

    if (tokep) {
        *tokep = toke;
    }
    return 0;

error:
    if (tokep) {
        _makefile_dest_token(&toke);
    }
    return err;
}

int
_gnumake_static_rule_tail (
    _makefile_context_t*        ctxt,
    _makefile_state_t*          state,
    _makefile_rule_token_t*     rule
    )
{
    _makefile_token_t*      patt = NULL;
    _makefile_token_t*      deps = NULL;
    _makefile_token_t*      part = NULL;
    int                     err  = 0;

    if ((err = _makefile_make_token(state, &patt, &_gnumake_pattern_token_type))) {
        return err;
    }

    /* Read a single pattern. */
    if (! (err = _gnumake_dep_patt(ctxt, state, &part))) {
        if ((err = _makefile_token_add_part(patt, &part))) {
            goto error;
        }
    }
    rule->patt = patt;
    if ((err = _makefile_token_add_part((_makefile_token_t*) rule, &patt))) {
        goto error;
    }
    (void) _makefile_hspace(state, NULL);

    /* Require a single colon after the pattern. */
    if ((err = _gnumake_single_colon(ctxt, state, &part))) {
        goto error;
    } else if ((err = _makefile_token_add_part((_makefile_token_t*) rule, &part))) {
        goto error;
    }

    if ((err = _makefile_make_token(state, &deps, &_gnumake_depends_token_type))) {
        return err;
    }
    for ( ; ; ) {
        (void) _makefile_hspace(ctxt, state, NULL);
        if ((err = _gnumake_dep_patt(ctxt, state, &part))) {
            break;
        } else if ((err = _makefile_token_add_part(deps, &part))) {
            goto error;
        }
    }
    rule->deps = deps;
    if ((err = _makefile_token_add_part((_makefile_token_t*) rule, &deps))) {
        goto error;
    }

    return 0;

error:
    _makefile_state_back_token(state, patt);
    _makefile_dest_token(&part);
    _makefile_dest_token(&deps);
    _makefile_dest_token(&patt);
    return err;
}

int
__gnumake_word (
    _makefile_context_t*        ctxt,
    _makefile_state_t*          state,
    _makefile_token_t**         tokep,
    const char*                 term
    )
{
    _makefile_token_t*      toke = NULL;
    _makefile_token_t*      part = NULL;
    int                     err  = 0;

    if ((err = _makefile_make_token(state, &toke, &_gnumake_word_token_type))) {
        return err;
    }

    for ( ; ; ) {
        if ((err = _makefile_read_char(state, &cval, &csiz))) {
            break;
        } else if (_makefile_isspace(ctxt, state, cval) || strchr(term, cval)) {
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
                        || ! (err = _makefile_escapeseq(ctxt, state, &part))
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
                if (_makefile_token_get_type(toke) == &_gnumake_patt_token_type) {
                    _makefile_token_add_size(toke, csiz, 0);
                } else if ((err = _makefile_back_char(state, csiz, 0))) {
                    goto error;
                } else if ((err = _gnumake_percent(ctxt, state, &part))) {
                    goto error;
                } else if ((err = _makefile_token_add_part(toke, &part))) {
                    goto error;
                } else {
                    _makefile_token_set_type(toke, &_gnumake_patt_token_type);
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

