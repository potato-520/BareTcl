#include <stdio.h>
#include <stdlib.h>
#include "tcl_core.c"
#include "extcmd.c"

/* Implement HAL puts for Linux demo */
void tcl_hal_puts(const tcl_u8 *s) {
    fputs((const char *)s, stdout);
    fflush(stdout);
}

#define ARENA_SIZE (1024 * 1024)
static char arena[ARENA_SIZE];

int main(int argc, char **argv) {
    tcl_init(arena, ARENA_SIZE);
    TclCtx *ctx = (TclCtx *)arena;
    tcl_register_ext_cmds(ctx);
    if (tcl_load_bootstrap(ctx) != TCL_OK) {
        printf("Bootstrap Error: %s\n", (const char *)tcl_get_result(ctx));
    }
    
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

        int status = tcl_eval(ctx, (const tcl_u8 *)script);
        if (status == TCL_ERROR) {
            printf("Error: %s\n", (const char *)tcl_get_result(ctx));
        }
        free(script);
    } else {
        /* REPL */
        char buf[256];
        printf("Tclsh.v2 REPL (Type 'exit' to quit)\n");
        while (1) {
            printf("> ");
            if (!fgets(buf, sizeof(buf), stdin)) break;
            int status = tcl_eval(ctx, (const tcl_u8 *)buf);
            if (status == TCL_EXIT) break;
            if (status == TCL_ERROR) {
                printf("Error: %s\n", (const char *)tcl_get_result(ctx));
            } else {
                const tcl_u8 *res = tcl_get_result(ctx);
                if (res && res[0]) printf("%s\n", (const char *)res);
            }
        }
    }
    return 0;
}
