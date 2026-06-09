/*
 * Tclsh.v2 - 极致精简、无栈化、脱离 Libc 的 Tcl 解释器内核
 */

/* 基础数据类型定义，确保跨平台物理一致性 */
typedef unsigned char      tcl_u8;  /* 无符号 8 位整数，用于字节流与字符 */
typedef signed char        tcl_i8;  /* 有符号 8 位整数，用于小量偏移 */
typedef unsigned short     tcl_u16; /* 无符号 16 位整数，用于短偏移量 */
typedef signed short       tcl_i16; /* 有符号 16 位整数 */
typedef unsigned int       tcl_u32; /* 无符号 32 位整数，用于 Arena 游标及大容量计数 */
typedef signed int         tcl_i32; /* 有符号 32 位整数，Tcl 脚本内的默认整数类型 */

/* 平台相关的指针宽度整数类型定义 */
#if defined(__x86_64__) || defined(__aarch64__)
typedef unsigned long long tcl_ptr; /* 64 位平台指针宽度 */
#else
typedef unsigned int       tcl_ptr; /* 32 位平台指针宽度 */
#endif

/* 核心常量定义 */
#define TCL_NULL (0xFFFFFFFF)       /* 空偏移量常量，用于表示无效引用 */
#define MAX_ARGS 32                 /* 单条指令支持的最大参数数量 */
#define tcl_i32_MAX 2147483647      /* 32 位有符号整数最大值 */

/* 调试日志宏 */
#ifdef TCL_DEBUG
#define TCL_LOG(message) tcl_hal_puts((const tcl_u8 *)message) /* 输出调试信息 */
#else
#define TCL_LOG(message)                                       /* 禁用调试日志 */
#endif

/* 解释器全局上下文结构体 */
typedef struct {
    tcl_u8  *arena;             /* 静态内存池起始物理地址 */
    tcl_u32  size;              /* 内存池总物理容量 */
    tcl_u32  p_top;             /* 变量区（低地址区）当前分配游标 */
    tcl_u32  t_bot;             /* 栈帧区（高地址区）当前分配游标 */
    tcl_u32  g_vars;            /* 全局变量链表起始偏移量 */
    tcl_u32  result;            /* 最近一次执行结果的字符串偏移量 */
    tcl_i32  status;            /* 解释器当前运行状态码（TCL_OK/ERROR 等） */
    tcl_u32  curr_f;            /* 当前正活跃的执行栈帧偏移量 */
    tcl_u32  tmp_roots[16];     /* GC 保护根，防止分配过程中的中间对象被回收 */
} TclCtx;

/* 变量对象结构体 */
typedef struct {
    tcl_u32 name;               /* 变量名字符串偏移量 */
    tcl_u32 val;                /* 变量值字符串偏移量 */
    tcl_u32 next;               /* 链表中下一个变量的偏移量 */
    tcl_u8  flags;              /* 变量标志位（如 VAR_LINK） */
} TclVar;

/* 变量标志定义 */
#define VAR_LINK 0x01           /* 链接标志，表示该变量是 upvar 的引用 */

/* 执行栈帧结构体 */
typedef struct {
    tcl_u32 script;             /* 当前正在解析的脚本字符串偏移量 */
    tcl_u32 pc;                 /* 程序计数器，记录当前解析位置偏移 */
    tcl_u32 vars;               /* 当前作用域下的局部变量链表偏移量 */
    tcl_u32 parent;             /* 父执行栈帧的偏移量 */
    tcl_u32 result;             /* 栈帧级别的临时结果偏移量 */
    tcl_u32 cond;               /* 循环或条件判定的条件表达式脚本偏移量 */
    tcl_u32 body;               /* 循环或条件判定的执行主体脚本偏移量 */
    tcl_u32 argv[MAX_ARGS];     /* 当前指令解析出的参数偏移量数组 */
    tcl_u8  argc;               /* 当前指令的参数总数 */
    tcl_u8  state;              /* 状态机当前所处的状态（TOKENIZE/EXPAND 等） */
    tcl_u8  flags;              /* 栈帧标志位（如 FRAME_IS_PROC） */
    tcl_u8  exp_idx;            /* 参数展开进度的索引计数 */
} TclFrame;

/* 栈帧标志定义 */
#define FRAME_SHARE_SCOPE 1     /* 共享作用域标志，用于 eval/if/while 等指令 */
#define FRAME_IS_PROC     2     /* 过程调用标志，用于标记这是一个独立的函数调用 */

/* 解释器返回码定义 */
#define TCL_OK 0                /* 执行成功 */
#define TCL_ERROR 1             /* 执行出错 */
#define TCL_RETURN 2            /* 触发 return 指令 */
#define TCL_BREAK 3             /* 触发 break 指令 */
#define TCL_CONTINUE 4          /* 触发 continue 指令 */
#define TCL_EXIT 5              /* 触发退出指令 */
#define TCL_YIELD 6             /* 状态机挂起，等待子任务完成 */

/* 状态机状态定义 */
#define ST_TOKENIZE 0           /* 分词阶段：解析指令边界与参数 */
#define ST_EXPAND   1           /* 展开阶段：处理变量与命令替换 */
#define ST_EXECUTE  2           /* 执行阶段：调用 C 处理函数或进入子结界 */
#define ST_RESUME   3           /* 恢复阶段：子结界返回后清理并继续 */
#define ST_COND     6           /* while 循环条件判断预备态 */
#define ST_LOOP     7           /* while 循环执行主体预备态 */
#define ST_IF_COND  8           /* if 分支条件判断预备态 */
#define ST_IF_BODY  9           /* if 分支主体执行预备态 */
#define ST_CATCH    10          /* catch 指令执行前置态 */
#define ST_CATCH_END 11         /* catch 指令执行后置态 */

/* 地址转换宏：将偏移量转换为实际物理指针 */
#define TO_PTR(ctx, offset) ((offset) == TCL_NULL ? 0 : (void *)((ctx)->arena + (offset)))

/* 内部字符串工具函数：计算字符串长度 */
static tcl_u32 t_slen(const tcl_u8 *string) {
    tcl_u32 length = 0;         /* 初始化长度为 0 */
    while (string && string[length]) { /* 遍历直至遇到结束符 */
        length++;               /* 累加长度 */
    }
    return length;              /* 返回总长度 */
}

/* 内部内存工具函数：物理内存拷贝 */
static void t_mcpy(void *dest, const void *src, tcl_u32 count) {
    tcl_u8 *d_ptr = (tcl_u8*)dest; /* 目的地址指针 */
    const tcl_u8 *s_ptr = (const tcl_u8*)src; /* 源地址指针 */
    while (count--) {           /* 循环拷贝指定字节数 */
        *d_ptr++ = *s_ptr++;    /* 逐字节搬运 */
    }
}

/* 内部字符串工具函数：字符串比较 */
static tcl_i32 t_scmp(const tcl_u8 *s1, const tcl_u8 *s2) {
    if (!s1 || !s2) {           /* 判空逻辑 */
        return s1 == s2 ? 0 : (s1 ? -1 : 1); /* 处理空指针比较 */
    }
    while (*s1 && (*s1 == *s2)) { /* 循环比较对应字符 */
        s1++;                   /* 移动到下一个字符 */
        s2++;                   /* 移动到下一个字符 */
    }
    return *(tcl_u8*)s1 - *(tcl_u8*)s2; /* 返回差值以指示顺序 */
}

/* 内部字符串工具函数：字符串转整数 */
static tcl_i32 t_atoi(const tcl_u8 *string) {
    tcl_i32 result = 0;         /* 累加结果变量 */
    tcl_i32 sign = 1;           /* 符号位，默认为正 */
    if (*string == '-') {       /* 处理负号 */
        sign = -1;              /* 标记为负 */
        string++;               /* 跳过负号 */
    }
    while (*string >= '0' && *string <= '9') { /* 循环处理数字字符 */
        result = result * 10 + (*string++ - '0'); /* 累加十进制数值 */
    }
    return result * sign;       /* 返回带符号的最终结果 */
}

/* 内部字符串工具函数：整数转字符串 */
static void t_itoa(tcl_i32 number, tcl_u8 *buffer) {
    tcl_i32 index = 0;          /* 缓冲区写入位置索引 */
    if (number == 0) {          /* 特殊处理零值 */
        buffer[index++] = '0';  /* 写入字符 0 */
        buffer[index] = 0;      /* 写入结束符 */
        return;                 /* 退出 */
    }
    if (number < 0) {           /* 处理负数 */
        buffer[index++] = '-';  /* 写入负号 */
        number = -number;       /* 转为正数处理 */
    }
    tcl_u8 temp_buf[12];        /* 临时存放逆序数字的缓冲区 */
    tcl_i32 digit_count = 0;    /* 位数计数器 */
    while (number > 0) {        /* 循环提取每一位 */
        temp_buf[digit_count++] = (tcl_u8)((number % 10) + '0'); /* 取模获得字符 */
        number /= 10;           /* 进位 */
    }
    while (digit_count > 0) {   /* 将数字从临时区搬运至结果区 */
        buffer[index++] = temp_buf[--digit_count]; /* 逆序写回以保证顺序正确 */
    }
    buffer[index] = 0;          /* 写入结束符 */
}

/* 前置声明 */
const tcl_u8 *tcl_get_result(TclCtx *context);
void tcl_hal_puts(const tcl_u8 *string);

/* 对象头结构体，用于 GC 标记与对象管理 */
typedef struct {
    tcl_u32 size_and_flags;     /* 对象物理大小及标志位（如 MARK/VAR 位） */
    tcl_u32 forward;            /* GC 压缩阶段的目标物理偏移量 */
} ObjHeader;

/* 对象头标志位定义 */
#define OBJ_MARK_BIT 0x80000000 /* GC 存活标记位 */
#define OBJ_VAR_BIT  0x40000000 /* 变量对象标记位，用于递归标记链表 */
#define OBJ_SIZE(header) ((header)->size_and_flags & ~(OBJ_MARK_BIT|OBJ_VAR_BIT)) /* 提取纯净的对象大小 */

/* 上下文结构体相对于 Arena 的对齐起始偏移 */
#define HS ((sizeof(TclCtx) + 15) & ~15) /* 16 字节对齐 */

