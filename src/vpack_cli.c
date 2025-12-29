#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vaultpack/vaultpack.h"

static int usage(void)
{
    fprintf(stderr,
        "vpack — vaultpack command-line tool\n"
        "\n"
        "Usage:\n"
        "  vpack create <out.vpack> <files...>\n"
        "  vpack list   <in.vpack>\n"
        "\n");
    return 1;
}

int main(int argc, char **argv)
{
    if (argc < 2)
        return usage();

    if (strcmp(argv[1], "create") == 0) {
        if (argc < 4)
            return usage();
        fprintf(stderr, "vpack: create not yet implemented\n");
        return 1;
    }

    if (strcmp(argv[1], "list") == 0) {
        if (argc != 3)
            return usage();
        fprintf(stderr, "vpack: list not yet implemented\n");
        return 1;
    }

    return usage();
}
