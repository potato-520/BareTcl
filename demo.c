#include <stdio.h>
#include <stdlib.h>
#include "tcl_core.c"

/* Implement HAL puts for Linux demo */
void tcl_hal_puts(const char *s) {
    fputs(s, stdout);
}

#define ARENA_SIZE (1024 * 1024)
static char arena[ARENA_SIZE];

int main(int argc, char **argv) {
    tcl_init(arena, ARENA_SIZE);
    TclCtx *ctx = (TclCtx *)arena;
    printf("Init: p_top=%u, t_bot=%u, size=%u\n", ctx->p_top, ctx->t_bot, ctx->size);

    if (argc > 1) {
        /* Execute file */
        FILE *f = fopen(argv[1], "rb");
        if (!f) {
            perror("fopen");
            return 1;
        }
        fseek(f, 0, SEEK_END);
        long len = ftell(f);
        fseek(f, 0, SEEK_SET);
        char *script = malloc(len + 1);
        fread(script, 1, len, f);
        script[len] = 0;
        fclose(f);

        int status = tcl_eval(ctx, script);
        if (status == TCL_ERROR) {
            printf("Error: %s\n", tcl_get_result(ctx));
        }
        free(script);
    } else {
        /* REPL */
        char buf[256];
        printf("Tclsh.v2 REPL (Type 'exit' to quit)\n");
        while (1) {
            printf("> ");
            if (!fgets(buf, sizeof(buf), stdin)) break;
            int status = tcl_eval(ctx, buf);
            if (status == TCL_EXIT) break;
            if (status == TCL_ERROR) {
                printf("Error\n");
            } else {
                const char *res = tcl_get_result(ctx);
                if (res && res[0]) printf("%s\n", res);
            }
        }
    }
    return 0;
}