/* GC 标记函数：深度优先标记所有可达对象 */
static void mark_obj(TclCtx *context, tcl_u32 object_offset) {
    if (object_offset == TCL_NULL) { /* 忽略空对象 */
        return;                 /* 返回 */
    }
    if (object_offset < HS || object_offset >= context->p_top) { /* 越界检查 */
        return;                 /* 返回 */
    }
    /* 根据对象偏移量回溯获取对象头指针 */
    ObjHeader *header = (ObjHeader*)TO_PTR(context, object_offset - sizeof(ObjHeader));
    if (header->size_and_flags & OBJ_MARK_BIT) { /* 若已标记则跳过，防止死循环 */
        return;                 /* 返回 */
    }
    header->size_and_flags |= OBJ_MARK_BIT; /* 设置存活标记位 */
    if (header->size_and_flags & OBJ_VAR_BIT) { /* 如果是变量对象，需递归标记其引用的字段 */
        TclVar *variable = (TclVar*)TO_PTR(context, object_offset); /* 获取变量结构体物理指针 */
        mark_obj(context, variable->name); /* 标记变量名字符串 */
        mark_obj(context, variable->val);  /* 标记变量值对象 */
        mark_obj(context, variable->next); /* 标记链表中的下一个变量 */
    }
}

/* 核心垃圾回收函数：实现 Slide Compacting 算法 */
void tcl_gc(TclCtx *context) {
    TCL_LOG("-- GC 开始 --\n"); /* 输出 GC 启动日志 */
    /* 1. 标记阶段：从所有根对象开始遍历 */
    mark_obj(context, context->result); /* 标记当前执行结果 */
    mark_obj(context, context->g_vars); /* 标记全局变量表 */
    for (tcl_i32 index = 0; index < 16; index++) { /* 标记临时保护根 */
        mark_obj(context, context->tmp_roots[index]); /* 逐一标记根数组元素 */
    }
    tcl_u32 frame_offset = context->curr_f; /* 从当前执行栈帧开始回溯 */
    while (frame_offset != TCL_NULL) { /* 遍历整个调用链 */
        TclFrame *frame = TO_PTR(context, frame_offset); /* 获取栈帧物理指针 */
        mark_obj(context, frame->script); /* 标记当前帧的脚本源码 */
        mark_obj(context, frame->vars);   /* 标记当前帧的局部变量表 */
        mark_obj(context, frame->cond);   /* 标记循环/条件的表达式部分 */
        mark_obj(context, frame->body);   /* 标记循环/条件的主体部分 */
        for (tcl_i32 arg_idx = 0; arg_idx < frame->argc; arg_idx++) { /* 标记参数数组 */
            mark_obj(context, frame->argv[arg_idx]); /* 逐一标记参数对象 */
        }
        frame_offset = frame->parent; /* 移动到父级栈帧 */
    }

    /* 2. 计算迁移地址：顺序扫描 Arena，确定存活对象的压缩后位置 */
    tcl_u32 current_scan = HS;  /* 从 Arena 有效起始位置开始扫描 */
    tcl_u32 next_free_pos = HS; /* 记录压缩后的下一个可用空闲物理位置 */
    while (current_scan < context->p_top) { /* 遍历整个低地址区 */
        ObjHeader *header = (ObjHeader*)TO_PTR(context, current_scan); /* 获取当前扫描位置的对象头 */
        tcl_u32 object_size = OBJ_SIZE(header); /* 获取对象包含头的总大小 */
        if (header->size_and_flags & OBJ_MARK_BIT) { /* 如果该对象存活 */
            header->forward = next_free_pos; /* 记录其目标偏移量 */
            next_free_pos += object_size; /* 累加空闲位置指针 */
        }
        current_scan += object_size; /* 扫描下一个对象 */
    }

    /* 3. 更新指针：根据 forward 记录，修正所有存活对象间的引用偏移量 */
    /* 定义修正宏，内部处理偏移量重定向 */
    #define UPDATE_PTR(ptr_ref) do { \
        if ((ptr_ref) != TCL_NULL && (ptr_ref) >= HS && (ptr_ref) < context->p_top) { \
            ObjHeader *oh = (ObjHeader*)TO_PTR(context, (ptr_ref) - sizeof(ObjHeader)); \
            if (oh->size_and_flags & OBJ_MARK_BIT) (ptr_ref) = oh->forward + sizeof(ObjHeader); \
            else (ptr_ref) = TCL_NULL; \
        } \
    } while(0)

    UPDATE_PTR(context->result); /* 更新全局结果指针 */
    UPDATE_PTR(context->g_vars); /* 更新全局变量指针 */
    for (tcl_i32 index = 0; index < 16; index++) { /* 更新临时根数组 */
        UPDATE_PTR(context->tmp_roots[index]); /* 逐一更新保护根 */
    }
    frame_offset = context->curr_f; /* 回溯更新调用栈引用 */
    while (frame_offset != TCL_NULL) { /* 遍历执行链 */
        TclFrame *frame = TO_PTR(context, frame_offset); /* 获取栈帧指针 */
        UPDATE_PTR(frame->script); /* 更新脚本引用 */
        UPDATE_PTR(frame->vars);   /* 更新局部变量引用 */
        UPDATE_PTR(frame->cond);   /* 更新条件脚本引用 */
        UPDATE_PTR(frame->body);   /* 更新主体脚本引用 */
        for (tcl_i32 arg_idx = 0; arg_idx < frame->argc; arg_idx++) { /* 更新参数数组引用 */
            UPDATE_PTR(frame->argv[arg_idx]); /* 逐一修正参数偏移量 */
        }
        frame_offset = frame->parent; /* 移动到父级 */
    }

    /* 4. 更新变量内部指针：由于变量结构体包含 name/val/next，需深层修正 */
    current_scan = HS;          /* 重新扫描 Arena */
    while (current_scan < context->p_top) { /* 遍历对象池 */
        ObjHeader *header = (ObjHeader*)TO_PTR(context, current_scan); /* 获取头信息 */
        tcl_u32 object_size = OBJ_SIZE(header); /* 获取大小 */
        if ((header->size_and_flags & OBJ_MARK_BIT) && (header->size_and_flags & OBJ_VAR_BIT)) { /* 若为存活变量 */
            TclVar *variable = (TclVar*)TO_PTR(context, current_scan + sizeof(ObjHeader)); /* 定位变量结构体 */
            UPDATE_PTR(variable->name); /* 修正名偏移量 */
            UPDATE_PTR(variable->val);  /* 修正值偏移量 */
            UPDATE_PTR(variable->next); /* 修正链表偏移量 */
        }
        current_scan += object_size; /* 处理下一个 */
    }

    /* 5. 物理搬运：执行最终的内存拷贝，实现对象紧凑化 */
    current_scan = HS;          /* 第三次扫描 Arena */
    next_free_pos = HS;         /* 重置压缩指针 */
    while (current_scan < context->p_top) { /* 遍历池 */
        ObjHeader *header = (ObjHeader*)TO_PTR(context, current_scan); /* 获取原位置对象头 */
        tcl_u32 object_size = OBJ_SIZE(header); /* 获取大小 */
        if (header->size_and_flags & OBJ_MARK_BIT) { /* 如果标记为存活 */
            header->size_and_flags &= ~OBJ_MARK_BIT; /* 清除存活标记位以备下次使用 */
            if (current_scan != header->forward) { /* 如果物理位置发生变动 */
                t_mcpy(TO_PTR(context, header->forward), TO_PTR(context, current_scan), object_size); /* 物理搬运整个对象块 */
            }
            next_free_pos += object_size; /* 更新下一个写入位置 */
        }
        current_scan += object_size; /* 继续扫描 */
    }
    context->p_top = next_free_pos; /* 修正分配游标至压缩后的边界 */
    TCL_LOG("-- GC 结束 --\n"); /* 输出 GC 完成日志 */
}

/* 变量区内存分配函数：带自动 GC 触发机制 */
static tcl_u32 tcl_alc_p(TclCtx *context, tcl_u32 byte_count) {
    /* 计算包含对象头在内的总大小，并进行 8 字节对齐 */
    tcl_u32 total_size = (byte_count + sizeof(ObjHeader) + 7) & ~7;
    if (context->p_top + total_size > context->t_bot) { /* 检查内存溢出 */
        tcl_gc(context);        /* 空间不足，强制启动垃圾回收 */
        if (context->p_top + total_size > context->t_bot) { /* 回收后若依然不足 */
            context->status = TCL_ERROR; /* 设置全局错误码 */
            return TCL_NULL;    /* 分配失败，返回空 */
        }
    }
    tcl_u32 allocated_offset = context->p_top; /* 记录当前分配点偏移量 */
    ObjHeader *header = (ObjHeader*)TO_PTR(context, allocated_offset); /* 获取对象头空间 */
    header->size_and_flags = total_size; /* 初始化对象头的大小字段 */
    context->p_top += total_size; /* 移动分配游标 */
    tcl_u8 *data_ptr = TO_PTR(context, allocated_offset + sizeof(ObjHeader)); /* 获取有效载荷起始地址 */
    for (tcl_u32 index = 0; index < byte_count; index++) { /* 内存清零 */
        data_ptr[index] = 0;    /* 逐字节初始化为零 */
    }
    return allocated_offset + sizeof(ObjHeader); /* 返回数据区的物理偏移量 */
}

/* 栈帧区内存分配函数：从 Arena 顶部向下生长 */
static tcl_u32 tcl_alc_t(TclCtx *context, tcl_u32 byte_count) { 
    byte_count = (byte_count + 7) & ~7; /* 对齐申请大小 */
    if (context->p_top + byte_count > context->t_bot) { /* 检查与变量区的物理碰撞 */
        tcl_gc(context);        /* 触发同步 GC 尝试腾出空间 */
        if (context->p_top + byte_count > context->t_bot) { /* 依然不足则报错 */
            return TCL_NULL;    /* 物理空间枯竭 */
        }
    }
    context->t_bot -= byte_count; /* 游标向下移动，预留空间 */
    for (tcl_u32 index = 0; index < byte_count; index++) { /* 清理新分配的栈帧内存 */
        context->arena[context->t_bot + index] = 0; /* 逐字节清零 */
    }
    return context->t_bot;      /* 返回栈帧区的起始物理偏移量 */
}

/* 变量查找辅助函数：在指定栈帧链及全局表中查找变量节点 */
static tcl_u32 tcl_find_var_node(TclCtx *context, tcl_u32 frame_offset, const tcl_u8 *name) {
    while (frame_offset != TCL_NULL) { /* 遍历栈帧链 */
        TclFrame *frame = TO_PTR(context, frame_offset); /* 获取当前栈帧指针 */
        tcl_u32 var_offset = frame->vars; /* 获取当前帧的变量链表头 */
        while (var_offset != TCL_NULL) { /* 遍历变量链表 */
            TclVar *variable = TO_PTR(context, var_offset); /* 获取变量结构体指针 */
            if (t_scmp(name, TO_PTR(context, variable->name)) == 0) { /* 名称匹配 */
                return var_offset; /* 返回找到的变量节点偏移量 */
            }
            var_offset = variable->next; /* 移动到下一个变量 */
        }
        if (frame->flags & FRAME_SHARE_SCOPE) { /* 如果当前帧共享作用域（如 eval/if） */
            frame_offset = frame->parent; /* 继续在父帧中查找 */
        } else {                /* 否则停止向上查找 */
            break;              /* 退出循环 */
        }
    }
    tcl_u32 global_var_offset = context->g_vars; /* 最后在全局变量表中查找 */
    while (global_var_offset != TCL_NULL) { /* 遍历全局表 */
        TclVar *variable = TO_PTR(context, global_var_offset); /* 获取变量指针 */
        if (t_scmp(name, TO_PTR(context, variable->name)) == 0) { /* 名称匹配 */
            return global_var_offset; /* 返回全局变量节点偏移量 */
        }
        global_var_offset = variable->next; /* 移动到下一个 */
    }
    return TCL_NULL;            /* 未找到，返回空 */
}

