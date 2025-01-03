#ifndef _shmoo_handle_h
#define _shmoo_handle_h

#include <shmoo/compile.h>

__CDECL_BEG

#if defined(_Win32) || defined(_Win64)
typedef HANDLE  shmoo_handle_t;
#else
typedef int     shmoo_handle_t;
#endif

__CDECL_END

#endif /* ! _shmoo_handle_h */
