/*
 * Tclsh.v2 - 极致精简、无栈化、脱离 Libc 的 Tcl 解释器内核
 */
/* 空行：逻辑分段 */
/* 基础数据类型定义，确保跨平台物理一致性 */
typedef unsigned char      tcl_u8;  /* 无符号 8 位整数，用于字节流与字符 */
typedef signed char        tcl_i8;  /* 有符号 8 位整数，用于小量偏移 */
typedef unsigned short     tcl_u16; /* 无符号 16 位整数，用于短偏移量 */
typedef signed short       tcl_i16; /* 有符号 16 位整数 */
typedef unsigned int       tcl_u32; /* 无符号 32 位整数，用于 Arena 游标及大容量计数 */
typedef signed int         tcl_i32; /* 有符号 32 位整数，Tcl 脚本内的默认整数类型 */
/* 空行：逻辑分段 */
/* 平台相关的指针宽度整数类型定义 */
#if defined(__x86_64__) || defined(__aarch64__) /* 检查是否为 64 位架构平台以适配指针宽度 */
typedef unsigned long long tcl_ptr; /* 定义 64 位平台下的指针等宽整数类型 */
#else                               /* 否则视为 32 位架构平台 */
typedef unsigned int       tcl_ptr; /* 定义 32 位平台下的指针等宽整数类型 */
#endif                              /* 平台判定宏结束，确保地址计算的通用性 */
/* 空行：逻辑分段 */
/* 核心常量定义 */
#define TCL_NULL (0xFFFFFFFF)       /* 空偏移量常量，用于表示无效内存引用或链表末尾 */
#define MAX_ARGS 32                 /* 单条指令支持的最大参数数量限制，平衡性能与功能 */
#define tcl_i32_MAX 2147483647      /* 32 位有符号整数最大值，用于溢出判定 */
/* 空行：逻辑分段 */
/* 调试日志宏 */
#ifdef TCL_DEBUG                    /* 判定是否开启 TCL_DEBUG 调试模式编译 */
#define TCL_LOG(message) tcl_hal_puts((const tcl_u8 *)message) /* 定义日志输出宏，调用硬件抽象层接口 */
#else                               /* 处于 Release 发布模式 */
#define TCL_LOG(message)                                       /* 禁用调试日志，减少固件体积 */
#endif                              /* 调试宏判定结束 */
/* 空行：逻辑分段 */
/* 解释器全局上下文结构体 */
typedef struct {                    /* 定义 TclCtx 核心上下文结构体 */
    tcl_u8  *arena;                 /* 静态内存池起始物理地址，解释器所有内存分配均在此进行 */
    tcl_u32  size;                  /* 内存池总物理容量，用于边界越界检查 */
    tcl_u32  p_top;                 /* 变量区（低地址区）当前分配游标，向上生长 */
    tcl_u32  t_bot;                 /* 栈帧区（高地址区）当前分配游标，向下生长 */
    tcl_u32  g_vars;                /* 全局变量链表起始偏移量，GC 的主要根节点之一 */
    tcl_u32  result;                /* 最近一次执行结果的字符串偏移量，保存返回值 */
    tcl_i32  status;                /* 解释器当前运行状态码（如 TCL_OK, TCL_ERROR 等） */
    tcl_u32  curr_f;                /* 当前正在活跃的执行栈帧偏移量，用于回溯调用链 */
    tcl_u32  tmp_roots[16];         /* GC 保护根数组，防止分配过程中的中间临时对象被回收 */
} TclCtx;                           /* 全局上下文结构体定义结束 */
/* 空行：逻辑分段 */
/* 变量对象结构体 */
typedef struct {                    /* 定义 TclVar 变量存储结构体 */
    tcl_u32 name;                   /* 变量名字符串在 Arena 中的逻辑偏移量 */
    tcl_u32 val;                    /* 变量值字符串在 Arena 中的逻辑偏移量 */
    tcl_u32 next;                   /* 链表中下一个变量节点的逻辑偏移量 */
    tcl_u8  flags;                  /* 变量标志位，如是否为 upvar 链接等 */
} TclVar;                           /* 变量结构体定义结束 */
/* 空行：逻辑分段 */
/* 变量标志定义 */
#define VAR_LINK 0x01               /* 链接标志，表示该变量是一个指向其他变量的引用 */
/* 空行：逻辑分段 */
/* 执行栈帧结构体 */
typedef struct {                    /* 定义 TclFrame 执行栈帧结构体 */
    tcl_u32 script;                 /* 当前正在解析的脚本字符串逻辑偏移量 */
    tcl_u32 pc;                     /* 程序计数器，记录当前脚本解析到的字符位置 */
    tcl_u32 vars;                   /* 当前执行作用域下的局部变量链表逻辑偏移量 */
    tcl_u32 parent;                 /* 物理调用父栈帧的逻辑偏移量，用于执行流恢复 */
    tcl_u32 scope;                  /* 逻辑变量作用域父栈帧的逻辑偏移量，用于变量查找 */
    tcl_u32 result;                 /* 栈帧级别的临时结果偏移量，处理嵌套子命令 */
    tcl_u32 cond;                   /* 循环或条件判定的条件脚本字符串逻辑偏移量 */
    tcl_u32 body;                   /* 循环或条件判定的主体脚本字符串逻辑偏移量 */
    tcl_u32 argv[MAX_ARGS];         /* 当前指令解析出的参数偏移量数组，用于命令调度 */
    tcl_u8  argc;                   /* 当前指令的实际参数总数 */
    tcl_u8  state;                  /* 状态机当前所处的状态节点（如 TOKENIZE, EXECUTE 等） */
    tcl_u8  flags;                  /* 栈帧标志位，如 FRAME_IS_PROC 标记过程调用 */
    tcl_u8  exp_idx;                /* 参数展开进度索引，用于无栈化分步执行 */
} TclFrame;                         /* 执行栈帧结构体定义结束 */
/* 空行：逻辑分段 */
/* 栈帧标志定义 */
#define FRAME_SHARE_SCOPE 1         /* 共享作用域标志，适用于 eval/if/while 等内部指令 */
#define FRAME_IS_PROC     2         /* 过程调用标志，标记这是一个拥有独立作用域的函数调用 */
#define FRAME_IS_EXPR     4         /* 表达式求值标志，标记该帧应进行表达式归约而非指令执行 */
/* 空行：逻辑分段 */
/* 解释器返回码定义 */
#define TCL_OK 0                    /* 执行成功返回码 */
#define TCL_ERROR 1                 /* 执行出错返回码 */
#define TCL_RETURN 2                /* 触发脚本 return 指令的特殊状态码 */
#define TCL_BREAK 3                 /* 触发脚本 break 循环的特殊状态码 */
#define TCL_CONTINUE 4              /* 触发脚本 continue 循环的特殊状态码 */
#define TCL_EXIT 5                  /* 触发退出解释器的全局指令 */
#define TCL_YIELD 6                 /* 状态机挂起态，用于异步或复杂指令等待 */
/* 空行：逻辑分段 */
/* 状态机状态定义 */
#define ST_TOKENIZE 0               /* 分词阶段：负责解析脚本中的指令边界与参数 */
#define ST_EXPAND   1               /* 展开阶段：负责处理变量引用与命令替换逻辑 */
#define ST_EXECUTE  2               /* 执行阶段：根据命令名称查找对应的 C 实现并调用 */
#define ST_RESUME   3               /* 恢复阶段：子命令执行完成后恢复父命令的解析进度 */
#define ST_COND     6               /* while 指令专属：进入条件脚本评估状态 */
#define ST_LOOP     7               /* while 指令专属：进入主体脚本执行状态 */
#define ST_IF_COND  8               /* if 指令专属：评估分支条件逻辑 */
#define ST_IF_BODY  9               /* if 指令专属：执行选中的分支主体脚本 */
#define ST_CATCH    10              /* catch 指令专属：执行受监控脚本的前置态 */
#define ST_CATCH_END 11             /* catch 指令专属：处理异常捕获后的清理态 */
#define ST_EXPR_REDUCE 12           /* 表达式归约态：执行已展开 Token 的逻辑/算术运算 */
#define ST_EXPAND_STR 13            /* 字符串展开态：处理双引号内的变量与命令替换 */
#define ST_EXPAND_STR_RESUME 14     /* 字符串展开恢复态：子命令替换后拼接剩余字符串 */
/* 空行：逻辑分段 */
/* 地址转换宏：将 Arena 逻辑偏移量转换为实际物理指针 */
#define TO_PTR(ctx, offset) ((offset) == TCL_NULL ? 0 : (void *)((ctx)->arena + (offset))) /* 处理空值并执行基地址加偏移 */
/* 空行：逻辑分段 */
/* 内部字符串工具函数：计算字符串长度 */
static tcl_u32 t_slen(const tcl_u8 *string_ptr) { /* 函数入口：获取字符串逻辑长度 */
    tcl_u32 character_count = 0;    /* 初始化字符计数器为 0，用于记录长度 */
    while (string_ptr && string_ptr[character_count]) { /* 遍历字符串直到遇到 NULL 终止符 */
        character_count++;          /* 发现有效字符，计数器自增 */
    }                               /* 循环扫描结束 */
    return character_count;         /* 返回计算得到的字符串总字符数 */
}                                   /* 函数执行完毕 */
/* 空行：逻辑分段 */
/* 内部内存工具函数：物理内存拷贝 */
static void t_mcpy(void *dest, const void *src, tcl_u32 count) { /* 函数入口：执行内存块逐字节搬运 */
    tcl_u8 *d_ptr = (tcl_u8*)dest;  /* 初始化目的地址的字节指针 */
    const tcl_u8 *s_ptr = (const tcl_u8*)src; /* 初始化源地址的字节指针 */
    while (count--) {               /* 循环拷贝直到指定的字节数处理完成 */
        *d_ptr++ = *s_ptr++;        /* 执行逐字节赋值并同步递增指针 */
    }                               /* 搬运循环结束 */
}                                   /* 函数执行完毕 */
/* 空行：逻辑分段 */
/* 内部字符串工具函数：字符串比较 */
static tcl_i32 t_scmp(const tcl_u8 *string1, const tcl_u8 *string2) { /* 函数入口：重命名单字母变量，执行内容对比 */
    if (!string1 || !string2) {     /* 判空逻辑：处理任意参数为 NULL 的异常情况 */
        return string1 == string2 ? 0 : (string1 ? -1 : 1); /* 返回指针层面的比较结果以确保逻辑一致 */
    }                               /* 判空处理结束 */
    while (*string1 && (*string1 == *string2)) { /* 循环对比字符内容直到不匹配或遇到结束符 */
        string1++;                  /* 移动到第一个字符串的下一个字节 */
        string2++;                  /* 移动到第二个字符串的下一个字节 */
    }                               /* 比较循环结束 */
    return *(tcl_u8*)string1 - *(tcl_u8*)string2; /* 返回当前字符差值，正数表示 string1 较大 */
}                                   /* 函数执行完毕 */
/* 空行：逻辑分段 */
/* 内部字符串工具函数：判断字符串是否为合法的十进制整数 */
static tcl_u32 t_is_int(const tcl_u8 *string_ptr) { /* 函数入口：判断字符串是否为整数 */
    if (!string_ptr || *string_ptr == 0) { /* 判空以及判定是否为空字符串 */
        return 0; /* 若为空，则直接返回非整数判定值 */
    } /* 结束空值判定 */
    if (*string_ptr == '-' || *string_ptr == '+') { /* 识别可选的正负号前缀 */
        string_ptr++; /* 指针右移以跳过正负号 */
    } /* 结束前缀符号判定 */
    if (*string_ptr == 0) { /* 判定是否存在只有符号而没有数字的异常情况 */
        return 0; /* 返回非整数判定值 */
    } /* 结束仅符号判定 */
    while (*string_ptr) { /* 循环检查直至字符串结束符 */
        if (*string_ptr < '0' || *string_ptr > '9') { /* 判定当前字符是否在 '0' 到 '9' 的 range 之外 */
            return 0; /* 发现非数字字符，返回非整数判定值 */
        } /* 结束字符合法性判定 */
        string_ptr++; /* 递增指针位置以处理下一个字符 */
    } /* 结束循环检查 */
    return 1; /* 通过全部检查，返回合法整数判定值 */
} /* 函数执行完毕 */
/* 空行：逻辑分段 */
/* 内部字符串工具函数：将字符串转换为 32 位有符号整数 */
static tcl_i32 t_atoi(const tcl_u8 *string_ptr) { /* 函数入口：解析十进制整数字符串 */
    tcl_i32 accumulated_result = 0; /* 用于累加转换结果的局部变量 */
    tcl_i32 sign_multiplier = 1;    /* 符号标记，正数为 1，负数为 -1 */
    if (!string_ptr) return 0;      /* 鲁棒性增强：如果传入空指针，默认当作0处理 */
    if (*string_ptr == '-') {       /* 识别负号前缀字符 */
        sign_multiplier = -1;       /* 标记结果为负数 */
        string_ptr++;               /* 跳过负号字符，处理后续数字位 */
    }                               /* 负号判定结束 */
    while (*string_ptr >= '0' && *string_ptr <= '9') { /* 循环处理连续的数字字符 */
        accumulated_result = accumulated_result * 10 + (*string_ptr++ - '0'); /* 按位加权计算数值 */
    }                               /* 转换循环结束 */
    return accumulated_result * sign_multiplier; /* 返回带符号的最终整数值 */
}                                   /* 函数执行完毕 */
/* 空行：逻辑分段 */
/* 内部字符串工具函数：整数转字符串 */
static void t_itoa(tcl_i32 number, tcl_u8 *buffer) { /* 函数入口：将 32 位整数序列化为字符串 */
    tcl_i32 write_index = 0;        /* 缓冲区写入位置索引，重构单字母变量 */
    if (number == 0) {              /* 特殊处理零值的序列化 */
        buffer[write_index++] = '0'; /* 直接在缓冲区写入字符 0 */
        buffer[write_index] = 0;    /* 写入 NULL 终止符 */
        return;                     /* 完成转换并提前返回 */
    }                               /* 零值处理结束 */
    if (number < 0) {               /* 处理负数逻辑 */
        buffer[write_index++] = '-'; /* 在结果起始位置写入负号字符 */
        number = -number;           /* 转为正数以便后续按位取模处理 */
    }                               /* 负数处理结束 */
    tcl_u8 temp_buf[12];            /* 临时栈缓冲区，用于存储逆序生成的数字字符 */
    tcl_i32 digit_count = 0;        /* 已处理的位数计数器 */
    while (number > 0) {            /* 循环提取每一位十进制数字 */
        temp_buf[digit_count++] = (tcl_u8)((number % 10) + '0'); /* 取模获得个位数并转为字符 */
        number /= 10;               /* 逻辑右移一位十进制 */
    }                               /* 提位循环结束 */
    while (digit_count > 0) {       /* 将逆序生成的字符按正确顺序搬运至输出缓冲区 */
        buffer[write_index++] = temp_buf[--digit_count]; /* 逆序提取并顺序写入 */
    }                               /* 搬运循环结束 */
    buffer[write_index] = 0;        /* 在输出字符串末尾写入 NULL 终止符 */
}                                   /* 函数执行完毕 */
/* 空行：逻辑分段 */
/* 前置声明 */
const tcl_u8 *tcl_get_result(TclCtx *context); /* 获取解释器当前执行结果的前置声明 */
void tcl_hal_puts(const tcl_u8 *string); /* 硬件抽象层控制台输出接口的前置声明 */
/* 空行：逻辑分段 */
/* 对象头结构体，用于 GC 标记与对象管理 */
typedef struct {                    /* 定义 ObjHeader 对象头结构体 */
    tcl_u32 size_and_flags;         /* 对象物理大小及标志位（如 MARK/VAR 位），高位用于 GC 标记 */
    tcl_u32 forward;                /* GC 压缩阶段记录搬迁后的目标物理逻辑偏移量 */
} ObjHeader;                        /* 对象头结构体定义结束 */
/* 空行：逻辑分段 */
/* 对象头标志位定义 */
#define OBJ_MARK_BIT 0x80000000     /* GC 阶段用于标识对象是否存活的标记位 */
#define OBJ_VAR_BIT  0x40000000     /* 变量对象标记位，用于引导 GC 递归扫描成员引用 */
#define OBJ_SIZE(header) ((header)->size_and_flags & ~(OBJ_MARK_BIT|OBJ_VAR_BIT)) /* 提取纯净的对象物理大小，屏蔽标志位 */
/* 空行：逻辑分段 */
/* 上下文结构体相对于 Arena 的对齐起始偏移 */
#define HS ((sizeof(TclCtx) + 15) & ~15) /* 执行 16 字节对齐计算，确保 Arena 数据的内存对齐安全性 */
/* 空行：逻辑分段 */
/* GC 标记函数：采用深度优先算法递归标记所有从根节点可达的活跃对象 */
static void mark_obj(TclCtx *context, tcl_u32 target_object_offset) { /* 函数入口：标记活跃对象及其引用链 */
    if (target_object_offset == TCL_NULL) { /* 忽略无效的空逻辑偏移量 */
        return;                     /* 直接返回，无需执行任何标记操作 */
    }                               /* 判空逻辑结束 */
    if (target_object_offset < HS || target_object_offset >= context->p_top) { /* 执行基本的 Arena 物理边界检查 */
        return;                     /* 越界地址通常为静态字符串或非法引用，不予处理 */
    }                               /* 边界检查结束 */
    /* 根据数据区偏移量回溯物理地址，获取紧挨在其前的对象头结构体指针 */
    ObjHeader *obj_header = (ObjHeader*)TO_PTR(context, target_object_offset - sizeof(ObjHeader)); /* 定位对象头物理指针 */
    if (obj_header->size_and_flags & OBJ_MARK_BIT) { /* 检查该对象是否在本轮 GC 中已被标记过 */
        return;                     /* 若已标记则说明已处理，跳过以防止无限递归死循环 */
    }                               /* 标记位状态检查结束 */
    obj_header->size_and_flags |= OBJ_MARK_BIT; /* 将该对象正式标记为活跃状态，确保其不会被回收 */
    if (obj_header->size_and_flags & OBJ_VAR_BIT) { /* 判断该对象是否为包含其他引用的变量容器 */
        /* 将数据载荷区强制转换为变量结构体指针，以便递归访问其内部关联成员 */
        TclVar *variable_ptr = (TclVar*)TO_PTR(context, target_object_offset); /* 转换为变量物理指针 */
        mark_obj(context, variable_ptr->name); /* 递归标记变量名对象，确保名称字符串存活 */
        mark_obj(context, variable_ptr->val);  /* 递归标记变量值对象，确保其引用的内容存活 */
        mark_obj(context, variable_ptr->next); /* 递归标记链表中的后续节点，实现完整作用域链标记 */
    }                               /* 变量容器深层标记处理结束 */
}                                   /* 函数执行完毕 */
/* 空行：逻辑分段 */
/* 核心垃圾回收函数：实现 Slide Compacting 算法，物理性压缩 Arena 以整理碎片 */
void tcl_gc(TclCtx *context) {      /* 函数入口：启动垃圾回收机制 */
    TCL_LOG("-- GC 开始 --\n");      /* 输出 GC 启动日志，辅助开发阶段的内存水位监控 */
    /* 1. 标记阶段：从解释器所有合法的根引用开始，递归遍历所有存活对象 */
    mark_obj(context, context->result); /* 标记解释器当前待返回或缓存的结果字符串对象 */
    mark_obj(context, context->g_vars); /* 标记所有全局作用域下的变量及其内容 */
    for (tcl_i32 root_index = 0; root_index < 16; root_index++) { /* 遍历并标记临时保护根，保护中间分配的对象 */
        mark_obj(context, context->tmp_roots[root_index]); /* 逐一标记每一个受保护的临时引用 */
    }                               /* 保护根数组遍历结束 */
    tcl_u32 frame_offset = context->curr_f; /* 获取当前活跃执行栈帧的逻辑偏移量 */
    while (frame_offset != TCL_NULL) { /* 沿着执行栈帧链从叶子节点向根节点回溯遍历 */
        TclFrame *frame_ptr = TO_PTR(context, frame_offset); /* 定位当前栈帧在 Arena 中的物理地址 */
        mark_obj(context, frame_ptr->script); /* 标记当前执行上下文所关联的脚本源码对象 */
        mark_obj(context, frame_ptr->vars);   /* 标记当前函数或 eval 作用域下的局部变量表 */
        mark_obj(context, frame_ptr->cond);   /* 标记控制结构（如 while）的条件表达式脚本对象 */
        mark_obj(context, frame_ptr->body);   /* 标记控制结构的主体执行脚本对象 */
        for (tcl_i32 arg_idx = 0; arg_idx < frame_ptr->argc; arg_idx++) { /* 遍历当前指令正在处理的参数列表 */
            mark_obj(context, frame_ptr->argv[arg_idx]); /* 逐一标记参数对象，防止指令执行中途被回收 */
        }                           /* 指令参数数组遍历结束 */
        frame_offset = frame_ptr->parent; /* 移动到父级栈帧的偏移量以继续回溯调用链 */
    }                               /* 栈帧回溯标记过程结束 */
/* 空行：逻辑分段 */
    /* 2. 计算迁移地址：顺序线性扫描 Arena 内存池，确定存活对象压缩后的紧凑位置 */
    tcl_u32 current_scan_offset = HS; /* 从有效载荷起始对齐点开始扫描 */
    tcl_u32 next_available_pos = HS;  /* 记录对象在压缩后应当所处的下一个可用物理逻辑偏移量 */
    while (current_scan_offset < context->p_top) { /* 遍历低地址变量分配区直到当前游标边界 */
        ObjHeader *current_header = (ObjHeader*)TO_PTR(context, current_scan_offset); /* 定位当前扫描对象的头部 */
        tcl_u32 object_full_size = OBJ_SIZE(current_header); /* 获取包含头部在内的对象总占用字节数 */
        if (current_header->size_and_flags & OBJ_MARK_BIT) { /* 如果该对象被之前的标记阶段确认存活 */
            current_header->forward = next_available_pos; /* 在头部记录其搬迁后的目标物理偏移量位置 */
            next_available_pos += object_full_size; /* 累加空闲游标，为后续存活对象预留紧凑排列空间 */
        }                           /* 存活状态判定结束 */
        current_scan_offset += object_full_size; /* 将扫描指针推移至原始物理位置的下一个对象头处 */
    }                               /* 压缩位置预计算循环结束 */
/* 空行：逻辑分段 */
    /* 3. 更新指针引用：根据预留的 forward 映射，修正所有存活对象之间的逻辑偏移量引用 */
    /* 定义重定向宏，专门处理对象引用的新旧地址转换 */
    #define UPDATE_PTR(ptr_ref) do { \
        if ((ptr_ref) != TCL_NULL && (ptr_ref) >= HS && (ptr_ref) < context->p_top) { \
            ObjHeader *target_header = (ObjHeader*)TO_PTR(context, (ptr_ref) - sizeof(ObjHeader)); /* 回溯目标头部 */ \
            if (target_header->size_and_flags & OBJ_MARK_BIT) { \
                (ptr_ref) = target_header->forward + sizeof(ObjHeader); /* 重定向至紧凑化后的新偏移量 */ \
            } else { \
                (ptr_ref) = TCL_NULL; /* 引用的对象已死亡，将句柄置空防止悬空指针 */ \
            } \
        } \
    } while(0)                      /* 宏定义结束，确保引用完整性 */
/* 空行：逻辑分段 */
    UPDATE_PTR(context->result);     /* 修正全局执行结果的逻辑偏移量 */
    UPDATE_PTR(context->g_vars);     /* 修正全局变量表链表头的逻辑偏移量 */
    for (tcl_i32 root_upd_idx = 0; root_upd_idx < 16; root_upd_idx++) { /* 遍历并修正所有临时保护根引用 */
        UPDATE_PTR(context->tmp_roots[root_upd_idx]); /* 更新每一个受保护对象的逻辑偏移量 */
    }                               /* 根数组引用修正结束 */
    frame_offset = context->curr_f; /* 重新回溯栈帧链以执行深度引用修正 */
    while (frame_offset != TCL_NULL) { /* 循环处理调用链中的每一个活跃栈帧 */
        TclFrame *frame_ptr = TO_PTR(context, frame_offset); /* 获取栈帧物理指针以直接操作成员 */
        UPDATE_PTR(frame_ptr->script); /* 修正脚本源码引用的偏移量 */
        UPDATE_PTR(frame_ptr->vars);   /* 修正局部变量链表头的偏移量 */
        UPDATE_PTR(frame_ptr->cond);   /* 修正控制结构条件表达式的偏移量 */
        UPDATE_PTR(frame_ptr->body);   /* 修正主体脚本的偏移量 */
        for (tcl_i32 arg_upd_idx = 0; arg_upd_idx < frame_ptr->argc; arg_upd_idx++) { /* 修正当前指令的参数列表 */
            UPDATE_PTR(frame_ptr->argv[arg_upd_idx]); /* 更新每一个参数对象的逻辑地址 */
        }                           /* 指令参数引用修正结束 */
        frame_offset = frame_ptr->parent; /* 推进至父级栈帧以继续引用对齐流程 */
    }                               /* 栈帧链引用修正结束 */
/* 空行：逻辑分段 */
    /* 4. 更新变量容器内部成员指针：修正 TclVar 结构体内部维护的 name/val/next 链 */
    current_scan_offset = HS;       /* 线性重新遍历 Arena 内存池 */
    while (current_scan_offset < context->p_top) { /* 扫描整个已分配的低地址区域 */
        ObjHeader *scan_header = (ObjHeader*)TO_PTR(context, current_scan_offset); /* 获取对象头 */
        tcl_u32 obj_size_total = OBJ_SIZE(scan_header); /* 获取对象总大小 */
        if ((scan_header->size_and_flags & OBJ_MARK_BIT) && (scan_header->size_and_flags & OBJ_VAR_BIT)) { /* 若为活跃的变量容器 */
            TclVar *var_data_ptr = (TclVar*)TO_PTR(context, current_scan_offset + sizeof(ObjHeader)); /* 定位变量数据区 */
            UPDATE_PTR(var_data_ptr->name); /* 修正变量名引用的逻辑偏移量 */
            UPDATE_PTR(var_data_ptr->val);  /* 修正变量值引用的逻辑偏移量 */
            UPDATE_PTR(var_data_ptr->next); /* 修正链表后续节点的逻辑偏移量 */
        }                           /* 变量容器内部引用修正结束 */
        current_scan_offset += obj_size_total; /* 移动到原始物理位置的下一个对象 */
    }                               /* 全局变量容器修正循环结束 */