/* 变量值获取函数：返回变量关联的字符串偏移量 */
static tcl_u32 tcl_get_var(TclCtx *context, tcl_u32 frame_offset, const tcl_u8 *name) {
    tcl_u32 var_offset = tcl_find_var_node(context, frame_offset, name); /* 查找节点 */
    if (var_offset == TCL_NULL) { /* 若未找到 */
        return TCL_NULL;        /* 返回空 */
    }
    TclVar *variable = TO_PTR(context, var_offset); /* 获取物理指针 */
    if (variable->flags & VAR_LINK) { /* 如果是 upvar 链接对象 */
        TclVar *linked_variable = TO_PTR(context, variable->val); /* 获取实际指向的变量 */
        return linked_variable->val; /* 返回实际值 */
    }
    return variable->val;       /* 返回普通变量值 */
}

/* 变量设置函数：创建或更新变量值 */
static tcl_i32 tcl_set_var(TclCtx *context, tcl_u32 frame_offset, tcl_u32 name_offset, tcl_u32 value_offset) {
    context->tmp_roots[4] = name_offset; /* 保护名对象防止 GC */
    context->tmp_roots[5] = value_offset; /* 保护值对象防止 GC */
    /* 尝试定位已有变量节点 */
    tcl_u32 existing_var_offset = tcl_find_var_node(context, frame_offset, TO_PTR(context, context->tmp_roots[4]));
    if (existing_var_offset != TCL_NULL) { /* 如果变量已存在 */
        context->tmp_roots[6] = existing_var_offset; /* 保护该节点 */
        if (context->tmp_roots[5] == TCL_NULL) { /* 如果设置为空值 */
            TclVar *variable = TO_PTR(context, context->tmp_roots[6]); /* 获取变量指针 */
            if (variable->flags & VAR_LINK) { /* 处理链接 */
                variable = TO_PTR(context, variable->val); /* 指向目标变量 */
            }
            variable->val = TCL_NULL; /* 清空值 */
            context->tmp_roots[4] = context->tmp_roots[5] = context->tmp_roots[6] = TCL_NULL; /* 清理保护根 */
            return TCL_OK;      /* 返回成功 */
        }
        /* 计算新值长度并分配空间 */
        tcl_u32 value_length = t_slen(TO_PTR(context, context->tmp_roots[5])) + 1;
        tcl_u32 new_value_offset = tcl_alc_p(context, value_length);
        if (new_value_offset == TCL_NULL) { /* 分配失败 */
            context->tmp_roots[4] = context->tmp_roots[5] = context->tmp_roots[6] = TCL_NULL;
            return TCL_ERROR;
        }
        /* 拷贝新值字符串 */
        t_mcpy(TO_PTR(context, new_value_offset), TO_PTR(context, context->tmp_roots[5]), value_length);
        TclVar *variable = TO_PTR(context, context->tmp_roots[6]); /* 更新引用 */
        if (variable->flags & VAR_LINK) {
            variable = TO_PTR(context, variable->val);
        }
        variable->val = new_value_offset; /* 指向新值 */
        context->tmp_roots[4] = context->tmp_roots[5] = context->tmp_roots[6] = TCL_NULL;
        return TCL_OK;
    }
    /* 变量不存在，需要新建。首先确定变量应存放的物理栈帧 */
    tcl_u32 target_frame_offset = frame_offset;
    while (target_frame_offset != TCL_NULL) {
        TclFrame *parent_frame = TO_PTR(context, target_frame_offset);
        if (!(parent_frame->flags & FRAME_SHARE_SCOPE)) { /* 找到真正的局部作用域边界 */
            break;
        }
        target_frame_offset = parent_frame->parent; /* 向上穿透共享作用域 */
    }
    context->tmp_roots[6] = target_frame_offset; /* 保护目标栈帧偏移 */
    
    /* 分配变量结构体空间 */
    tcl_u32 new_var_offset = tcl_alc_p(context, sizeof(TclVar));
    if (new_var_offset == TCL_NULL) {
        context->tmp_roots[4] = context->tmp_roots[5] = context->tmp_roots[6] = TCL_NULL;
        return TCL_ERROR;
    }
    context->tmp_roots[7] = new_var_offset;
    TclVar *new_variable = TO_PTR(context, context->tmp_roots[7]);
    new_variable->name = new_variable->val = new_variable->next = TCL_NULL; /* 初始化 */
    new_variable->flags = 0;
    /* 在对象头中标记这是一个变量对象，以便 GC 深度扫描 */
    ((ObjHeader*)TO_PTR(context, context->tmp_roots[7] - sizeof(ObjHeader)))->size_and_flags |= OBJ_VAR_BIT;
    
    /* 分配变量名字符串空间并拷贝 */
    tcl_u32 new_name_offset = tcl_alc_p(context, t_slen(TO_PTR(context, context->tmp_roots[4])) + 1);
    if (new_name_offset == TCL_NULL) {
        context->tmp_roots[4] = context->tmp_roots[5] = context->tmp_roots[6] = context->tmp_roots[7] = TCL_NULL;
        return TCL_ERROR;
    }
    context->tmp_roots[8] = new_name_offset;
    
    tcl_u32 final_value_offset = TCL_NULL;
    if (context->tmp_roots[5] != TCL_NULL) { /* 分配变量值空间并拷贝 */
        final_value_offset = tcl_alc_p(context, t_slen(TO_PTR(context, context->tmp_roots[5])) + 1);
        if (final_value_offset == TCL_NULL) {
            context->tmp_roots[4] = context->tmp_roots[5] = context->tmp_roots[6] = context->tmp_roots[7] = context->tmp_roots[8] = TCL_NULL;
            return TCL_ERROR;
        }
    }
    /* 物理数据搬运 */
    t_mcpy(TO_PTR(context, context->tmp_roots[8]), TO_PTR(context, context->tmp_roots[4]), t_slen(TO_PTR(context, context->tmp_roots[4])) + 1);
    if (context->tmp_roots[5] != TCL_NULL) {
        t_mcpy(TO_PTR(context, final_value_offset), TO_PTR(context, context->tmp_roots[5]), t_slen(TO_PTR(context, context->tmp_roots[5])) + 1);
    }
    
    /* 将新变量链入对应的作用域或全局表 */
    TclFrame *target_frame = (context->tmp_roots[6] == TCL_NULL) ? 0 : TO_PTR(context, context->tmp_roots[6]);
    tcl_u32 *head_ptr = target_frame ? &target_frame->vars : &context->g_vars;
    
    new_variable = TO_PTR(context, context->tmp_roots[7]);
    new_variable->name = context->tmp_roots[8];
    new_variable->val = final_value_offset;
    new_variable->next = *head_ptr; /* 头插法 */
    *head_ptr = context->tmp_roots[7]; /* 更新链表头 */
    
    context->tmp_roots[4] = context->tmp_roots[5] = context->tmp_roots[6] = context->tmp_roots[7] = context->tmp_roots[8] = TCL_NULL;
    return TCL_OK;
}

/* set 指令实现：变量读写 */
static tcl_i32 tcl_cmd_set(TclCtx *context, tcl_i32 arg_count, tcl_u32 *arg_values) {
    if (arg_count < 2) {        /* 参数不足 */
        return TCL_ERROR;       /* 报错 */
    }
    if (arg_count == 2) {       /* 读取变量场景 */
        tcl_u32 value = tcl_get_var(context, context->curr_f, TO_PTR(context, arg_values[1]));
        if (value == TCL_NULL) { /* 变量不存在 */
            return TCL_ERROR;   /* 报错 */
        }
        context->result = value; /* 将变量值设为当前结果 */
    } else {                    /* 写入变量场景 */
        if (tcl_set_var(context, context->curr_f, arg_values[1], arg_values[2]) != TCL_OK) {
            return TCL_ERROR;   /* 分配失败报错 */
        }
        context->result = arg_values[2]; /* 返回设置的值 */
    }
    return TCL_OK;              /* 成功 */
}

/* proc 指令实现：注册脚本级函数 */
static tcl_i32 tcl_cmd_proc(TclCtx *context, tcl_i32 arg_count, tcl_u32 *arg_values) {
    if (arg_count < 4) {        /* proc name args body */
        return TCL_ERROR;       /* 参数不足 */
    }
    /* 构造内部函数体标识名 p:<name> */
    context->tmp_roots[0] = tcl_alc_p(context, 64);
    if (context->tmp_roots[0] == TCL_NULL) {
        return TCL_ERROR;
    }
    tcl_u8 *proc_name_ptr = TO_PTR(context, context->tmp_roots[0]);
    proc_name_ptr[0] = 'p';     /* 标记为 proc 体 */
    proc_name_ptr[1] = ':';     /* 分隔符 */
    const tcl_u8 *original_name = TO_PTR(context, arg_values[1]);
    tcl_i32 index = 0;
    while (original_name[index] && index < 60) { /* 拷贝原名 */
        proc_name_ptr[index + 2] = original_name[index];
        index++;
    }
    proc_name_ptr[index + 2] = 0; /* 结束符 */
    /* 存储函数体源码至全局表 */
    if (tcl_set_var(context, TCL_NULL, context->tmp_roots[0], arg_values[3]) != TCL_OK) {
        context->tmp_roots[0] = TCL_NULL;
        return TCL_ERROR;
    }
    /* 构造内部参数列表标识名 a:<name> */
    context->tmp_roots[1] = tcl_alc_p(context, 64);
    if (context->tmp_roots[1] == TCL_NULL) {
        context->tmp_roots[0] = TCL_NULL;
        return TCL_ERROR;
    }
    tcl_u8 *args_name_ptr = TO_PTR(context, context->tmp_roots[1]);
    t_mcpy(args_name_ptr, TO_PTR(context, context->tmp_roots[0]), 64);
    args_name_ptr[0] = 'a';     /* 标记为 args 列表 */
    /* 存储参数定义至全局表 */
    tcl_i32 result = tcl_set_var(context, TCL_NULL, context->tmp_roots[1], arg_values[2]);
    context->tmp_roots[0] = context->tmp_roots[1] = TCL_NULL;
    return result;
}

