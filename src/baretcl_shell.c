/*
 * baretcl_shell.c - High-reliability, Zero-malloc Line Editor for BareTcl
 */

#define SHELL_MAX_LINE 256
#define SHELL_MAX_HIST 16

typedef struct {
    tcl_u8  line[SHELL_MAX_LINE];
    tcl_u32 len;
    tcl_u32 cursor;
    
    tcl_u8  history[SHELL_MAX_HIST][SHELL_MAX_LINE];
    tcl_i32 hist_top;
    tcl_i32 hist_idx;
    tcl_i32 hist_cnt;

    tcl_u8  esc_state;
    tcl_u8  esc_buf[8];
    tcl_u8  esc_idx;

    tcl_u8  multi_line;
    tcl_i32 brace_level;
} TclShell;

#define ESC_IDLE 0
#define ESC_SAW_ESC 1
#define ESC_SAW_BRKT 2

static void shell_init(TclShell *sh) {
    for (tcl_u32 i=0; i<sizeof(TclShell); i++) ((tcl_u8*)sh)[i] = 0;
    sh->hist_idx = -1;
}

static void shell_clear_line(TclShell *sh) {
    /* ANSI: Move cursor to start, Clear line */
    tcl_hal_puts((const tcl_u8*)"\r\x1b[K");
}

static void shell_refresh(TclShell *sh, const char *prompt) {
    shell_clear_line(sh);
    tcl_hal_puts((const tcl_u8*)prompt);
    tcl_hal_puts(sh->line);
    /* ANSI: Move cursor back to current position */
    if (sh->cursor < sh->len) {
        char buf[16];
        tcl_u32 move = sh->len - sh->cursor;
        tcl_hal_puts((const tcl_u8*)"\r");
        tcl_hal_puts((const tcl_u8*)prompt);
        for(tcl_u32 i=0; i<sh->cursor; i++) {
            tcl_u8 c[2] = {sh->line[i], 0};
            tcl_hal_puts(c);
        }
    }
}

static void shell_insert(TclShell *sh, tcl_u8 c) {
    if (sh->len + 1 >= SHELL_MAX_LINE) return;
    if (sh->cursor == sh->len) {
        sh->line[sh->len++] = c;
        sh->line[sh->len] = 0;
        sh->cursor++;
        tcl_u8 s[2] = {c, 0};
        tcl_hal_puts(s);
    } else {
        for (tcl_i32 i = sh->len; i >= (tcl_i32)sh->cursor; i--) sh->line[i+1] = sh->line[i];
        sh->line[sh->cursor++] = c;
        sh->len++;
        sh->line[sh->len] = 0;
        /* Simple refresh for insertion */
        tcl_hal_puts((const tcl_u8*)"\x1b[K"); // Clear from cursor to end
        tcl_hal_puts(sh->line + sh->cursor - 1);
        for (tcl_u32 i=0; i < sh->len - sh->cursor; i++) tcl_hal_puts((const tcl_u8*)"\b");
    }
}

static void shell_backspace(TclShell *sh) {
    if (sh->cursor == 0) return;
    if (sh->cursor == sh->len) {
        sh->line[--sh->len] = 0;
        sh->cursor--;
        tcl_hal_puts((const tcl_u8*)"\b \b");
    } else {
        for (tcl_u32 i = sh->cursor - 1; i < sh->len; i++) sh->line[i] = sh->line[i+1];
        sh->len--;
        sh->cursor--;
        tcl_hal_puts((const tcl_u8*)"\b");
        tcl_hal_puts((const tcl_u8*)"\x1b[K");
        tcl_hal_puts(sh->line + sh->cursor);
        for (tcl_u32 i=0; i < sh->len - sh->cursor; i++) tcl_hal_puts((const tcl_u8*)"\b");
    }
}