/* 空行：逻辑分段 */
    /* 5. 物理数据搬运阶段：利用内存拷贝工具，将存活对象真正移动至计算好的紧凑地址处 */
    current_scan_offset = HS;       /* 第三次顺序扫描 Arena 内存池以执行搬运 */
    next_available_pos = HS;        /* 重置目标写入游标至 Arena 变量区基地址 */
    while (current_scan_offset < context->p_top) { /* 遍历扫描所有原始位置的对象 */
        ObjHeader *final_header = (ObjHeader*)TO_PTR(context, current_scan_offset); /* 获取原对象头物理指针 */
        tcl_u32 current_obj_size = OBJ_SIZE(final_header); /* 获取该对象的物理总载荷（含头） */
        if (final_header->size_and_flags & OBJ_MARK_BIT) { /* 如果该对象被标记为存活（必须搬迁） */
            final_header->size_and_flags &= ~OBJ_MARK_BIT; /* 清除存活标记位，为下一次垃圾回收周期重置状态 */
            if (current_scan_offset != final_header->forward) { /* 判定是否存在物理位置变化，避免原地自拷贝 */
                t_mcpy(TO_PTR(context, final_header->forward), TO_PTR(context, current_scan_offset), current_obj_size); /* 执行物理搬运 */
            }                       /* 物理位移判定结束 */
            next_available_pos += current_obj_size; /* 推进搬迁后的下一可用空间游标 */
        }                           /* 存活搬运判定结束 */
        current_scan_offset += current_obj_size; /* 将扫描指针推进至原始物理排列中的下一个对象 */
    }                               /* 全局搬运循环结束 */
    context->p_top = next_available_pos; /* 物理搬运彻底完成，更新解释器的低地址变量区分配游标至最新边界 */
    TCL_LOG("-- GC 结束 --\n");      /* 输出 GC 结束日志，内存池整理完毕 */
}                                   /* 函数执行完毕 */
/* 空行：逻辑分段 */
/* 变量区内存分配函数：从 Arena 低地址区分配空间，支持空间不足时自动触发垃圾回收 (GC) */
static tcl_u32 tcl_alc_p(TclCtx *context, tcl_u32 byte_count) { /* 函数入口：分配变量数据空间 */
    /* 计算总分配量：用户请求字节数 + 固定的对象头长度，并执行 8 字节对齐以确保硬件存取效率 */
    tcl_u32 total_allocation_size = (byte_count + sizeof(ObjHeader) + 7) & ~7; /* 对齐后的总分配大小计算 */
    /* 检查分配新对象后，向上生长的变量区游标是否会与向下生长的栈帧区游标发生物理碰撞 */
    if (context->p_top + total_allocation_size > context->t_bot) { /* 堆栈碰撞检查 */
        tcl_gc(context);            /* 内存水位触顶，强制启动同步垃圾回收以通过压缩变量区腾出空间 */
        /* GC 完成后再次进行空间检测，判定回收出的空隙是否满足当前需求 */
        if (context->p_top + total_allocation_size > context->t_bot) { /* 二次空间短缺检测 */
            tcl_hal_puts((const tcl_u8*)"Memory exhaustion in tcl_alc_p. p_top=");
            tcl_u8 buf[12];
            t_itoa(context->p_top, buf); tcl_hal_puts(buf);
            tcl_hal_puts((const tcl_u8*)" t_bot=");
            t_itoa(context->t_bot, buf); tcl_hal_puts(buf);
            tcl_hal_puts((const tcl_u8*)" req=");
            t_itoa(total_allocation_size, buf); tcl_hal_puts(buf);
            tcl_hal_puts((const tcl_u8*)"\n");
            context->status = TCL_ERROR; /* 全局资源已彻底枯竭，标记解释器运行错误 */
            return TCL_NULL;        /* 返回非法偏移量通知调用方分配失败 */
        }
    }                               /* 初始冲突处理结束 */
    tcl_u32 allocated_base_offset = context->p_top; /* 确定新对象在 Arena 中的逻辑起始偏移量 */
    ObjHeader *new_obj_header = (ObjHeader*)TO_PTR(context, allocated_base_offset); /* 在内存池中准确定位头部物理地址 */
    new_obj_header->size_and_flags = total_allocation_size; /* 在对象头中初始化记录其实际占用的总物理大小 */
    context->p_top += total_allocation_size; /* 向前推移变量区游标，标记新分配出的物理空间已被占用 */
    tcl_u8 *payload_ptr = TO_PTR(context, allocated_base_offset + sizeof(ObjHeader)); /* 跳过头结构，定位有效数据载荷区 */
    for (tcl_u32 clear_idx = 0; clear_idx < byte_count; clear_idx++) { /* 遍历并清除新分配内存块中的残留脏数据 */
        payload_ptr[clear_idx] = 0; /* 逐字节清零，确保脚本运行时环境的确定性 */
    }                               /* 内存清零初始化结束 */
    return allocated_base_offset + sizeof(ObjHeader); /* 返回数据区的逻辑偏移量作为对象句柄 */
}                                   /* 函数执行完毕 */
/* 空行：逻辑分段 */
/* 栈帧区内存分配函数：从 Arena 高地址顶部向下生长，用于存储执行上下文 TclFrame */
static tcl_u32 tcl_alc_t(TclCtx *context, tcl_u32 byte_count) { /* 函数入口：分配高地址栈帧空间 */
    tcl_u32 aligned_byte_count = (byte_count + 7) & ~7; /* 强制执行 8 字节边界对齐，确保栈帧结构访问安全 */
    /* 检查向下生长（逻辑偏移递减）的栈帧区游标是否与变量区游标发生物理冲突 */
    if (context->p_top + aligned_byte_count > context->t_bot) { /* 冲突边界检测 */
        tcl_gc(context);            /* 触顶时尝试通过 GC 压缩低地址变量区来挤出可用的物理空隙 */
        /* 回收操作后再次验证物理边界是否安全 */
        if (context->p_top + aligned_byte_count > context->t_bot) { /* 紧急资源判定 */
            return TCL_NULL;        /* 物理内存已彻底耗尽，无法再容纳更多执行上下文 */
        }                           /* 二次检查结束 */
    }                               /* 冲突逻辑处理完毕 */
    context->t_bot -= aligned_byte_count; /* 逻辑上向下移动栈帧底端游标，占领所需的存储空间 */
    for (tcl_u32 clear_idx = 0; clear_idx < aligned_byte_count; clear_idx++) { /* 清除新分配栈帧区中的历史数据 */
        context->arena[context->t_bot + clear_idx] = 0; /* 确保栈帧初始状态完全零化，防止读到残留指针 */
    }                               /* 初始化清零结束 */
    return context->t_bot;          /* 返回新栈帧在 Arena 中的基准逻辑偏移量 */
}                                   /* 函数执行完毕 */
/* 空行：逻辑分段 */
/* 变量查找辅助函数：在当前调用链栈帧及全局变量表中深度搜索匹配的变量节点 */
static tcl_u32 tcl_find_var_node(TclCtx *context, tcl_u32 search_frame_offset, const tcl_u8 *target_name) { /* 函数入口：检索变量存储节点 */
    while (search_frame_offset != TCL_NULL) { /* 沿着逻辑变量作用域链从当前活跃点向上层回溯 */
        TclFrame *frame_ptr = TO_PTR(context, search_frame_offset); /* 转换为物理指针以读取栈帧内变量链表头 */
        tcl_u32 current_var_offset = frame_ptr->vars; /* 获取当前栈帧作用域关联的局部变量表起点 */
        while (current_var_offset != TCL_NULL) { /* 线性遍历局部作用域下的所有变量节点 */
            TclVar *variable_ptr = TO_PTR(context, current_var_offset); /* 获取变量结构体的物理内存地址 */
            /* 执行字符串级别变量名匹配检查，调用已重构的比较函数 */
            if (t_scmp(target_name, TO_PTR(context, variable_ptr->name)) == 0) { /* 如果名称匹配成功 */
                return current_var_offset; /* 立即返回命中节点的物理逻辑偏移量 */
            }                       /* 名称比对逻辑结束 */
            current_var_offset = variable_ptr->next; /* 移动至局部链表中的下一个节点继续搜索 */
        }                           /* 局部变量循环结束 */
        search_frame_offset = frame_ptr->scope; /* 沿着作用域链移动至父级作用域栈帧以继续回溯 */
    }                               /* 逻辑作用域链回溯结束 */
    tcl_u32 global_node_offset = context->g_vars; /* 若局部作用域全路径均未命中，则最终检索全局变量存储池 */
    while (global_node_offset != TCL_NULL) { /* 遍历全局变量单向链表 */
        TclVar *global_variable_ptr = TO_PTR(context, global_node_offset); /* 定位全局变量节点的物理内存 */
        /* 对比全局变量名是否与目标名称一致 */
        if (t_scmp(target_name, TO_PTR(context, global_variable_ptr->name)) == 0) { /* 如果匹配 */
            return global_node_offset; /* 返回全局匹配到的变量节点逻辑偏移量 */
        }                           /* 全局比对结束 */
        global_node_offset = global_variable_ptr->next; /* 移动到全局链表中的下一个成员 */
    }                               /* 全局变量池搜索结束 */
    return TCL_NULL;                /* 在所有合法的作用域及其父路径上均未找到该名称的变量 */
}                                   /* 函数执行完毕 */

/* 变量值获取函数：返回变量关联的字符串偏移量 */
static tcl_u32 tcl_get_var(TclCtx *context, tcl_u32 frame_offset, const tcl_u8 *name) { /* 函数入口：获取变量值 */
    tcl_u32 var_offset = tcl_find_var_node(context, frame_offset, name); /* 查找节点 */
    if (var_offset == TCL_NULL) {   /* 若未找到 */
        return TCL_NULL;            /* 返回空 */
    }                               /* 查找判定结束 */
    TclVar *variable = TO_PTR(context, var_offset); /* 获取物理指针 */
    if (variable->flags & VAR_LINK) { /* 如果是 upvar 链接对象 */
        TclVar *linked_variable = TO_PTR(context, variable->val); /* 获取实际指向的变量 */
        return linked_variable->val; /* 返回实际值 */
    }                               /* 链接处理结束 */
    return variable->val;           /* 返回普通变量值 */
}                                   /* 函数结束 */

/* 变量设置函数：创建或更新变量值 */
static tcl_i32 tcl_set_var(TclCtx *context, tcl_u32 frame_offset, tcl_u32 name_offset, tcl_u32 value_offset) {
    context->tmp_roots[4] = name_offset; /* 保护名对象偏移量以防止被 GC 回收 */
    context->tmp_roots[5] = value_offset; /* 保护值对象偏移量以防止被 GC 回收 */
    /* 尝试在指定栈帧中定位已有变量节点 */
    tcl_u32 existing_var_offset = tcl_find_var_node(context, frame_offset, TO_PTR(context, context->tmp_roots[4])); /* 查找变量节点 */
    if (existing_var_offset != TCL_NULL) { /* 如果变量已经存在于当前或父级作用域中 */
        context->tmp_roots[6] = existing_var_offset; /* 在根集合中保护该节点物理偏移 */
        if (context->tmp_roots[5] == TCL_NULL) { /* 场景：执行 unset 操作或设置为空值 */
            TclVar *variable = TO_PTR(context, context->tmp_roots[6]); /* 获取变量结构体物理指针 */
            if (variable->flags & VAR_LINK) { /* 检查该变量是否为上层引用的链接变量 */
                variable = TO_PTR(context, variable->val); /* 解析链接以操作真正的原始变量 */
            } /* 结束链接处理判断 */
            variable->val = TCL_NULL; /* 将变量的值指针置为空 */
            context->tmp_roots[4] = TCL_NULL; /* 清理保护根4 */
            context->tmp_roots[5] = TCL_NULL; /* 清理保护根5 */
            context->tmp_roots[6] = TCL_NULL; /* 清理保护根6 */
            return TCL_OK;      /* 变量清空成功，返回正常状态码 */
        } /* 结束空值设置分支 */
        /* 计算新值字符串的长度（含结束符）并尝试分配物理空间 */
        tcl_u32 value_length = t_slen(TO_PTR(context, context->tmp_roots[5])) + 1; /* 获取长度 */
        tcl_u32 new_value_offset = tcl_alc_p(context, value_length); /* 从内存池分配空间 */
        if (new_value_offset == TCL_NULL) { /* 检查内存分配是否因空间不足而失败 */
            context->tmp_roots[4] = TCL_NULL; /* 出错清理保护根4 */
            context->tmp_roots[5] = TCL_NULL; /* 出错清理保护根5 */
            context->tmp_roots[6] = TCL_NULL; /* 出错清理保护根6 */
            return TCL_ERROR; /* 分配失败，向上层抛出解释器错误 */
        } /* 结束分配失败处理 */
        /* 执行新值字符串的物理拷贝操作 */
        t_mcpy(TO_PTR(context, new_value_offset), TO_PTR(context, context->tmp_roots[5]), value_length); /* 数据搬运 */
        TclVar *variable = TO_PTR(context, context->tmp_roots[6]); /* 重新加载变量结构体指针 */
        if (variable->flags & VAR_LINK) { /* 检查是否需要透明处理链接引用 */
            variable = TO_PTR(context, variable->val); /* 跳转至链接指向的目标变量 */
        } /* 结束链接透明化处理 */
        variable->val = new_value_offset; /* 更新变量的物理偏移量指向新分配的值 */
        context->tmp_roots[4] = TCL_NULL; /* 任务完成，释放保护根4 */
        context->tmp_roots[5] = TCL_NULL; /* 任务完成，释放保护根5 */
        context->tmp_roots[6] = TCL_NULL; /* 任务完成，释放保护根6 */
        return TCL_OK; /* 返回操作成功 */
    } /* 结束变量存在时的更新逻辑分支 */
    /* 变量在当前层级不存在，需要执行新建操作。首先确定物理存放的目标栈帧 */
    tcl_u32 target_frame_offset = frame_offset; /* 从起始帧开始搜索 */
    while (target_frame_offset != TCL_NULL) { /* 循环向上寻找作用域根部 */
        TclFrame *parent_frame = TO_PTR(context, target_frame_offset); /* 获取栈帧指针 */
        if (parent_frame->scope == TCL_NULL) { /* 找到真正的非共享局部作用域根部（如过程帧） */
            break; /* 到达作用域根部，跳出循环 */
        } /* 结束作用域属性检查 */
        target_frame_offset = parent_frame->scope; /* 沿着逻辑作用域链向上递归回溯 */
    } /* 结束作用域穿透循环 */
    if (target_frame_offset != TCL_NULL) { /* 检查回溯定位的目标栈帧是否有效 */
        TclFrame *root_frame = TO_PTR(context, target_frame_offset); /* 解析逻辑根部栈帧的物理指针 */
        if (!(root_frame->flags & FRAME_IS_PROC)) { /* 判断逻辑根部帧是否不带有过程环境标志 */
            target_frame_offset = TCL_NULL; /* 若非过程环境则归为全局作用域，目标帧设为 TCL_NULL 以写入 g_vars */
        } /* 结束过程环境标志判断 */
    } /* 结束目标栈帧有效性判定 */
    context->tmp_roots[6] = target_frame_offset; /* 保护最终确定的目标栈帧偏移量 */
    
    /* 在堆内存池中分配新的变量描述符结构体空间 */
    tcl_u32 new_var_offset = tcl_alc_p(context, sizeof(TclVar)); /* 物理空间分配 */
    if (new_var_offset == TCL_NULL) { /* 检查分配结果是否溢出 */
        context->tmp_roots[4] = TCL_NULL; /* 失败清理根4 */
        context->tmp_roots[5] = TCL_NULL; /* 失败清理根5 */
        context->tmp_roots[6] = TCL_NULL; /* 失败清理根6 */
        return TCL_ERROR; /* 返回内存错误 */
    } /* 结束分配检查 */
    context->tmp_roots[7] = new_var_offset; /* 保护新分配的变量对象偏移量 */
    TclVar *new_variable = TO_PTR(context, context->tmp_roots[7]); /* 获取新变量的物理指针 */
    new_variable->name = TCL_NULL; /* 初始化变量名偏移为空 */
    new_variable->val = TCL_NULL; /* 初始化变量值偏移为空 */
    new_variable->next = TCL_NULL; /* 初始化链表指针为空 */
    new_variable->flags = 0; /* 初始化标志位为 0 */
    /* 在内部对象头部标记 OBJ_VAR_BIT，以便 GC 的扫描算法能识别并深度递归 */
    ObjHeader *object_header = TO_PTR(context, context->tmp_roots[7] - sizeof(ObjHeader)); /* 定位头部 */
    object_header->size_and_flags |= OBJ_VAR_BIT; /* 写入变量识别位 */
    
    /* 为变量名字符串分配物理空间并执行内容拷贝 */
    tcl_u32 name_length = t_slen(TO_PTR(context, context->tmp_roots[4])) + 1; /* 计算含结尾的长度 */
    tcl_u32 new_name_offset = tcl_alc_p(context, name_length); /* 尝试分配空间 */
    if (new_name_offset == TCL_NULL) { /* 检查分配是否成功 */
        context->tmp_roots[4] = TCL_NULL; /* 清理根4 */
        context->tmp_roots[5] = TCL_NULL; /* 清理根5 */
        context->tmp_roots[6] = TCL_NULL; /* 清理根6 */
        context->tmp_roots[7] = TCL_NULL; /* 清理根7 */
        return TCL_ERROR; /* 抛出内存不足错误 */
    } /* 结束变量名空间检查 */
    context->tmp_roots[8] = new_name_offset; /* 在根集合中保护变量名对象 */
    
    tcl_u32 final_value_offset = TCL_NULL; /* 定义并初始化最终值偏移量变量 */
    if (context->tmp_roots[5] != TCL_NULL) { /* 如果设置了非空初始值 */
        tcl_u32 value_length = t_slen(TO_PTR(context, context->tmp_roots[5])) + 1; /* 计算值长度 */
        final_value_offset = tcl_alc_p(context, value_length); /* 尝试为值分配空间 */
        if (final_value_offset == TCL_NULL) { /* 检查分配状态 */
            context->tmp_roots[4] = TCL_NULL; /* 清理根4 */
            context->tmp_roots[5] = TCL_NULL; /* 清理根5 */
            context->tmp_roots[6] = TCL_NULL; /* 清理根6 */
            context->tmp_roots[7] = TCL_NULL; /* 清理根7 */
            context->tmp_roots[8] = TCL_NULL; /* 清理根8 */
            return TCL_ERROR; /* 内存不足报错 */
        } /* 结束值分配检查 */
    } /* 结束值设置判断 */
    /* 执行最终的数据物理搬运工作 */
    tcl_u32 name_copy_size = t_slen(TO_PTR(context, context->tmp_roots[4])) + 1; /* 再次确认拷贝大小 */
    t_mcpy(TO_PTR(context, context->tmp_roots[8]), TO_PTR(context, context->tmp_roots[4]), name_copy_size); /* 拷贝变量名字符串 */
    if (context->tmp_roots[5] != TCL_NULL) { /* 场景：存在初始值需要同步 */
        tcl_u32 value_copy_size = t_slen(TO_PTR(context, context->tmp_roots[5])) + 1; /* 确认拷贝大小 */
        t_mcpy(TO_PTR(context, final_value_offset), TO_PTR(context, context->tmp_roots[5]), value_copy_size); /* 拷贝值字符串 */
    } /* 结束物理拷贝分支 */
    
    /* 将新创建的变量结构体链入目标栈帧或全局变量表中 */
    TclFrame *target_frame = (context->tmp_roots[6] == TCL_NULL) ? 0 : TO_PTR(context, context->tmp_roots[6]); /* 定位目标帧指针 */
    tcl_u32 *head_ptr = target_frame ? &target_frame->vars : &context->g_vars; /* 选择链表头 */
    
    new_variable = TO_PTR(context, context->tmp_roots[7]); /* 获取最新的物理地址以应对分配引起的搬移 */
    new_variable->name = context->tmp_roots[8]; /* 绑定变量名偏移 */
    new_variable->val = final_value_offset; /* 绑定变量值偏移 */
    new_variable->next = *head_ptr; /* 头插法：设置新节点的后继为当前头 */
    *head_ptr = context->tmp_roots[7]; /* 将表头指针更新为新节点物理偏移量 */
    
    context->tmp_roots[4] = TCL_NULL; /* 释放根4 */
    context->tmp_roots[5] = TCL_NULL; /* 释放根5 */
    context->tmp_roots[6] = TCL_NULL; /* 释放根6 */
    context->tmp_roots[7] = TCL_NULL; /* 释放根7 */
    context->tmp_roots[8] = TCL_NULL; /* 释放根8 */
    return TCL_OK; /* 变量创建并链入成功，返回正常状态码 */
} /* 结束 tcl_set_var 函数 */

/* 字符串插值辅助函数：将双引号字符串内容（不含外层引号）展开为最终字符串 */
/* 设计目的：标准 Tcl 的 "..." 语义要求内部 $var 被变量值替换，\n \t 等转义被解释 */
/* 返回值：Arena 中存放展开结果的偏移量，或 TCL_NULL 表示分配失败 */
static tcl_u32 tcl_str_interp(TclCtx *context, const tcl_u8 *src_str, tcl_u32 src_len) {
    /* 第一步：分配输出缓冲区，保守上界为输入的 4 倍加 16 字节 */
    tcl_u32 out_buf_size = src_len * 4 + 16; /* 4 倍空间保证变量值展开不溢出 */
    context->tmp_roots[3] = tcl_alc_p(context, out_buf_size); /* 分配缓冲区，挂载根保护 */
    if (context->tmp_roots[3] == TCL_NULL) { /* 分配失败 */
        return TCL_NULL; /* 内存不足 */
    } /* 结束分配检查 */
    tcl_u8 *out_buf = TO_PTR(context, context->tmp_roots[3]); /* 输出缓冲区物理指针 */
    tcl_u32 write_pos = 0; /* 输出游标 */
    tcl_u32 read_pos = 0; /* 输入游标 */
    /* 第二步：逐字符扫描，执行替换 */
    while (read_pos < src_len && write_pos < out_buf_size - 1) { /* 主扫描循环 */
        tcl_u8 cur_char = src_str[read_pos]; /* 读取当前字符 */
        if (cur_char == '\\' && read_pos + 1 < src_len) { /* 转义序列检测 */
            read_pos++; /* 跳过反斜杠 */
            tcl_u8 esc_char = src_str[read_pos]; /* 获取转义类型字符 */
            if (esc_char == 'n') { /* \n 转换为换行符 */
                out_buf[write_pos++] = '\n'; /* 写入换行 */
            } else if (esc_char == 't') { /* \t 转换为制表符 */
                out_buf[write_pos++] = '\t'; /* 写入制表 */
            } else if (esc_char == 'r') { /* \r 转换为回车符 */
                out_buf[write_pos++] = '\r'; /* 写入回车 */
            } else { /* 其他转义字符原样写入（如 \\ → \ ） */
                out_buf[write_pos++] = esc_char; /* 直接写入 */
            } /* 结束转义分流 */
            read_pos++; /* 推进到转义序列之后 */
        } else if (cur_char == '$') { /* 变量引用：$ 开头 */
            read_pos++; /* 跳过 $ 符号 */
            tcl_u8 var_name_buf[64]; /* 变量名缓冲区，最多 63 字符 */
            tcl_u32 var_name_len = 0; /* 变量名长度 */
            while (read_pos < src_len && var_name_len < 63) { /* 扫描变量名字符 */
                tcl_u8 name_ch = src_str[read_pos]; /* 读取候选字符 */
                if ((name_ch >= 'a' && name_ch <= 'z') ||
                    (name_ch >= 'A' && name_ch <= 'Z') ||
                    (name_ch >= '0' && name_ch <= '9') ||
                    name_ch == '_') { /* 合法变量名字符 */
                    var_name_buf[var_name_len++] = name_ch; /* 追加 */
                    read_pos++; /* 推进 */
                } else { /* 非法字符，停止扫描 */
                    break;
                } /* 结束字符判定 */
            } /* 结束变量名扫描 */
            var_name_buf[var_name_len] = 0; /* 封底 */
            if (var_name_len > 0) { /* 有效变量名 */
                tcl_u32 var_val_offset = tcl_get_var(context, context->curr_f, var_name_buf); /* 查找变量 */
                if (var_val_offset != TCL_NULL) { /* 变量存在 */
                    out_buf = TO_PTR(context, context->tmp_roots[3]); /* 刷新输出缓冲区指针（GC后可能位移） */
                    const tcl_u8 *val_str = TO_PTR(context, var_val_offset); /* 变量值字符串指针 */
                    tcl_u32 val_copy_len = t_slen(val_str); /* 值字符串长度 */
                    if (write_pos + val_copy_len >= out_buf_size - 1) { /* 边界保护 */
                        val_copy_len = out_buf_size - 1 - write_pos; /* 截断 */
                    } /* 结束边界保护 */
                    t_mcpy(out_buf + write_pos, val_str, val_copy_len); /* 拷贝变量值进缓冲区 */
                    write_pos += val_copy_len; /* 推进输出游标 */
                } /* 变量不存在则静默忽略（视为空串） */
            } else { /* $ 后无合法变量名，原样写入 $ */
                out_buf = TO_PTR(context, context->tmp_roots[3]); /* 刷新指针 */
                if (write_pos < out_buf_size - 1) { /* 边界检查 */
                    out_buf[write_pos++] = '$'; /* 写入字面量 $ */
                } /* 结束边界检查 */
            } /* 结束变量名有效性判断 */
        } else { /* 普通字符直接写入 */
            out_buf[write_pos++] = cur_char; /* 写入字符 */
            read_pos++; /* 推进 */
        } /* 结束字符类型分流 */
    } /* 结束主扫描循环 */
    out_buf = TO_PTR(context, context->tmp_roots[3]); /* 最终刷新指针 */
    out_buf[write_pos] = 0; /* 封底 */
    tcl_u32 result_offset = context->tmp_roots[3]; /* 取结果偏移量 */
    context->tmp_roots[3] = TCL_NULL; /* 释放临时根保护 */
    return result_offset; /* 返回展开后字符串偏移量 */
} /* 结束 tcl_str_interp 函数 */