/* if 指令实现：流程控制挂起 */
static tcl_i32 tcl_cmd_if(TclCtx *context, tcl_i32 arg_count, tcl_u32 *arg_values) {
    if (arg_count < 3) {        /* if cond body */
        return TCL_ERROR;
    }
    TclFrame *frame = TO_PTR(context, context->curr_f);
    frame->cond = arg_values[1]; /* 设置待评估条件 */
    frame->body = arg_values[2]; /* 设置待执行主体 */
    frame->state = ST_IF_COND;  /* 状态机跳转至 if 条件判断态 */
    return TCL_YIELD;           /* 挂起当前指令，进入子结界执行 */
}

/* while 指令实现：循环控制挂起 */
static tcl_i32 tcl_cmd_while(TclCtx *context, tcl_i32 arg_count, tcl_u32 *arg_values) {
    if (arg_count < 3) {        /* while cond body */
        return TCL_ERROR;
    }
    TclFrame *frame = TO_PTR(context, context->curr_f);
    frame->cond = arg_values[1]; /* 设置循环条件 */
    frame->body = arg_values[2]; /* 设置循环主体 */
    frame->state = ST_COND;     /* 状态机跳转至 while 条件判断态 */
    return TCL_YIELD;           /* 挂起 */
}

/* expr 指令实现：基础数学与逻辑运算 */
static tcl_i32 tcl_cmd_expr(TclCtx *context, tcl_i32 arg_count, tcl_u32 *arg_values) {
    if (arg_count < 2) {
        return TCL_ERROR;
    }
    tcl_u8 *script_ptr = TO_PTR(context, arg_values[1]);
    /* 延迟求值支持：处理带花括号 {} 的表达式 */
    if (script_ptr[0] == '{') {
        tcl_u32 length = t_slen(script_ptr);
        context->tmp_roots[0] = tcl_alc_p(context, length); /* 分配空间去除花括号 */
        if (context->tmp_roots[0] == TCL_NULL) {
            return TCL_ERROR;
        }
        tcl_u8 *dest_ptr = TO_PTR(context, context->tmp_roots[0]);
        t_mcpy(dest_ptr, script_ptr + 1, length - 2); /* 剥离 {} */
        dest_ptr[length - 2] = 0; /* 结束符 */
        /* 分配新栈帧执行内部表达式 */
        tcl_u32 sub_frame_offset = tcl_alc_t(context, sizeof(TclFrame));
        if (sub_frame_offset == TCL_NULL) {
            return TCL_ERROR;
        }
        TclFrame *sub_frame = TO_PTR(context, sub_frame_offset);
        sub_frame->script = context->tmp_roots[0];
        sub_frame->pc = 0;
        sub_frame->vars = TCL_NULL;
        sub_frame->parent = context->curr_f;
        sub_frame->state = ST_TOKENIZE;
        sub_frame->flags = FRAME_SHARE_SCOPE; /* 表达式在当前作用域执行 */
        sub_frame->cond = sub_frame->body = sub_frame->result = TCL_NULL;
        sub_frame->argc = sub_frame->exp_idx = 0;
        context->tmp_roots[0] = TCL_NULL;
        /* 挂起当前帧，设置为 RESUME 态以接收结果 */
        ((TclFrame*)TO_PTR(context, context->curr_f))->state = ST_RESUME;
        context->curr_f = sub_frame_offset; /* 切换执行上下文 */
        return TCL_YIELD;       /* 挂起 */
    }
    /* 基础算术三元运算实现：expr val1 op val2 */
    if (arg_count == 4) {
        tcl_i32 val1 = t_atoi(TO_PTR(context, arg_values[1]));
        tcl_i32 val2 = t_atoi(TO_PTR(context, arg_values[3]));
        const tcl_u8 *operator = TO_PTR(context, arg_values[2]);
        tcl_i32 result = 0;
        if (t_scmp(operator, (tcl_u8*)"==") == 0) result = (val1 == val2);
        else if (t_scmp(operator, (tcl_u8*)"!=") == 0) result = (val1 != val2);
        else if (t_scmp(operator, (tcl_u8*)">") == 0) result = (val1 > val2);
        else if (t_scmp(operator, (tcl_u8*)">=") == 0) result = (val1 >= val2);
        else if (t_scmp(operator, (tcl_u8*)"<") == 0) result = (val1 < val2);
        else if (t_scmp(operator, (tcl_u8*)"<=") == 0) result = (val1 <= val2);
        else if (t_scmp(operator, (tcl_u8*)"+") == 0) result = (val1 + val2);
        else if (t_scmp(operator, (tcl_u8*)"-") == 0) result = (val1 - val2);
        else if (t_scmp(operator, (tcl_u8*)"*") == 0) result = (val1 * val2);
        else if (t_scmp(operator, (tcl_u8*)"/") == 0) result = (val2 ? val1 / val2 : 0);
        else if (t_scmp(operator, (tcl_u8*)"%") == 0) result = (val2 ? val1 % val2 : 0);
        tcl_u32 result_offset = tcl_alc_p(context, 12); /* 分配 12 字节存放整数字符串 */
        if (result_offset != TCL_NULL) {
            t_itoa(result, TO_PTR(context, result_offset));
            context->result = result_offset; /* 存储计算结果 */
        }
    } else {                    /* 单个值直接作为结果（用于展开后的求值） */
        context->result = arg_values[1];
    }
    return TCL_OK;
}

/* return 指令实现：显式退出结界并返回值 */
static tcl_i32 tcl_cmd_return(TclCtx *context, tcl_i32 arg_count, tcl_u32 *arg_values) {
    if (arg_count == 2) {
        context->result = arg_values[1]; /* 设置返回值 */
    }
    return TCL_RETURN;          /* 抛出返回状态码 */
}

/* break 指令实现：终止循环 */
static tcl_i32 tcl_cmd_break(TclCtx *context, tcl_i32 arg_count, tcl_u32 *arg_values) {
    return TCL_BREAK;           /* 抛出中断状态码 */
}

/* continue 指令实现：跳过当前循环步 */
static tcl_i32 tcl_cmd_continue(TclCtx *context, tcl_i32 arg_count, tcl_u32 *arg_values) {
    return TCL_CONTINUE;        /* 抛出继续状态码 */
}

/* error 指令实现：主动抛出运行时错误 */
static tcl_i32 tcl_cmd_error(TclCtx *context, tcl_i32 arg_count, tcl_u32 *arg_values) {
    if (arg_count > 1) {
        context->result = arg_values[1]; /* 设置错误描述信息 */
    }
    return TCL_ERROR;           /* 抛出错误状态码 */
}

/* eval 指令实现：二次解析并执行脚本 */
static tcl_i32 tcl_cmd_eval(TclCtx *context, tcl_i32 arg_count, tcl_u32 *arg_values) {
    if (arg_count < 2) {
        return TCL_ERROR;
    }
    /* 分配新栈帧 */
    tcl_u32 frame_offset = tcl_alc_t(context, sizeof(TclFrame));
    if (frame_offset == TCL_NULL) {
        return TCL_ERROR;
    }
    TclFrame *frame = TO_PTR(context, frame_offset);
    frame->script = arg_values[1]; /* 设置脚本内容 */
    frame->pc = 0;              /* 重置程序计数器 */
    frame->vars = TCL_NULL;     /* eval 不创建新的变量作用域 */
    frame->parent = context->curr_f; /* 记录父帧 */
    frame->state = ST_TOKENIZE; /* 初始进入分词态 */
    frame->flags = FRAME_SHARE_SCOPE; /* 标记为共享作用域 */
    frame->cond = frame->body = frame->result = TCL_NULL;
    frame->argc = frame->exp_idx = 0;
    context->curr_f = frame_offset; /* 切换上下文 */
    return TCL_YIELD;           /* 挂起 */
}

/* catch 指令实现：捕获子脚本执行异常 */
static tcl_i32 tcl_cmd_catch(TclCtx *context, tcl_i32 arg_count, tcl_u32 *arg_values) {
    if (arg_count < 2) {
        return TCL_ERROR;
    }
    context->tmp_roots[8] = arg_values[1]; /* 保护待执行脚本 */
    context->tmp_roots[9] = arg_count > 2 ? arg_values[2] : TCL_NULL; /* 保护变量名 */
    /* 分配执行子脚本的栈帧 */
    tcl_u32 frame_offset = tcl_alc_t(context, sizeof(TclFrame));
    if (frame_offset == TCL_NULL) {
        return TCL_ERROR;
    }
    TclFrame *current_frame = TO_PTR(context, context->curr_f);
    current_frame->cond = context->tmp_roots[8]; /* 暂存脚本偏移 */
    current_frame->body = context->tmp_roots[9]; /* 暂存结果变量名偏移 */
    current_frame->state = ST_CATCH_END; /* 标记 catch 结束后返回的位置 */
    
    TclFrame *new_frame = TO_PTR(context, frame_offset);
    new_frame->script = context->tmp_roots[8];
    new_frame->pc = 0;
    new_frame->vars = TCL_NULL;
    new_frame->parent = context->curr_f;
    new_frame->state = ST_TOKENIZE;
    new_frame->flags = FRAME_SHARE_SCOPE; /* catch 默认在当前作用域执行 */
    new_frame->cond = new_frame->body = new_frame->result = TCL_NULL;
    new_frame->argc = new_frame->exp_idx = 0;
    
    context->tmp_roots[8] = context->tmp_roots[9] = TCL_NULL;
    context->curr_f = frame_offset; /* 启动子结界 */
    return TCL_YIELD;
}