static void shell_hist_push(TclShell *sh) {
    if (sh->len == 0) return;
    /* Don't push duplicates */
    if (sh->hist_cnt > 0) {
        tcl_i32 prev = (sh->hist_top - 1 + SHELL_MAX_HIST) % SHELL_MAX_HIST;
        if (t_scmp(sh->line, sh->history[prev]) == 0) return;
    }
    t_mcpy(sh->history[sh->hist_top], sh->line, sh->len + 1);
    sh->hist_top = (sh->hist_top + 1) % SHELL_MAX_HIST;
    if (sh->hist_cnt < SHELL_MAX_HIST) sh->hist_cnt++;
}

static void shell_hist_move(TclShell *sh, tcl_i32 dir) {
    if (sh->hist_cnt == 0) return;
    if (sh->hist_idx == -1) sh->hist_idx = sh->hist_top;
    
    tcl_i32 next = sh->hist_idx + dir;
    if (next < 0) next = 0;
    if (sh->hist_cnt < SHELL_MAX_HIST) {
        if (next >= sh->hist_cnt) next = sh->hist_cnt - 1;
    } else {
        /* Circular boundary logic could be added here for full rotation */
    }
    
    /* For simplicity, just linear scan within cnt */
    tcl_i32 target = (sh->hist_top + (dir < 0 ? -1 : 1) * 1) % SHELL_MAX_HIST; // Placeholder logic
    /* Actually let's just use hist_idx relative to top */
    // ... logic for prev/next
}

/* Simplified input handler for demo */
tcl_i32 shell_handle_char(TclShell *sh, tcl_u8 c, const char *prompt) {
    if (sh->esc_state == ESC_SAW_ESC) {
        if (c == '[') { sh->esc_state = ESC_SAW_BRKT; return 0; }
        sh->esc_state = ESC_IDLE; return 0;
    }
    if (sh->esc_state == ESC_SAW_BRKT) {
        if (c == 'A') { /* UP */ 
            if (sh->hist_cnt > 0) {
                if (sh->hist_idx == -1) sh->hist_idx = (sh->hist_top - 1 + SHELL_MAX_HIST) % SHELL_MAX_HIST;
                else sh->hist_idx = (sh->hist_idx - 1 + SHELL_MAX_HIST) % SHELL_MAX_HIST;
                t_mcpy(sh->line, sh->history[sh->hist_idx], SHELL_MAX_LINE);
                sh->len = t_slen(sh->line); sh->cursor = sh->len;
                shell_refresh(sh, prompt);
            }
        }
        else if (c == 'B') { /* DOWN */
             if (sh->hist_idx != -1) {
                sh->hist_idx = (sh->hist_idx + 1) % SHELL_MAX_HIST;
                t_mcpy(sh->line, sh->history[sh->hist_idx], SHELL_MAX_LINE);
                sh->len = t_slen(sh->line); sh->cursor = sh->len;
                shell_refresh(sh, prompt);
             }
        }
        else if (c == 'C') { if (sh->cursor < sh->len) { sh->cursor++; tcl_hal_puts((const tcl_u8*)"\x1b[C"); } }
        else if (c == 'D') { if (sh->cursor > 0) { sh->cursor--; tcl_hal_puts((const tcl_u8*)"\x1b[D"); } }
        sh->esc_state = ESC_IDLE; return 0;
    }

    if (c == '\x1b') { sh->esc_state = ESC_SAW_ESC; return 0; }
    if (c == '\r' || c == '\n') {
        tcl_hal_puts((const tcl_u8*)"\n");
        /* Check brace level */
        sh->brace_level = 0;
        for (tcl_u32 i=0; i<sh->len; i++) {
            if (sh->line[i] == '{') sh->brace_level++;
            if (sh->line[i] == '}') sh->brace_level--;
        }
        if (sh->brace_level <= 0) {
            sh->brace_level = 0;
            shell_hist_push(sh);
            sh->hist_idx = -1;
            return 1; // Line complete
        } else {
            /* Continue multi-line */
            sh->line[sh->len++] = '\n';
            sh->line[sh->len] = 0;
            sh->cursor = sh->len;
            tcl_hal_puts((const tcl_u8*)".. ");
            return 0;
        }
    }
    if (c == 0x08 || c == 0x7f) { shell_backspace(sh); return 0; }
    if (c >= 32 && c < 127) { shell_insert(sh, c); return 0; }
    return 0;
}