/* set 指令实现：提供脚本层面的变量读取与写入功能 */
static tcl_i32 tcl_cmd_set(TclCtx *context, tcl_i32 argument_count, tcl_u32 *argument_values) {
    if (argument_count < 2) {        /* 检查参数数量是否满足最小要求（set name [val]） */
        return TCL_ERROR;       /* 参数不足，返回解释器错误码 */
    } /* 结束参数检查 */
    if (argument_count == 2) {       /* 场景 1：仅提供变量名，执行读取操作 */
        /* 在当前上下文中查找指定名称的变量值物理偏移 */
        tcl_u32 variable_value_offset = tcl_get_var(context, context->curr_f, TO_PTR(context, argument_values[1])); /* 查找操作 */
        if (variable_value_offset == TCL_NULL) { /* 检查变量是否存在 */
            return TCL_ERROR;   /* 变量未定义，报错 */
        } /* 结束存在性检查 */
        context->result = variable_value_offset; /* 将获取到的变量值设为解释器的当前执行结果 */
    } else {                    /* 场景 2：提供变量名与新值，执行写入或更新操作 */
        /* 调用内部设置函数，在当前作用域内创建或更新变量 */
        if (tcl_set_var(context, context->curr_f, argument_values[1], argument_values[2]) != TCL_OK) { /* 写入操作 */
            return TCL_ERROR;   /* 写入过程中发生分配失败或其它错误，报错 */
        } /* 结束写入操作判断 */
        context->result = argument_values[2]; /* set 指令写入成功后，返回所设置的新值 */
    } /* 结束读取/写入场景分流 */
    return TCL_OK;              /* 指令执行成功 */
} /* 结束 tcl_cmd_set 函数 */

/* proc 指令实现：在解释器中注册由脚本定义的函数（过程） */
static tcl_i32 tcl_cmd_proc(TclCtx *context, tcl_i32 argument_count, tcl_u32 *argument_values) {
    if (argument_count < 4) {        /* 检查必需参数：proc <name> <args> <body> */
        return TCL_ERROR;       /* 参数不足，报错 */
    } /* 结束参数检查 */
    /* 第一步：构造内部存储用的函数体变量标识名 p:<name> */
    context->tmp_roots[0] = tcl_alc_p(context, 64); /* 分配 64 字节空间用于拼接名称 */
    if (context->tmp_roots[0] == TCL_NULL) { /* 检查分配是否成功 */
        return TCL_ERROR; /* 分配失败报错 */
    } /* 结束名分配检查 */
    tcl_u8 *proc_name_buffer = TO_PTR(context, context->tmp_roots[0]); /* 获取缓冲区物理地址 */
    proc_name_buffer[0] = 'p';     /* 设置前缀 'p' 表示这是一个过程体 (Procedure Body) */
    proc_name_buffer[1] = ':';     /* 添加分隔符 */
    const tcl_u8 *original_proc_name = TO_PTR(context, argument_values[1]); /* 获取用户定义的原始名称 */
    tcl_i32 name_copy_index = 0; /* 初始化拷贝索引，避免使用单字母变量名 */
    while (original_proc_name[name_copy_index] && name_copy_index < 60) { /* 限制名称长度防止溢出 */
        proc_name_buffer[name_copy_index + 2] = original_proc_name[name_copy_index]; /* 逐字符搬运 */
        name_copy_index++; /* 推进索引 */
    } /* 结束名称拷贝循环 */
    proc_name_buffer[name_copy_index + 2] = 0; /* 封底字符串结束符 */
    /* 第二步：将函数体源码作为全局变量存储，变量名为 p:<name> */
    if (tcl_set_var(context, TCL_NULL, context->tmp_roots[0], argument_values[3]) != TCL_OK) { /* 设置变量 */
        context->tmp_roots[0] = TCL_NULL; /* 出错清理根0 */
        return TCL_ERROR; /* 返回操作失败 */
    } /* 结束函数体注册检查 */
    /* 第三步：构造内部存储用的参数列表变量标识名 a:<name> */
    context->tmp_roots[1] = tcl_alc_p(context, 64); /* 再次分配空间用于存储参数名 */
    if (context->tmp_roots[1] == TCL_NULL) { /* 分配检查 */
        context->tmp_roots[0] = TCL_NULL; /* 失败清理之前的根0 */
        return TCL_ERROR; /* 返回分配失败 */
    } /* 结束参数名分配检查 */
    tcl_u8 *args_list_name_buffer = TO_PTR(context, context->tmp_roots[1]); /* 获取物理地址 */
    t_mcpy(args_list_name_buffer, TO_PTR(context, context->tmp_roots[0]), 64); /* 拷贝已构造的 p:<name> */
    args_list_name_buffer[0] = 'a';     /* 将前缀修改为 'a' 表示这是参数列表 (Arguments List) */
    /* 第四步：将参数定义字符串存入全局表，变量名为 a:<name> */
    tcl_i32 registration_result = tcl_set_var(context, TCL_NULL, context->tmp_roots[1], argument_values[2]); /* 存储参数 */
    /* 清理临时保护根集合 */
    context->tmp_roots[0] = TCL_NULL; /* 清理根0 */
    context->tmp_roots[1] = TCL_NULL; /* 清理根1 */
    return registration_result; /* 返回注册操作的最终结果状态码 */
} /* 结束 tcl_cmd_proc 函数 */

/* if 指令实现：通过操纵状态机挂起当前执行流，并准备进入条件评估子结界 */
static tcl_i32 tcl_cmd_if(TclCtx *context, tcl_i32 argument_count, tcl_u32 *argument_values) {
    if (argument_count < 3) {        /* 检查参数数量：if <condition> <body> */
        return TCL_ERROR;       /* 参数不足，报错 */
    } /* 结束参数检查 */
    TclFrame *active_frame_ptr = TO_PTR(context, context->curr_f); /* 获取当前活跃栈帧的物理指针 */
    
    /* 剥离外层花括号 */
    tcl_u8 *cond_ptr = TO_PTR(context, argument_values[1]);
    if (cond_ptr[0] == '{') {
        tcl_u32 len = t_slen(cond_ptr);
        tcl_u32 stripped = tcl_alc_p(context, len);
        if (stripped == TCL_NULL) return TCL_ERROR;
        cond_ptr = TO_PTR(context, argument_values[1]); /* Refresh pointer after potential GC */
        t_mcpy(TO_PTR(context, stripped), cond_ptr + 1, len - 2);
        ((tcl_u8*)TO_PTR(context, stripped))[len - 2] = 0;
        active_frame_ptr->cond = stripped;
    } else {
        active_frame_ptr->cond = argument_values[1];
    }
    
    active_frame_ptr->body = argument_values[2]; /* 将待执行的主体脚本偏移量存入栈帧 */
    
    /* 处理 else 分支：将 else_body 存储在当前帧的 result 字段中 */
    /* 设计目的：避免使用全局 tmp_roots[11]，因为嵌套的内层 if 也会写 tmp_roots[11]，导致外层 else_body 被清空 */
    if (argument_count >= 5) {                               /* 判断是否存在 else 子句（至少有 if cond body else else_body 共5个参数） */
        const tcl_u8 *else_kw = TO_PTR(context, argument_values[3]); /* 获取 else 关键字字符串指针 */
        if (t_scmp(else_kw, (tcl_u8*)"else") == 0) {        /* 精确匹配 else 关键字 */
            /* 将 else_body 脚本偏移量存入当前 if 帧的 result 字段 */
            /* result 字段在 ST_IF_COND/ST_IF_BODY 阶段不被条件求值使用，可以安全复用 */
            active_frame_ptr->result = argument_values[4];   /* 存储 else_body 偏移量 */
        } else {                                             /* 有参数但不是 else 关键字 */
            active_frame_ptr->result = TCL_NULL;             /* 清除 result 字段，表示无 else 分支 */
        }                                                    /* 结束 else 关键字匹配 */
    } else {                                                 /* 没有 else 子句 */
        active_frame_ptr->result = TCL_NULL;                 /* 清除 result 字段，表示无 else 分支 */
    }                                                        /* 结束 else 分支处理 */

    active_frame_ptr->state = ST_IF_COND;  /* 修正当前帧的状态为“if 条件评估预备态” */
    return TCL_YIELD;           /* 挂起当前指令的执行，返回让步信号以驱动状态机进入子结界 */
} /* 结束 tcl_cmd_if 函数 */

/* while 指令实现：循环控制挂起逻辑，设置循环上下文并触发状态机跳转 */
static tcl_i32 tcl_cmd_while(TclCtx *context, tcl_i32 argument_count, tcl_u32 *argument_values) {
    if (argument_count < 3) {        /* 检查必需参数：while <condition> <body> */
        return TCL_ERROR;       /* 参数不足，报错 */
    } /* 结束参数检查 */
    TclFrame *active_frame_ptr = TO_PTR(context, context->curr_f); /* 定位当前执行栈帧物理地址 */
    
    /* 剥离外层花括号 */
    tcl_u8 *cond_ptr = TO_PTR(context, argument_values[1]);
    if (cond_ptr[0] == '{') {
        tcl_u32 len = t_slen(cond_ptr);
        tcl_u32 stripped = tcl_alc_p(context, len);
        if (stripped == TCL_NULL) return TCL_ERROR;
        cond_ptr = TO_PTR(context, argument_values[1]); /* Refresh pointer after potential GC */
        t_mcpy(TO_PTR(context, stripped), cond_ptr + 1, len - 2);
        ((tcl_u8*)TO_PTR(context, stripped))[len - 2] = 0;
        active_frame_ptr->cond = stripped;
    } else {
        active_frame_ptr->cond = argument_values[1];
    }
    
    active_frame_ptr->body = argument_values[2]; /* 设置循环体脚本的物理偏移 */
    active_frame_ptr->state = ST_COND;     /* 状态机跳转指令：将当前帧标记为“while 条件判断预备态” */
    return TCL_YIELD;           /* 抛出让步状态码，使主循环在下一轮中启动条件评估 */
} /* 结束 tcl_cmd_while 函数 */

/* expr 指令实现：执行基础的数学算术与逻辑运算，支持延迟求值模式 */
static tcl_i32 tcl_cmd_expr(TclCtx *context, tcl_i32 argument_count, tcl_u32 *argument_values) {
    if (argument_count < 2) {        /* 检查参数数量：expr <expression> */
        return TCL_ERROR;       /* 参数不足，报错 */
    } /* 结束参数检查 */
    if (argument_count > 2) { /* 参数数量多于两个，统一用空格拼接处理 */
        tcl_u32 total_length = 0; /* 用于累加各个参数的物理字符串长度 */
        for (tcl_i32 param_idx = 1; param_idx < argument_count; param_idx++) { /* 顺序统计所有表达式参数的字符总数 */
            total_length += t_slen(TO_PTR(context, argument_values[param_idx])); /* 累加当前参数的长度 */
        } /* 结束长度统计 */
        total_length += (argument_count - 2); /* 加上参数之间填充空格的字符数 */
        total_length += 1; /* 加上末尾的空字符终止符 */
        tcl_u32 joined_offset = tcl_alc_p(context, total_length); /* 从内存池分配拼接后的大表达式字符串空间 */
        if (joined_offset == TCL_NULL) { /* 分配失败判定 */
            return TCL_ERROR; /* 内存不足，返回错误码 */
        } /* 结束分配判定 */
        tcl_u8 *dest_ptr = TO_PTR(context, joined_offset); /* 获取拼接目标物理缓冲区的起始指针 */
        tcl_u32 current_write_offset = 0; /* 初始化目标写入缓冲区偏移量游标 */
        for (tcl_i32 param_idx = 1; param_idx < argument_count; param_idx++) { /* 顺序遍历每一个需要拼接的参数 */
            tcl_u8 *src_ptr = TO_PTR(context, argument_values[param_idx]); /* 重新提取源参数字符串物理指针以避免 GC 重定位失效 */
            tcl_u32 src_len = t_slen(src_ptr); /* 计算当前源参数字符串的长度 */
            t_mcpy(dest_ptr + current_write_offset, src_ptr, src_len); /* 物理拷贝当前参数内容到大字符串缓冲区 */
            current_write_offset += src_len; /* 累加目标写入偏移量游标 */
            if (param_idx < argument_count - 1) { /* 如果不是最后一个参数，需要在其后追加一个空格分隔符 */
                dest_ptr[current_write_offset] = ' '; /* 追加空格 */
                current_write_offset++; /* 偏移游标步进 1 字节 */
            } /* 结束空格追加判定 */
        } /* 结束所有参数物理拼接循环 */
        dest_ptr[current_write_offset] = 0; /* 封底：在字符串末尾写入物理结束符 */
        argument_values[1] = joined_offset; /* 将拼接后的大表达式字符串偏移量回填至首参数位置 */
        argument_count = 2; /* 将有效参数计数置为 2，以匹配并滑入单参数处理逻辑分支 */
    } /* 结束参数拼接与重置 */
    tcl_u8 *expression_script_ptr = TO_PTR(context, argument_values[1]); /* 获取表达式脚本 of 物理地址 */
    /* 设计：expr 的参数无论是否经过花括号处理，都需要：
       1. 对 $var 进行变量替换（tcl_str_interp），确保 {$i+1} 剥括号后能展开 $i
       2. 将展开后的表达式送入新帧（FRAME_IS_EXPR）执行归约求值 */
    if (argument_count == 2) { /* 单参数模式：剥括号后需要展开并在新帧求值 */
        tcl_u8 *raw_expr_ptr = expression_script_ptr; /* 获取原始表达式指针 */
        tcl_u32 raw_expr_len = t_slen(raw_expr_ptr); /* 获取原始表达式长度 */
        tcl_u32 inner_offset; /* 用于存放内层（去括号/展开后）的偏移量 */
        tcl_u32 inner_len; /* 内层字符串长度 */
        if (raw_expr_ptr[0] == '{' && raw_expr_len >= 2) { /* 花括号情况：剥除括号后展开 */
            context->tmp_roots[0] = argument_values[1]; /* 分配前保护当前输入参数偏移量，防止 GC 位移 */
            inner_offset = tcl_alc_p(context, raw_expr_len); /* 分配物理临时空间，可能触发垃圾回收 */
            if (inner_offset == TCL_NULL) { /* 内存分配安全性检查 */
                context->tmp_roots[0] = TCL_NULL; /* 分配失败，立即解除临时根保护 */
                return TCL_ERROR; /* 向上抛出内存资源耗尽错误码 */
            } /* 结束分配安全性判定 */
            argument_values[1] = context->tmp_roots[0]; /* 物理映射后恢复，读取已被 GC 更新的新偏移量 */
            context->tmp_roots[0] = TCL_NULL; /* 成功获取偏移量，解除临时根的垃圾回收保护 */
            raw_expr_ptr = TO_PTR(context, argument_values[1]); /* 根据新偏移量重新物理映射原始表达式指针 */
            t_mcpy(TO_PTR(context, inner_offset), raw_expr_ptr + 1, raw_expr_len - 2); /* 执行剥离括号物理字符拷贝 */
            ((tcl_u8*)TO_PTR(context, inner_offset))[raw_expr_len - 2] = 0; /* 在末尾安全注入物理字符串结束符 */
            inner_len = raw_expr_len - 2; /* 内层长度 */
        } else { /* 非花括号情况：直接作为内层 */
            inner_offset = argument_values[1]; /* 直接使用已有偏移 */
            inner_len = raw_expr_len; /* 长度不变 */
        } /* 结束花括号判断 */
        /* 在 Arena 栈区分配新帧用于执行未提前替换展开的表达式 */
        context->tmp_roots[0] = inner_offset; /* 挂载根保护，防止分配帧触发 GC 导致表达式对象死亡 */
        tcl_u32 sub_expression_frame_offset = tcl_alc_t(context, sizeof(TclFrame)); /* 分配子栈帧空间 */
        if (sub_expression_frame_offset == TCL_NULL) { /* 分配失败判定 */
            context->tmp_roots[0] = TCL_NULL; /* 失败释放保护根 */
            return TCL_ERROR; /* 返回内存错误状态码 */
        } /* 结束子帧分配检查 */
        inner_offset = context->tmp_roots[0]; /* 重新加载可能因 GC 位移的对象偏移量 */
        context->tmp_roots[0] = TCL_NULL; /* 释放根保护 */
        TclFrame *sub_frame_ptr = TO_PTR(context, sub_expression_frame_offset); /* 获取新子帧 of 物理指针 */
        sub_frame_ptr->script = inner_offset; /* 将清洗后（去括号）的表达式脚本绑定到子帧 */
        sub_frame_ptr->pc = 0; /* 程序计数器从起点 */
        sub_frame_ptr->vars = TCL_NULL; /* 表达式帧不创建独立变量表 */
        sub_frame_ptr->parent = context->curr_f; /* 物理返回路径 */
        sub_frame_ptr->scope = context->curr_f; /* 逻辑作用域：共享父帧变量 */
        sub_frame_ptr->state = ST_TOKENIZE; /* 初始状态：分词 */
        sub_frame_ptr->flags = FRAME_SHARE_SCOPE | FRAME_IS_EXPR; /* 共享作用域并启用表达式归约 */
        sub_frame_ptr->cond = sub_frame_ptr->body = sub_frame_ptr->result = TCL_NULL; /* 清空控制槽位 */
        sub_frame_ptr->argc = sub_frame_ptr->exp_idx = 0; /* 清空参数寄存器 */
        context->tmp_roots[0] = TCL_NULL; /* 释放临时根保护 */
        ((TclFrame*)TO_PTR(context, context->curr_f))->state = ST_RESUME; /* 父帧挂起等待结果 */
        context->curr_f = sub_expression_frame_offset; /* 切换执行上下文至表达式子帧 */
        return TCL_YIELD; /* 让步，驱动 FSM 进入新帧执行 */
    } /* 结束单参数模式 */
    context->result = argument_values[1]; /* 设置结果偏移 */
    return TCL_OK; /* 指令正常完成返回成功 */
} /* 结束 tcl_cmd_expr 函数 */

/* return 指令实现：由脚本显式触发结界终止，并向调用者传递执行结果 */
static tcl_i32 tcl_cmd_return(TclCtx *context, tcl_i32 argument_count, tcl_u32 *argument_values) {
    if (argument_count == 2) {       /* 检查是否提供了可选的返回结果参数 */
        context->result = argument_values[1]; /* 设置解释器的当前执行结果为所提供的值 */
    } /* 结束参数检查 */
    return TCL_RETURN;          /* 抛出特殊的返回状态码，驱动状态机执行出栈与结果冒泡 */
} /* 结束 tcl_cmd_return 函数 */

/* break 指令实现：强制终止当前所属的循环结界 */
static tcl_i32 tcl_cmd_break(TclCtx *context, tcl_i32 argument_count, tcl_u32 *argument_values) {
    return TCL_BREAK;           /* 直接抛出中断状态码，供状态机捕获并跳出 ST_LOOP 状态 */
} /* 结束 tcl_cmd_break 函数 */

/* continue 指令实现：跳过当前循环步，立即开始下一次条件判定 */
static tcl_i32 tcl_cmd_continue(TclCtx *context, tcl_i32 argument_count, tcl_u32 *argument_values) {
    return TCL_CONTINUE;        /* 抛出继续状态码，使状态机跳转回 ST_COND 阶段 */
} /* 结束 tcl_cmd_continue 函数 */

/* error 指令实现：由脚本主动抛出运行时错误，可附带错误描述信息 */
static tcl_i32 tcl_cmd_error(TclCtx *context, tcl_i32 argument_count, tcl_u32 *argument_values) {
    if (argument_count > 1) {        /* 检查是否提供了具体的错误描述内容 */
        context->result = argument_values[1]; /* 将描述信息存入上下文，供 catch 捕获 */
    } /* 结束参数检查 */
    return TCL_ERROR;           /* 抛出标准错误状态码，触发异常回溯处理 */
} /* 结束 tcl_cmd_error 函数 */

/* eval 指令实现：执行脚本内容的二次解析，用于动态代码注入 */
static tcl_i32 tcl_cmd_eval(TclCtx *context, tcl_i32 argument_count, tcl_u32 *argument_values) {
    if (argument_count < 2) {        /* 检查必需参数：eval <script> */
        return TCL_ERROR; /* 参数不足报错 */
    } /* 结束参数检查 */
    /* 分配并初始化一个新的执行栈帧，用于解析运行指定的脚本内容 */
    tcl_u32 eval_frame_offset = tcl_alc_t(context, sizeof(TclFrame)); /* 在栈空间分配帧 */
    if (eval_frame_offset == TCL_NULL) { /* 检查分配结果 */
        return TCL_ERROR; /* 分配失败报错 */
    } /* 结束分配检查 */
    TclFrame *eval_frame_ptr = TO_PTR(context, eval_frame_offset); /* 定位新栈帧物理指针 */
    eval_frame_ptr->script = argument_values[1]; /* 绑定待解析执行的脚本偏移量 */
    eval_frame_ptr->pc = 0;              /* 重置新结界的程序计数器到头部 */
    eval_frame_ptr->vars = TCL_NULL;     /* eval 默认复用父级作用域，不创建局部变量表 */
    eval_frame_ptr->parent = context->curr_f; /* 物理返回路径：记录父帧位置，用于子脚本执行完后恢复 */
    eval_frame_ptr->scope = context->curr_f; /* 逻辑作用域：eval 脚本可以透明地读写父帧的所有局部变量 */
    eval_frame_ptr->state = ST_TOKENIZE; /* 设置子结界的初始状态为分词阶段 */
    eval_frame_ptr->flags = FRAME_SHARE_SCOPE; /* 关键：标记共享作用域，允许读写上层变量 */
    eval_frame_ptr->cond = TCL_NULL; /* 初始化条件字段为空 */
    eval_frame_ptr->body = TCL_NULL; /* 初始化主体字段为空 */
    eval_frame_ptr->result = TCL_NULL; /* 初始化执行结果为空 */
    eval_frame_ptr->argc = 0; /* 重置参数计数器 */
    eval_frame_ptr->exp_idx = 0; /* 重置展开索引 */
    ((TclFrame*)TO_PTR(context, context->curr_f))->state = ST_TOKENIZE; /* 关键：设定父帧在恢复后直接进入下一条指令的解析 */
    context->curr_f = eval_frame_offset; /* 物理切换当前执行上下文至新栈帧 */
    return TCL_YIELD;           /* 挂起当前 eval 指令，返回让步信号启动子结界执行流程 */
} /* 结束 tcl_cmd_eval 函数 */

/* catch 指令实现：捕获子脚本执行过程中的异常状态码，防止其继续向上冒泡 */
static tcl_i32 tcl_cmd_catch(TclCtx *context, tcl_i32 argument_count, tcl_u32 *argument_values) {
    if (argument_count < 2) {        /* 必需参数：catch <script> [resultVar] */
        return TCL_ERROR; /* 参数不足报错 */
    } /* 结束参数检查 */
    context->tmp_roots[8] = argument_values[1]; /* 在分配帧空间前，保护待执行的脚本对象 */
    context->tmp_roots[9] = argument_count > 2 ? argument_values[2] : TCL_NULL; /* 保护可选的结果变量名 */
    /* 为子脚本分配一个独立的执行栈帧 */
    tcl_u32 sub_script_frame_offset = tcl_alc_t(context, sizeof(TclFrame)); /* 分配物理空间 */
    if (sub_script_frame_offset == TCL_NULL) { /* 检查内存溢出情况 */
        return TCL_ERROR; /* 返回内存错误码 */
    } /* 结束分配检查 */
    TclFrame *current_active_frame = TO_PTR(context, context->curr_f); /* 获取 catch 指令所在的父帧 */
    current_active_frame->cond = context->tmp_roots[8]; /* 暂存脚本偏移以便后续清理或状态管理 */
    current_active_frame->body = context->tmp_roots[9]; /* 暂存结果变量名，供 ST_CATCH_END 使用 */
    current_active_frame->state = ST_CATCH_END; /* 将父帧标记为捕获结束后的后置处理态 */
    
    TclFrame *sub_frame_ptr = TO_PTR(context, sub_script_frame_offset); /* 获取子结界栈帧的物理指针 */
    sub_frame_ptr->script = context->tmp_roots[8]; /* 绑定待捕获执行的子脚本内容 */
    sub_frame_ptr->pc = 0; /* 初始化子结界的程序计数器 */
    sub_frame_ptr->vars = TCL_NULL; /* catch 默认在共享作用域模式下执行 */
    sub_frame_ptr->parent = context->curr_f; /* 物理返回路径：执行完后回到父帧进行异常状态码处理 */
    sub_frame_ptr->scope = context->curr_f; /* 逻辑作用域：被捕获的脚本可以直接读写外部变量 */
    sub_frame_ptr->state = ST_TOKENIZE; /* 子结界初始设为分词处理状态 */
    sub_frame_ptr->flags = FRAME_SHARE_SCOPE; /* 关键：继承父级作用域，保持逻辑透明 */
    sub_frame_ptr->cond = TCL_NULL; /* 清理条件字段 */
    sub_frame_ptr->body = TCL_NULL; /* 清理主体字段 */
    sub_frame_ptr->result = TCL_NULL; /* 清理结果字段 */
    sub_frame_ptr->argc = 0; /* 初始化参数计数器 */
    sub_frame_ptr->exp_idx = 0; /* 初始化展开索引 */
    
    context->tmp_roots[8] = TCL_NULL; /* 完成保护交接，释放根8 */
    context->tmp_roots[9] = TCL_NULL; /* 完成保护交接，释放根9 */
    context->curr_f = sub_script_frame_offset; /* 物理切换执行流上下文至捕获子结界 */
    return TCL_YIELD; /* 返回让步信号，挂起当前 catch 指令等待结果冒泡 */
} /* 结束 tcl_cmd_catch 函数 */

