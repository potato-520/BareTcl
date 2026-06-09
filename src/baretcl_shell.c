/*
 * baretcl_shell.c - 为 BareTcl 设计的高可靠、零内存分配行编辑器
 */

#define SHELL_MAX_LINE 256      /* 单行输入最大长度 */
#define SHELL_MAX_HIST 16       /* 历史记录最大条数 */

/* Shell 编辑器状态结构体 */
typedef struct {
    tcl_u8  line[SHELL_MAX_LINE];       /* 当前输入行缓冲区 */
    tcl_u32 len;                        /* 当前行总长度 */
    tcl_u32 cursor;                     /* 光标当前物理位置索引 */
    
    tcl_u8  history[SHELL_MAX_HIST][SHELL_MAX_LINE]; /* 历史命令循环队列 */
    tcl_i32 hist_top;                   /* 队列写入头索引 */
    tcl_i32 hist_idx;                   /* 历史浏览当前索引 */
    tcl_i32 hist_cnt;                   /* 已存历史条数 */

    tcl_u8  esc_state;                  /* ANSI 转义序列解析状态机 */
    tcl_u8  esc_buf[8];                 /* 转义序列临时缓冲区 */
    tcl_u8  esc_idx;                    /* 缓冲区写入游标 */

    tcl_u8  multi_line;                 /* 多行模式标志位 */
    tcl_i32 brace_level;                /* 花括号嵌套深度，用于多行自动换行 */
} TclShell;

/* 转义解析状态枚举 */
#define ESC_IDLE 0              /* 空闲态 */
#define ESC_SAW_ESC 1           /* 已接收到 ESC 字符 */
#define ESC_SAW_BRKT 2          /* 已接收到 [ 字符 */

/* 初始化 Shell 状态 */
static void shell_init(TclShell *shell) {
    /* 物理内存清零 */
    for (tcl_u32 index = 0; index < sizeof(TclShell); index++) {
        ((tcl_u8*)shell)[index] = 0;
    }
    shell->hist_idx = -1;       /* 初始化历史索引为无效值 */
}

/* 清除当前终端行显示 */
static void shell_clear_line(TclShell *shell) {
    /* ANSI: 移动光标至行首并清除光标后所有内容 */
    tcl_hal_puts((const tcl_u8*)"\r\x1b[K");
}

/* 刷新 Shell 提示符与输入行显示 */
static void shell_refresh(TclShell *shell, const char *prompt) {
    shell_clear_line(shell);    /* 首先清空当前行 */
    tcl_hal_puts((const tcl_u8*)prompt); /* 重新输出提示符 */
    tcl_hal_puts(shell->line);  /* 输出缓冲区内容 */
    /* 修正光标物理位置 */
    if (shell->cursor < shell->len) {
        tcl_hal_puts((const tcl_u8*)"\r"); /* 回到行首 */
        tcl_hal_puts((const tcl_u8*)prompt); /* 重出提示符 */
        for (tcl_u32 index = 0; index < shell->cursor; index++) { /* 步进至光标位 */
            tcl_u8 char_buf[2] = {shell->line[index], 0}; /* 构造单字符字符串 */
            tcl_hal_puts(char_buf); /* 逐个字符物理输出 */
        }
    }
}

/* 在光标位置插入一个字符 */
static void shell_insert(TclShell *shell, tcl_u8 character) {
    if (shell->len + 1 >= SHELL_MAX_LINE) { /* 溢出检查 */
        return;                 /* 忽略 */
    }
    if (shell->cursor == shell->len) { /* 在行尾插入 */
        shell->line[shell->len++] = character; /* 写入字符并累加长度 */
        shell->line[shell->len] = 0; /* 维护结束符 */
        shell->cursor++;        /* 移动光标 */
        tcl_u8 char_buf[2] = {character, 0}; /* 物理输出新字符 */
        tcl_hal_puts(char_buf);
    } else {                    /* 在行中插入，需移动后随内容 */
        for (tcl_i32 index = (tcl_i32)shell->len; index >= (tcl_i32)shell->cursor; index--) {
            shell->line[index + 1] = shell->line[index]; /* 数据后移 */
        }
        shell->line[shell->cursor++] = character; /* 插入新字符 */
        shell->len++;           /* 增加总长度 */
        shell->line[shell->len] = 0; /* 结束符 */
        /* 刷新显示 */
        tcl_hal_puts((const tcl_u8*)"\x1b[K"); /* 清除后续显示 */
        tcl_hal_puts(shell->line + shell->cursor - 1); /* 输出新插入及后续内容 */
        for (tcl_u32 index = 0; index < shell->len - shell->cursor; index++) {
            tcl_hal_puts((const tcl_u8*)"\b"); /* 将物理光标退回逻辑位置 */
        }
    }
}

/* 执行退格删除操作 */
static void shell_backspace(TclShell *shell) {
    if (shell->cursor == 0) {   /* 已在行首 */
        return;                 /* 忽略 */
    }
    if (shell->cursor == shell->len) { /* 在行尾删除 */
        shell->line[--shell->len] = 0; /* 截断字符串 */
        shell->cursor--;        /* 回退光标 */
        tcl_hal_puts((const tcl_u8*)"\b \b"); /* 物理回退并擦除 */
    } else {                    /* 在行中删除，需前移后随内容 */
        for (tcl_u32 index = shell->cursor - 1; index < shell->len; index++) {
            shell->line[index] = shell->line[index + 1]; /* 数据前移 */
        }
        shell->len--;           /* 减少长度 */
        shell->cursor--;        /* 光标逻辑回退 */
        tcl_hal_puts((const tcl_u8*)"\b"); /* 物理光标回退 */
        tcl_hal_puts((const tcl_u8*)"\x1b[K"); /* 清除旧显内容 */
        tcl_hal_puts(shell->line + shell->cursor); /* 重新显示后随部分 */
        for (tcl_u32 index = 0; index < shell->len - shell->cursor; index++) {
            tcl_hal_puts((const tcl_u8*)"\b"); /* 物理回退至逻辑位置 */
        }
    }
}

