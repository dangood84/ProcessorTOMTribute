#include "tom.h"

#include <stdio.h>
#include <string.h>

#include "sdl_inc.h"

static void usage(void)
{
    fprintf(stderr,
            "usage: tom [program.tom]\n"
            "       tom --test [examples-dir]\n"
            "       tom --snap start.bmp mid.bmp [program.tom]\n");
}

int main(int argc, char **argv)
{
    const char *sample = NULL;
    const char *snap_start = NULL;
    const char *snap_mid = NULL;
    int i;

    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--test") == 0) {
            const char *dir = (i + 1 < argc) ? argv[i + 1] : "examples";
            return tom_run_tests(dir);
        }
        if (strcmp(argv[i], "--help") == 0) {
            usage();
            return 0;
        }
        if (strcmp(argv[i], "--snap") == 0) {
            if (i + 2 >= argc) {
                usage();
                return 1;
            }
            snap_start = argv[++i];
            snap_mid = argv[++i];
            continue;
        }
        if (argv[i][0] == '-') {
            usage();
            return 1;
        }
        sample = argv[i];
    }

    return tom_ui_main(sample, snap_start, snap_mid);
}
