#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>
#include "../src/tcl_core.c"
#include "../src/extcmd.c"
#include "../src/baretcl_shell.c"

/* Implement HAL puts for Linux demo */
void tcl_hal_puts(const tcl_u8 *s) {
    fputs((const char *)s, stdout);
    fflush(stdout);
}

#define ARENA_SIZE (1024 * 1024)
static char arena[ARENA_SIZE];

static struct termios orig_termios;
void disableRawMode() {
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
}
void enableRawMode() {
    tcgetattr(STDIN_FILENO, &orig_termios);
    atexit(disableRawMode);
    struct termios raw = orig_termios;
    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

int main(int argc, char **argv) {
    tcl_init(arena, ARENA_SIZE);
    TclCtx *ctx = (TclCtx *)arena;
    tcl_register_ext_cmds(ctx);
    if (tcl_load_bootstrap(ctx) != TCL_OK) {
        printf("Bootstrap Error: %s\n", (const char *)tcl_get_result(ctx));
    }
    
    if (argc > 1) {
        /* Execute file */
        FILE *f = fopen(argv[1], "rb");
        if (!f) { perror("fopen"); return 1; }
        fseek(f, 0, SEEK_END);
        long len = ftell(f);
        fseek(f, 0, SEEK_SET);
        char *script = malloc(len + 1);
        fread(script, 1, len, f);
        script[len] = 0;
        fclose(f);
        int status = tcl_eval(ctx, (const tcl_u8 *)script);
        if (status == TCL_ERROR) printf("Error: %s\n", (const char *)tcl_get_result(ctx));
        free(script);
    } else {
        /* Advanced Shell REPL */
        enableRawMode();
        TclShell sh;
        shell_init(&sh);
        
        tcl_hal_puts((const tcl_u8 *)"BareTcl Shell (Raw Mode). Type 'exit' to quit.\n");
        tcl_hal_puts((const tcl_u8 *)"> ");
        
        while (1) {
            tcl_u8 c;
            if (read(STDIN_FILENO, &c, 1) <= 0) break;
            
            if (shell_handle_char(&sh, c, "> ") == 1) {
                int status = tcl_eval(ctx, sh.line);
                if (status == TCL_EXIT) break;
                if (status == TCL_ERROR) {
                    tcl_hal_puts((const tcl_u8 *)"Error: ");
                    tcl_hal_puts(tcl_get_result(ctx));
                    tcl_hal_puts((const tcl_u8 *)"\n");
                } else {
                    const tcl_u8 *res = tcl_get_result(ctx);
                    if (res && res[0]) {
                        tcl_hal_puts(res);
                        tcl_hal_puts((const tcl_u8 *)"\n");
                    }
                }
                /* Reset line buffer */
                for(tcl_u32 i=0; i<SHELL_MAX_LINE; i++) sh.line[i] = 0;
                sh.len = 0; sh.cursor = 0;
                tcl_hal_puts((const tcl_u8 *)"> ");
            }
        }
        disableRawMode();
    }
    return 0;
}