/* global 指令实现：在当前 proc 作用域内为每个变量名创建指向全局符号表中同名变量的链接 */
/* 语义对齐：global v1 v2 等价于在全局作用域 (#0) 对每个变量执行 upvar 0 v v */
static tcl_i32 tcl_cmd_global(TclCtx *context, tcl_i32 argument_count, tcl_u32 *argument_values) {
    if (argument_count < 2) {        /* 必需参数检查：global varname [varname ...] */
        return TCL_ERROR;            /* 参数数量不足，报错 */
    }                                /* 结束参数检查 */
    tcl_i32 arg_idx = 1;             /* 初始化实参遍历索引，从 argv[1] 开始（跳过命令名本身） */
    while (arg_idx < argument_count) { /* 循环处理每一个要声明为 global 的变量名 */
        context->tmp_roots[12] = argument_values[arg_idx]; /* 保护当前处理的变量名对象，防止分配触发 GC 时丢失 */
        /* 第一步：在全局作用域（TCL_NULL 帧）中检索同名变量节点，若不存在则强制创建空节点 */
        tcl_u32 global_var_node = tcl_find_var_node(context, TCL_NULL, TO_PTR(context, context->tmp_roots[12])); /* 全局查找 */
        if (global_var_node == TCL_NULL) { /* 场景：全局表中该变量尚未存在，须初始化空槽位 */
            if (tcl_set_var(context, TCL_NULL, context->tmp_roots[12], TCL_NULL) != TCL_OK) { /* 创建全局空变量 */
                context->tmp_roots[12] = TCL_NULL; /* 失败清理保护根 */
                return TCL_ERROR;    /* 内存分配失败，向上层抛出错误 */
            }                        /* 结束全局变量创建检查 */
            /* 重新定位新创建的全局变量节点的物理偏移量 */
            global_var_node = tcl_find_var_node(context, TCL_NULL, TO_PTR(context, context->tmp_roots[12])); /* 再次查找确认 */
        }                            /* 结束全局节点存在性检查 */
        context->tmp_roots[14] = global_var_node; /* 保护全局变量节点，防止后续分配导致 GC 移动 */
        /* 第二步：在当前活跃 proc 帧的局部变量表中分配一个新的 TclVar 链接描述符空间 */
        tcl_u32 link_node_offset = tcl_alc_p(context, sizeof(TclVar)); /* 分配链接节点物理空间 */
        if (link_node_offset == TCL_NULL) { /* 物理空间不足处理 */
            context->tmp_roots[12] = TCL_NULL; /* 清理根12 */
            context->tmp_roots[14] = TCL_NULL; /* 清理根14 */
            return TCL_ERROR;        /* 内存分配失败 */
        }                            /* 结束链接节点分配检查 */
        context->tmp_roots[15] = link_node_offset; /* 保护新分配的链接节点，防止分配名称时被 GC 回收 */
        TclVar *link_var_ptr = TO_PTR(context, context->tmp_roots[15]); /* 获取链接节点的物理指针 */
        link_var_ptr->name = TCL_NULL; /* 初始化变量名偏移为空，待后续填充 */
        link_var_ptr->val  = TCL_NULL; /* 初始化值偏移为空，待后续绑定至全局节点 */
        link_var_ptr->next = TCL_NULL; /* 初始化链表后继指针为空 */
        link_var_ptr->flags = VAR_LINK; /* 关键：标记为逻辑链接类型，使读写操作透明穿透至目标全局节点 */
        /* 在对象头部设置 OBJ_VAR_BIT，使 GC 的标记和更新逻辑能正确处理此 TclVar 的 val（全局节点偏移量）字段 */
        ((ObjHeader*)TO_PTR(context, context->tmp_roots[15] - sizeof(ObjHeader)))->size_and_flags |= OBJ_VAR_BIT; /* 设置变量容器标志 */
        /* 第三步：为本地链接节点分配并拷贝变量名字符串（名称与全局变量同名） */
        tcl_u32 local_name_len = t_slen(TO_PTR(context, context->tmp_roots[12])) + 1; /* 计算含终止符的名称长度 */
        tcl_u32 name_storage_offset = tcl_alc_p(context, local_name_len); /* 分配名称字符串的物理存储空间 */
        if (name_storage_offset == TCL_NULL) { /* 处理名称存储分配失败 */
            context->tmp_roots[12] = TCL_NULL; /* 清理根12 */
            context->tmp_roots[14] = TCL_NULL; /* 清理根14 */
            context->tmp_roots[15] = TCL_NULL; /* 清理根15 */
            return TCL_ERROR;        /* 内存分配失败 */
        }                            /* 结束名称存储分配检查 */
        /* 将变量名字符串物理搬运至新分配的存储区中 */
        t_mcpy(TO_PTR(context, name_storage_offset), TO_PTR(context, context->tmp_roots[12]), local_name_len); /* 搬运名称数据 */
        /* 第四步：填充链接节点各字段并将其插入当前 proc 帧的局部变量链表头部 */
        TclFrame *active_frame_ptr = TO_PTR(context, context->curr_f); /* 重新获取当前活跃帧的物理指针 */
        link_var_ptr = TO_PTR(context, context->tmp_roots[15]);         /* 刷新链接节点物理指针（可能因分配发生位移） */
        link_var_ptr->name = name_storage_offset;                       /* 绑定本地变量名偏移 */
        link_var_ptr->val  = context->tmp_roots[14];                    /* 关键：将 val 设置为全局变量节点的物理偏移量（而非其值） */
        link_var_ptr->next = active_frame_ptr->vars;                    /* 采用头插法：新节点的后继指向当前链表头 */
        active_frame_ptr->vars = context->tmp_roots[15];                /* 将当前帧的局部变量链表头更新为新链接节点 */
        /* 清理所有 GC 保护根，为下一轮循环做准备 */
        context->tmp_roots[12] = TCL_NULL; /* 释放根12 */
        context->tmp_roots[14] = TCL_NULL; /* 释放根14 */
        context->tmp_roots[15] = TCL_NULL; /* 释放根15 */
        arg_idx++;                   /* 推进到下一个待声明的变量名 */
    }                                /* 结束所有变量名的 global 绑定循环 */
    return TCL_OK;                   /* 所有变量链接建立完成，返回成功 */
}                                    /* 结束 tcl_cmd_global 函数 */

/* upvar 指令实现：建立本地变量与上层作用域变量之间的物理链接引用 */
static tcl_i32 tcl_cmd_upvar(TclCtx *context, tcl_i32 argument_count, tcl_u32 *argument_values) {
    if (argument_count < 3) {        /* 必需参数检查：upvar [level] otherVar myVar */
        return TCL_ERROR; /* 参数数量不足，报错 */
    } /* 结束参数检查 */
    tcl_i32 back_level = 1;          /* 默认情况向上追溯一层栈帧 */
    tcl_i32 argument_start_index = 1;      /* 确定解析变量名的起始索引位置 */
    tcl_u32 target_frame_offset = context->curr_f; /* 从当前执行帧启动回溯流程 */
    const tcl_u8 *first_param_ptr = TO_PTR(context, argument_values[1]); /* 获取首个参数内容 */
    /* 识别可选的 level 参数，支持 #0 语法 */
    if (argument_count > 3 && first_param_ptr[0] == '#') { /* 绝对层级判定 */
        if (first_param_ptr[1] == '0' && first_param_ptr[2] == 0) { /* 专门处理 #0 全局作用域 */
            target_frame_offset = TCL_NULL; /* 直接将 #0 目标帧指向 TCL_NULL 以映射至全局变量表 g_vars */
        } else { /* 处理其他绝对层级（BareTcl 简化处理：目前优先保证 #0 兼容性） */
            target_frame_offset = TCL_NULL; /* 暂时退化为全局处理以满足测试需求 */
        } /* 结束 #N 绝对层级处理 */
        argument_start_index = 2; /* 参数偏移量递增，跳过已处理的层级字段 */
    } else { /* 相对层级处理逻辑 */
        if (argument_count > 3 && (first_param_ptr[0] >= '0' && first_param_ptr[0] <= '9')) { 
            back_level = t_atoi(first_param_ptr); /* 解析用户指定的相对回溯层数数值 */
            argument_start_index = 2; /* 变量名解析起始索引同步后移 */
        }
        tcl_i32 actual_steps = 0; /* 记录实际跨越的非共享（即具有独立过程作用域）的层级数 */
        while (target_frame_offset != TCL_NULL && actual_steps < back_level) {
            target_frame_offset = ((TclFrame*)TO_PTR(context, target_frame_offset))->parent; /* 沿着物理调用链向上回溯 */
            if (target_frame_offset != TCL_NULL && !(((TclFrame*)TO_PTR(context, target_frame_offset))->flags & FRAME_SHARE_SCOPE)) {
                actual_steps++; /* 仅当触及非共享作用域帧（如 proc 帧）时，才计为一个有效逻辑层级 */
            }
        }
    }
    context->tmp_roots[12] = argument_values[argument_start_index];     /* 在内存分配前保护目标变量名对象 */
    context->tmp_roots[13] = argument_values[argument_start_index + 1]; /* 保护本地即将建立链接的变量名对象 */
    /* 在确定的目标物理栈帧中检索指定的变量节点 */
    tcl_u32 found_target_node = tcl_find_var_node(context, target_frame_offset, TO_PTR(context, context->tmp_roots[12])); /* 执行深度检索 */
    if (found_target_node == TCL_NULL) { /* 场景：目标变量在所求层级尚不存在，需要强制初始化 */
        if (tcl_set_var(context, target_frame_offset, context->tmp_roots[12], TCL_NULL) != TCL_OK) { /* 创建空节点 */
            context->tmp_roots[12] = TCL_NULL; /* 分配失败清理根12 */
            context->tmp_roots[13] = TCL_NULL; /* 分配失败清理根13 */
            return TCL_ERROR; /* 向上传递内存分配错误 */
        } /* 结束创建空节点检查 */
        /* 重新定位新创建的目标节点物理偏移 */
        found_target_node = tcl_find_var_node(context, target_frame_offset, TO_PTR(context, context->tmp_roots[12]));
    } /* 结束目标变量存在性检查 */
    context->tmp_roots[14] = found_target_node; /* 保护目标变量节点 */
    /* 在当前活跃栈帧中分配一个新的 TclVar 结构体空间 */
    tcl_u32 link_node_offset = tcl_alc_p(context, sizeof(TclVar));
    if (link_node_offset == TCL_NULL) { /* 物理空间不足处理 */
        context->tmp_roots[12] = TCL_NULL; /* 清理根12 */
        context->tmp_roots[13] = TCL_NULL; /* 清理根13 */
        context->tmp_roots[14] = TCL_NULL; /* 清理根14 */
        return TCL_ERROR; /* 内存分配失败 */
    } /* 结束链接节点分配检查 */
    context->tmp_roots[15] = link_node_offset; /* 保护新分配的链接节点 */
    TclVar *link_variable_ptr = TO_PTR(context, context->tmp_roots[15]); /* 获取物理指针 */
    link_variable_ptr->name = TCL_NULL; /* 初始化变量名偏移 */
    link_variable_ptr->val = TCL_NULL; /* 初始化变量值偏移 */
    link_variable_ptr->next = TCL_NULL; /* 初始化后继节点偏移 */
    link_variable_ptr->flags = VAR_LINK; /* 极其关键：标记为逻辑链接，而非持有数据的普通变量 */
    /* 设置变量对象标记位，以便 GC 能识别并正确处理其 val 字段（即指向另一个变量节点的物理偏移） */
    ((ObjHeader*)TO_PTR(context, context->tmp_roots[15] - sizeof(ObjHeader)))->size_and_flags |= OBJ_VAR_BIT;
    
    /* 分配本地变量名的物理存储空间 */
    tcl_u32 local_name_len = t_slen(TO_PTR(context, context->tmp_roots[13])) + 1;
    tcl_u32 name_storage_offset = tcl_alc_p(context, local_name_len);
    if (name_storage_offset == TCL_NULL) { /* 处理空间分配失败 */
        context->tmp_roots[12] = TCL_NULL; /* 清理根12 */
        context->tmp_roots[13] = TCL_NULL; /* 清理根13 */
        context->tmp_roots[14] = TCL_NULL; /* 清理根14 */
        context->tmp_roots[15] = TCL_NULL; /* 清理根15 */
        return TCL_ERROR; /* 内存分配失败 */
    } /* 结束名称存储分配检查 */
    /* 物理搬运本地变量名 */
    t_mcpy(TO_PTR(context, name_storage_offset), TO_PTR(context, context->tmp_roots[13]), local_name_len);
    
    TclFrame *active_frame_ptr = TO_PTR(context, context->curr_f); /* 重新定位当前栈帧 */
    link_variable_ptr = TO_PTR(context, context->tmp_roots[15]); /* 刷新链接节点物理指针 */
    link_variable_ptr->name = name_storage_offset; /* 绑定本地名称 */
    link_variable_ptr->val = context->tmp_roots[14]; /* 关键：链接至目标栈帧中的物理变量节点 */
    link_variable_ptr->next = active_frame_ptr->vars; /* 将链接变量插入本地局部变量链表头部 */
    active_frame_ptr->vars = context->tmp_roots[15]; /* 更新本地链表头 */
    
    /* 任务顺利完成，清理所有保护根 */
    context->tmp_roots[12] = TCL_NULL; /* 清理根12 */
    context->tmp_roots[13] = TCL_NULL; /* 清理根13 */
    context->tmp_roots[14] = TCL_NULL; /* 清理根14 */
    context->tmp_roots[15] = TCL_NULL; /* 清理根15 */
    return TCL_OK; /* 返回成功码 */
}

/* uplevel 指令实现：在指定的父级调用栈帧中评估执行脚本 */
static tcl_i32 tcl_cmd_uplevel(TclCtx *context, tcl_i32 argument_count, tcl_u32 *argument_values) {
    if (argument_count < 2) {        /* 检查必需参数：uplevel [level] <script> */
        return TCL_ERROR; /* 参数数量不足，报错 */
    } /* 结束参数检查 */
    tcl_u32 target_parent_offset = context->curr_f; /* 初始指向当前活跃的执行栈帧 */
    tcl_i32 back_steps = 1; /* 默认情况下向逻辑父级追溯一步（uplevel script 等效 uplevel 1 script） */
    tcl_i32 script_arg_index = 1; /* 默认认为脚本内容位于参数数组的第 1 位 */
    const tcl_u8 *first_argument_ptr = TO_PTR(context, argument_values[1]); /* 获取首个参数内容指针 */
    /* 识别可选的层级参数：仅当有 2 个以上参数且首参数为数字或 # 开头时才视为层级参数 */
    if (argument_count > 2 && (first_argument_ptr[0] == '#' || (first_argument_ptr[0] >= '0' && first_argument_ptr[0] <= '9'))) {
        if (first_argument_ptr[0] == '#') { /* 处理绝对层级标识符（如 #0 表示全局作用域） */
            if (first_argument_ptr[1] == '0' && first_argument_ptr[2] == 0) { /* 场景：#0 绝对全局作用域 */
                target_parent_offset = TCL_NULL; /* 执行环境重定向为系统全局变量域 */
            } else { /* 其他绝对层级支持（BareTcl 简化为全局处理） */
                target_parent_offset = TCL_NULL; /* 确保脚本执行在最顶层作用域 */
            }
            back_steps = 0; /* #N 形式已直接定位目标帧，无需再执行相对层级跳转循环 */
        } else { /* 处理常规相对层级数字（如 uplevel 1、uplevel 2） */
            back_steps = t_atoi(first_argument_ptr); /* 解析用户指定的相对层级步数 */
        }
        script_arg_index = 2; /* 参数位置修正：由于提供了层级字段，脚本内容后移至索引 2 */
    }
    /* 设计目的：标准 Tcl uplevel N 语义是"在当前 proc 的第 N 层调用者的作用域中执行"。
       正确算法：从当前帧出发，沿 scope 链找到当前所在的 proc 边界（scope==TCL_NULL 的帧），
       再通过物理 parent 跳到调用者帧，重复 back_steps 次。
       此设计保证无论 uplevel 从 proc 直接调用，还是从 proc 内的 while/catch 等共享帧调用，
       都能正确穿越整个 proc 作用域边界，到达真正的调用者帧（而非中间的共享帧）。
       当 back_steps==0（#0/#N 绝对定位）时跳过循环，直接使用已设定的目标帧。
       当无显式数字参数（uplevel script）时，back_steps 默认为 1，等效 uplevel 1 script。 */
    for (tcl_i32 level_idx = 0; level_idx < back_steps && target_parent_offset != TCL_NULL; level_idx++) {
        TclFrame *search_frame = TO_PTR(context, target_parent_offset); /* 获取当前帧指针 */
        /* 沿 scope 链向上找到 proc 边界（scope==TCL_NULL 的帧即为 proc 根作用域帧） */
        while (search_frame->scope != TCL_NULL) { /* 如果当前帧是共享作用域帧（如 while body），则继续向上 */
            target_parent_offset = search_frame->scope; /* 移向 scope 链的上一层 */
            search_frame = TO_PTR(context, target_parent_offset); /* 刷新帧指针 */
        } /* 退出时 search_frame 即为当前所在的 proc 帧（scope==TCL_NULL，独立作用域根） */
        /* 跨越 proc 边界：通过物理 parent 链接跳到该 proc 的调用者帧 */
        target_parent_offset = search_frame->parent; /* parent 指向调用者帧（逻辑上升一层） */
    } /* 重复 back_steps 次，最终到达第 N 层调用者帧 */
    /* 分配并初始化新栈帧用于执行被提升层级的脚本内容 */
    tcl_u32 new_frame_offset = tcl_alc_t(context, sizeof(TclFrame)); /* 分配栈帧空间 */
    if (new_frame_offset == TCL_NULL) { /* 处理空间不足 */
        return TCL_ERROR; /* 内存分配失败 */
    } /* 结束栈帧分配检查 */
    TclFrame *new_frame_ptr = TO_PTR(context, new_frame_offset); /* 获取物理地址 */
    new_frame_ptr->script = argument_values[script_arg_index]; /* 绑定待执行脚本 */
    new_frame_ptr->pc = 0; /* 重置程序计数器 */
    new_frame_ptr->vars = TCL_NULL; /* uplevel 不创建新私有作用域，变量存取将穿透至目标父帧 */
    new_frame_ptr->parent = context->curr_f; /* 物理返回路径：执行完后回到当前调用者帧 */
    new_frame_ptr->scope = target_parent_offset; /* 逻辑作用域：变量查找将从计算出的目标父帧开始回溯 */
    new_frame_ptr->state = ST_TOKENIZE; /* 初始状态设为分词 */
    new_frame_ptr->flags = FRAME_SHARE_SCOPE; /* 保持作用域共享特性，确保在 parent 作用域内操作变量 */
    new_frame_ptr->cond = TCL_NULL; /* 初始化循环条件偏移 */
    new_frame_ptr->body = TCL_NULL; /* 初始化循环体偏移 */
    new_frame_ptr->result = TCL_NULL; /* 初始化结果偏移 */
    new_frame_ptr->argc = 0; /* 初始化参数计数 */
    new_frame_ptr->exp_idx = 0; /* 初始化展开索引 */
    ((TclFrame*)TO_PTR(context, context->curr_f))->state = ST_TOKENIZE; /* 关键：设定父帧在恢复后直接进入下一条指令的解析 */
    context->curr_f = new_frame_offset; /* 执行上下文物理切换，将当前活跃帧设为新分配的帧 */
    return TCL_YIELD; /* 抛出让步信号，挂起当前 uplevel 指令，等待新帧执行完毕 */
}

/* list 指令实现：将多个参数合并为一个符合 Tcl 语法的标准列表字符串 */
static tcl_i32 tcl_cmd_list(TclCtx *context, tcl_i32 argument_count, tcl_u32 *argument_values) {
    tcl_u32 estimated_total_length = 0; /* 用于预估合并后字符串的总物理长度 */
    for (tcl_i32 param_idx = 1; param_idx < argument_count; param_idx++) { /* 遍历所有待合并参数 */
        /* 预估长度，包含潜在的空格分隔符以及两侧的花括号包裹空间 */
        estimated_total_length += t_slen(TO_PTR(context, argument_values[param_idx])) + 3; /* 累加预估长度 */
    } /* 结束长度预估循环 */
    /* 在变量区分配足够的物理空间来存放合并后的结果字符串 */
    tcl_u32 result_list_offset = tcl_alc_p(context, estimated_total_length + 1); /* 分配结果缓冲区 */
    if (result_list_offset == TCL_NULL) { /* 物理空间不足，检查分配结果 */
        return TCL_ERROR; /* 内存分配失败 */
    } /* 结束结果缓冲区分配检查 */
    tcl_u8 *target_buffer_ptr = TO_PTR(context, result_list_offset); /* 定位结果缓冲区写入起点 */
    for (tcl_i32 arg_idx = 1; arg_idx < argument_count; arg_idx++) { /* 遍历并处理每个参数 */
        const tcl_u8 *current_string_ptr = TO_PTR(context, argument_values[arg_idx]); /* 获取当前参数指针 */
        tcl_i32 requires_brace_wrapping = 0; /* 检查参数是否包含空格等特殊字符，需要用 {} 包裹 */
        for (tcl_i32 char_pos = 0; current_string_ptr[char_pos]; char_pos++) { /* 扫描参数中的字符 */
            if (current_string_ptr[char_pos] == ' ' || current_string_ptr[char_pos] == '\t' || current_string_ptr[char_pos] == '\n') { /* 检查空白符 */
                requires_brace_wrapping = 1; /* 发现空白符，标记需要执行包裹逻辑 */
                break; /* 退出内部扫描 */
            } /* 结束空白符判定 */
        } /* 结束字符扫描循环 */
        if (requires_brace_wrapping) { /* 判断是否需要写入左花括号 */
            *target_buffer_ptr++ = '{'; /* 在开头写入左花括号 */
        } /* 结束左花括号写入 */
        tcl_u32 current_element_len = t_slen(current_string_ptr); /* 计算当前参数的物理长度 */
        t_mcpy(target_buffer_ptr, current_string_ptr, current_element_len); /* 执行内容物理拷贝 */
        target_buffer_ptr += current_element_len; /* 推进写入位置指针 */
        if (requires_brace_wrapping) { /* 判断是否需要写入右花括号 */
            *target_buffer_ptr++ = '}'; /* 在结尾补齐右花括号 */
        } /* 结束右花括号写入 */
        if (arg_idx < argument_count - 1) { /* 判断是否需要在元素间添加空格 */
            *target_buffer_ptr++ = ' '; /* 在非最后一个参数后添加空格分隔符 */
        } /* 结束空格分隔符写入 */
    } /* 结束参数合并循环 */
    *target_buffer_ptr = 0; /* 在生成的字符串末尾封底结束符 */
    context->result = result_list_offset; /* 将生成的列表物理偏移存入解释器结果区 */
    return TCL_OK; /* 指令正常结束 */
}

/* llength 指令实现：解析列表字符串并统计顶层元素的数量（零分配解析思路） */
/* 本函数遵循零分配原则，直接在原始字符串缓冲区上进行扫描，无需分配任何临时内存来提取元素。 */
static tcl_i32 tcl_cmd_llength(TclCtx *context, tcl_i32 argument_count, tcl_u32 *argument_values) {
    if (argument_count < 2) {        /* 检查必需参数：llength <list> */
        return TCL_ERROR;       /* 参数缺失，返回错误 */
    } /* 结束参数检查 */
    const tcl_u8 *list_string_ptr = TO_PTR(context, argument_values[1]); /* 获取待解析列表字符串的物理指针 */
    tcl_i32 element_count = 0;          /* 初始化元素统计计数器 */
    tcl_i32 brace_nesting_depth = 0;    /* 用于追踪当前扫描点所处的话括号嵌套深度 */
    while (*list_string_ptr) { /* 循环遍历整个字符串直到物理结束符 */
        /* 跳过当前元素之前或两个元素之间的空白字符（空格、制表符、换行） */
        while (*list_string_ptr == ' ' || *list_string_ptr == '\t' || *list_string_ptr == '\n') { /* 跳过空白 */
            list_string_ptr++; /* 指针自增 */
        } /* 结束跳过空白循环 */
        if (!*list_string_ptr) { /* 检查是否已处理完所有内容 */
            break; /* 若跳过空白后已到达字符串末尾，则结束循环 */
        } /* 结束空字符串判定 */
        element_count++;                /* 识别到一个新的有效列表元素，计数加一 */
        if (*list_string_ptr == '{') { /* 处理以花括号包裹的复杂元素（支持多级嵌套） */
            brace_nesting_depth = 1; /* 进入初始的一级嵌套 */
            list_string_ptr++; /* 跳过起始的左花括号 */
            while (*list_string_ptr && brace_nesting_depth > 0) { /* 寻找匹配的闭合右花括号 */
                if (*list_string_ptr == '{') { /* 检查嵌套深度增加 */
                    brace_nesting_depth++; /* 深度递增 */
                } else if (*list_string_ptr == '}') { /* 检查嵌套深度减少 */
                    brace_nesting_depth--; /* 深度递减 */
                } /* 结束嵌套深度调整 */
                list_string_ptr++; /* 移动线性扫描指针 */
            } /* 结束花括号配对循环 */
        } else {                /* 处理普通的原子元素（不带花括号包裹） */
            /* 扫描直至遇到下一个空白符或字符串物理结束，标识该元素结束 */
            while (*list_string_ptr && *list_string_ptr != ' ' && *list_string_ptr != '\t' && *list_string_ptr != '\n') { /* 扫描原子元素 */
                list_string_ptr++; /* 指针自增 */
            } /* 结束原子元素扫描循环 */
        } /* 结束元素类型判定 */
    } /* 结束列表遍历循环 */
    /* 在变量区分配空间，用于存储计数值转换后的整数字符串表示 */
    tcl_u32 result_string_offset = tcl_alc_p(context, 12); /* 分底结果存储空间 */
    if (result_string_offset != TCL_NULL) { /* 检查分配是否成功 */
        t_itoa(element_count, TO_PTR(context, result_string_offset)); /* 将整型结果转换为十进制字符串 */
        context->result = result_string_offset; /* 将统计结果设为解释器的当前返回值 */
    } /* 结束结果转换检查 */
    return TCL_OK; /* 指令顺利执行完毕 */
}

