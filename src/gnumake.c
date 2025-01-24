#include <stdio.h>
#include <string.h>

#include <shmoo/input.h>

int
main (int argc, char* const argv[], char* const envp[]) {
    shmoo_input_t*  input = NULL;
    const char*     file;
    int             err;

    if (argc == 0) {
        file = "Makefile";
    } else {
        file = argv[1];
    }

    if ((err = shmoo_input_open_path(&input, file))) {
        fprintf(stderr, "ERROR: %d = %s\n", err, strerror(err));
    }

    return 0;

    (void) envp;
}


