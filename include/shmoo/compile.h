#undef __CDECL_BEG
#undef __CDECL_END
#ifdef __cplusplus
#  define __CDECL_BEG   extern "C" {
#  define __CDECL_END   } /*C*/
#else
#  define __CDECL_BEG   /*nothing*/
#  define __CDECL_END   /*nothing*/
#endif

#undef elemsof
#define elemsof(x)      (sizeof((x)) / sizeof((x)[0]))

#undef offsetof
#define offsetof(t,m)   ((unsigned long int) ((const t*) 0)->m)