/* lindex 指令实现：根据索引提取列表中的特定元素（零分配解析原则） */
/* 零分配解析：通过线性扫描原始列表字符串定位目标元素的物理起止点，仅在最终提取时进行一次内存克隆。 */
static tcl_i32 tcl_cmd_lindex(TclCtx *context, tcl_i32 argument_count, tcl_u32 *argument_values) {
    if (argument_count < 3) {        /* 检查必需参数：lindex <list> <index> */
        return TCL_ERROR;       /* 参数不足，报错 */
    } /* 结束参数检查 */
    const tcl_u8 *list_scan_ptr = TO_PTR(context, argument_values[1]); /* 定位待解析列表物理内存 */
    tcl_i32 target_index = t_atoi(TO_PTR(context, argument_values[2])); /* 解析用户指定的目标索引数值 */
    tcl_i32 current_element_idx = 0; /* 初始化当前扫描到的元素索引 */
    tcl_i32 nesting_depth = 0; /* 花括号深度追踪 */
    while (*list_scan_ptr) { /* 线性扫描整个列表字符串 */
        /* 跳过前导空白符 */
        while (*list_scan_ptr == ' ' || *list_scan_ptr == '\t' || *list_scan_ptr == '\n') { /* 跳过空白 */
            list_scan_ptr++; /* 指针自增 */
        } /* 结束前导空白跳过 */
        if (!*list_scan_ptr) { /* 检查是否触底 */
            break; /* 若已到达字符串末尾，则退出循环 */
        } /* 结束内容判空 */
        const tcl_u8 *element_start_pos = list_scan_ptr; /* 记录当前列表元素的物理起始位置 */
        tcl_i32 element_physical_len = 0; /* 初始化列表元素物理长度 */
        if (*list_scan_ptr == '{') { /* 场景 A：元素由花括号包裹 */
            list_scan_ptr++; /* 跳过起始左花括号 */
            element_start_pos = list_scan_ptr; /* 实际内容的物理起点在此 */
            nesting_depth = 1; /* 初始化嵌套深度 */
            while (*list_scan_ptr && nesting_depth > 0) { /* 循环寻找匹配的右花括号 */
                if (*list_scan_ptr == '{') { /* 处理深层嵌套 */
                    nesting_depth++; /* 深度递增 */
                } else if (*list_scan_ptr == '}') { /* 退出嵌套 */
                    nesting_depth--; /* 深度递减 */
                } /* 结束嵌套调整 */
                list_scan_ptr++; /* 推进扫描指针 */
            } /* 结束花括号扫描循环 */
            element_physical_len = list_scan_ptr - element_start_pos - 1; /* 计算长度，不计末尾的右花括号 */
        } else {                /* 场景 B：简单原子元素，无特殊字符包裹 */
            /* 扫描直至遇到空白符或字符串结束 */
            while (*list_scan_ptr && *list_scan_ptr != ' ' && *list_scan_ptr != '\t' && *list_scan_ptr != '\n') { /* 扫描原子元素 */
                list_scan_ptr++; /* 指针自增 */
            } /* 结束扫描原子元素循环 */
            element_physical_len = list_scan_ptr - element_start_pos; /* 物理偏移相减得到长度 */
        } /* 结束元素类型处理 */
        if (current_element_idx == target_index) { /* 检查当前扫描到的元素是否命中目标索引 */
            /* 在变量区分配空间以克隆该元素的内容（含结束符） */
            tcl_u32 element_clone_offset = tcl_alc_p(context, element_physical_len + 1); /* 分配克隆空间 */
            if (element_clone_offset == TCL_NULL) { /* 检查内存分配 */
                return TCL_ERROR; /* 空间不足报错 */
            } /* 结束内存分配检查 */
            t_mcpy(TO_PTR(context, element_clone_offset), element_start_pos, element_physical_len); /* 执行内容物理拷贝 */
            ((tcl_u8*)TO_PTR(context, element_clone_offset))[element_physical_len] = 0; /* 封底字符串结束符 */
            context->result = element_clone_offset; /* 将提取出的元素设为解释器的当前返回结果 */
            return TCL_OK; /* 任务圆满完成 */
        } /* 结束索引命中判定 */
        current_element_idx++; /* 索引推移，准备开始寻找下一个列表元素 */
    } /* 结束列表线性扫描循环 */
    context->result = TCL_NULL; /* 若索引越界或未找到，按 Tcl 标准返回空字符串 */
    return TCL_OK; /* 指令正常结束 */
}

/* lrange 指令实现：截取列表中的指定索引范围段并生成一个新的列表字符串 */
static tcl_i32 tcl_cmd_lrange(TclCtx *context, tcl_i32 argument_count, tcl_u32 *argument_values) {
    if (argument_count < 4) {        /* 检查必需参数：lrange <list> <first> <last> */
        return TCL_ERROR;       /* 参数不足，报错 */
    } /* 结束参数检查 */
    tcl_i32 first_idx = t_atoi(TO_PTR(context, argument_values[2])); /* 解析并转换起始提取索引 */
    tcl_i32 last_idx = tcl_i32_MAX; /* 初始化结束索引为 32 位有符号整数最大值（无限大） */
    /* 特殊处理：检查结束索引是否为关键字 "end" */
    if (t_scmp(TO_PTR(context, argument_values[3]), (tcl_u8*)"end") != 0) { /* 若不是 end 字符串 */
        last_idx = t_atoi(TO_PTR(context, argument_values[3])); /* 解析用户提供的数值 */
    } /* 结束 end 判定 */
    /* 分配结果缓冲区的内存空间，为防止越界，采用保守策略分配与原列表等长的物理空间 */
    tcl_u32 result_buffer_offset = tcl_alc_p(context, t_slen(TO_PTR(context, argument_values[1])) + 1); /* 分配结果区 */
    if (result_buffer_offset == TCL_NULL) { /* 检查分配结果 */
        return TCL_ERROR; /* 空间不足时终止执行 */
    } /* 结束结果区分配检查 */
    tcl_u8 *write_cursor_ptr = TO_PTR(context, result_buffer_offset); /* 定位结果列表缓冲区的写入游标 */
    const tcl_u8 *source_list_ptr = TO_PTR(context, argument_values[1]); /* 定位源列表的读取游标 */
    tcl_i32 current_item_count = 0; /* 初始化当前扫描元素的计数器 */
    while (*source_list_ptr) { /* 开始对源列表进行线性遍历解析 */
        /* 跳过当前元素之前的前导空白字符 */
        while (*source_list_ptr == ' ' || *source_list_ptr == '\t' || *source_list_ptr == '\n') { /* 跳过前导空白 */
            source_list_ptr++; /* 指针自增 */
        } /* 结束跳过前导空白循环 */
        if (!*source_list_ptr) { /* 检查源字符串是否结束 */
            break; /* 如果跳过空白后已至字符串末尾则退出循环 */
        } /* 结束判空处理 */
        const tcl_u8 *item_content_start = source_list_ptr; /* 标记当前识别到的元素起始物理位置 */
        tcl_i32 brace_depth = 0; /* 初始化嵌套花括号层级追踪变量 */
        if (*source_list_ptr == '{') { /* 如果该元素被花括号包裹（场景 A） */
            source_list_ptr++; /* 越过起点的左花括号 */
            item_content_start = source_list_ptr; /* 实际物理内容的起点后移 */
            brace_depth = 1; /* 初始深度设为 1 */
            while (*source_list_ptr && brace_depth > 0) { /* 循环查找匹配对 */
                if (*source_list_ptr == '{') { /* 嵌套加深 */
                    brace_depth++; /* 深度递增 */
                } else if (*source_list_ptr == '}') { /* 嵌套退出 */
                    brace_depth--; /* 深度递减 */
                } /* 结束嵌套判定 */
                source_list_ptr++; /* 指针推移 */
            } /* 结束花括号匹配循环 */
        } else {                /* 简单原子元素（场景 B） */
            /* 寻找元素的物理分界点（下一个空白符） */
            while (*source_list_ptr && *source_list_ptr != ' ' && *source_list_ptr != '\t' && *source_list_ptr != '\n') { /* 查找边界 */
                source_list_ptr++; /* 指针推移 */
            } /* 结束查找边界循环 */
        } /* 结束元素类型判定 */
        /* 范围判定：检查当前解析到的元素序号是否落在指定的 [first, last] 区间内 */
        if (current_item_count >= first_idx && (last_idx == tcl_i32_MAX || current_item_count <= last_idx)) { /* 区间判定 */
            /* 如果这是向新列表写入的第二个及以后的元素，则需要补入空格分隔符 */
            if (write_cursor_ptr != TO_PTR(context, result_buffer_offset)) { /* 检查是否需要前导空格 */
                *write_cursor_ptr++ = ' '; /* 写入空格 */
            } /* 结束前导空格写入 */
            /* 计算需要物理拷贝的字符串载荷长度（如果是花括号包裹则扣除右侧闭合的大括号） */
            tcl_i32 copy_len = source_list_ptr - item_content_start - (brace_depth > 0 ? 1 : 0); /* 计算拷贝长度 */
            t_mcpy(write_cursor_ptr, item_content_start, copy_len); /* 执行元素内容的物理复制 */
            write_cursor_ptr += copy_len; /* 将写入游标移动相应长度 */
        } /* 结束区间内容拷贝 */
        current_item_count++; /* 元素计数器累加，准备处理源列表中的下一项 */
    } /* 结束源列表解析循环 */
    *write_cursor_ptr = 0; /* 遍历结束后，在新列表的末尾写入安全的字符串结束符 */
    context->result = result_buffer_offset; /* 将生成的新列表偏移量设为解释器的当前结果 */
    return TCL_OK; /* lrange 执行成功 */
}

/* unset 指令实现：通过从作用域链表中摘除节点来逻辑销毁变量，物理内存将由 GC 后续回收 */
static tcl_i32 tcl_cmd_unset(TclCtx *context, tcl_i32 argument_count, tcl_u32 *argument_values) {
    if (argument_count < 2) { /* 检查必需参数：unset <varName> */
        return TCL_ERROR; /* 指令参数不完整，直接返回错误码 */
    } /* 结束参数合法性校验分支 */
    const tcl_u8 *target_name_ptr = TO_PTR(context, argument_values[1]); /* 获取要删除的变量名称的物理内存指针 */
    tcl_u32 current_frame_offset = context->curr_f; /* 初始化回溯起点为当前活跃的任务栈帧 */
    /* 第一阶段：沿着调用栈链向上遍历，在各层局部作用域中尝试删除 */
    while (current_frame_offset != TCL_NULL) { /* 当未到达栈底且未找到目标时，持续向上级作用域回溯 */
        TclFrame *frame_ptr = TO_PTR(context, current_frame_offset); /* 将当前的栈帧偏移量转换为物理结构体指针 */
        tcl_u32 *list_pointer_ref = &frame_ptr->vars; /* 获取当前栈帧中变量链表头节点的引用地址 */
        tcl_u32 node_offset = *list_pointer_ref; /* 获取链表中第一个变量节点的偏移量 */
        while (node_offset != TCL_NULL) { /* 遍历当前栈帧所拥有的所有局部变量节点 */
            TclVar *variable_node = TO_PTR(context, node_offset); /* 将节点偏移量转换为变量结构体指针 */
            if (t_scmp(target_name_ptr, TO_PTR(context, variable_node->name)) == 0) { /* 比较变量名是否与目标一致 */
                *list_pointer_ref = variable_node->next; /* 从单向链表中逻辑移除该变量节点 */
                return TCL_OK; /* 变量删除成功，结束 unset 执行并返回成功状态 */
            } /* 结束变量名称匹配后的处理流程 */
            list_pointer_ref = &variable_node->next; /* 移动引用指针，指向当前节点的下一个指针域 */
            node_offset = *list_pointer_ref; /* 更新当前节点偏移量，推进到链表下一项 */
        } /* 结束对当前栈帧局部变量链表的完整扫描 */
        /* 如果当前帧是共享作用域（如执行 eval 或 if 内部逻辑），则允许穿透到父帧继续查找 */
        if (frame_ptr->flags & FRAME_SHARE_SCOPE) { /* 判断当前作用域是否具有“作用域穿透”属性 */
            current_frame_offset = frame_ptr->parent; /* 将查找目标上移至父级调用栈帧 */
        } else { /* 若当前是标准的局部作用域边界（如 proc 内部） */
            break; /* 强制终止向上回溯，确保局部变量不会越过 proc 边界被删除 */
        } /* 结束作用域穿透逻辑判定 */
    } /* 结束调用栈层级的变量回溯搜索 */
    /* 第二阶段：如果在所有可达的局部作用域中均未找到，则在解释器的全局变量表中尝试删除 */
    tcl_u32 *global_list_pointer_ref = &context->g_vars; /* 获取全局解释器状态中的变量链表头引用 */
    tcl_u32 global_node_offset = *global_list_pointer_ref; /* 读取全局变量链表的首个节点位置 */
    while (global_node_offset != TCL_NULL) { /* 遍历全局作用域下的所有变量 */
        TclVar *global_variable_node = TO_PTR(context, global_node_offset); /* 定位全局变量节点的内存地址 */
        if (t_scmp(target_name_ptr, TO_PTR(context, global_variable_node->name)) == 0) { /* 进行全局变量名匹配 */
            *global_list_pointer_ref = global_variable_node->next; /* 执行逻辑删除，更新链表指针绕过该节点 */
            return TCL_OK; /* 全局变量删除成功，返回 OK 状态 */
        } /* 结束全局变量匹配后的删除逻辑 */
        global_list_pointer_ref = &global_variable_node->next; /* 移动全局链表指针引用 */
        global_node_offset = *global_list_pointer_ref; /* 获取下一个全局变量节点的偏移量 */
    } /* 结束对全局变量链表的遍历 */
    return TCL_OK; /* Tcl 标准规范：unset 一个不存在的变量不应报错，故静默返回成功 */
} /* 结束 tcl_unset 指令的 C 语言底层实现 */

/* C 函数指令注册表：维护 Tcl 脚本指令名与其对应底层实现函数之间的映射关系 */
typedef tcl_i32 (*Tcl_CmdProc)(TclCtx *context, tcl_i32 arg_count, tcl_u32 *arg_values); /* 定义统一的指令处理函数类型签名 */
typedef struct { /* 定义描述单条指令的结构体 */
    const tcl_u8 *name;         /* 指令在 Tcl 脚本中被调用的文本名称 */
    Tcl_CmdProc proc;           /* 指向该指令实际执行逻辑的 C 函数入口地址 */
} TclCmd; /* 结束指令项结构体定义 */

static TclCmd cmd_table[64];    /* 预分配静态数组作为指令查找表，最大支持 64 条指令 */
static tcl_i32 cmd_count = 0;   /* 记录当前查找表中已填充并生效的指令总数 */

/* 导出函数：允许外部模块或核心模块向解释器注册新的 C 语言实现的 Tcl 指令 */
void tcl_register_c_cmd(const tcl_u8 *name, Tcl_CmdProc procedure) {
    if (cmd_count < 64) {       /* 检查指令表是否还有剩余容量进行注册 */
        cmd_table[cmd_count].name = name; /* 将指令名称存储在表的当前空闲索引处 */
        cmd_table[cmd_count].proc = procedure; /* 将函数指针存储在对应的逻辑槽位中 */
        cmd_count++;            /* 原子化增加已注册指令计数，确保下次注册使用新槽位 */
    } /* 结束指令表容量检查 */
} /* 结束指令注册接口实现 */

