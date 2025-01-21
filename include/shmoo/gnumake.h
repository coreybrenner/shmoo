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
#ifndef _shmoo_gnumake_h
#define _shmoo_gnumake_h

/*forward*/ struct _shmoo_gnumake_gmk_floc;
typedef     struct _shmoo_gnumake_gmk_floc shmoo_gnumake_gmk_floc_t;

/*forward*/ struct _shmoo_gnumake_gmk_load;
typedef     struct _shmoo_gnumake_gmk_load shmoo_gnumake_gmk_load_t;

struct _shmoo_gnumake_gmk_load {
    const char*     file;
    const char*     path;
    const char*     name;
};

GMK_EXPORT _shmoo_gnumake_gmk_get_loadlib (
    const char*     path,
    shmoo
struct _shmoo_gnumake_gmk_floc {
    const char*     filenm;
    unsigned long   lineno;
};

typedef char* (*_shmoo_gnumake_gmk_func_ptr_f) (
    const char*     __name,
    unsigned int    __argc,
    char**          __argv,
    ...
/*  void*           __makefile_context,
 *  void*           __input_state
 */
);

/* Part of GNU Make module API. */
typedef _shmoo_gnumake_gmk_floc_t     gmk_floc;
typedef _shmoo_gnumake_gmk_func_ptr_f gmk_func_ptr;

/* Part of GNU Make module API. */
#undef GMK_EXPORT
#if defined(_WIN32) || defined(_WIN64)
# if defined(GMK_BUILDING_MAKE)
#  define GMK_EXPORT __declspec(dllexport)
# else
#  define GMK_EXPORT __declspec(dllimport)
# endif
#else
# define  GMK_EXPORT /*nothing*/
#endif

/* Free memory returned by gmk_expand().  */
GMK_EXPORT void gmk_free (
    char*           __str
);

/* Allocate memory in libshmoo's current makefile context.  */
GMK_EXPORT char *gmk_alloc (
    unsigned int    __len       /* Number of bytes to allocate. */
);

/* Loads up libshmoo context parts, then evaluates the buffer. */
GMK_EXPORT void gmk_eval (
    const char*     __buffer,   /* NUL-terminated buffer to parse. */
    const gmk_floc* __floc      /* Becomes a shmoo_token_t locating the evaluation. */
);

/* Expands into a shmoo_buffer_t, saved and possibly logged in the back end,
 * but a copy of the buffer _IS_ handed to the caller to gmk_free().  The
 * evaluation and its various steps are still being recorded by libshmoo.
 */
GMK_EXPORT char* gmk_expand (
    const char*     __str       /* buffer to parse and expand against the current context. */
);


/***
 * Register an entry point with libshmoo which should be called when the
 * macro interpreter expands a call to the associated name.
 *
 * ARGUMENTS:
 *   name:
 *      Name of the function which should be present and callable in
 *      a macro expansion.
 *   func:
 *      Function entry point called when the above name is found in the
 *      function name slot in a macro expansion.
 *   min_args:
 *      Minimum number of arguments the function requires.
 *      May not exceed 255.
 *   max_args:
 *      Maximum number of arguments the function takes; 0 means no maximum.
 *      May not exceed 255.
 *   flags:
 *      This may be GMK_FUNC_DEFAULT or one or more of the following flags
 *      OR'd together:
 *          GMK_FUNC_NOEXPAND   
 *              The arguments given the function are not expanded prior
 *              to calling the function.  This allows the function to
 *              parse the arguments on its own?
 *
 * RETURNS:
 *   A pointer to the result of an expansion in the current makefile
 *   context, or a static empty buffer.  It is for this reason that
 *   libshmoo will manage this memory and associate it with the current
 *   makefile context.  The caller need not worry about managing this
 *   memory.
 */
GMK_EXPORT void gmk_add_function (
    const char*     __name,
    gmk_func_ptr    __func,
    unsigned int    __min_args,
    unsigned int    __max_args,
    unsigned int    __flags
);

#define GMK_FUNC_DEFAULT    0x00
#define GMK_FUNC_NOEXPAND   0x01

#undef gmk_get_function
#undef gmk_pop_function
#undef gmk_get_directive
#undef gmk_add_directive
#undef gmk_pop_directive

#ifndef _STRICT_GNU

# define gmk_get_function(name,funcp,min_argsp,max_argsp,flagsp) \
    (_shmoo_gnumake_gmk_get_function( \
        (name),(funcp)(min_argsp),(max_argsp), \
        (flagsp),(void*)0,(void*)0 \
    ))
GMK_EXPORT int _shmoo_gnumake_gmk_get_function(
    const char*     __name,
    gmk_func_ptr*   __funcp,
    unsigned int*   __min_argsp,
    unsigned int*   __max_argsp,
    unsigned int*   __flagsp,
    void*           __makefile_context,
    void*           __input_state
);

GMK_EXPORT int __shmoo_gnumake_gmk_pop_function (
    const char*     __name,
    gmk_func_ptr*   __funcp,
    void*           __makefile_context,
    void*           __input_state
);

# define gmk_pop_function(name) \
    (_shmoo_gnumake_gmk_pop_function((name),(void*)0,(void*)0))
inline gmk_func_ptr _shmoo_gnumake_gmk_pop_function(
    const char*     name,
    void*           makefile_context,
    void*           input_state
    )
{
    gmk_func_ptr func = 0;
    __shmoo_gnumake_pop_function(name, &func, makefile_context, input_state);
    return func;
}
        
/* Pluggable directive parser. */
typedef int (*gmk_dir_ptr) (
    const char*     __unexpanded_rest_of_line,
    ...
/*  void*           __makefile_context,
 *  void*           __input_state
 *
 * NULLs select defaults from the current makefile context.
 * Functions of these forms are used in libshmoo to register
 * pluggable directives into a context.
 */
);

GMK_EXPORT int __shmoo_gnumake_gmk_get_directive (
    const char*     __name,
    gmk_dir_ptr*    __funcp,
    void*           __makefile_context,
    void*           __input_state
);

# define gmk_get_directive(name) \
    (_shmoo_gnumake_gmk_get_directive((name),(void*)0,(void*)0))
inline gmk_dir_ptr _shmoo_gnumake_gmk_get_directive (
    const char*     name,
    void*           makefile_context,
    void*           input_state
    )
{
    gmk_dir_ptr func = 0;
    __shmoo_gnumake_gmk_get_directive(
        name, &func, makefile_context, input_state
    );
    return func;
}

GMK_EXPORT int __shmoo_gnumake_pop_directive (
    const char*     __name,
    gmk_dir_ptr*    __funcp,
    void*           __makefile_context,
    void*           __input_state
);

# define gmk_pop_directive(name) \
    (_shmoo_gnumake_gmk_pop_directive((name),(void*)0,(void*)0))
inline gmk_dir_ptr _shmoo_gnumake_gmk_pop_directive(
    const char*     name,
    void*           makefile_context,
    void*           input_state
    )
{
    gmk_dir_ptr func = 0;
    __shmoo_gnumake_gmk_pop_directive(
        name, &func, makefile_context, input_state
    );
    return func;
}

# define gmk_add_directive(name,func) \
    (_shmoo_gnumake_gmk_add_directive((name),(func),(void*)0,(void*)0))
GMK_EXPORT gmk_dir_ptr _shmoo_gnumake_gmk_add_directive (
    const char*     __name,
    gmk_dir_ptr     __func,
    void*           __makefile_context,
    void*           __input_state
);

#endif /* ! _STRICT_GNU */

#endif /* ! _shmoo_gnumake_h */