/* upvar 指令实现：变量跨作用域链接 */
static tcl_i32 tcl_cmd_upvar(TclCtx *context, tcl_i32 arg_count, tcl_u32 *arg_values) {
    if (arg_count < 3) {
        return TCL_ERROR;
    }
    tcl_i32 level = 1;          /* 默认向上查找 1 层 */
    tcl_i32 arg_index = 1;      /* 参数解析起始索引 */
    /* 处理可选的 level 参数（如 upvar 2 ... 或 upvar #0 ...） */
    const tcl_u8 *first_arg = TO_PTR(context, arg_values[1]);
    if (arg_count > 3 && first_arg[0] != '#' && !(first_arg[0] >= '0' && first_arg[0] <= '9')) {
        /* 不是数字也不是 #，说明省略了 level */
    } else if (arg_count > 3) {
        level = t_atoi(first_arg); /* TODO: 处理 #0 绝对层级，目前暂按相对层级 */
        arg_index = 2;
    }
    /* 回溯查找目标栈帧 */
    tcl_u32 target_frame_offset = context->curr_f;
    for (tcl_i32 index = 0; index < level && target_frame_offset != TCL_NULL; index++) {
        target_frame_offset = ((TclFrame*)TO_PTR(context, target_frame_offset))->parent;
    }
    context->tmp_roots[12] = arg_values[arg_index];     /* 目标变量名 */
    context->tmp_roots[13] = arg_values[arg_index + 1]; /* 本地变量名 */
    /* 在目标帧中查找变量节点 */
    tcl_u32 target_var_offset = tcl_find_var_node(context, target_frame_offset, TO_PTR(context, context->tmp_roots[12]));
    if (target_var_offset == TCL_NULL) { /* 如果目标不存在，先创建它 */
        if (tcl_set_var(context, target_frame_offset, context->tmp_roots[12], TCL_NULL) != TCL_OK) {
            context->tmp_roots[12] = context->tmp_roots[13] = TCL_NULL;
            return TCL_ERROR;
        }
        target_var_offset = tcl_find_var_node(context, target_frame_offset, TO_PTR(context, context->tmp_roots[12]));
    }
    context->tmp_roots[14] = target_var_offset;
    /* 在当前帧创建链接变量节点 */
    tcl_u32 new_var_offset = tcl_alc_p(context, sizeof(TclVar));
    if (new_var_offset == TCL_NULL) {
        context->tmp_roots[12] = context->tmp_roots[13] = context->tmp_roots[14] = TCL_NULL;
        return TCL_ERROR;
    }
    context->tmp_roots[15] = new_var_offset;
    TclVar *new_variable = TO_PTR(context, context->tmp_roots[15]);
    new_variable->name = new_variable->val = new_variable->next = TCL_NULL;
    new_variable->flags = VAR_LINK; /* 关键：标记为链接变量 */
    ((ObjHeader*)TO_PTR(context, context->tmp_roots[15] - sizeof(ObjHeader)))->size_and_flags |= OBJ_VAR_BIT;
    
    /* 分配本地变量名空间 */
    tcl_u32 local_name_offset = tcl_alc_p(context, t_slen(TO_PTR(context, context->tmp_roots[13])) + 1);
    if (local_name_offset == TCL_NULL) {
        context->tmp_roots[12] = context->tmp_roots[13] = context->tmp_roots[14] = context->tmp_roots[15] = TCL_NULL;
        return TCL_ERROR;
    }
    t_mcpy(TO_PTR(context, local_name_offset), TO_PTR(context, context->tmp_roots[13]), t_slen(TO_PTR(context, context->tmp_roots[13])) + 1);
    
    TclFrame *current_frame = TO_PTR(context, context->curr_f);
    new_variable = TO_PTR(context, context->tmp_roots[15]);
    new_variable->name = local_name_offset;
    new_variable->val = context->tmp_roots[14]; /* 链接至目标变量节点 */
    new_variable->next = current_frame->vars; /* 链入本地局部变量表 */
    current_frame->vars = context->tmp_roots[15];
    
    context->tmp_roots[12] = context->tmp_roots[13] = context->tmp_roots[14] = context->tmp_roots[15] = TCL_NULL;
    return TCL_OK;
}

/* uplevel 指令实现：在父级作用域执行脚本 */
static tcl_i32 tcl_cmd_uplevel(TclCtx *context, tcl_i32 arg_count, tcl_u32 *arg_values) {
    if (arg_count < 2) {
        return TCL_ERROR;
    }
    tcl_u32 parent_frame_offset = context->curr_f;
    tcl_i32 level = 1;
    tcl_i32 arg_index = 1;
    /* 解析可选层级参数 */
    const tcl_u8 *first_arg = TO_PTR(context, arg_values[1]);
    if (arg_count > 2 && first_arg[0] != '{' && first_arg[0] != '[') {
        level = t_atoi(first_arg);
        arg_index = 2;
    }
    /* 回溯层级 */
    for (tcl_i32 index = 0; index < level && parent_frame_offset != TCL_NULL; index++) {
        parent_frame_offset = ((TclFrame*)TO_PTR(context, parent_frame_offset))->parent;
    }
    /* 分配新栈帧，但 parent 指向回溯到的层级 */
    tcl_u32 frame_offset = tcl_alc_t(context, sizeof(TclFrame));
    if (frame_offset == TCL_NULL) {
        return TCL_ERROR;
    }
    TclFrame *frame = TO_PTR(context, frame_offset);
    frame->script = arg_values[arg_index];
    frame->pc = 0;
    frame->vars = TCL_NULL;
    frame->parent = parent_frame_offset; /* 关键：重定向执行上下文父级 */
    frame->state = ST_TOKENIZE;
    frame->flags = FRAME_SHARE_SCOPE;
    frame->cond = frame->body = frame->result = TCL_NULL;
    frame->argc = frame->exp_idx = 0;
    context->curr_f = frame_offset;
    return TCL_YIELD;
}

/* list 指令实现：将参数合并为标准的 Tcl 列表 */
static tcl_i32 tcl_cmd_list(TclCtx *context, tcl_i32 arg_count, tcl_u32 *arg_values) {
    tcl_u32 total_length = 0;
    for (tcl_i32 index = 1; index < arg_count; index++) { /* 预估长度，包含空格与花括号转义 */
        total_length += t_slen(TO_PTR(context, arg_values[index])) + 3;
    }
    tcl_u32 new_offset = tcl_alc_p(context, total_length + 1);
    if (new_offset == TCL_NULL) {
        return TCL_ERROR;
    }
    tcl_u8 *dest_ptr = TO_PTR(context, new_offset);
    for (tcl_i32 index = 1; index < arg_count; index++) {
        const tcl_u8 *string_ptr = TO_PTR(context, arg_values[index]);
        tcl_i32 need_brace = 0; /* 检查是否包含特殊字符需要 {} 包裹 */
        for (tcl_i32 char_idx = 0; string_ptr[char_idx]; char_idx++) {
            if (string_ptr[char_idx] == ' ' || string_ptr[char_idx] == '\t' || string_ptr[char_idx] == '\n') {
                need_brace = 1;
                break;
            }
        }
        if (need_brace) *dest_ptr++ = '{';
        tcl_u32 len = t_slen(string_ptr);
        t_mcpy(dest_ptr, string_ptr, len);
        dest_ptr += len;
        if (need_brace) *dest_ptr++ = '}';
        if (index < arg_count - 1) *dest_ptr++ = ' '; /* 参数间添加空格 */
    }
    *dest_ptr = 0; /* 结束符 */
    context->result = new_offset;
    return TCL_OK;
}

/* llength 指令实现：获取列表元素数量 */
static tcl_i32 tcl_cmd_llength(TclCtx *context, tcl_i32 arg_count, tcl_u32 *arg_values) {
    if (arg_count < 2) {
        return TCL_ERROR;
    }
    const tcl_u8 *string_ptr = TO_PTR(context, arg_values[1]);
    tcl_i32 count = 0;          /* 元素计数 */
    tcl_i32 depth = 0;          /* 花括号嵌套深度 */
    while (*string_ptr) {
        while (*string_ptr == ' ' || *string_ptr == '\t' || *string_ptr == '\n') string_ptr++; /* 跳过空白 */
        if (!*string_ptr) break;
        count++;                /* 发现新元素 */
        if (*string_ptr == '{') { /* 处理包裹元素 */
            depth = 1;
            string_ptr++;
            while (*string_ptr && depth > 0) {
                if (*string_ptr == '{') depth++;
                else if (*string_ptr == '}') depth--;
                string_ptr++;
            }
        } else {                /* 处理普通元素 */
            while (*string_ptr && *string_ptr != ' ' && *string_ptr != '\t' && *string_ptr != '\n') string_ptr++;
        }
    }
    tcl_u32 result_offset = tcl_alc_p(context, 12);
    if (result_offset != TCL_NULL) {
        t_itoa(count, TO_PTR(context, result_offset));
        context->result = result_offset;
    }
    return TCL_OK;
}

/* lindex 指令实现：按索引提取列表元素 */
static tcl_i32 tcl_cmd_lindex(TclCtx *context, tcl_i32 arg_count, tcl_u32 *arg_values) {
    if (arg_count < 3) {
        return TCL_ERROR;
    }
    const tcl_u8 *string_ptr = TO_PTR(context, arg_values[1]);
    tcl_i32 target_index = t_atoi(TO_PTR(context, arg_values[2])); /* 获取目标索引 */
    tcl_i32 current_count = 0;
    tcl_i32 depth = 0;
    while (*string_ptr) {
        while (*string_ptr == ' ' || *string_ptr == '\t' || *string_ptr == '\n') string_ptr++;
        if (!*string_ptr) break;
        const tcl_u8 *element_start = string_ptr;
        tcl_i32 element_len = 0;
        if (*string_ptr == '{') { /* 包裹提取 */
            string_ptr++;
            element_start = string_ptr;
            depth = 1;
            while (*string_ptr && depth > 0) {
                if (*string_ptr == '{') depth++;
                else if (*string_ptr == '}') depth--;
                string_ptr++;
            }
            element_len = string_ptr - element_start - 1; /* 长度不计外层 } */
        } else {                /* 普通提取 */
            while (*string_ptr && *string_ptr != ' ' && *string_ptr != '\t' && *string_ptr != '\n') string_ptr++;
            element_len = string_ptr - element_start;
        }
        if (current_count == target_index) { /* 命中索引 */
            tcl_u32 result_offset = tcl_alc_p(context, element_len + 1);
            if (result_offset == TCL_NULL) return TCL_ERROR;
            t_mcpy(TO_PTR(context, result_offset), element_start, element_len);
            ((tcl_u8*)TO_PTR(context, result_offset))[element_len] = 0;
            context->result = result_offset;
            return TCL_OK;
        }
        current_count++;
    }
    context->result = TCL_NULL; /* 索引越界返回空 */
    return TCL_OK;
}