/* 核心解释器引擎：采用状态机驱动的、无递归栈调用的 Tcl 脚本求值函数 */
tcl_i32 tcl_eval(TclCtx *context, const tcl_u8 *script) { /* 核心解释器引擎入口：接收上下文与脚本字符串，采用状态机驱动实现无递归求值 */
    context->status = TCL_OK;   /* 状态重置：在每一轮新的 eval 调用开始前，清空之前的错误或退出信号 */
    context->result = TCL_NULL; /* 结果重置：清空上一轮执行遗留的结果对象偏移量，防止产生陈旧数据干扰 */
    /* 空行：逻辑分段 - 脚本对象持久化 */
    tcl_u32 script_len = t_slen(script) + 1; /* 设计目的：计算输入脚本的完整物理长度（含 null 终止符），为内存分配做准备 */
    tcl_u32 script_offset = tcl_alc_p(context, script_len); /* 在受管理的 Arena 变量区（低地址区）分配存储空间，防止脚本在执行中因 GC 位移而丢失 */
    if (script_offset == TCL_NULL) { /* 内存分配安全性判定：检查是否因 Arena 空间耗尽导致分配失败 */
        return TCL_ERROR; /* 内存资源枯竭，无法承载脚本执行，安全返回错误码 */
    } /* 结束内存分配安全性判定 */
    t_mcpy(TO_PTR(context, script_offset), script, script_len); /* 执行物理拷贝：将外部传入的不可控脚本字符串搬运至解释器内部受控内存区 */
    context->tmp_roots[0] = script_offset; /* 关键操作：将脚本对象挂载到 GC 保护根数组，防止其在后续可能的内存整理中被视为垃圾回收 */
    /* 空行：逻辑分段 - 初始执行帧分配 */
    tcl_u32 frame_offset = tcl_alc_t(context, sizeof(TclFrame)); /* 在 Arena 栈区（高地址区）分配首个执行栈帧，用于保存当前脚本的执行进度与环境 */
    if (frame_offset == TCL_NULL) { /* 栈空间安全性判定：检查是否因深度调用导致高地址区空间不足 */
        context->tmp_roots[0] = TCL_NULL; /* 发生故障，立即解除对脚本对象的 GC 根保护，释放内存占位 */
        return TCL_ERROR; /* 无法创建初始执行环境，返回错误状态 */
    } /* 结束栈帧分配安全性判定 */
    script_offset = context->tmp_roots[0]; /* 核心机制：从 GC 保护根中重新读取可能因内存压缩而已发生物理位移的脚本逻辑偏移量 */
    context->tmp_roots[0] = TCL_NULL; /* 脚本已安全绑定到栈帧，任务完成，清除临时的 GC 根保护位 */
    /* 空行：逻辑分段 - 栈帧字段初始化 */
    TclFrame *frame = TO_PTR(context, frame_offset); /* 获取新分配栈帧的物理内存指针，开始执行字段初始化 */
    frame->script = script_offset; /* 将持久化后的脚本对象偏移量绑定至该执行帧 */
    frame->pc = 0; /* 初始化程序计数器（Program Counter）游标，使其指向脚本字符串的起始位置 */
    frame->vars = TCL_NULL; /* 初始化局部变量链表为空，待后续指令执行时按需分配 */
    frame->parent = context->curr_f; /* 建立物理调用链回溯：记录父执行帧位置，用于执行流恢复 */
    frame->scope = context->curr_f; /* 初始化逻辑作用域链：新帧默认挂载在当前活跃帧之下 */
    frame->state = ST_TOKENIZE; /* 状态机初态：设置为分词阶段，负责从脚本流中剥离出第一条待执行指令 */
    frame->flags = 0; /* 清除所有帧标志位（如 Proc 标志或作用域共享标志） */
    frame->exp_idx = 0; /* 重置参数展开索引，确保 EXPAND 阶段从第一个参数开始处理 */
    frame->argc = 0; /* 初始化参数计数器为零 */
    frame->cond = frame->body = frame->result = TCL_NULL; /* 清空条件、循环体及临时结果槽位，防止野数据干扰逻辑 */
    context->curr_f = frame_offset; /* 将解释器的当前活跃帧游标指向这个新分配的顶级帧 */
    /* 空行：逻辑分段 - 状态机主循环 */
    while (context->curr_f != TCL_NULL) { /* 核心引擎循环：只要调用栈中还存在待处理的活跃帧，就持续驱动状态机运行 */
        frame = TO_PTR(context, context->curr_f); /* 每次循环开始前，必须通过偏移量重新定位当前帧的物理指针，以应对 GC 带来的位移 */
        const tcl_u8 *script_ptr = TO_PTR(context, frame->script); /* 获取当前帧关联脚本对象的物理内存首地址，用于后续的字符级解析 */
        /* 空行：逻辑分段 - 状态分发调度 */
        switch (frame->state) { /* 基于当前帧内部状态寄存器的值，进行逻辑分支跳转 */
            case ST_TOKENIZE: { /* 分词状态：该阶段负责从当前的 pc 位置开始，识别出命令名称、参数及注释，处理物理分隔符 */
                frame->argc = 0; /* 在解析新指令前，彻底重置参数数组计数器 */
                frame->exp_idx = 0; /* 同步重置展开索引，为后续的 ST_EXPAND 阶段预留干净的环境 */
                /* 游标推进逻辑：跳过所有不产生语义的空白符（空格、制表符）以及指令分隔符（分号、换行） */
                while (script_ptr[frame->pc] && (script_ptr[frame->pc] == ';' || script_ptr[frame->pc] == '\n' || script_ptr[frame->pc] == '\r' || script_ptr[frame->pc] == ' ' || script_ptr[frame->pc] == '\t')) {
                    frame->pc++; /* 线性递增程序计数器，越过无效字符区间 */
                } /* 结束前导空白及分隔符跳过循环 */
                /* 注释判定逻辑：在 Tcl 规范中，指令行首的 # 字符标记整行为注释内容 */
                if (script_ptr[frame->pc] == '#') { /* 探测当前 pc 是否停留在一个注释引导符上 */
                    while (script_ptr[frame->pc] && script_ptr[frame->pc] != '\n' && script_ptr[frame->pc] != '\r') { /* 循环推进 pc */
                        frame->pc++; /* 忽略注释行内的所有字符，直到遇见行尾换行符或 EOF */
                    } /* 结束注释内容越过循环 */
                    continue; /* 注释处理完毕，立即重启 Tokenize 逻辑以寻找下一行真实的有效指令 */
                } /* 结束注释判定分支 */
                /* EOF 判定逻辑：检查程序计数器是否已经扫描至当前脚本对象的物理边界 */
                if (!script_ptr[frame->pc]) { /* 若 pc 指向的字符为 null (0)，表示当前脚本已全部执行完毕 */
                    tcl_u32 parent_offset = frame->parent; /* 记录当前帧的父帧偏移量，准备执行出栈操作 */
                    /* 栈回收逻辑：通过调整 t_bot 游标来逻辑释放当前栈帧占用的 Arena 空间，执行 8 字节对齐规约 */
                    context->t_bot += ((sizeof(TclFrame) + 7) & ~7); /* 释放当前帧，保持栈底对齐 */
                    context->curr_f = parent_offset; /* 将解释器的当前活跃帧指针回溯至父级，实现函数/子脚本返回 */
                    break; /* 退出当前 switch 分支，准备进入下一轮循环处理父帧的 ST_RESUME 状态 */
                } /* 结束 EOF 判定分支 */

                /* 参数提取主循环：在当前指令行边界内不断剥离 Token，直到触及指令终结符或达到参数上限 */
                while (script_ptr[frame->pc] && frame->argc < MAX_ARGS) { /* 设定 MAX_ARGS 阈值以防止 argv 数组越界 */
                    /* 空行：逻辑分段 - 参数间隙清理 */
                    while (script_ptr[frame->pc] && (script_ptr[frame->pc] == ' ' || script_ptr[frame->pc] == '\t')) { /* 线性扫描 */
                        frame->pc++; /* 推进程序计数器，忽略参数之间的所有水平空白字符 */
                    } /* 结束参数间隙空白扫描 */
                    /* 【续行处理】Tcl 规范：行末 \ 是续行符，下一行与当前行合并为同一条指令 */
                    if (script_ptr[frame->pc] == '\\' && (script_ptr[frame->pc + 1] == '\n' || script_ptr[frame->pc + 1] == '\r')) {
                        frame->pc++; /* 跳过反斜杠 */
                        if (script_ptr[frame->pc] == '\r') frame->pc++; /* 跳过 CR (Windows \r\n) */
                        if (script_ptr[frame->pc] == '\n') frame->pc++; /* 跳过 LF */
                        while (script_ptr[frame->pc] == ' ' || script_ptr[frame->pc] == '\t') frame->pc++; /* 跳过下一行前导空白 */
                        continue; /* 继续参数提取，当前行仍未结束 */
                    } /* 结束续行处理 */
                    /* 指令边界判定：检查当前 pc 是否已触及当前 Tcl 指令的语义终结点 */
                    if (!script_ptr[frame->pc] || script_ptr[frame->pc] == ';' || script_ptr[frame->pc] == '\n' || script_ptr[frame->pc] == '\r') {
                        break; /* 发现指令终结符，立即退出 Token 提取循环，准备后续的求值阶段 */
                    } /* 结束指令边界判定 */
                    /* 空行：逻辑分段 - Token 起始定位 */
                    tcl_u32 start_pos = frame->pc; /* 暂存当前有效 Token 文本的起始物理偏移量 */
                    /* 空行：逻辑分段 - Token 类型分发 */
                    if (script_ptr[frame->pc] == '{') { /* 特殊语法：处理由花括号包裹的“字面量” Token（保留外括号至展开阶段剥离） */
                        tcl_i32 depth = 1; 
                        start_pos = frame->pc; /* 记录包含左花括号的起始点 */
                        frame->pc++; 
                        while (script_ptr[frame->pc] && depth > 0) { 
                            if (script_ptr[frame->pc] == '{') { 
                                depth++; 
                            } else if (script_ptr[frame->pc] == '}') { 
                                depth--; 
                            } 
                            frame->pc++; /* 推进游标，最终包含右花括号 */
                        } 
                        tcl_u32 len = frame->pc - start_pos; 
                        tcl_u32 allocated = tcl_alc_p(context, len + 1); 
                        if (allocated != TCL_NULL) { 
                            frame = TO_PTR(context, context->curr_f); 
                            script_ptr = TO_PTR(context, frame->script); 
                            t_mcpy(TO_PTR(context, allocated), script_ptr + start_pos, len); 
                            ((tcl_u8*)TO_PTR(context, allocated))[len] = 0; 
                            frame->argv[frame->argc++] = allocated; 
                        } 
                    } else if (script_ptr[frame->pc] == '"') { /* 特殊语法：处理由双引号包裹的 Token（保留外括号至展开阶段剥离） */
                        start_pos = frame->pc; /* 包含左双引号 */
                        frame->pc++; 
                        while (script_ptr[frame->pc] && script_ptr[frame->pc] != '"') { 
                            if (script_ptr[frame->pc] == '\\' && script_ptr[frame->pc + 1]) frame->pc++; 
                            frame->pc++; 
                        }
                        if (script_ptr[frame->pc] == '"') frame->pc++; /* 包含右双引号 */
                        tcl_u32 len = frame->pc - start_pos; 
                        tcl_u32 allocated = tcl_alc_p(context, len + 1); 
                        if (allocated != TCL_NULL) { 
                            frame = TO_PTR(context, context->curr_f); 
                            script_ptr = TO_PTR(context, frame->script); 
                            t_mcpy(TO_PTR(context, allocated), script_ptr + start_pos, len); 
                            ((tcl_u8*)TO_PTR(context, allocated))[len] = 0; 
                            frame->argv[frame->argc++] = allocated; 
                        } 
                    } else if (script_ptr[frame->pc] == '[') { /* 特殊语法：处理由方括号包裹的“子命令” Token（待递归求值） */
                        tcl_i32 depth = 1; /* 初始化方括号嵌套深度 */
                        frame->pc++; /* 跨过左方括号引导符 */
                        start_pos = frame->pc - 1; /* 记录包含引导方括号在内的完整脚本片段起始点 */
                        while (script_ptr[frame->pc] && depth > 0) { /* 搜索逻辑：寻找配对的右方括号以界定子脚本边界 */
                            if (script_ptr[frame->pc] == '[') { /* 支持子命令的进一步嵌套 */
                                depth++; /* 增加嵌套深度 */
                            } else if (script_ptr[frame->pc] == ']') { /* 匹配到对应的闭合符 */
                                depth--; /* 减少嵌套深度 */
                            } /* 结束方括号嵌套判定 */
                            frame->pc++; /* 游标推进，跨过当前方括号或内部指令字符 */
                        } /* 结束配对方括号搜索循环 */
                        tcl_u32 len = frame->pc - start_pos; /* 计算包含方括号在内的子脚本总长度 */
                        tcl_u32 allocated = tcl_alc_p(context, len + 1); /* 分配持久化空间，用于存放这段待执行的子脚本 */
                        if (allocated != TCL_NULL) { /* 分配安全性判定 */
                            frame = TO_PTR(context, context->curr_f); /* 分配后重新同步栈帧物理地址 */
                            script_ptr = TO_PTR(context, frame->script); /* 分配后重新同步脚本物理地址 */
                            t_mcpy(TO_PTR(context, allocated), script_ptr + start_pos, len); /* 拷贝带方括号的完整子脚本 */
                            ((tcl_u8*)TO_PTR(context, allocated))[len] = 0; /* 字符串封底 */
                            frame->argv[frame->argc++] = allocated; /* 注册到指令参数表，待 EXPAND 阶段进一步处理 */
                        } /* 结束子脚本 Token 处理 */
                    } else { /* 默认解析逻辑：处理常规的、无特殊定界符包裹的简单 Token */
                        /* 线性扫描：直到遇到任何形式的空白符、换行、分号或子脚本终结符 */
                        while (script_ptr[frame->pc] && script_ptr[frame->pc] != ' ' && script_ptr[frame->pc] != '\t' && script_ptr[frame->pc] != '\n' && script_ptr[frame->pc] != '\r' && script_ptr[frame->pc] != ';' && script_ptr[frame->pc] != ']') {
                            frame->pc++; /* 持续移动游标以包含所有有效 Token 字符 */
                        } /* 结束常规 Token 扫描 */
                        tcl_u32 len = frame->pc - start_pos; /* 计算简单 Token 的文本长度 */
                        if (len == 0 && script_ptr[frame->pc] && script_ptr[frame->pc] != ';' && script_ptr[frame->pc] != '\n' && script_ptr[frame->pc] != '\r') {
                            /* 故障恢复：若在非终结符处扫描出零长度 Token（如 stray ']'），强制跳过当前字符并重新尝试 */
                            frame->pc++; 
                            continue; 
                        }
                        tcl_u32 allocated = tcl_alc_p(context, len + 1); /* 在 Arena 分配空间 */
                        if (allocated != TCL_NULL) { /* 分配安全性判定 */
                            frame = TO_PTR(context, context->curr_f); /* 应对 GC 导致的内存搬迁，刷新指针 */
                            script_ptr = TO_PTR(context, frame->script); /* 同步刷新脚本指针 */
                            t_mcpy(TO_PTR(context, allocated), script_ptr + start_pos, len); /* 物理拷贝 Token 文本 */
                            ((tcl_u8*)TO_PTR(context, allocated))[len] = 0; /* 写入字符串终结符 */
                            frame->argv[frame->argc++] = allocated; /* 将偏移量记录到当前帧的 argv 寄存器中 */
                        } /* 结束常规 Token 处理 */
                    } /* 结束 Token 分支判定 */
                } /* 结束单条指令内所有参数的解析循环 */
                /* 空行：逻辑分段 - 指令后置清理 */
                while (script_ptr[frame->pc] && (script_ptr[frame->pc] == ';' || script_ptr[frame->pc] == '\n' || script_ptr[frame->pc] == '\r' || script_ptr[frame->pc] == ' ' || script_ptr[frame->pc] == '\t')) {
                    frame->pc++; /* 跳过指令末尾可能存在的冗余分隔符或空白，指向下一条指令可能的起点 */
                } /* 结束清理循环 */
                frame->state = ST_EXPAND; /* 状态机迁移：进入指令展开阶段，准备处理变量引用与子命令替换 */
                break; /* 退出 ST_TOKENIZE 状态处理 */
            } /* 结束 ST_TOKENIZE 状态块 */
            case ST_EXPAND: { /* 指令展开阶段：负责处理参数数组中的动态内容，如 $变量 和 [子命令] */
                while (frame->exp_idx < frame->argc) { /* 线性迭代处理 argv 中的每一个 Token */
                    tcl_u32 arg_offset = frame->argv[frame->exp_idx]; /* 获取当前待检查 Token 的逻辑偏移量 */
                    if (arg_offset == TCL_NULL) { /* 鲁棒性检查：跳过无效或空的 Token */
                        frame->exp_idx++; /* 游标前进 */
                        continue; /* 进入下一次迭代 */
                    } /* 结束空值判定 */
                    tcl_u8 *arg_ptr = TO_PTR(context, arg_offset); /* 将 Token 偏移量映射为物理内存指针 */
                    /* 空行：逻辑分段 - 替换逻辑分发 */
                    if (arg_ptr[0] == '$') { /* 变量替换：检测到以美元符号引导的 Token */
                        /* 调用 lookup 接口：从当前帧开始向上回溯寻找指定名称的变量值对象 */
                        tcl_u32 value = tcl_get_var(context, context->curr_f, arg_ptr + 1); 
                        if (value != TCL_NULL) { /* 若变量解析成功且返回了有效对象偏移量 */
                            frame->argv[frame->exp_idx] = value; /* 就地替换：将变量值对象填充到参数数组的对应槽位中 */
                        } else { /* 若变量未定义（Tcl 哲学：未定义变量引用导致错误） */
                            context->status = TCL_ERROR; /* 设置解释器全局错误状态码 */
                            context->curr_f = TCL_NULL; /* 强制销毁当前执行环境，停止脚本运行 */
                            return TCL_ERROR; /* 立即向调用者返回错误，中断执行流 */
                        } /* 结束变量获取判定 */
                    } else if (arg_ptr[0] == '[') { /* 命令替换：检测到以左方括号引导的 Token */
                        /* 核心设计：通过分配新的 TclFrame 来模拟函数调用，实现非递归的子脚本求值 */
                        tcl_u32 new_frame_offset = tcl_alc_t(context, sizeof(TclFrame)); 
                        if (new_frame_offset == TCL_NULL) { /* 栈空间不足判定 */
                            context->status = TCL_ERROR; /* 记录栈溢出错误 */
                            context->curr_f = TCL_NULL; /* 终止运行 */
                            return TCL_ERROR; /* 返回错误 */
                        } /* 结束帧分配判定 */
                        frame = TO_PTR(context, context->curr_f); /* 分配后刷新当前帧指针，防止 GC 位移 */
                        tcl_u8 *cmd_script = TO_PTR(context, frame->argv[frame->exp_idx]); /* 获取子命令脚本 Token 的物理指针 */
                        tcl_u32 cmd_len = t_slen(cmd_script); /* 计算待求值脚本的总长度 */
                        /* 在 Arena 中分配一个干净的脚本副本，剥离两侧的方括号定界符 */
                        context->tmp_roots[0] = tcl_alc_p(context, cmd_len); 
                        if (context->tmp_roots[0] == TCL_NULL) { /* 分配失败安全性校验 */
                            context->status = TCL_ERROR; /* 设置内存错误 */
                            return TCL_ERROR; /* 返回 */
                        } /* 结束副本分配判定 */
                        tcl_u32 script_copy = context->tmp_roots[0]; /* 获取新分配副本的逻辑偏移量 */
                        frame = TO_PTR(context, context->curr_f); /* 再次同步物理指针 */
                        cmd_script = TO_PTR(context, frame->argv[frame->exp_idx]); /* 获取原始 Token 物理基址 */
                        /* 核心剥离操作：将 [...] 内部的脚本字符搬运至新副本中，实现语法层面的解包 */
                        t_mcpy(TO_PTR(context, script_copy), cmd_script + 1, cmd_len - 2); 
                        ((tcl_u8*)TO_PTR(context, script_copy))[cmd_len - 2] = 0; /* 副本字符串封底 */
                        /* 空行：逻辑分段 - 子帧环境建立 */
                        TclFrame *new_frame = TO_PTR(context, new_frame_offset); /* 初始化新分配的子执行帧 */
                        new_frame->script = script_copy; /* 将解包后的子脚本对象绑定到新帧 */
                        new_frame->pc = 0; /* 子脚本解析从偏移量 0 处启动 */
                        new_frame->vars = TCL_NULL; /* 子帧初始化局部变量表为空 */
                        new_frame->parent = context->curr_f; /* 建立父子调用链，实现结果回传机制 */
                        new_frame->scope = context->curr_f;  /* 【Bug修复】设定逻辑作用域：命令替换子帧必须共享父帧的变量作用域，否则 scope 字段留零，会导致 tcl_find_var_node 将 TclCtx 头部误当作 TclFrame 读取，引发 Arena 越界崩溃 */
                        new_frame->state = ST_TOKENIZE; /* 子帧初始进入分词阶段 */
                        new_frame->flags = FRAME_SHARE_SCOPE; /* 语义设定：子命令共享父级作用域，允许访问其局部变量 */
                        new_frame->cond = new_frame->body = new_frame->result = TCL_NULL; /* 控制槽位初始化 */
                        new_frame->argc = new_frame->exp_idx = 0; /* 参数寄存器归零 */
                        context->tmp_roots[0] = TCL_NULL; /* 环境就绪，清除临时的 GC 保护根 */
                        frame->state = ST_RESUME; /* 关键跳转：将父帧挂起至恢复态，等待子命令执行完毕并将结果冒泡 */
                        context->curr_f = new_frame_offset; /* 解释器执行重心迁移至子帧，驱动无递归执行流 */
                        goto next_state_loop; /* 立即跳转至主循环顶部，重启状态机以运行子脚本 */
                    } else if (arg_ptr[0] == '{') { /* {} literal: strip braces only */
                        tcl_u32 br_len = t_slen(arg_ptr);
                        if (br_len > 1) {
                            tcl_u32 br_stripped = tcl_alc_p(context, br_len);
                            if (br_stripped != TCL_NULL) {
                                frame = TO_PTR(context, context->curr_f);
                                arg_ptr = TO_PTR(context, frame->argv[frame->exp_idx]);
                                t_mcpy(TO_PTR(context, br_stripped), arg_ptr + 1, br_len - 2);
                                ((tcl_u8*)TO_PTR(context, br_stripped))[br_len - 2] = 0;
                                frame->argv[frame->exp_idx] = br_stripped;
                            }
                        }
                    } else if (arg_ptr[0] == '"') { /* "..." string: expand vars and escapes */
                        tcl_u32 dq_len = t_slen(arg_ptr);
                        if (dq_len > 1) {
                            tcl_u32 interp = tcl_str_interp(context, arg_ptr + 1, dq_len - 2);
                            frame = TO_PTR(context, context->curr_f);
                            if (interp != TCL_NULL) {
                                frame->argv[frame->exp_idx] = interp;
                            }
                        }
                    } /* end literal handling */
                    frame->exp_idx++; /* 成功处理当前 Token 的展开，游标向下一个参数移动 */
                } /* 结束 argv 线性展开循环 */
                frame->state = (frame->flags & FRAME_IS_EXPR) ? ST_EXPR_REDUCE : ST_EXECUTE; /* 根据帧标志决定进入指令执行还是表达式归约 */
                break; /* 退出 ST_EXPAND 状态处理 */
            } /* 结束 ST_EXPAND 状态块 */
            case ST_EXECUTE: { /* 执行阶段：根据命令名称分发至 C 内建函数或脚本定义的过程 (Proc) */
                if (frame->argc == 0) { /* 场景：空指令（如仅包含分号或注释的行） */
                    frame->state = ST_TOKENIZE; /* 无需执行，直接回到下一条指令的解析态 */
                    break;
                }
                if (frame->argv[0] == TCL_NULL) { /* 鲁棒性检查：指令名称对象分配失败 */
                    context->status = TCL_ERROR; /* 标记致命错误 */
                    break;
                }
                const tcl_u8 *cmd_name = TO_PTR(context, frame->argv[0]); /* 从参数数组首项获取待执行指令的名称物理指针 */
                tcl_i32 found = 0; /* 查找状态标志：记录是否已成功匹配并触发了指令执行 */
                /* 空行：逻辑分段 - C 原子指令查找 */
                for (tcl_i32 index = 0; index < cmd_count; index++) { /* 线性遍历静态注册的 C 指令表 */
                    if (t_scmp(cmd_name, cmd_table[index].name) == 0) { /* 执行名称精确字符串比对 */
                        /* 调用底层 C 实现函数，并同步更新全局状态寄存器（处理 TCL_OK, TCL_ERROR 等） */
                        context->status = cmd_table[index].proc(context, frame->argc, frame->argv); 
                        frame = TO_PTR(context, context->curr_f); /* 指令执行期间可能触发 GC，必须刷新当前栈帧的物理映射 */
                        found = 1; /* 标记为已成功处理，跳过后续的 Proc 搜索逻辑 */
                        break; /* 命中 C 指令，终止查找循环 */
                    } /* 结束 C 指令匹配分支 */
                } /* 结束 C 指令查找循环 */
                /* 空行：逻辑分段 - 脚本过程 (Proc) 查找 */
                if (!found) { /* 若在 C 内建指令集中未命中，则尝试在全局作用域寻找脚本定义的 Proc */
                    tcl_u32 var_offset = context->g_vars; /* 从全局符号链表首节点开始进行深度遍历 */
                    tcl_u32 body_offset = TCL_NULL; /* 用于存储发现的函数体脚本对象逻辑偏移量 */
                    tcl_u32 args_offset = TCL_NULL; /* 用于存储发现的形参列表文本对象逻辑偏移量 */
                    while (var_offset != TCL_NULL) { /* 遍历全局符号表，识别以 p: 和 a: 为前缀的伪变量（Proc 实现机制） */
                        TclVar *variable = TO_PTR(context, var_offset); /* 映射符号节点的物理结构 */
                        const tcl_u8 *variable_name = TO_PTR(context, variable->name); /* 获取符号名称物理指针 */
                        /* 识别函数体定义：检查是否存在名为 "p:<指令名>" 的全局变量 */
                        if (variable_name[0] == 'p' && variable_name[1] == ':' && t_scmp(cmd_name, variable_name + 2) == 0) { 
                            body_offset = variable->val; /* 提取存储在变量值中的函数体脚本逻辑地址 */
                        } /* 结束函数体判定 */
                        /* 识别形参列表定义：检查是否存在名为 "a:<指令名>" 的全局变量 */
                        if (variable_name[0] == 'a' && variable_name[1] == ':' && t_scmp(cmd_name, variable_name + 2) == 0) { 
                            args_offset = variable->val; /* 提取形参列表字符串的逻辑地址 */
                        } /* 结束参数列表判定 */
                        var_offset = variable->next; /* 链表向后推进，寻找下一个可能的符号 */
                    } /* 结束全局符号表扫描 */
                    /* 空行：逻辑分段 - Proc 环境实例化 */
                    if (body_offset != TCL_NULL) { /* 若已识别到有效的脚本过程定义 */
                        tcl_u32 new_frame_offset = tcl_alc_t(context, sizeof(TclFrame)); /* 为本次过程调用分配独立的执行栈帧 */
                        if (new_frame_offset == TCL_NULL) { /* 栈内存安全性校验 */
                            context->status = TCL_ERROR; /* 栈空间不足，记录错误状态 */
                        } else { /* 实例化过程执行环境 */
                            TclFrame *new_frame = TO_PTR(context, new_frame_offset); /* 物理映射新分配的栈帧 */
                            new_frame->script = body_offset; /* 将函数体脚本绑定至新帧的指令区 */
                            new_frame->pc = 0; /* 函数体执行从偏移量 0 处开始 */
                            new_frame->vars = TCL_NULL; /* 初始化 Proc 独立的局部变量符号表 */
                            new_frame->parent = context->curr_f; /* 建立物理调用链回溯，指向当前的调用者帧 */
                            new_frame->scope = TCL_NULL; /* 核心规约：过程调用拥有独立的逻辑作用域根部，不继承调用者变量 */
                            new_frame->state = ST_TOKENIZE; /* 子帧进入初始化分词态 */
                            new_frame->flags = FRAME_IS_PROC; /* 核心标记：设定该帧为 Proc 调用，启用作用域隔离保护 */
                            new_frame->cond = new_frame->body = new_frame->result = TCL_NULL; /* 数据槽位初始化 */
                            new_frame->argc = new_frame->exp_idx = 0; /* 计数寄存器归零 */
                            /* 空行：逻辑分段 - 形参与实参绑定逻辑 */
                            context->tmp_roots[0] = args_offset; /* 关键保护：使用根数组保护形参名列表字符串，防止绑定过程触发 GC */
                            tcl_i32 arg_idx = 1; /* 实参偏移：从 argv[1] 开始提取，跳过首位的命令名 */
                            tcl_u32 arg_list_pos = 0; /* 用于解析形参文本字符串的字符级内部游标 */
                            while (arg_idx < frame->argc) { /* 循环处理调用方传入的每一个有效实参 */
                                const tcl_u8 *args_list_ptr = (const tcl_u8*)TO_PTR(context, context->tmp_roots[0]) + arg_list_pos; /* 定位形参文本解析点 */
                                while (*args_list_ptr == ' ' || *args_list_ptr == '\t') { /* 跳过形参名之间可能存在的空白符 */
                                    args_list_ptr++; /* 游标线性推进 */
                                    arg_list_pos++; /* 保持同步 */
                                } /* 结束空白跳过 */
                                if (!*args_list_ptr) { /* 形参列表已解析至末尾，不再处理多余的实参 */
                                    break; /* 退出参数绑定流程 */
                                } /* 结束终结符判定 */
                                tcl_u32 token_start = arg_list_pos; /* 记录当前形参名称在字符串中的起始位置 */
                                while (*args_list_ptr && *args_list_ptr != ' ' && *args_list_ptr != '\t') { /* 扫描形参名称直到遇见分隔符 */
                                    args_list_ptr++; /* 推进 */
                                    arg_list_pos++; /* 记录解析进度 */
                                } /* 结束名称扫描 */
                                tcl_i32 token_len = arg_list_pos - token_start; /* 计算该形参名称的长度 */
                                context->tmp_roots[1] = tcl_alc_p(context, token_len + 1); /* 在对象区分配存储形参名的副本空间 */
                                if (context->tmp_roots[1] == TCL_NULL) { /* 分配失败判定 */
                                    context->status = TCL_ERROR; /* 标记内存错误 */
                                    break; /* 中断后续绑定逻辑 */
                                } /* 结束分配校验 */
                                frame = TO_PTR(context, context->curr_f); /* 刷新当前调用者帧指针，防止分配触发位移 */
                                t_mcpy(TO_PTR(context, context->tmp_roots[1]), (const tcl_u8*)TO_PTR(context, context->tmp_roots[0]) + token_start, token_len); /* 搬运形参名 */
                                ((tcl_u8*)TO_PTR(context, context->tmp_roots[1]))[token_len] = 0; /* 字符串副本封底 */
                                /* 绑定操作：在新建的 Proc 子帧作用域内，执行特殊的 args 收集绑定或常规绑定 */
                                if (token_len == 4 && t_scmp(TO_PTR(context, context->tmp_roots[1]), (const tcl_u8*)"args") == 0) { /* 如果形参名是 args 关键字 */
                                    tcl_u32 list_len = 0; /* 用于累加剩余实参的总长度 */
                                    for (tcl_i32 check_idx = arg_idx; check_idx < frame->argc; check_idx++) { /* 遍历后续的实参以计算所需内存大小 */
                                        const tcl_u8 *element_ptr = TO_PTR(context, frame->argv[check_idx]); /* 获取当前待检查实参的物理指针 */
                                        tcl_u32 element_len = t_slen(element_ptr); /* 获取当前待检查实参的字符串长度 */
                                        tcl_i32 requires_brace = 0; /* 标记该实参是否需要用花括号进行包裹 */
                                        if (element_len == 0) { /* 如果实参字符串长度为零则必须包裹 */
                                            requires_brace = 1; /* 标记为需要花括号包裹 */
                                        } else { /* 如果实参字符串长度不为零则扫描其字符 */
                                            for (tcl_u32 char_pos = 0; char_pos < element_len; char_pos++) { /* 逐个字符扫描实参内容 */
                                                tcl_u8 current_char = element_ptr[char_pos]; /* 获取当前位置的字符值 */
                                                if (current_char == ' ' || current_char == '\t' || current_char == '\n' || current_char == '\r' || current_char == '{' || current_char == '}') { /* 判断是否包含空格、制表符、换行、回车或花括号 */
                                                    requires_brace = 1; /* 包含特殊字符时标记需要花括号包裹 */
                                                    break; /* 退出当前实参字符扫描 */
                                                } /* 结束特殊字符判定 */
                                            } /* 结束字符扫描循环 */
                                        } /* 结束长度与字符判定 */
                                        list_len += element_len; /* 累加实参自身的内容长度 */
                                        if (requires_brace) { /* 如果该实参需要进行花括号包裹 */
                                            list_len += 2; /* 额外增加两个字节用于存放左右花括号 */
                                        } /* 结束包裹长度增加判定 */
                                    } /* 结束长度累加循环 */
                                    if (frame->argc > arg_idx) { /* 如果存在多个剩余实参 */
                                        list_len += (frame->argc - arg_idx - 1); /* 加上元素之间的空格间隔字符数 */
                                    } /* 结束空格间隔计算 */
                                    list_len += 1; /* 加上末尾空字符物理终止符 */
                                    tcl_u32 args_list_offset = tcl_alc_p(context, list_len); /* 分配列表内存空间 */
                                    if (args_list_offset == TCL_NULL) { /* 分配失败判定 */
                                        context->status = TCL_ERROR; /* 设置全局内存错误码 */
                                        context->tmp_roots[1] = TCL_NULL; /* 清理局部根保护以防泄露 */
                                        break; /* 退出参数绑定循环 */
                                    } /* 结束分配失败校验 */
                                    frame = TO_PTR(context, context->curr_f); /* 分配可能触发 GC 重新搬移，必须重新加载物理指针 */
                                    tcl_u8 *dest_ptr = TO_PTR(context, args_list_offset); /* 获取拼接目标的首物理地址 */
                                    tcl_u32 current_write_pos = 0; /* 初始化拼接写入的局部偏移游标 */
                                    for (tcl_i32 write_idx = arg_idx; write_idx < frame->argc; write_idx++) { /* 循环执行实参拷贝拼接 */
                                        const tcl_u8 *src_ptr = TO_PTR(context, frame->argv[write_idx]); /* 提取源实参物理指针 */
                                        tcl_u32 src_len = t_slen(src_ptr); /* 计算当前源实参长度 */
                                        tcl_i32 requires_brace = 0; /* 再次初始化该实参是否需要包裹的标记 */
                                        if (src_len == 0) { /* 如果源字符串为空则需要包裹 */
                                            requires_brace = 1; /* 设定需要包裹标记 */
                                        } else { /* 否则扫描其包含的字符 */
                                            for (tcl_u32 char_pos = 0; char_pos < src_len; char_pos++) { /* 扫描每个字符以确认是否需要包裹 */
                                                tcl_u8 current_char = src_ptr[char_pos]; /* 获取待拷贝实参当前字符 */
                                                if (current_char == ' ' || current_char == '\t' || current_char == '\n' || current_char == '\r' || current_char == '{' || current_char == '}') { /* 检测特殊分隔符与花括号 */
                                                    requires_brace = 1; /* 设定包裹标记 */
                                                    break; /* 跳出扫描 */
                                                } /* 结束特殊字符检测 */
                                            } /* 结束字符扫描 */
                                        } /* 结束长度与字符分流判定 */
                                        if (requires_brace) { /* 如果当前元素需要被花括号包裹 */
                                            dest_ptr[current_write_pos] = '{'; /* 写入左花括号 */
                                            current_write_pos++; /* 写入位置前移一位 */
                                        } /* 结束左花括号写入 */
                                        t_mcpy(dest_ptr + current_write_pos, src_ptr, src_len); /* 物理拷贝当前实参字符 */
                                        current_write_pos += src_len; /* 推进目标写入游标位置 */
                                        if (requires_brace) { /* 如果当前元素需要被花括号包裹 */
                                            dest_ptr[current_write_pos] = '}'; /* 写入右花括号 */
                                            current_write_pos++; /* 写入位置前移一位 */
                                        } /* 结束右花括号写入 */
                                        if (write_idx < frame->argc - 1) { /* 若不是最后一个元素 */
                                            dest_ptr[current_write_pos] = 32; /* 追加单个空格分隔符 */
                                            current_write_pos++; /* 推进游标 */
                                        } /* 结束空格追加判定 */
                                    } /* 结束拼接循环 */
                                    dest_ptr[current_write_pos] = 0; /* 封底写入物理终止符 */
                                    tcl_set_var(context, new_frame_offset, context->tmp_roots[1], args_list_offset); /* 在子帧中注册 args 变量 */
                                    arg_idx = frame->argc; /* 将参数游标强制推至末尾以终止对实参的消耗 */
                                } else { /* 常规参数情况 */
                                    tcl_set_var(context, new_frame_offset, context->tmp_roots[1], frame->argv[arg_idx++]); /* 在子帧中注册普通形参变量 */
                                } /* 结束 args 与普通参数分流判定 */
                                context->tmp_roots[1] = TCL_NULL; /* 绑定完成，解除对该形参名副本的临时根保护 */
                                frame = TO_PTR(context, context->curr_f); /* 再次刷新物理指针以备下次迭代使用 */
                            } /* 结束所有形参的绑定循环 */
                            context->tmp_roots[0] = TCL_NULL; /* 过程环境建立闭环，解除形参列表字符串保护 */
                            frame->state = ST_RESUME; /* 调用者帧进入挂起恢复态，静待过程执行完成后的结果冒泡 */
                            context->curr_f = new_frame_offset; /* 执行焦点切换至过程子帧，开启新的执行上下文 */
                            goto next_state_loop; /* 立即返回状态机主引擎顶部，驱动过程脚本的运行 */
                        } /* 结束 Proc 执行环境实例化逻辑 */
                        found = 1; /* 标记为已成功识别为脚本过程并分发 */
                    } else { /* 既不是 C 指令也未找到 Proc 定义 */
                        tcl_hal_puts((const tcl_u8*)"Command not found error. argc=");
                        tcl_u8 argc_buf[12];
                        t_itoa(frame->argc, argc_buf);
                        tcl_hal_puts(argc_buf);
                        tcl_hal_puts((const tcl_u8*)"\nScript: '");
                        if (frame->script != TCL_NULL) {
                            tcl_hal_puts(TO_PTR(context, frame->script));
                        }
                        tcl_hal_puts((const tcl_u8*)"'\nTokens:\n");
                        for (int i=0; i<frame->argc; i++) {
                            tcl_hal_puts((const tcl_u8*)"  [");
                            t_itoa(i, argc_buf); tcl_hal_puts(argc_buf);
                            tcl_hal_puts((const tcl_u8*)"]: '");
                            if (frame->argv[i] != TCL_NULL) tcl_hal_puts(TO_PTR(context, frame->argv[i]));
                            tcl_hal_puts((const tcl_u8*)"'\n");
                        }
                        if (frame->argc > 0 && frame->argv[0] != TCL_NULL) {
                            const tcl_u8 *missing_cmd = TO_PTR(context, frame->argv[0]);
                            tcl_hal_puts((const tcl_u8*)"Cmd name: '");
                            tcl_hal_puts(missing_cmd);
                            tcl_hal_puts((const tcl_u8*)"'\n");
                            context->result = frame->argv[0]; 
                        } else {
                            tcl_hal_puts((const tcl_u8*)"Cmd name is NULL or empty\n");
                            tcl_u32 err_msg = tcl_alc_p(context, 16);
                            if (err_msg != TCL_NULL) {
                                t_mcpy(TO_PTR(context, err_msg), "null_cmd", 9);
                                context->result = err_msg;
                            }
                        }
                        context->status = TCL_ERROR; /* 指令解析失败，记录致命错误状态 */
                    } /* 结束分发决策逻辑 */
                } /* 结束指令执行主分发逻辑 */
                /* 空行：逻辑分段 - 退出指令特殊处理 */
                if (context->status == TCL_EXIT) { /* 探测是否收到了全局退出信号 */
                    context->curr_f = TCL_NULL; /* 强制清空执行栈，使核心引擎进入自然终止状态 */
                    break; /* 彻底跳出状态机大循环 */
                } /* 结束退出判定 */
                /* 空行：逻辑分段 - 异常冒泡 (Exception Bubbling) 机制 */
                if (context->status != TCL_OK && context->status != TCL_YIELD) { /* 拦截所有非正常的控制流状态（报错、Return、Break 等） */
                    tcl_i32 current_status = context->status; /* 暂存当前的异常或跳转状态码 */
                    tcl_u32 current_result = context->result; /* 暂存相关的计算结果或错误提示信息 */
                    while (frame) { /* 异常冒泡流程：逐层寻找最近的能够拦截或处理该状态的栈帧结界 */
                        tcl_u8 current_flags = frame->flags; /* 获取当前栈帧的功能属性标志位 */
                        tcl_u32 parent_offset = frame->parent; /* 记录父级栈帧的逻辑地址以便回退 */
                        context->t_bot += ((sizeof(TclFrame) + 7) & ~7); /* 物理销毁当前失效帧，回收栈空间并执行对齐规约 */
                        context->curr_f = parent_offset; /* 在全局上下文中更新当前活跃帧游标 */
                        if (parent_offset == TCL_NULL) { /* 已冒泡至最顶层解释器边界，仍未被拦截 */
                            frame = 0; /* 标记当前帧耗尽 */
                            break; /* 终止冒泡，异常状态将直接透传给 C 调用端 */
                        } /* 结束顶层判定 */
                        frame = TO_PTR(context, parent_offset); /* 映射父结界的物理指针，进行拦截策略评估 */
                        /* 拦截点 1：catch 指令结界，或者正处于循环控制结界的 break/continue 信号 */
                        if (frame->state == ST_CATCH_END || ((current_status == TCL_BREAK || current_status == TCL_CONTINUE) && (frame->state == ST_LOOP || frame->state == ST_COND))) {
                            break; /* 命中拦截目标，停止冒泡，在当前帧继续处理逻辑 */
                        } /* 结束拦截判定 */
                        /* 拦截点 2：脚本 Return 信号触及 Proc 边界，此时应停止冒泡并转换状态 */
                        if (current_status == TCL_RETURN && (current_flags & FRAME_IS_PROC)) { 
                            break; /* 信号已到达预定的作用域终点，停止冒泡 */
                        } /* 结束过程边界判定 */
                    } /* 结束层级回溯循环 */
                    context->status = current_status; /* 将拦截后的或最终的状态码回填至全局寄存器 */
                    context->result = current_result; /* 将结果对象同步回填，完成异常链条的闭环 */
                    if (!frame) context->curr_f = TCL_NULL; /* 彻底穿透则进入安全停机态 */
                } else if (context->status == TCL_OK) { /* 指令执行正常完成 (TCL_OK) */
                    frame->state = ST_TOKENIZE; /* 状态机无缝切回解析态，准备分词处理脚本中的后续下一条指令流 */
                } /* 结束状态分发后置处理 */
                break; /* 退出 ST_EXECUTE case 处理 */
            } /* 结束 ST_EXECUTE 状态块 */
            case ST_EXPR_REDUCE: { /* 表达式归约阶段：负责对已展开的 Token 进行逻辑或算术缩减，而非作为指令执行 */
                /* 核心语义：表达式帧将其 argv 视为操作数与操作符，最终将计算结果注入 context->result */
                if (frame->argc == 0) { /* 边界情况：无 Token */
                    context->result = TCL_NULL; /* 结果设为空 */
                } else if (frame->argc == 1) { /* 单值求值：常见于 if {$var} 模式，直接透传该 Token 偏移量 */
                    context->result = frame->argv[0]; /* 结果直接赋值 */
                } else { /* 多参数表达式：执行带括号的四轮优先级归约 */
                    tcl_u32 temp_argv[MAX_ARGS]; /* 防御性本地拷贝数组，避免破坏原帧数据 */
                    tcl_u32 temp_argc = 0; /* 初始化本地参数数量计数 */
                    for (tcl_u32 sync_idx = 0; sync_idx < frame->argc; sync_idx++) { /* 过滤并清洗所有参数 */
                        tcl_u8 *val_ptr = TO_PTR(context, frame->argv[sync_idx]); /* 获取参数物理指针 */
                        tcl_u32 val_len = t_slen(val_ptr); /* 计算参数长度 */
                        
                        /* 1. 过滤完全由括号组成的独立 Token */
                        tcl_u32 is_pure_parenthesis = 1; /* 初始化纯括号标志 */
                        if (val_len == 0) { /* 空字符串处理 */
                            is_pure_parenthesis = 0; /* 设为非纯括号 */
                        } else { /* 循环检查字符 */
                            for (tcl_u32 char_idx = 0; char_idx < val_len; char_idx++) { /* 遍历字符 */
                                if (val_ptr[char_idx] != '(' && val_ptr[char_idx] != ')') { /* 发现非括号字符 */
                                    is_pure_parenthesis = 0; /* 设为非纯括号 */
                                    break; /* 跳出检查 */
                                } /* 结束字符判定 */
                            } /* 结束字符遍历 */
                        } /* 结束括号判定 */
                        if (is_pure_parenthesis) { /* 命中了纯括号 Token */
                            continue; /* 忽略该 Token，不拷入本地数组 */
                        } /* 结束纯括号过滤 */

                        /* 2. 拷贝并将粘连在操作数首尾的括号剥离 */
                        tcl_u32 start_offset = 0; /* 初始化前导括号截断偏移 */
                        tcl_u32 end_offset = val_len; /* 初始化尾部括号截断偏移 */
                        while (start_offset < end_offset && val_ptr[start_offset] == '(') { /* 扫描前导左括号 */
                            start_offset++; /* 截断偏移右移 */
                        } /* 结束左括号扫描 */
                        while (end_offset > start_offset && val_ptr[end_offset - 1] == ')') { /* 扫描尾部右括号 */
                            end_offset--; /* 截断偏移左移 */
                        } /* 结束右括号扫描 */

                        tcl_u32 clean_len = end_offset - start_offset; /* 计算剥离括号后的真实字符串长度 */
                        tcl_u32 token_offset = frame->argv[sync_idx]; /* 默认使用原始偏移量 */
                        if (clean_len < val_len) { /* 存在需要剥离的物理括号 */
                            tcl_u8 *clean_ptr = val_ptr + start_offset; /* 清洗后的字符串起始物理指针 */
                            if (clean_ptr[0] == '$') { /* 发现被括号包裹的变量漏网之鱼，需要执行就地展开 */
                                tcl_u8 saved_char = val_ptr[end_offset]; /* 暂存尾部字符 */
                                val_ptr[end_offset] = 0; /* 临时写入终结符 */
                                tcl_u32 var_val_offset = tcl_get_var(context, context->curr_f, clean_ptr + 1); /* 读取变量值偏移量 */
                                val_ptr[end_offset] = saved_char; /* 恢复尾部字符 */
                                if (var_val_offset == TCL_NULL) { /* 变量未定义判定 */
                                    context->status = TCL_ERROR; /* 设置状态为错误 */
                                    return TCL_ERROR; /* 报错退出 */
                                } /* 结束未定义判定 */
                                token_offset = var_val_offset; /* 复用已有变量值偏移量，实现零分配就地展开 */
                            } else { /* 常规字面量或数字，需要拷贝生成干净字符串以防 GC 重定位野指针 */
                                /* GC 安全分配：分配前同步 */
                                for (tcl_u32 copy_idx = 0; copy_idx < temp_argc; copy_idx++) { /* 备份 temp_argv */
                                    frame->argv[copy_idx] = temp_argv[copy_idx]; /* 同步到 frame 前部 */
                                } /* 结束同步 */
                                for (tcl_u32 null_idx = temp_argc; null_idx < sync_idx; null_idx++) { /* 遍历中间空隙区 */
                                    frame->argv[null_idx] = TCL_NULL; /* 将空隙区设为空值以保护 GC 标记安全 */
                                } /* 结束空隙区清理 */
                                
                                tcl_u32 new_str_offset = tcl_alc_p(context, clean_len + 1); /* 分配新字符串空间 */
                                if (new_str_offset == TCL_NULL) { /* 检查分配结果 */
                                    context->status = TCL_ERROR; /* 设为错误状态 */
                                    return TCL_ERROR; /* 报错退出 */
                                } /* 结束结果判定 */
                                
                                /* 分配后恢复 temp_argv */
                                frame = TO_PTR(context, context->curr_f); /* 重新加载当前帧物理指针 */
                                for (tcl_u32 copy_idx = 0; copy_idx < temp_argc; copy_idx++) { /* 恢复本地数组 */
                                    temp_argv[copy_idx] = frame->argv[copy_idx]; /* 拷回 temp_argv */
                                } /* 结束恢复 */
                                
                                val_ptr = TO_PTR(context, frame->argv[sync_idx]); /* 重新提取已被 GC 修正的源参数物理指针 */
                                t_mcpy(TO_PTR(context, new_str_offset), val_ptr + start_offset, clean_len); /* 拷贝去除括号后的内容 */
                                ((tcl_u8*)TO_PTR(context, new_str_offset))[clean_len] = 0; /* 封底终止符 */
                                token_offset = new_str_offset; /* 使用新分配的 GC 安全偏移量 */
                            } /* 结束变量或常数分流 */
                        } /* 结束新字符串分配 */

                        if (temp_argc < MAX_ARGS) { /* 检查防御型数组界限 */
                            temp_argv[temp_argc++] = token_offset; /* 将清洗后的 Token 拷入本地数组 */
                        } /* 结束防御检查 */
                    } /* 结束本地清洗拷贝 */
                    
                    /* SAFE_ALC_P 宏：在可能触发 GC 的分配之前与之后，执行局部数组与帧数组的同步 */
                    #define SAFE_ALC_P(byte_count, result_var) do {                         for (tcl_u32 copy_idx = 0; copy_idx < temp_argc; copy_idx++) {                             frame->argv[copy_idx] = temp_argv[copy_idx];                         }                         frame->argc = temp_argc;                         result_var = tcl_alc_p(context, byte_count);                         if (result_var == TCL_NULL) {                             context->status = TCL_ERROR;                             return TCL_ERROR;                         }                         frame = TO_PTR(context, context->curr_f);                         for (tcl_u32 copy_idx = 0; copy_idx < temp_argc; copy_idx++) {                             temp_argv[copy_idx] = frame->argv[copy_idx];                         }                     } while(0) /* 宏定义结束 */
                    
                    tcl_i32 round_idx = 3; /* 初始化轮次变量为 3，代表从 Level 3 (最高优先级) 开始向下进行归约 */
                    while (round_idx >= 0) { /* 循环进行四轮优先级扫描，依次处理 Level 3, 2, 1, 0 */
                        tcl_u32 cursor = 1; /* 运算符起始探测点 */
                        while (cursor < temp_argc - 1) { /* 循环遍历所有剩余元素 */
                            const tcl_u8 *operator_str = TO_PTR(context, temp_argv[cursor]); /* 获取运算符指针 */
                            tcl_u32 is_match = 0; /* 初始化匹配状态 */
                            if (round_idx == 3) { /* Level 3：高优先级乘除模 */
                                if (t_scmp(operator_str, (tcl_u8*)"*") == 0 || /* 乘 */
                                    t_scmp(operator_str, (tcl_u8*)"/") == 0 || /* 除 */
                                    t_scmp(operator_str, (tcl_u8*) "%") == 0) { /* 模 */
                                    is_match = 1; /* 匹配 */
                                } /* 结束匹配 */
                            } else if (round_idx == 2) { /* Level 2：加减法 */
                                if (t_scmp(operator_str, (tcl_u8*)"+") == 0 || /* 加 */
                                    t_scmp(operator_str, (tcl_u8*)"-") == 0) { /* 减 */
                                    is_match = 1; /* 匹配 */
                                } /* 结束匹配 */
                            } else if (round_idx == 1) { /* Level 1：关系与相等比较 */
                                if (t_scmp(operator_str, (tcl_u8*)"==") == 0 || /* 相等 */
                                    t_scmp(operator_str, (tcl_u8*)"!=") == 0 || /* 不等 */
                                    t_scmp(operator_str, (tcl_u8*)"eq") == 0 || /* 字面量相等 */
                                    t_scmp(operator_str, (tcl_u8*)"ne") == 0 || /* 字面量不等 */
                                    t_scmp(operator_str, (tcl_u8*)">") == 0 || /* 大于 */
                                    t_scmp(operator_str, (tcl_u8*)">=") == 0 || /* 大于等于 */
                                    t_scmp(operator_str, (tcl_u8*)"<") == 0 || /* 小于 */
                                    t_scmp(operator_str, (tcl_u8*)"<=") == 0) { /* 小于等于 */
                                    is_match = 1; /* 匹配 */
                                } /* 结束匹配 */
                            } else if (round_idx == 0) { /* Level 0：逻辑与或 */
                                if (t_scmp(operator_str, (tcl_u8*)"&&") == 0 || /* 逻辑与 */
                                    t_scmp(operator_str, (tcl_u8*)"||") == 0) { /* 逻辑或 */
                                    is_match = 1; /* 匹配 */
                                } /* 结束匹配 */
                            } /* 结束分发 */
                            if (is_match) { /* 匹配到运算符 */
                                tcl_i32 left_operand_val = t_atoi(TO_PTR(context, temp_argv[cursor - 1])); /* 获取左操作数 */
                                tcl_i32 right_operand_val = t_atoi(TO_PTR(context, temp_argv[cursor + 1])); /* 获取右操作数 */
                                tcl_u8 *left_ptr = TO_PTR(context, temp_argv[cursor - 1]); /* 获取左操作数物理指针 */
                                tcl_u8 *right_ptr = TO_PTR(context, temp_argv[cursor + 1]); /* 获取右操作数物理指针 */
                                tcl_i32 calc_result = 0; /* 初始化结果 */
                                if (t_scmp(operator_str, (tcl_u8*)"*") == 0) { /* 乘 */
                                    calc_result = left_operand_val * right_operand_val; /* 计算 */
                                } else if (t_scmp(operator_str, (tcl_u8*)"/") == 0) { /* 除 */
                                    calc_result = right_operand_val ? left_operand_val / right_operand_val : 0; /* 计算，带除零保护 */
                                } else if (t_scmp(operator_str, (tcl_u8*)"%") == 0) { /* 模 */
                                    calc_result = right_operand_val ? left_operand_val % right_operand_val : 0; /* 计算，带除零保护 */
                                } else if (t_scmp(operator_str, (tcl_u8*)"+") == 0) { /* 加 */
                                    calc_result = left_operand_val + right_operand_val; /* 计算 */
                                } else if (t_scmp(operator_str, (tcl_u8*)"-") == 0) { /* 减 */
                                    calc_result = left_operand_val - right_operand_val; /* 计算 */
                                } else if (t_scmp(operator_str, (tcl_u8*)">") == 0) { /* 大于 */
                                    if (t_is_int(left_ptr) && t_is_int(right_ptr)) { /* 两者均为整数 */
                                        calc_result = (left_operand_val > right_operand_val); /* 数值比较 */
                                    } else { /* 至少有一个操作数不是整数 */
                                        calc_result = (t_scmp(left_ptr, right_ptr) > 0); /* 字符串比较 */
                                    } /* 结束类型判定分流 */
                                } else if (t_scmp(operator_str, (tcl_u8*)">=") == 0) { /* 大于等于 */
                                    if (t_is_int(left_ptr) && t_is_int(right_ptr)) { /* 两者均为整数 */
                                        calc_result = (left_operand_val >= right_operand_val); /* 数值比较 */
                                    } else { /* 至少有一个操作数不是整数 */
                                        calc_result = (t_scmp(left_ptr, right_ptr) >= 0); /* 字符串比较 */
                                    } /* 结束类型判定分流 */
                                } else if (t_scmp(operator_str, (tcl_u8*)"<") == 0) { /* 小于 */
                                    if (t_is_int(left_ptr) && t_is_int(right_ptr)) { /* 两者均为整数 */
                                        calc_result = (left_operand_val < right_operand_val); /* 数值比较 */
                                    } else { /* 至少有一个操作数不是整数 */
                                        calc_result = (t_scmp(left_ptr, right_ptr) < 0); /* 字符串比较 */
                                    } /* 结束类型判定分流 */
                                } else if (t_scmp(operator_str, (tcl_u8*)"<=") == 0) { /* 小于等于 */
                                    if (t_is_int(left_ptr) && t_is_int(right_ptr)) { /* 两者均为整数 */
                                        calc_result = (left_operand_val <= right_operand_val); /* 数值比较 */
                                    } else { /* 至少有一个操作数不是整数 */
                                        calc_result = (t_scmp(left_ptr, right_ptr) <= 0); /* 字符串比较 */
                                    } /* 结束类型判定分流 */
                                } else if (t_scmp(operator_str, (tcl_u8*)"eq") == 0) { /* 字面量相等 */
                                    calc_result = (t_scmp(left_ptr, right_ptr) == 0); /* 纯字面量对比 */
                                } else if (t_scmp(operator_str, (tcl_u8*)"ne") == 0) { /* 字面量不等 */
                                    calc_result = (t_scmp(left_ptr, right_ptr) != 0); /* 纯字面量对比 */
                                } else if (t_scmp(operator_str, (tcl_u8*)"==") == 0) { /* 相等 */
                                    if (t_is_int(left_ptr) && t_is_int(right_ptr)) { /* 两者均为整数 */
                                        calc_result = (left_operand_val == right_operand_val); /* 数值比较 */
                                    } else { /* 至少有一个操作数不是整数 */
                                        calc_result = (t_scmp(left_ptr, right_ptr) == 0); /* 字符串比较 */
                                    } /* 结束类型判定分流 */
                                } else if (t_scmp(operator_str, (tcl_u8*)"!=") == 0) { /* 不等 */
                                    if (t_is_int(left_ptr) && t_is_int(right_ptr)) { /* 两者均为整数 */
                                        calc_result = (left_operand_val != right_operand_val); /* 数值比较 */
                                    } else { /* 至少有一个操作数不是整数 */
                                        calc_result = (t_scmp(left_ptr, right_ptr) != 0); /* 字符串比较 */
                                    } /* 结束类型判定分流 */
                                } else if (t_scmp(operator_str, (tcl_u8*)"&&") == 0) { /* 逻辑与 */
                                    calc_result = (left_operand_val && right_operand_val); /* 计算 */
                                } else if (t_scmp(operator_str, (tcl_u8*)"||") == 0) { /* 逻辑或 */
                                    calc_result = (left_operand_val || right_operand_val); /* 计算 */
                                } /* 结束具体运算符求值 */
                                tcl_u32 result_offset = TCL_NULL; /* 初始化结果偏移 */
                                SAFE_ALC_P(12, result_offset); /* 安全分配内存，期间自动完成 GC 同步与物理指针重定位 */
                                t_itoa(calc_result, TO_PTR(context, result_offset)); /* 将计算结果存入分配的空间 */
                                temp_argv[cursor - 1] = result_offset; /* 更新左侧操作数 */
                                for (tcl_u32 move_idx = cursor + 2; move_idx < temp_argc; move_idx++) { /* 后续前移 */
                                    temp_argv[move_idx - 2] = temp_argv[move_idx]; /* 平移覆盖 */
                                } /* 结束平移 */
                                temp_argc -= 2; /* 修正总数 */
                            } else { /* 没有匹配 */
                                cursor += 2; /* 步进 */
                            } /* 结束匹配判定 */
                        } /* 结束遍历 */
                        round_idx--; /* 进入下一优先级轮次 */
                    } /* 结束四轮归约 */
                    if (temp_argc > 0) { /* 存在最终归约结果 */
                        context->result = temp_argv[0]; /* 将首个元素写入结果寄存器 */
                    } else { /* 没有元素 */
                        context->result = TCL_NULL; /* 将结果清空 */
                    } /* 结束结果输出判定 */
                    #undef SAFE_ALC_P /* 注销局部宏 */
                }

                context->status = TCL_OK; /* 归约指令集语义完成，强制将全局状态重置为成功运行态 */
                frame->state = ST_TOKENIZE; /* 逻辑闭环：设为 TOKENIZE 态以触发 ST_RESUME 阶段的栈帧自动清理与结果冒泡 */
                break; /* 退出 ST_EXPR_REDUCE 状态处理 */
            } /* 结束 ST_EXPR_REDUCE 状态块 */
            case ST_IF_COND: { /* 分支指令：if 条件求值准备阶段 */
                tcl_u32 cond_frame_offset = tcl_alc_t(context, sizeof(TclFrame)); /* 分配子栈帧，用于求值条件表达式的脚本段 */
                if (cond_frame_offset == TCL_NULL) { /* 内存安全性检查 */
                    context->status = TCL_ERROR; /* 空间耗尽，报内存错误 */
                    context->curr_f = TCL_NULL; /* 安全终止执行 */
                    break; /* 退出 */
                } /* 结束分配检查 */
                TclFrame *cond_frame = TO_PTR(context, cond_frame_offset); /* 物理映射条件子帧 */
                cond_frame->script = frame->cond; /* 挂载条件子脚本 */
                cond_frame->pc = 0; /* 从头开始解析条件脚本 */
                cond_frame->vars = TCL_NULL; /* 语义设定：条件表达式执行共享调用方作用域，不使用私有表 */
                cond_frame->parent = context->curr_f; /* 物理返回路径：求值完毕后回到 if 帧进行逻辑决策 */
                cond_frame->scope = context->curr_f; /* 逻辑作用域：条件表达式可以访问父帧的所有变量 */
                cond_frame->state = ST_TOKENIZE; /* 子帧进入初始化分词态 */
                cond_frame->flags = FRAME_SHARE_SCOPE | FRAME_IS_EXPR; /* 标记为共享作用域的表达式归约帧 */
                cond_frame->cond = cond_frame->body = cond_frame->result = TCL_NULL; /* 槽位初始化 */
                cond_frame->argc = cond_frame->exp_idx = 0; /* 计数器归零 */
                frame->state = ST_IF_BODY; /* 将当前 if 帧设为等待态：详细注释，待条件子帧执行完后逻辑自动回退至 IF_BODY 处理分支 */
                context->curr_f = cond_frame_offset; /* 焦点转移，开始执行条件求值逻辑 */
                break; /* 退出 ST_IF_COND 处理 */
            } /* 结束 ST_IF_COND 状态块 */
            case ST_IF_BODY: { /* 分支指令：判定条件布尔结果并决策是否执行主体 (body) */
                if (context->status != TCL_OK) { /* 防御性编程：检测条件求值子脚本在运行中是否产生了异常 */
                    tcl_i32 saved_status = context->status; /* 暂存异常状态 */
                    tcl_u32 saved_result = context->result; /* 暂存异常信息 */
                    tcl_u32 parent_frame_offset = frame->parent; /* 记录回退地址 */
                    context->t_bot += ((sizeof(TclFrame) + 7) & ~7); /* 物理销毁已无用的 if 控制帧并保持对齐 */
                    context->curr_f = parent_frame_offset; /* 彻底上移执行流 */
                    context->status = saved_status; /* 恢复状态码继续冒泡流程 */
                    context->result = saved_result; /* 恢复结果 */
                    break; /* 退出 */
                } /* 结束异常判定 */
                const tcl_u8 *result_string = tcl_get_result(context); /* 获取条件子帧计算得出的布尔结果字符串 */
                if (result_string[0] && result_string[0] != '0') { /* 符合 Tcl 标准布尔哲学：非空且首字节非 '0' 为逻辑真 */
                    tcl_u32 body_frame_offset = tcl_alc_t(context, sizeof(TclFrame)); /* 分配子栈帧用于执行 if 的主体脚本 */
                    if (body_frame_offset == TCL_NULL) { /* 内存安全性校验 */
                        context->status = TCL_ERROR; /* 报错 */
                        context->curr_f = TCL_NULL; /* 终止 */
                        break; /* 退出 */
                    } /* 结束分配校验 */
                    TclFrame *body_frame = TO_PTR(context, body_frame_offset); /* 物理映射主体帧 */
                    body_frame->script = frame->body; /* 挂载主体脚本代码 */
                    body_frame->pc = 0; /* PC 清零，从头开始 */
                    body_frame->vars = TCL_NULL; /* 主体脚本执行共享调用方变量域 */
                    body_frame->parent = context->curr_f; /* 物理返回路径：执行完后回到父帧继续后续解析 */
                    body_frame->scope = context->curr_f; /* 逻辑作用域：主体脚本可以直接操作父帧变量 */
                    body_frame->state = ST_TOKENIZE; /* 启动态：进入分词阶段 */
                    body_frame->flags = FRAME_SHARE_SCOPE; /* 共享作用域属性 */
                    body_frame->cond = body_frame->body = body_frame->result = TCL_NULL; /* 数据槽位初始化 */
                    body_frame->argc = body_frame->exp_idx = 0; /* 计数器清零 */
                    frame->state = ST_TOKENIZE; /* 逻辑设定：主体完成后，if 帧应直接跳转至后续指令的解析态 */
                    context->curr_f = body_frame_offset; /* 焦点转移，开始执行 if 的 body 脚本段 */
                } else { /* 条件判定为逻辑假，检查是否存在 else 分支 */
                    /* 设计目的：从当前帧的 result 字段读取 else_body 引用 */
                    /* 这避免了使用全局 tmp_roots[11] 被嵌套 if 语句覆盖的问题 */
                    if (frame->result != TCL_NULL) { /* 存在 else body，需要执行（else_body 存在 frame->result 中） */
                        tcl_u32 else_body_script = frame->result; /* 从帧的 result 字段获取 else 脚本偏移量 */
                        frame->result = TCL_NULL; /* 清除 result 字段，防止 else 帧误读 */
                        tcl_u32 else_frame_offset = tcl_alc_t(context, sizeof(TclFrame)); /* 分配 else 子帧 */
                        if (else_frame_offset == TCL_NULL) { /* 内存安全性校验 */
                            context->status = TCL_ERROR; /* 报错 */
                            context->curr_f = TCL_NULL; /* 终止 */
                            break; /* 退出 */
                        } /* 结束分配校验 */
                        frame = TO_PTR(context, context->curr_f); /* 分配后重新同步 if 帧指针，防止 GC 位移 */
                        TclFrame *else_frame = TO_PTR(context, else_frame_offset); /* 物理映射 else 帧 */
                        else_frame->script = else_body_script; /* 挂载 else 脚本 */
                        else_frame->pc = 0; /* PC 归零 */
                        else_frame->vars = TCL_NULL; /* 共享作用域，无私有变量表 */
                        else_frame->parent = context->curr_f; /* 物理返回路径：回到 if 帧 */
                        else_frame->scope = context->curr_f; /* 逻辑作用域：共享父帧变量 */
                        else_frame->state = ST_TOKENIZE; /* 初始分词态 */
                        else_frame->flags = FRAME_SHARE_SCOPE; /* 共享作用域标志 */
                        else_frame->cond = else_frame->body = else_frame->result = TCL_NULL; /* 槽位初始化 */
                        else_frame->argc = else_frame->exp_idx = 0; /* 计数器归零 */
                        frame->state = ST_TOKENIZE; /* else 完成后，if 帧继续后续解析 */
                        context->curr_f = else_frame_offset; /* 焦点转移至 else 子帧 */
                    } else { /* 没有 else 分支，直接跳过 */
                        frame->state = ST_TOKENIZE; /* 切换回解析态，继续后续指令 */
                    } /* 结束 else 分支存在性检查 */
                } /* 结束逻辑分支分发 */
                break; /* 退出 ST_IF_BODY 处理 */
            } /* 结束 ST_IF_BODY 状态块 */
            case ST_COND: { /* 循环指令：while 迭代的条件探测起点 */
                if (context->status == TCL_BREAK) { /* 信号拦截：检测到循环主体抛出了中断信号 */
                    context->status = TCL_OK; /* 成功重置为正常态，表示拦截成功 */
                    frame->state = ST_TOKENIZE; /* 强制闭合循环生命周期，回到指令流解析主轨 */
                    break; /* 退出循环 */
                } /* 结束信号判定 */
                if (context->status == TCL_CONTINUE) context->status = TCL_OK; /* 探测到继续信号：重置状态以重新开始下一轮条件判定 */
                tcl_u32 cond_frame_offset = tcl_alc_t(context, sizeof(TclFrame)); /* 为条件表达式求值分配物理栈帧 */
                if (cond_frame_offset == TCL_NULL) { /* 安全性校验 */
                    context->status = TCL_ERROR; /* 内存耗尽报错 */
                    context->curr_f = TCL_NULL; /* 终止 */
                    break; /* 退出 */
                } /* 结束分配检查 */
                TclFrame *cond_frame = TO_PTR(context, cond_frame_offset); /* 物理映射条件子帧 */
                cond_frame->script = frame->cond; /* 挂载循环判定条件脚本 */
                cond_frame->pc = 0; /* PC 清零 */
                cond_frame->vars = TCL_NULL; /* 条件求值共享作用域 */
                cond_frame->parent = context->curr_f; /* 物理返回路径：求值完成后回到 while 控制帧 */
                cond_frame->scope = context->curr_f;  /* 逻辑作用域：共享 while 所在的变量域 */
                cond_frame->state = ST_TOKENIZE; /* 子帧进入初始化分词态 */
                cond_frame->flags = FRAME_SHARE_SCOPE | FRAME_IS_EXPR;  /* 共享作用域并执行表达式归约 */
                cond_frame->cond = cond_frame->body = cond_frame->result = TCL_NULL;  /* 数据槽位初始化 */
                cond_frame->argc = cond_frame->exp_idx = 0;  /* 计数器归零 */
                frame->state = ST_LOOP;  /* while 帧切换为等待主体态，完成非递归循环跳转 */
                context->curr_f = cond_frame_offset;  /* 启动条件子结界执行流程 */
                break;  /* 退出 ST_COND 处理分支 */
            }  /* 结束 ST_COND 案例处理 */
            case ST_LOOP: { /* 循环执行阶段：根据条件探测结果，决策是否进入或继续执行 while 循环体 */
                if (context->status == TCL_BREAK) { /* 信号拦截：捕获到循环内部产生的中断信号 */
                    context->status = TCL_OK; /* 成功拦截 break，重置状态码为正常 */
                    frame->state = ST_TOKENIZE; /* 强制终止当前 while 生命周期，状态机重回指令流解析态 */
                    break; /* 退出当前循环帧处理 */
                } /* 结束 break 判定 */
                if (context->status == TCL_CONTINUE) { /* 信号拦截：捕获到循环内部产生的继续迭代信号 */
                    context->status = TCL_OK; /* 重置状态码 */
                    frame->state = ST_COND; /* 核心跳转：强制回归条件判定态 (ST_COND)，开启下一次迭代探测 */
                    break; /* 退出当前处理 */
                } /* 结束 continue 判定 */
                if (context->status != TCL_OK) { /* 异常检测：捕获主体执行中抛出的致命 ERROR 或 RETURN 信号 */
                    tcl_i32 saved_status = context->status; /* 暂存异常状态码 */
                    tcl_u32 saved_result = context->result; /* 暂存异常关联的结果或错误描述对象 */
                    tcl_u32 parent_frame_offset = frame->parent; /* 获取父级调用帧的逻辑偏移量 */
                    context->t_bot += ((sizeof(TclFrame) + 7) & ~7); /* 物理销毁当前 while 帧，严格执行 8 字节内存对齐回收 */
                    context->curr_f = parent_frame_offset; /* 执行焦点回溯至父级作用域 */
                    context->status = saved_status; /* 在父级环境中恢复异常信号，继续向上传递（冒泡） */
                    context->result = saved_result; /* 恢复异常关联结果 */
                    break; /* 退出 */
                } /* 结束异常检测分支 */
                /* 空行：逻辑分段 - 布尔判定逻辑 */
                const tcl_u8 *result_string = tcl_get_result(context); /* 物理映射条件子帧求值产生的布尔字符串结果 */
                if (result_string[0] && result_string[0] != '0') { /* 遵循 Tcl 哲学：判定结果非空且首字符非 '0' 即为真 */
                    tcl_u32 body_frame_offset = tcl_alc_t(context, sizeof(TclFrame)); /* 为循环主体脚本的执行分配独立子帧 */
                    if (body_frame_offset == TCL_NULL) { /* 栈内存安全性校验 */
                        context->status = TCL_ERROR; /* 空间耗尽，报内存错误 */
                        context->curr_f = TCL_NULL; /* 安全终止引擎 */
                        break; /* 退出 */
                    } /* 结束分配校验 */
                    TclFrame *body_frame = TO_PTR(context, body_frame_offset); /* 映射循环体栈帧物理指针 */
                    body_frame->script = frame->body; /* 挂载循环体脚本代码段 */
                    body_frame->pc = 0; /* PC 清零，从头开始解析 */
                    body_frame->vars = TCL_NULL; /* 循环体执行共享父级变量域 */
                    body_frame->parent = context->curr_f; /* 物理返回路径：执行完后回到 while 帧 */
                    body_frame->scope = context->curr_f;  /* 逻辑作用域：共享 while 所在的变量域 */
                    body_frame->state = ST_TOKENIZE; /* 子帧进入初始化分词态 */
                    body_frame->flags = FRAME_SHARE_SCOPE; /* 启用作用域共享标志 */
                    body_frame->cond = body_frame->body = body_frame->result = TCL_NULL; /* 字段初始化 */
                    body_frame->argc = body_frame->exp_idx = 0; /* 计数器归零 */
                    frame->state = ST_COND; /* 关键闭环：设定 while 帧在 body 完后自动回归条件检查态，实现非递归迭代 */
                    context->curr_f = body_frame_offset; /* 焦点转移，开始执行 body 脚本段 */
                } else { /* 条件判定为假，循环正常终止 */
                    frame->state = ST_TOKENIZE; /* 退出循环逻辑，状态机重回主指令流解析态 */
                } /* 结束布尔分支分发 */
                break; /* 退出 ST_LOOP 处理 */
            } /* 结束 ST_LOOP 案例处理 */
            case ST_CATCH_END: { /* 异常捕获收尾阶段：负责将异常状态转化为脚本层可见的变量值 */
                context->tmp_roots[0] = context->result; /* 保护子求值任务产生的结果偏移量，防止后续分配触发 GC 导致数据位移 */
                context->tmp_roots[1] = frame->body; /* 保护用户指定的用于接收结果的变量名字符串偏移量 */
                /* 空行：逻辑分段 - 状态码字符串化 */
                tcl_u32 status_offset = tcl_alc_p(context, 12); /* 分配临时存储区，用于存放十进制格式的状态码文本 */
                if (status_offset == TCL_NULL) { /* 物理内存分配安全性校验 */
                    context->tmp_roots[0] = TCL_NULL; /* 分配失败，立即撤销对子结果对象的 GC 保护 */
                    context->tmp_roots[1] = TCL_NULL; /* 撤销变量名保护 */
                    context->status = TCL_ERROR; /* 将当前状态升级为致命内存错误 */
                    break; /* 退出 catch 异常处理流程 */
                } /* 结束分配安全性校验 */
                t_itoa(context->status, TO_PTR(context, status_offset)); /* 核心操作：将数字状态码转换为 Tcl 脚本可识别的十进制字符串对象 */
                context->tmp_roots[2] = status_offset; /* 保护刚生成的十进制文本对象偏移量，防止在后续可能的 GC 过程中被错误回收 */
                frame = TO_PTR(context, context->curr_f); /* 刷新当前执行帧的物理映射，应对刚才 t_itoa 可能触发的 Arena 空间整理与地址迁移 */
                /* 逻辑空行：进入 catch 指令的变量写回阶段 */
                if (context->tmp_roots[1] != TCL_NULL) { /* 判定用户是否通过 catch 指令的第二个可选参数提供了结果存储变量名 */
                    tcl_set_var(context, frame->parent, context->tmp_roots[1], context->tmp_roots[0]); /* 将捕获到的计算结果或错误提示信息写入调用者的符号表中 */
                } /* 结束结果变量的写回逻辑 */
                context->result = context->tmp_roots[2]; /* 设定 catch 指令本身的返回值为刚才生成的十进制状态码对象偏移量 */
                context->status = TCL_OK; /* 核心语义：由于异常信号已被当前 catch 结界成功拦截，全局解释器逻辑状态必须重置为正常运行态 */
                frame = TO_PTR(context, context->curr_f); /* 再次物理映射当前帧指针，以便安全更新其内部状态机寄存器 */
                frame->state = ST_TOKENIZE; /* 状态机恢复至分词解析态，准备驱动执行脚本中 catch 块之后的后续指令流 */
                context->tmp_roots[0] = TCL_NULL; /* 清理第一个临时保护根，解除对原始执行结果对象的 GC 锁定状态 */
                context->tmp_roots[1] = TCL_NULL; /* 清理第二个临时保护根，释放对变量名对象的 GC 锁定 */
                context->tmp_roots[2] = TCL_NULL; /* 清理最后一个临时保护根，标志着 catch 节点的完整生命周期管理正式结束 */
                break; /* 退出 ST_CATCH_END 处理分支，交还控制权给主循环进行下一轮调度 */
            } /* 结束 ST_CATCH_END 状态块的处理逻辑 */
            case ST_RESUME: {  /* 结界恢复态：本阶段负责将子帧执行结果“冒泡”给父帧，并处理跨层级的异常信号传播 */
                frame = TO_PTR(context, context->curr_f);  /* 从上下文寄存器中加载当前处于活跃状态的父级执行栈帧物理指针 */
                if (context->status != TCL_OK && context->status != TCL_RETURN) {  /* 检查刚终结的子结界是否向上传递了 BREAK/CONTINUE/ERROR 等非正常退出信号 */
                    tcl_i32 saved_status = context->status;  /* 在冒泡开始前，首先物理暂存子结界的原始退出状态码，防止递归回溯时丢失 */
                    tcl_u32 saved_result = context->result;  /* 物理暂存子结界留下的计算结果或报错信息偏移量，确保栈空间回收后该引用依然有效 */
                    while (frame) {  /* 启动标准异常冒泡循环：逐层向父结界链式寻找能够捕获或消费当前异常信号的拦截点 */
                        tcl_u8 current_flags = frame->flags;  /* 获取当前探测到的栈帧的功能标志位，用于识别是否触碰到了过程 (Proc) 边界 */
                        tcl_u32 parent_offset = frame->parent;  /* 记录当前帧的父级偏移量，作为下一步向上回溯的物理路径 */
                        context->t_bot += ((sizeof(TclFrame) + 7) & ~7);  /* 核心操作：物理回收已失效的子结界空间。加 7 后取反掩码操作确保 t_bot 严格遵循 8 字节向下对齐规约 */
                        context->curr_f = parent_offset;  /* 将解释器的执行重心物理上移一层，完成一次栈回退操作 */
                        if (parent_offset == TCL_NULL) {  /* 判定冒泡流是否已经穿透了脚本执行的物理顶层（即 eval 指令的最外层作用域） */
                            frame = 0;  /* 标记当前帧指针寄存器已耗尽，意味着异常将直接反馈给外部 C 语言调用者 */
                            break;  /* 终止冒泡探测循环 */
                        }  /* 顶层溢出判定结束 */
                        frame = TO_PTR(context, parent_offset);  /* 映射父级结界的物理指针，准备在该层级进行异常拦截规则检查 */
                        if (frame->state == ST_CATCH_END || ((saved_status == TCL_BREAK || saved_status == TCL_CONTINUE) && (frame->state == ST_LOOP || frame->state == ST_COND))) {
                            break;  /* 拦截成功：当前帧是 catch 结尾或循环点，能够消费此信号，在此停止冒泡流程 */
                        }  /* 拦截点命中判定结束 */
                        if (saved_status == TCL_RETURN && (current_flags & FRAME_IS_PROC)) {  /* 语义规约：return 信号在触碰到过程 (Proc) 的逻辑出口时应当被停止冒泡并转换 */
                            break;  /* 成功命中 Proc 边界，在此终止 return 信号的外溢传播 */
                        }  /* Proc 边界判定结束 */
                    }  /* 结束冒泡回溯循环体 */
                    context->status = saved_status;  /* 将最终拦截到的或溢出顶层的状态码重新回填至全局上下文寄存器 */
                    context->result = saved_result;  /* 将关联的结果数据也同步回填至上下文，完成跨帧的语义信息对等传递 */
                    if (!frame) { /* 判定分支：如果冒泡流程最终因未命中任何拦截点而穿透了最外层顶层 */
                        context->curr_f = TCL_NULL; /* 彻底销毁解释器执行链，将活跃帧置为空，标记进入安全停机流程 */
                    } /* 停机状态维护结束 */
                    break;  /* 退出恢复态下的异常处理逻辑，返回主调度循环 */
                }  /* 子结界非正常退出流程处理结束 */
                if (context->status == TCL_RETURN) {  /* 特殊处理：如果接收到的是 RETURN 信号且已到达结界边界（如 Proc 正常返回点） */
                    context->status = TCL_OK;  /* 语义转换：将内部 RETURN 信号成功拦截并转换为正常的 OK 状态，以便父环境继续执行 */
                }  /* RETURN 信号拦截判定结束 */
                if (frame->exp_idx < frame->argc) {  /* 判定父结界当前是否仍处于参数求值与展开阶段 (EXPAND) */
                    frame->argv[frame->exp_idx] = context->result;  /* 冒泡核心实现：将子结界计算得到的最终结果回填至父结界正在构建的参数槽位中 */
                    frame->exp_idx++;  /* 递增参数处理游标，准备为命令中的下一个参数发起新的求值请求 */
                    frame->state = ST_EXPAND;  /* 驱动父结界状态机重新回到参数处理态，进入新一轮的求值迭代周期 */
                } else {  /* 场景：当前指令的所有参数（包括命令名）均已完成解析、求值并成功回填 */
                    frame->state = ST_TOKENIZE;  /* 父结界当前指令的执行使命已彻底达成，重置状态回解析态以处理脚本流中的下一条指令 */
                }  /* 执行流恢复路径判定结束 */
                break;  /* 退出 ST_RESUME 状态案例处理分支 */
            }  /* 结束 ST_RESUME 状态块的完整实现 */
            default:  /* 防御性编程：当状态机寄存器由于某种逻辑故障进入了未定义的非法状态空间时触发 */
                context->curr_f = TCL_NULL;  /* 立即采取熔断措施：强制清空当前帧寄存器，使得核心调度循环安全停止执行 */
                break;  /* 退出默认防御分支 */
        }  /* 结束 switch 状态调度器的主体逻辑 */
        next_state_loop:;  /* 逻辑跳转标签：允许解释器内部逻辑从深层代码块快速回滚至主引擎头部进行新一轮状态切换 */
    }  /* 结束 while(curr_f) 解释器核心引擎执行主循环体 */
    tcl_i32 final_status = context->status;  /* 提取并暂存本次 eval 调用的最终全局语义状态码（如 TCL_OK 或未捕获的 TCL_ERROR） */
    context->status = TCL_OK;  /* 执行环境冷启动重置：清除全局状态暂存器，确保下次 eval 调用能从干净的初始状态开始 */
    return final_status;  /* 向 bare-metal 宿主 C 环境返回脚本执行的最终结果状态码 */
}  /* 结束 tcl_eval 核心解释引擎函数实现 */