/* 将当前行压入历史记录栈 */
static void shell_hist_push(TclShell *shell) {
    if (shell->len == 0) {      /* 忽略空行 */
        return;
    }
    /* 避免连续重复记录 */
    if (shell->hist_cnt > 0) {
        tcl_i32 prev_index = (shell->hist_top - 1 + SHELL_MAX_HIST) % SHELL_MAX_HIST;
        if (t_scmp(shell->line, shell->history[prev_index]) == 0) {
            return;             /* 与前一笔相同则跳过 */
        }
    }
    /* 拷贝数据至历史环形缓冲区 */
    t_mcpy(shell->history[shell->hist_top], shell->line, shell->len + 1);
    shell->hist_top = (shell->hist_top + 1) % SHELL_MAX_HIST; /* 移动写入头 */
    if (shell->hist_cnt < SHELL_MAX_HIST) {
        shell->hist_cnt++;      /* 增加计数 */
    }
}

/* 核心输入处理函数：处理来自串口的原始字节流 */
tcl_i32 shell_handle_char(TclShell *shell, tcl_u8 character, const char *prompt) {
    /* 1. 处理 ANSI 转义序列 */
    if (shell->esc_state == ESC_SAW_ESC) {
        if (character == '[') {
            shell->esc_state = ESC_SAW_BRKT; /* 进入子序列态 */
            return 0;
        }
        shell->esc_state = ESC_IDLE; /* 无效序列，回到空闲 */
        return 0;
    }
    if (shell->esc_state == ESC_SAW_BRKT) {
        if (character == 'A') { /* 方向键：上 (UP) */ 
            if (shell->hist_cnt > 0) {
                if (shell->hist_idx == -1) { /* 首次查历史 */
                    shell->hist_idx = (shell->hist_top - 1 + SHELL_MAX_HIST) % SHELL_MAX_HIST;
                } else {                /* 继续翻阅 */
                    shell->hist_idx = (shell->hist_idx - 1 + SHELL_MAX_HIST) % SHELL_MAX_HIST;
                }
                /* 从历史记录载入 */
                t_mcpy(shell->line, shell->history[shell->hist_idx], SHELL_MAX_LINE);
                shell->len = t_slen(shell->line);
                shell->cursor = shell->len;
                shell_refresh(shell, prompt); /* 重新渲染显示 */
            }
        }
        else if (character == 'B') { /* 方向键：下 (DOWN) */
             if (shell->hist_idx != -1) {
                shell->hist_idx = (shell->hist_idx + 1) % SHELL_MAX_HIST;
                t_mcpy(shell->line, shell->history[shell->hist_idx], SHELL_MAX_LINE);
                shell->len = t_slen(shell->line);
                shell->cursor = shell->len;
                shell_refresh(shell, prompt);
             }
        }
        else if (character == 'C') { /* 方向键：右 (RIGHT) */
            if (shell->cursor < shell->len) {
                shell->cursor++; /* 逻辑步进 */
                tcl_hal_puts((const tcl_u8*)"\x1b[C"); /* 物理步进 */
            }
        }
        else if (character == 'D') { /* 方向键：左 (LEFT) */
            if (shell->cursor > 0) {
                shell->cursor--; /* 逻辑回退 */
                tcl_hal_puts((const tcl_u8*)"\x1b[D"); /* 物理回退 */
            }
        }
        shell->esc_state = ESC_IDLE; /* 序列解析完毕 */
        return 0;
    }

    /* 2. 处理特殊功能键 */
    if (character == '\x1b') {  /* 检测到转义起始字符 */
        shell->esc_state = ESC_SAW_ESC;
        return 0;
    }
    if (character == '\r' || character == '\n') { /* 回车/换行：提交当前行 */
        tcl_hal_puts((const tcl_u8*)"\n"); /* 回显换行 */
        /* 检测花括号配对状态，实现自动多行支持 */
        shell->brace_level = 0;
        for (tcl_u32 index = 0; index < shell->len; index++) {
            if (shell->line[index] == '{') shell->brace_level++;
            if (shell->line[index] == '}') shell->brace_level--;
        }
        if (shell->brace_level <= 0) { /* 嵌套已平衡，完成输入 */
            shell->brace_level = 0;
            shell_hist_push(shell); /* 记录历史 */
            shell->hist_idx = -1;   /* 重置浏览游标 */
            return 1;               /* 返回 1 指示指令已就绪 */
        } else {                    /* 嵌套未平衡，进入多行续写态 */
            if (shell->len + 1 < SHELL_MAX_LINE) {
                shell->line[shell->len++] = '\n'; /* 自动补入换行符 */
                shell->line[shell->len] = 0;
                shell->cursor = shell->len;
                tcl_hal_puts((const tcl_u8*)".. "); /* 输出续行提示符 */
            }
            return 0;               /* 指令未完成 */
        }
    }
    /* 处理删除键 */
    if (character == 0x08 || character == 0x7f) {
        shell_backspace(shell);
        return 0;
    }
    /* 处理普通可打印字符 */
    if (character >= 32 && character < 127) {
        shell_insert(shell, character);
        return 0;
    }
    return 0;
}