/* lrange 指令实现：提取子列表 */
static tcl_i32 tcl_cmd_lrange(TclCtx *context, tcl_i32 arg_count, tcl_u32 *arg_values) {
    if (arg_count < 4) {
        return TCL_ERROR;
    }
    tcl_i32 first_index = t_atoi(TO_PTR(context, arg_values[2]));
    tcl_i32 last_index = tcl_i32_MAX;
    if (t_scmp(TO_PTR(context, arg_values[3]), (tcl_u8*)"end") != 0) {
        last_index = t_atoi(TO_PTR(context, arg_values[3]));
    }
    /* 分配结果空间，保守起见分配与原字符串等长空间 */
    tcl_u32 result_list_offset = tcl_alc_p(context, t_slen(TO_PTR(context, arg_values[1])) + 1);
    if (result_list_offset == TCL_NULL) return TCL_ERROR;
    tcl_u8 *dest_ptr = TO_PTR(context, result_list_offset);
    const tcl_u8 *string_ptr = TO_PTR(context, arg_values[1]);
    tcl_i32 current_count = 0;
    while (*string_ptr) {
        while (*string_ptr == ' ' || *string_ptr == '\t' || *string_ptr == '\n') string_ptr++;
        if (!*string_ptr) break;
        const tcl_u8 *element_start = string_ptr;
        tcl_i32 depth = 0;
        if (*string_ptr == '{') {
            string_ptr++;
            element_start = string_ptr;
            depth = 1;
            while (*string_ptr && depth > 0) {
                if (*string_ptr == '{') depth++;
                else if (*string_ptr == '}') depth--;
                string_ptr++;
            }
        } else {
            while (*string_ptr && *string_ptr != ' ' && *string_ptr != '\t' && *string_ptr != '\n') string_ptr++;
        }
        /* 范围判定 */
        if (current_count >= first_index && (last_index == tcl_i32_MAX || current_count <= last_index)) {
            if (dest_ptr != TO_PTR(context, result_list_offset)) *dest_ptr++ = ' '; /* 添加分隔空格 */
            tcl_i32 len = string_ptr - element_start - (depth > 0 ? 1 : 0);
            t_mcpy(dest_ptr, element_start, len);
            dest_ptr += len;
        }
        current_count++;
    }
    *dest_ptr = 0;
    context->result = result_list_offset;
    return TCL_OK;
}

/* unset 指令实现：销毁变量 */
static tcl_i32 tcl_cmd_unset(TclCtx *context, tcl_i32 arg_count, tcl_u32 *arg_values) {
    if (arg_count < 2) return TCL_ERROR;
    const tcl_u8 *name = TO_PTR(context, arg_values[1]);
    tcl_u32 frame_offset = context->curr_f;
    /* 局部作用域遍历删除 */
    while (frame_offset != TCL_NULL) {
        TclFrame *frame = TO_PTR(context, frame_offset);
        tcl_u32 *var_ptr = &frame->vars;
        tcl_u32 var_offset = *var_ptr;
        while (var_offset != TCL_NULL) {
            TclVar *variable = TO_PTR(context, var_offset);
            if (t_scmp(name, TO_PTR(context, variable->name)) == 0) {
                *var_ptr = variable->next; /* 从链表中摘除节点，等待下一次 GC 回收物理空间 */
                return TCL_OK;
            }
            var_ptr = &variable->next;
            var_offset = *var_ptr;
        }
        if (frame->flags & FRAME_SHARE_SCOPE) frame_offset = frame->parent; else break;
    }
    /* 全局表遍历删除 */
    tcl_u32 *global_var_ptr = &context->g_vars;
    tcl_u32 global_var_offset = *global_var_ptr;
    while (global_var_offset != TCL_NULL) {
        TclVar *variable = TO_PTR(context, global_var_offset);
        if (t_scmp(name, TO_PTR(context, variable->name)) == 0) {
            *global_var_ptr = variable->next;
            return TCL_OK;
        }
        global_var_ptr = &variable->next;
        global_var_offset = *global_var_ptr;
    }
    return TCL_OK; /* 变量不存在不视为错误 */
}

/* C 函数指令注册表 */
typedef tcl_i32 (*Tcl_CmdProc)(TclCtx *context, tcl_i32 arg_count, tcl_u32 *arg_values);
typedef struct {
    const tcl_u8 *name;         /* 指令名 */
    Tcl_CmdProc proc;           /* C 处理函数指针 */
} TclCmd;

static TclCmd cmd_table[64];    /* 静态指令查找表 */
static tcl_i32 cmd_count = 0;   /* 已注册指令计数 */

/* 导出函数：向解释器注册新的 C 指令 */
void tcl_register_c_cmd(const tcl_u8 *name, Tcl_CmdProc procedure) {
    if (cmd_count < 64) {       /* 检查表是否已满 */
        cmd_table[cmd_count].name = name; /* 记录名称 */
        cmd_table[cmd_count].proc = procedure; /* 记录函数指针 */
        cmd_count++;            /* 计数累加 */
    }
}