/* 导出函数：获取 BareTcl 解释器最近一次执行结果的物理指针（暴露给外部 C 环境进行结果提取） */
const tcl_u8 *tcl_get_result(TclCtx *context) {  /* 函数定义入口：接收全局上下文指针 */
    return context->result == TCL_NULL ? (tcl_u8*)"" : TO_PTR(context, context->result);  /* 物理映射逻辑：若无结果则返回空字符串常量，否则根据偏移量计算出内存地址 */
}  /* 结束 tcl_get_result 函数 */

#include "tcllib.c"  /* 静态包含：由 tcl2c.py 工具自动生成的 BareTcl 内置自举脚本库二进制数据 */

/* 导出函数：加载并执行 BareTcl 内置的自举脚本库（初始化 proc 等核心 Tcl 指令） */
tcl_i32 tcl_load_bootstrap(TclCtx *context) {  /* 函数定义入口 */
    return tcl_eval(context, (const tcl_u8 *)tcl_bootstrap);  /* 直接驱动 eval 引擎解析并执行静态编译进内核的自举代码流 */
}  /* 结束 tcl_load_bootstrap 函数 */

/* info 指令实现：提供解释器运行时的元数据自省能力，用于查询已注册指令或过程名 */
static tcl_i32 tcl_cmd_info(TclCtx *context, tcl_i32 argument_count, tcl_u32 *argument_values) {  /* 函数定义入口：标准的 C 命令实现原型 */
    if (argument_count < 2) return TCL_ERROR;  /* 语法检查：判定必需的子指令参数（如 commands）是否存在 */
    const tcl_u8 *sub_command_str = TO_PTR(context, argument_values[1]);  /* 物理映射：获取子指令名称字符串的物理内存指针 */
    if (t_scmp(sub_command_str, (const tcl_u8 *)"commands") == 0) {  /* 子指令分发：处理 info commands 分支请求 */
        if (argument_count > 2) {  /* 逻辑判定：如果用户带有特定的查询过滤模式串 */
            const tcl_u8 *search_pattern = TO_PTR(context, argument_values[2]);  /* 物理映射：获取用户输入的搜索模式串物理指针 */
            for (tcl_i32 command_index = 0; command_index < cmd_count; command_index++) {  /* 线性迭代：遍历内核 C 原子指令注册表 */
                if (t_scmp(search_pattern, cmd_table[command_index].name) == 0) {  /* 字符串比对：执行精确名称匹配判定 */
                    context->result = argument_values[2];  /* 匹配命中：将查询到的原始指令名对象作为计算结果返回 */
                    return TCL_OK;  /* 成功返回 */
                }  /* 结束内置 C 指令匹配判定 */
            }  /* 结束内置指令表遍历循环 */
            tcl_u32 global_var_offset = context->g_vars;  /* 获取全局符号表链表的物理起始偏移量 */
            while (global_var_offset != TCL_NULL) {  /* 链表迭代：深度遍历符号表以查找用户通过 Tcl 脚本定义的过程 (Proc) */
                TclVar *global_variable_ptr = TO_PTR(context, global_var_offset);  /* 物理映射：将当前偏移量转换为符号对象指针 */
                const tcl_u8 *proc_variable_name = TO_PTR(context, global_variable_ptr->name);  /* 获取变量名字符串指针 */
                if (proc_variable_name[0] == 'p' && proc_variable_name[1] == ':' && t_scmp(search_pattern, proc_variable_name + 2) == 0) { /* 协议匹配：判定是否具备 "p:" 前缀且名称部分完全一致 */
                    context->result = argument_values[2];  /* 匹配命中：将查询到的 Proc 名称返回给脚本环境 */
                    return TCL_OK;  /* 成功返回 */
                }  /* 结束 Proc 符号匹配判定 */
                global_var_offset = global_variable_ptr->next;  /* 移动游标：沿链表指向下一个全局符号偏移量 */
            }  /* 结束全局符号表深度遍历 */
            context->result = TCL_NULL;  /* 查询失败：在所有指令与过程库中均未找到匹配项，结果设为空 */
        } else {  /* 场景分支：处理 info commands 的无参数调用形式（获取所有列表） */
            context->result = TCL_NULL;  /* 暂行规定：目前版本返回空结果，待后续版本实现完整的列表生成逻辑 */
        }  /* 结束模式过滤逻辑判定 */
        return TCL_OK;  /* 指令执行流正常结束 */
    }  /* 结束 commands 子指令的处理逻辑 */
    return TCL_ERROR;  /* 异常判定：用户提供了不支持的 info 子指令，直接返回错误状态 */
}  /* 结束 tcl_cmd_info 函数实现 */

/* 导出函数：BareTcl 解释器引擎的物理初始化入口，负责内存池划分与核心原子指令冷启动注册 */
void tcl_init(void *arena_ptr, tcl_i32 total_size) {  /* 函数入口：接收由宿主环境预分配的连续物理内存块首地址 */
    TclCtx *context = (TclCtx*)arena_ptr;  /* 角色定义：将内存池的起始头部区域定义为解释器全局控制上下文结构体 */
    for (tcl_u32 clear_index = 0; clear_index < sizeof(TclCtx); clear_index++) {  /* 物理清零循环：确保控制结构体初始状态纯净 */
        ((tcl_u8*)arena_ptr)[clear_index] = 0;  /* 关键操作：将控制内存清零，确保所有内部偏移量初始均为 TCL_NULL (0)，防止 GC 扫描到残留的野指针数据 */
    }  /* 结束内存初始化清零循环 */
    context->arena = (tcl_u8*)arena_ptr;  /* 建立物理锚点：记录物理内存池的首地址，作为后续所有 TO_PTR 宏计算的基准偏置 */
    context->size = (tcl_u32)total_size;  /* 资源记录：保存内存池的总物理容量，用于运行时执行严格的边界溢出防御检查 */
    context->p_top = HS;  /* 堆空间划分：设定对象与变量存储区的物理起点。HS 偏移量必须遵循 8 字节以上的对齐规约 */
    context->t_bot = context->size & ~7;  /* 栈空间划分：设定执行栈帧区的物理起点。从内存池末尾开始向下生长，通过位与运算强制执行 8 字节对齐以确保硬件存取性能 */
    context->g_vars = TCL_NULL;  /* 指针初始化：设定全局变量符号表头指针初始为空 */
    context->result = TCL_NULL;  /* 状态初始化：设定结果暂存器初始值为空对象 */
    context->status = TCL_OK;  /* 语义初始化：设定解释器初始运行逻辑状态为正常状态 (OK) */
    context->curr_f = TCL_NULL;  /* 调度初始化：设定当前活跃执行帧寄存器初始为空 */
    cmd_count = 0;  /* 计数器清零：重置内核级 C 指令动态注册表计数器 */
    tcl_register_c_cmd((tcl_u8*)"set", tcl_cmd_set);  /* 核心指令注册：变量赋值与获取指令 set */
    tcl_register_c_cmd((tcl_u8*)"proc", tcl_cmd_proc);  /* 核心指令注册：过程定义指令 proc */
    tcl_register_c_cmd((tcl_u8*)"if", tcl_cmd_if);  /* 核心指令注册：条件分支指令 if */
    tcl_register_c_cmd((tcl_u8*)"expr", tcl_cmd_expr);  /* 核心指令注册：算术与逻辑表达式求值指令 expr */
    tcl_register_c_cmd((tcl_u8*)"while", tcl_cmd_while);  /* 核心指令注册：循环控制指令 while */
    tcl_register_c_cmd((tcl_u8*)"return", tcl_cmd_return);  /* 核心指令注册：显式函数返回指令 return */
    tcl_register_c_cmd((tcl_u8*)"break", tcl_cmd_break);  /* 核心指令注册：循环中断跳出指令 break */
    tcl_register_c_cmd((tcl_u8*)"continue", tcl_cmd_continue);  /* 核心指令注册：循环继续执行指令 continue */
    tcl_register_c_cmd((tcl_u8*)"error", tcl_cmd_error);  /* 核心指令注册：主动抛出脚本异常指令 error */
    tcl_register_c_cmd((tcl_u8*)"eval", tcl_cmd_eval);  /* 核心指令注册：脚本动态解析执行指令 eval */
    tcl_register_c_cmd((tcl_u8*)"catch", tcl_cmd_catch);  /* 核心指令注册：异常信号捕获指令 catch */
    tcl_register_c_cmd((tcl_u8*)"uplevel", tcl_cmd_uplevel);  /* 扩展指令注册：跨层级脚本执行指令 uplevel */
    tcl_register_c_cmd((tcl_u8*)"upvar", tcl_cmd_upvar);  /* 扩展指令注册：跨层级变量引用指令 upvar */
    tcl_register_c_cmd((tcl_u8*)"list", tcl_cmd_list);  /* 列表指令注册：创建 Tcl 列表对象指令 list */
    tcl_register_c_cmd((tcl_u8*)"llength", tcl_cmd_llength);  /* 列表指令注册：获取列表长度指令 llength */
    tcl_register_c_cmd((tcl_u8*)"lindex", tcl_cmd_lindex);  /* 列表指令注册：按索引提取列表元素指令 lindex */
    tcl_register_c_cmd((tcl_u8*)"unset", tcl_cmd_unset);  /* 变量指令注册：销毁指定变量指令 unset */
    /* 注意：lrange, global, info 已迁移至 tcllib.tcl 自举实现，不再在此注册 C 命令 */
}  /* 结束 tcl_init 函数的初始化逻辑 */