/* 核心执行引擎：基于状态机的无栈化 Tcl 解释器 */
tcl_i32 tcl_eval(TclCtx *context, const tcl_u8 *script) {
    /* 将脚本源码拷贝进变量区以支持动态生命周期管理 */
    tcl_u32 script_len = t_slen(script) + 1;
    tcl_u32 script_offset = tcl_alc_p(context, script_len);
    if (script_offset == TCL_NULL) {
        return TCL_ERROR;
    }
    t_mcpy(TO_PTR(context, script_offset), script, script_len);
    context->tmp_roots[0] = script_offset; /* 保护脚本对象 */
    
    /* 分配初始顶层栈帧 */
    tcl_u32 frame_offset = tcl_alc_t(context, sizeof(TclFrame));
    if (frame_offset == TCL_NULL) {
        context->tmp_roots[0] = TCL_NULL;
        return TCL_ERROR;
    }
    script_offset = context->tmp_roots[0];
    context->tmp_roots[0] = TCL_NULL;
    
    TclFrame *frame = TO_PTR(context, frame_offset);
    frame->script = script_offset;
    frame->pc = 0;
    frame->vars = TCL_NULL;
    frame->parent = context->curr_f;
    frame->state = ST_TOKENIZE;
    frame->flags = 0;
    frame->exp_idx = 0;
    frame->argc = 0;
    frame->cond = frame->body = frame->result = TCL_NULL;
    context->curr_f = frame_offset; /* 设置当前活跃帧 */

    /* 主执行循环：驱动状态机运转直至所有帧处理完毕 */
    while (context->curr_f != TCL_NULL) {
        frame = TO_PTR(context, context->curr_f); /* 获取当前帧指针 */
        const tcl_u8 *script_ptr = TO_PTR(context, frame->script); /* 获取脚本物理地址 */
        
        switch (frame->state) {
            case ST_TOKENIZE: { /* 分词阶段：识别命令边界及参数 */
                frame->argc = 0; /* 重置参数计数 */
                frame->exp_idx = 0; /* 重置展开索引 */
                /* 跳过前导空白及空行 */
                while (script_ptr[frame->pc] && (script_ptr[frame->pc] == ';' || script_ptr[frame->pc] == '\n' || script_ptr[frame->pc] == '\r' || script_ptr[frame->pc] == ' ' || script_ptr[frame->pc] == '\t')) {
                    frame->pc++;
                }
                /* 处理行注释 */
                if (script_ptr[frame->pc] == '#') {
                    while (script_ptr[frame->pc] && script_ptr[frame->pc] != '\n' && script_ptr[frame->pc] != '\r') {
                        frame->pc++; /* 跳过整行 */
                    }
                    continue;   /* 重新开始下一条指令的 Tokenize */
                }
                /* 检查脚本是否结束 */
                if (!script_ptr[frame->pc]) {
                    tcl_u32 parent_offset = frame->parent; /* 记录父帧 */
                    /* 回收当前栈帧空间（栈式释放） */
                    context->t_bot += ((sizeof(TclFrame) + 7) & ~7);
                    context->curr_f = parent_offset; /* 切换回父帧 */
                    break;      /* 退出当前 switch */
                }
                /* 解析参数 Token */
                while (script_ptr[frame->pc] && frame->argc < MAX_ARGS) {
                    /* 跳过参数间空格 */
                    while (script_ptr[frame->pc] && (script_ptr[frame->pc] == ' ' || script_ptr[frame->pc] == '\t')) {
                        frame->pc++;
                    }
                    /* 检查指令边界 */
                    if (!script_ptr[frame->pc] || script_ptr[frame->pc] == ';' || script_ptr[frame->pc] == '\n' || script_ptr[frame->pc] == '\r') {
                        break;
                    }
                    tcl_u32 start_pos = frame->pc;
                    if (script_ptr[frame->pc] == '{') { /* 处理花括号包裹的 Token */
                        tcl_i32 depth = 1;
                        frame->pc++;
                        start_pos = frame->pc;
                        while (script_ptr[frame->pc] && depth > 0) {
                            if (script_ptr[frame->pc] == '{') depth++;
                            else if (script_ptr[frame->pc] == '}') depth--;
                            if (depth > 0) frame->pc++;
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
                        if (script_ptr[frame->pc] == '}') frame->pc++;
                    } else if (script_ptr[frame->pc] == '[') { /* 处理命令替换起始 Token */
                        tcl_i32 depth = 1;
                        frame->pc++;
                        start_pos = frame->pc - 1; /* 保留 [ */
                        while (script_ptr[frame->pc] && depth > 0) {
                            if (script_ptr[frame->pc] == '[') depth++;
                            else if (script_ptr[frame->pc] == ']') depth--;
                            frame->pc++;
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
                    } else { /* 处理普通 Token */
                        while (script_ptr[frame->pc] && script_ptr[frame->pc] != ' ' && script_ptr[frame->pc] != '\t' && script_ptr[frame->pc] != '\n' && script_ptr[frame->pc] != '\r' && script_ptr[frame->pc] != ';' && script_ptr[frame->pc] != ']') {
                            frame->pc++;
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
                    }
                }
                /* 跳过后随空白 */
                while (script_ptr[frame->pc] && (script_ptr[frame->pc] == ';' || script_ptr[frame->pc] == '\n' || script_ptr[frame->pc] == '\r' || script_ptr[frame->pc] == ' ' || script_ptr[frame->pc] == '\t')) {
                    frame->pc++;
                }
                frame->state = ST_EXPAND; /* 进入展开阶段 */
                break;
            }
            case ST_EXPAND: { /* 展开阶段：处理变量替换与子命令替换 */
                while (frame->exp_idx < frame->argc) {
                    tcl_u32 arg_offset = frame->argv[frame->exp_idx];
                    if (arg_offset == TCL_NULL) {
                        frame->exp_idx++;
                        continue;
                    }
                    tcl_u8 *arg_ptr = TO_PTR(context, arg_offset);
                    if (arg_ptr[0] == '$') { /* 处理变量替换 $var */
                        tcl_u32 value = tcl_get_var(context, context->curr_f, arg_ptr + 1);
                        if (value != TCL_NULL) {
                            frame->argv[frame->exp_idx] = value;
                        } else {
                            context->status = TCL_ERROR;
                            context->curr_f = TCL_NULL;
                            return TCL_ERROR; /* 变量不存在报错 */
                        }
                    } else if (arg_ptr[0] == '[') { /* 处理子命令替换 [...] */
                        tcl_u32 new_frame_offset = tcl_alc_t(context, sizeof(TclFrame));
                        if (new_frame_offset == TCL_NULL) {
                            context->status = TCL_ERROR;
                            context->curr_f = TCL_NULL;
                            return TCL_ERROR;
                        }
                        frame = TO_PTR(context, context->curr_f);
                        tcl_u8 *cmd_script = TO_PTR(context, frame->argv[frame->exp_idx]);
                        tcl_u32 cmd_len = t_slen(cmd_script);
                        context->tmp_roots[0] = tcl_alc_p(context, cmd_len); /* 分配子脚本空间 */
                        if (context->tmp_roots[0] == TCL_NULL) {
                            context->status = TCL_ERROR;
                            return TCL_ERROR;
                        }
                        tcl_u32 script_copy = context->tmp_roots[0];
                        frame = TO_PTR(context, context->curr_f);
                        cmd_script = TO_PTR(context, frame->argv[frame->exp_idx]);
                        t_mcpy(TO_PTR(context, script_copy), cmd_script + 1, cmd_len - 2); /* 去除 [] */
                        ((tcl_u8*)TO_PTR(context, script_copy))[cmd_len - 2] = 0;
                        
                        TclFrame *new_frame = TO_PTR(context, new_frame_offset);
                        new_frame->script = script_copy;
                        new_frame->pc = 0;
                        new_frame->vars = TCL_NULL;
                        new_frame->parent = context->curr_f;
                        new_frame->state = ST_TOKENIZE;
                        new_frame->flags = FRAME_SHARE_SCOPE;
                        new_frame->cond = new_frame->body = new_frame->result = TCL_NULL;
                        new_frame->argc = new_frame->exp_idx = 0;
                        context->tmp_roots[0] = TCL_NULL;
                        frame->state = ST_RESUME; /* 当前帧进入恢复态，等待子命令结果 */
                        context->curr_f = new_frame_offset; /* 切换至子命令帧 */
                        goto next_state_loop;
                    }
                    frame->exp_idx++;
                }
                frame->state = ST_EXECUTE; /* 全部展开完成后进入执行阶段 */
                break;
            }
            case ST_EXECUTE: { /* 执行阶段：调用 C 处理函数或脚本 Procs */
                const tcl_u8 *cmd_name = TO_PTR(context, frame->argv[0]);
                tcl_i32 found = 0;
                /* 首先在 C 指令查找表中定位 */
                for (tcl_i32 index = 0; index < cmd_count; index++) {
                    if (t_scmp(cmd_name, cmd_table[index].name) == 0) {
                        context->status = cmd_table[index].proc(context, frame->argc, frame->argv);
                        frame = TO_PTR(context, context->curr_f);
                        found = 1;
                        break;
                    }
                }
                /* 若未找到 C 指令，则在全局变量表中查找脚本定义的 proc */
                if (!found) {
                    tcl_u32 var_offset = context->g_vars;
                    tcl_u32 body_offset = TCL_NULL;
                    tcl_u32 args_offset = TCL_NULL;
                    while (var_offset != TCL_NULL) {
                        TclVar *variable = TO_PTR(context, var_offset);
                        const tcl_u8 *variable_name = TO_PTR(context, variable->name);
                        if (variable_name[0] == 'p' && variable_name[1] == ':' && t_scmp(cmd_name, variable_name + 2) == 0) {
                            body_offset = variable->val;
                        }
                        if (variable_name[0] == 'a' && variable_name[1] == ':' && t_scmp(cmd_name, variable_name + 2) == 0) {
                            args_offset = variable->val;
                        }
                        var_offset = variable->next;
                    }
                    if (body_offset != TCL_NULL) { /* 发现匹配的脚本函数 */
                        tcl_u32 new_frame_offset = tcl_alc_t(context, sizeof(TclFrame));
                        if (new_frame_offset == TCL_NULL) {
                            context->status = TCL_ERROR;
                        } else {
                            TclFrame *new_frame = TO_PTR(context, new_frame_offset);
                            new_frame->script = body_offset;
                            new_frame->pc = 0;
                            new_frame->vars = TCL_NULL;
                            new_frame->parent = context->curr_f;
                            new_frame->state = ST_TOKENIZE;
                            new_frame->flags = FRAME_IS_PROC;
                            new_frame->cond = new_frame->body = new_frame->result = TCL_NULL;
                            new_frame->argc = new_frame->exp_idx = 0;
                            /* 绑定实参至形参变量 */
                            context->tmp_roots[0] = args_offset;
                            tcl_i32 arg_idx = 1; /* 第 0 个是命令名，跳过 */
                            tcl_u32 arg_list_pos = 0;
                            while (arg_idx < frame->argc) {
                                const tcl_u8 *args_list_ptr = (const tcl_u8*)TO_PTR(context, context->tmp_roots[0]) + arg_list_pos;
                                while (*args_list_ptr == ' ' || *args_list_ptr == '\t') {
                                    args_list_ptr++;
                                    arg_list_pos++;
                                }
                                if (!*args_list_ptr) break;
                                tcl_u32 token_start = arg_list_pos;
                                while (*args_list_ptr && *args_list_ptr != ' ' && *args_list_ptr != '\t') {
                                    args_list_ptr++;
                                    arg_list_pos++;
                                }
                                tcl_i32 token_len = arg_list_pos - token_start;
                                context->tmp_roots[1] = tcl_alc_p(context, token_len + 1);
                                if (context->tmp_roots[1] == TCL_NULL) {
                                    context->status = TCL_ERROR;
                                    break;
                                }
                                frame = TO_PTR(context, context->curr_f);
                                t_mcpy(TO_PTR(context, context->tmp_roots[1]), (const tcl_u8*)TO_PTR(context, context->tmp_roots[0]) + token_start, token_len);
                                ((tcl_u8*)TO_PTR(context, context->tmp_roots[1]))[token_len] = 0;
                                tcl_set_var(context, new_frame_offset, context->tmp_roots[1], frame->argv[arg_idx++]);
                                context->tmp_roots[1] = TCL_NULL;
                                frame = TO_PTR(context, context->curr_f);
                            }
                            context->tmp_roots[0] = TCL_NULL;
                            frame->state = ST_RESUME; /* 当前帧进入恢复态 */
                            context->curr_f = new_frame_offset; /* 切换执行上下文 */
                            goto next_state_loop;
                        }
                        found = 1;
                    } else {
                        context->status = TCL_ERROR; /* 未知指令报错 */
                    }
                }
                /* 处理执行后的状态 */
                if (context->status == TCL_EXIT) {
                    context->curr_f = TCL_NULL;
                    break;
                }
                /* 处理异常跳转（BREAK/CONTINUE/RETURN/ERROR） */
                if (context->status != TCL_OK && context->status != TCL_YIELD) {
                    tcl_i32 current_status = context->status;
                    tcl_u32 current_result = context->result;
                    while (frame) {
                        tcl_u8 current_flags = frame->flags;
                        tcl_u32 parent_offset = frame->parent;
                        context->t_bot += ((sizeof(TclFrame) + 7) & ~7); /* 销毁当前帧 */
                        context->curr_f = parent_offset;
                        if (parent_offset == TCL_NULL) {
                            frame = 0;
                            break;
                        }
                        frame = TO_PTR(context, parent_offset);
                        /* 检查异常是否在当前帧被拦截 */
                        if (frame->state == ST_CATCH_END || ((current_status == TCL_BREAK || current_status == TCL_CONTINUE) && (frame->state == ST_LOOP || frame->state == ST_COND))) {
                            break;
                        }
                        if (current_status == TCL_RETURN && (current_flags & FRAME_IS_PROC)) {
                            break; /* return 指令在函数边界停止冒泡 */
                        }
                    }
                    context->status = current_status;
                    context->result = current_result;
                    if (!frame) context->curr_f = TCL_NULL;
                } else if (context->status == TCL_OK) {
                    frame->state = ST_TOKENIZE; /* 正常完成后继续解析下一条指令 */
                }
                break;
            }
            case ST_IF_COND: { /* if 指令条件判断准备 */
                tcl_u32 cond_frame_offset = tcl_alc_t(context, sizeof(TclFrame));
                if (cond_frame_offset == TCL_NULL) {
                    context->status = TCL_ERROR;
                    context->curr_f = TCL_NULL;
                    break;
                }
                TclFrame *cond_frame = TO_PTR(context, cond_frame_offset);
                cond_frame->script = frame->cond;
                cond_frame->pc = 0;
                cond_frame->vars = TCL_NULL;
                cond_frame->parent = context->curr_f;
                cond_frame->state = ST_TOKENIZE;
                cond_frame->flags = FRAME_SHARE_SCOPE;
                cond_frame->cond = cond_frame->body = cond_frame->result = TCL_NULL;
                cond_frame->argc = cond_frame->exp_idx = 0;
                frame->state = ST_IF_BODY; /* 标记当前帧等待条件结果 */
                context->curr_f = cond_frame_offset;
                break;
            }
            case ST_IF_BODY: { /* if 指令根据条件执行主体 */
                if (context->status != TCL_OK) { /* 条件执行出错则冒泡 */
                    tcl_i32 s = context->status;
                    tcl_u32 r = context->result;
                    tcl_u32 p = frame->parent;
                    context->t_bot += ((sizeof(TclFrame) + 7) & ~7);
                    context->curr_f = p;
                    context->status = s;
                    context->result = r;
                    break;
                }
                const tcl_u8 *res = tcl_get_result(context);
                if (res[0] && res[0] != '0') { /* 条件为真（非零字符串） */
                    tcl_u32 body_frame_offset = tcl_alc_t(context, sizeof(TclFrame));
                    if (body_frame_offset == TCL_NULL) {
                        context->status = TCL_ERROR;
                        context->curr_f = TCL_NULL;
                        break;
                    }
                    TclFrame *body_frame = TO_PTR(context, body_frame_offset);
                    body_frame->script = frame->body;
                    body_frame->pc = 0;
                    body_frame->vars = TCL_NULL;
                    body_frame->parent = context->curr_f;
                    body_frame->state = ST_TOKENIZE;
                    body_frame->flags = FRAME_SHARE_SCOPE;
                    body_frame->cond = body_frame->body = body_frame->result = TCL_NULL;
                    body_frame->argc = body_frame->exp_idx = 0;
                    frame->state = ST_TOKENIZE; /* 执行完 body 后回到指令解析 */
                    context->curr_f = body_frame_offset;
                } else {
                    frame->state = ST_TOKENIZE; /* 条件为假，跳过 */
                }
                break;
            }
            case ST_COND: { /* while 循环条件判断准备 */
                if (context->status == TCL_BREAK) { /* 循环终止 */
                    context->status = TCL_OK;
                    frame->state = ST_TOKENIZE;
                    break;
                }
                if (context->status == TCL_CONTINUE) context->status = TCL_OK;
                tcl_u32 cond_frame_offset = tcl_alc_t(context, sizeof(TclFrame));
                if (cond_frame_offset == TCL_NULL) {
                    context->status = TCL_ERROR;
                    context->curr_f = TCL_NULL;
                    break;
                }
                TclFrame *cond_frame = TO_PTR(context, cond_frame_offset);
                cond_frame->script = frame->cond;
                cond_frame->pc = 0;
                cond_frame->vars = TCL_NULL;
                cond_frame->parent = context->curr_f;
                cond_frame->state = ST_TOKENIZE;
                cond_frame->flags = FRAME_SHARE_SCOPE;
                cond_frame->cond = cond_frame->body = cond_frame->result = TCL_NULL;
                cond_frame->argc = cond_frame->exp_idx = 0;
                frame->state = ST_LOOP; /* 进入循环主体预备态 */
                context->curr_f = cond_frame_offset;
                break;
            }
            case ST_LOOP: { /* while 循环执行主体 */
                if (context->status == TCL_BREAK) {
                    context->status = TCL_OK;
                    frame->state = ST_TOKENIZE;
                    break;
                }
                if (context->status == TCL_CONTINUE) {
                    context->status = TCL_OK;
                    frame->state = ST_COND;
                    break;
                }
                if (context->status != TCL_OK) {
                    tcl_i32 s = context->status;
                    tcl_u32 r = context->result;
                    tcl_u32 p = frame->parent;
                    context->t_bot += ((sizeof(TclFrame) + 7) & ~7);
                    context->curr_f = p;
                    context->status = s;
                    context->result = r;
                    break;
                }
                const tcl_u8 *res = tcl_get_result(context);
                if (res[0] && res[0] != '0') {
                    tcl_u32 body_frame_offset = tcl_alc_t(context, sizeof(TclFrame));
                    if (body_frame_offset == TCL_NULL) {
                        context->status = TCL_ERROR;
                        context->curr_f = TCL_NULL;
                        break;
                    }
                    TclFrame *body_frame = TO_PTR(context, body_frame_offset);
                    body_frame->script = frame->body;
                    body_frame->pc = 0;
                    body_frame->vars = TCL_NULL;
                    body_frame->parent = context->curr_f;
                    body_frame->state = ST_TOKENIZE;
                    body_frame->flags = FRAME_SHARE_SCOPE;
                    body_frame->cond = body_frame->body = body_frame->result = TCL_NULL;
                    body_frame->argc = body_frame->exp_idx = 0;
                    frame->state = ST_COND; /* 执行完 body 后再次检查条件 */
                    context->curr_f = body_frame_offset;
                } else {
                    frame->state = ST_TOKENIZE; /* 条件不成立，结束循环 */
                }
                break;
            }
            case ST_CATCH_END: { /* catch 指令结束后处理结果变量 */
                context->tmp_roots[0] = context->result;
                context->tmp_roots[1] = frame->body;
                tcl_u32 status_offset = tcl_alc_p(context, 12);
                if (status_offset == TCL_NULL) {
                    context->tmp_roots[0] = context->tmp_roots[1] = TCL_NULL;
                    context->status = TCL_ERROR;
                    break;
                }
                t_itoa(context->status, TO_PTR(context, status_offset));
                context->tmp_roots[2] = status_offset;
                frame = TO_PTR(context, context->curr_f);
                if (context->tmp_roots[1] != TCL_NULL) {
                    tcl_set_var(context, frame->parent, context->tmp_roots[1], context->tmp_roots[0]);
                }
                context->result = context->tmp_roots[2]; /* catch 返回状态码字符串 */
                context->status = TCL_OK;
                frame = TO_PTR(context, context->curr_f);
                frame->state = ST_TOKENIZE;
                context->tmp_roots[0] = context->tmp_roots[1] = context->tmp_roots[2] = TCL_NULL;
                break;
            }
            case ST_RESUME: { /* 恢复态：接收子结界返回结果并继续参数处理或下一条指令 */
                frame = TO_PTR(context, context->curr_f);
                if (context->status != TCL_OK && context->status != TCL_RETURN) {
                    /* 子结界发生异常，冒泡处理 */
                    tcl_i32 current_status = context->status;
                    tcl_u32 current_result = context->result;
                    while (frame) {
                        tcl_u8 current_flags = frame->flags;
                        tcl_u32 parent_offset = frame->parent;
                        context->t_bot += ((sizeof(TclFrame) + 7) & ~7);
                        context->curr_f = parent_offset;
                        if (parent_offset == TCL_NULL) {
                            frame = 0;
                            break;
                        }
                        frame = TO_PTR(context, parent_offset);
                        if (frame->state == ST_CATCH_END || ((current_status == TCL_BREAK || current_status == TCL_CONTINUE) && (frame->state == ST_LOOP || frame->state == ST_COND))) {
                            break;
                        }
                        if (current_status == TCL_RETURN && (current_flags & FRAME_IS_PROC)) {
                            break;
                        }
                    }
                    context->status = current_status;
                    context->result = current_result;
                    if (!frame) context->curr_f = TCL_NULL;
                    break;
                }
                if (context->status == TCL_RETURN) context->status = TCL_OK;
                /* 如果仍在 EXPAND 阶段（处理命令替换结果），则回填参数并继续 */
                if (frame->exp_idx < frame->argc) {
                    frame->argv[frame->exp_idx] = context->result;
                    frame->exp_idx++;
                    frame->state = ST_EXPAND;
                } else {
                    frame->state = ST_TOKENIZE; /* 否则继续解析下一条指令 */
                }
                break;
            }
            default:
                context->curr_f = TCL_NULL; /* 未知状态，强制终止 */
                break;
        }
        next_state_loop:;
    }
    tcl_i32 final_status = context->status;
    context->status = TCL_OK;
    return final_status;        /* 返回解释器最终状态码 */
}

/* 导出函数：获取解释器当前结果字符串 */
const tcl_u8 *tcl_get_result(TclCtx *context) {
    return context->result == TCL_NULL ? (tcl_u8*)"" : TO_PTR(context, context->result);
}

#include "tcllib.c"             /* 包含由 tcl2c.py 生成的脚本库字节数组 */

/* 导出函数：加载自举脚本库 */
tcl_i32 tcl_load_bootstrap(TclCtx *context) {
    return tcl_eval(context, (const tcl_u8 *)tcl_bootstrap);
}

/* info 指令实现：提供内省能力，目前仅支持 info commands [pattern] */
static tcl_i32 tcl_cmd_info(TclCtx *context, tcl_i32 arg_count, tcl_u32 *arg_values) {
    if (arg_count < 2) return TCL_ERROR;
    const tcl_u8 *sub_command = TO_PTR(context, arg_values[1]);
    if (t_scmp(sub_command, (const tcl_u8 *)"commands") == 0) {
        /* 简单实现：由于目前主要用于检测 t_scmp，这里返回所有 C 指令及 Procs 的合并列表 */
        /* 为了简化，目前仅返回空字符串或匹配特定查询 */
        if (arg_count > 2) {
            const tcl_u8 *pattern = TO_PTR(context, arg_values[2]);
            /* 遍历 C 指令表 */
            for (tcl_i32 index = 0; index < cmd_count; index++) {
                if (t_scmp(pattern, cmd_table[index].name) == 0) {
                    context->result = arg_values[2]; /* 找到了 */
                    return TCL_OK;
                }
            }
            /* 遍历全局 Procs */
            tcl_u32 var_offset = context->g_vars;
            while (var_offset != TCL_NULL) {
                TclVar *variable = TO_PTR(context, var_offset);
                const tcl_u8 *variable_name = TO_PTR(context, variable->name);
                if (variable_name[0] == 'p' && variable_name[1] == ':' && t_scmp(pattern, variable_name + 2) == 0) {
                    context->result = arg_values[2];
                    return TCL_OK;
                }
                var_offset = variable->next;
            }
            context->result = TCL_NULL; /* 未找到 */
        } else {
            /* TODO: 返回所有指令列表，目前暂返空 */
            context->result = TCL_NULL;
        }
        return TCL_OK;
    }
    return TCL_ERROR;
}

/* 导出函数：解释器初始化 */
void tcl_init(void *arena_ptr, tcl_i32 total_size) {
    TclCtx *context = (TclCtx*)arena_ptr;
    /* 清理 Arena 头部 */
    for (tcl_u32 index = 0; index < sizeof(TclCtx); index++) {
        ((tcl_u8*)arena_ptr)[index] = 0;
    }
    /* 初始化全局上下文 */
    context->arena = (tcl_u8*)arena_ptr;
    context->size = (tcl_u32)total_size;
    context->p_top = HS;        /* 变量区起点 */
    context->t_bot = context->size; /* 栈帧区起点（向下生长） */
    context->g_vars = TCL_NULL; /* 初始化全局变量表为空 */
    context->result = TCL_NULL; /* 初始化执行结果为空 */
    context->status = TCL_OK;   /* 初始状态为 OK */
    context->curr_f = TCL_NULL; /* 初始无活跃栈帧 */
    
    cmd_count = 0;              /* 重置已注册指令计数 */
    /* 注册所有内核原子指令 */
    tcl_register_c_cmd((tcl_u8*)"set", tcl_cmd_set);
    tcl_register_c_cmd((tcl_u8*)"proc", tcl_cmd_proc);
    tcl_register_c_cmd((tcl_u8*)"if", tcl_cmd_if);
    tcl_register_c_cmd((tcl_u8*)"expr", tcl_cmd_expr);
    tcl_register_c_cmd((tcl_u8*)"while", tcl_cmd_while);
    tcl_register_c_cmd((tcl_u8*)"return", tcl_cmd_return);
    tcl_register_c_cmd((tcl_u8*)"break", tcl_cmd_break);
    tcl_register_c_cmd((tcl_u8*)"continue", tcl_cmd_continue);
    tcl_register_c_cmd((tcl_u8*)"error", tcl_cmd_error);
    tcl_register_c_cmd((tcl_u8*)"eval", tcl_cmd_eval);
    tcl_register_c_cmd((tcl_u8*)"catch", tcl_cmd_catch);
    tcl_register_c_cmd((tcl_u8*)"uplevel", tcl_cmd_uplevel);
    tcl_register_c_cmd((tcl_u8*)"upvar", tcl_cmd_upvar);
    tcl_register_c_cmd((tcl_u8*)"list", tcl_cmd_list);
    tcl_register_c_cmd((tcl_u8*)"llength", tcl_cmd_llength);
    tcl_register_c_cmd((tcl_u8*)"lindex", tcl_cmd_lindex);
    tcl_register_c_cmd((tcl_u8*)"lrange", tcl_cmd_lrange);
    tcl_register_c_cmd((tcl_u8*)"unset", tcl_cmd_unset);
    tcl_register_c_cmd((tcl_u8*)"info", tcl_cmd_info);
}

