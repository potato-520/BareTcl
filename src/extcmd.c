/*
 * extcmd.c - Tclsh.v2 扩展指令集实现
 */

/* puts 指令实现：输出字符串至 HAL 层串口并换行 */
static tcl_i32 tcl_cmd_puts(TclCtx *context, tcl_i32 arg_count, tcl_u32 *arg_values) {
    if (arg_count < 2) {        /* 检查参数数量 */
        return TCL_ERROR;       /* 报错 */
    }
    tcl_hal_puts(TO_PTR(context, arg_values[1])); /* 调用 HAL 层物理输出 */
    tcl_hal_puts((const tcl_u8 *)"\n");           /* 附加换行符 */
    return TCL_OK;              /* 成功 */
}

/* exit 指令实现：触发解释器退出状态机 */
static tcl_i32 tcl_cmd_exit(TclCtx *context, tcl_i32 arg_count, tcl_u32 *arg_values) {
    return TCL_EXIT;            /* 返回终止状态码 */
}

/* append 指令实现：追加字符串至已有变量 */
static tcl_i32 tcl_cmd_append(TclCtx *context, tcl_i32 arg_count, tcl_u32 *arg_values) {
    if (arg_count < 2) {        /* 检查参数数量 */
        return TCL_ERROR;       /* 报错 */
    }
    /* 尝试获取原有变量内容 */
    tcl_u32 existing_val_offset = tcl_get_var(context, context->curr_f, TO_PTR(context, arg_values[1]));
    context->tmp_roots[10] = existing_val_offset; /* 保护已有值防止 GC 搬运 */
    
    tcl_u32 current_len = (existing_val_offset == TCL_NULL) ? 0 : t_slen(TO_PTR(context, existing_val_offset));
    tcl_u32 added_len = 0;      /* 计算所有待追加参数的总长度 */
    for (tcl_i32 index = 2; index < arg_count; index++) {
        added_len += t_slen(TO_PTR(context, arg_values[index]));
    }
    
    /* 分配足够的物理空间存放合并后的字符串 */
    tcl_u32 new_block_offset = tcl_alc_p(context, current_len + added_len + 1); 
    if (new_block_offset == TCL_NULL) { /* 物理空间不足 */
        context->tmp_roots[10] = TCL_NULL;
        return TCL_ERROR;
    }
    context->tmp_roots[11] = new_block_offset; /* 保护新分配块 */
    
    tcl_u8 *dest_ptr = TO_PTR(context, context->tmp_roots[11]); 
    if (context->tmp_roots[10] != TCL_NULL) { /* 拷贝旧值 */
        t_mcpy(dest_ptr, TO_PTR(context, context->tmp_roots[10]), current_len);
        dest_ptr += current_len;
    }
    for (tcl_i32 index = 2; index < arg_count; index++) { /* 依次拷贝追加参数 */
        tcl_u32 len = t_slen(TO_PTR(context, arg_values[index])); 
        t_mcpy(dest_ptr, TO_PTR(context, arg_values[index]), len);
        dest_ptr += len;
    }
    *dest_ptr = 0;              /* 写入字符串结束符 */
    
    /* 更新变量引用 */
    if (tcl_set_var(context, context->curr_f, arg_values[1], context->tmp_roots[11]) != TCL_OK) {
        context->tmp_roots[10] = context->tmp_roots[11] = TCL_NULL;
        return TCL_ERROR;
    }
    context->result = context->tmp_roots[11]; /* 返回最终追加后的结果 */
    context->tmp_roots[10] = context->tmp_roots[11] = TCL_NULL; /* 清理保护根 */
    return TCL_OK;
}

/* incr 指令实现：变量自增，支持可选步长参数 */
static tcl_i32 tcl_cmd_incr(TclCtx *context, tcl_i32 arg_count, tcl_u32 *arg_values) {
    if (arg_count < 2) {            /* 参数检查：至少需要 incr varName */
        return TCL_ERROR;           /* 报错 */
    }
    const tcl_u8 *var_name_ptr = TO_PTR(context, arg_values[1]); /* 变量名字符串 */
    tcl_u32 old_val_offset = tcl_get_var(context, context->curr_f, var_name_ptr); /* 查找当前值 */
    tcl_i32 current_value = 0;      /* 默认起始值为 0（变量不存在时） */
    if (old_val_offset != TCL_NULL) { /* 变量存在，解析其整数值 */
        current_value = t_atoi(TO_PTR(context, old_val_offset)); /* 字符串转整数 */
    }
    tcl_i32 step_value = 1;         /* 默认步长为 1 */
    if (arg_count >= 3) {           /* 用户提供了步长参数 */
        step_value = t_atoi(TO_PTR(context, arg_values[2])); /* 解析步长整数值 */
    }
    tcl_i32 new_value = current_value + step_value; /* 计算新值 */
    tcl_u32 result_offset = tcl_alc_p(context, 16); /* 分配存放整数字符串的空间 */
    if (result_offset == TCL_NULL) { /* 内存分配失败 */
        return TCL_ERROR;           /* 报错 */
    }
    t_itoa(new_value, TO_PTR(context, result_offset)); /* 将新整数值格式化为字符串 */
    if (tcl_set_var(context, context->curr_f, arg_values[1], result_offset) != TCL_OK) {
        return TCL_ERROR;           /* 变量写入失败报错 */
    }
    context->result = result_offset; /* 返回新值字符串 */
    return TCL_OK;                  /* 执行成功 */
}

/* lappend 指令实现：向列表变量追加一个或多个元素 */
static tcl_i32 tcl_cmd_lappend(TclCtx *context, tcl_i32 arg_count, tcl_u32 *arg_values) {
    if (arg_count < 2) {            /* 参数检查：至少需要 lappend varName */
        return TCL_ERROR;           /* 报错 */
    }
    if (arg_count == 2) {           /* 无追加值时直接返回当前变量值 */
        tcl_u32 cur = tcl_get_var(context, context->curr_f, TO_PTR(context, arg_values[1]));
        context->result = (cur != TCL_NULL) ? cur : arg_values[1]; /* 返回现有值或变量名 */
        return TCL_OK;
    }
    tcl_u32 old_val = tcl_get_var(context, context->curr_f, TO_PTR(context, arg_values[1]));
    context->tmp_roots[10] = old_val; /* 保护旧值 */
    tcl_u32 old_len = (old_val != TCL_NULL) ? t_slen(TO_PTR(context, old_val)) : 0;
    tcl_u32 need_len = old_len; /* 从旧值长度开始累计 */
    for (tcl_i32 idx = 2; idx < arg_count; idx++) { /* 遍历所有待追加元素 */
        tcl_u32 val_len = t_slen(TO_PTR(context, arg_values[idx])); /* 获取元素字符串长度 */
        need_len += val_len; /* 先累计元素本身长度 */
        if (old_len > 0 || idx > 2) { /* 仅在旧值非空或不是首个新元素时才需要分隔空格 */
            need_len += 1; /* 累计空格分隔符长度 */
        }
    }
    tcl_u32 new_buf = tcl_alc_p(context, need_len + 1); /* +1 存放字符串结束符 */
    if (new_buf == TCL_NULL) {      /* 内存分配失败 */
        context->tmp_roots[10] = TCL_NULL;
        return TCL_ERROR;
    }
    context->tmp_roots[11] = new_buf; /* 保护新分配块 */
    tcl_u8 *dest = TO_PTR(context, context->tmp_roots[11]); /* 目标写入指针 */
    if (context->tmp_roots[10] != TCL_NULL && old_len > 0) { /* 拷贝旧列表内容 */
        t_mcpy(dest, TO_PTR(context, context->tmp_roots[10]), old_len);
        dest += old_len; /* 移动写入指针 */
    }
    tcl_i32 has_output = (old_len > 0); /* 标记当前输出缓存中是否已有内容 */
    for (tcl_i32 idx = 2; idx < arg_count; idx++) { /* 依次追加每个新元素 */
        if (has_output) { /* 只有在已有内容时才写分隔空格 */
            *dest++ = ' '; /* 写入列表分隔符 */
        }
        tcl_u32 val_len = t_slen(TO_PTR(context, arg_values[idx]));
        t_mcpy(dest, TO_PTR(context, arg_values[idx]), val_len); /* 写入元素值 */
        dest += val_len; /* 更新写入指针 */
        has_output = 1; /* 标记输出已包含内容 */
    }
    *dest = 0; /* 写入字符串结束符 */
    if (tcl_set_var(context, context->curr_f, arg_values[1], context->tmp_roots[11]) != TCL_OK) {
        context->tmp_roots[10] = context->tmp_roots[11] = TCL_NULL;
        return TCL_ERROR;
    }
    context->result = context->tmp_roots[11]; /* 返回追加后的列表字符串 */
    context->tmp_roots[10] = context->tmp_roots[11] = TCL_NULL; /* 清理保护根 */
    return TCL_OK;
}

/* string 指令实现：支持标准 Tcl 的 string compare 等操作 */
static tcl_i32 tcl_cmd_string(TclCtx *context, tcl_i32 arg_count, tcl_u32 *arg_values) {
    if (arg_count < 2) {
        return TCL_ERROR;
    }
    const tcl_u8 *sub_command = TO_PTR(context, arg_values[1]);
    if (t_scmp(sub_command, (const tcl_u8 *)"compare") == 0) { /* 实现 string compare */
        if (arg_count < 4) return TCL_ERROR;
        tcl_i32 compare_result = t_scmp(TO_PTR(context, arg_values[2]), TO_PTR(context, arg_values[3]));
        /* 结果规格化为 -1, 0, 1 */
        if (compare_result < 0) compare_result = -1;
        else if (compare_result > 0) compare_result = 1;
        
        tcl_u32 result_offset = tcl_alc_p(context, 12); /* 预留存放整数字符串的空间 */
        if (result_offset != TCL_NULL) {
            t_itoa(compare_result, TO_PTR(context, result_offset));
            context->result = result_offset;
        }
        return TCL_OK;
    }
    if (t_scmp(sub_command, (const tcl_u8 *)"length") == 0) { /* 实现 string length */
        if (arg_count < 3) return TCL_ERROR;
        tcl_u32 length_val = t_slen(TO_PTR(context, arg_values[2])); /* 获取目标字符串的字符级长度 */
        tcl_u32 result_offset = tcl_alc_p(context, 12); /* 预留存放整型结果字符串的空间 */
        if (result_offset != TCL_NULL) {
            t_itoa(length_val, TO_PTR(context, result_offset)); /* 将长度数字格式化为字符串 */
            context->result = result_offset; /* 设置到执行结果寄存器 */
        }
        return TCL_OK;
    }
    return TCL_ERROR;           /* 不支持的 string 子指令 */
}


/* __info_commands_core 指令实现：底层命令表查询，供 tcllib.tcl 中 info commands 封装调用 */
/* 职能：在 C 命令注册表和 Tcl proc 全局符号表中搜索指定名称，或在无参数时列出全部命令 */
static tcl_u32 tcl_info_commands_collect_all(TclCtx *context) {
    tcl_u32 total_length = 1;       /* 结果至少需要一个终止符 */
    tcl_i32 has_written_name = 0;   /* 记录当前结果里是否已经写入过命令名 */
    tcl_i32 command_index = 0;      /* 线性扫描 C 命令注册表 */
    while (command_index < cmd_count) {
        const tcl_u8 *command_name = cmd_table[command_index].name; /* 读取命令名 */
        if (has_written_name) {
            total_length += 1;      /* 命令之间补一个空格 */
        }
        total_length += t_slen(command_name); /* 累加命令名长度 */
        has_written_name = 1;       /* 标记已写入至少一个命令名 */
        command_index++;            /* 推进到下一条注册命令 */
    }
    tcl_u32 global_var_offset = context->g_vars; /* 从全局变量表开始遍历 Tcl proc */
    while (global_var_offset != TCL_NULL) {
        TclVar *global_var = TO_PTR(context, global_var_offset); /* 定位变量节点 */
        const tcl_u8 *global_name = TO_PTR(context, global_var->name); /* 读取变量名 */
        if (global_name[0] == 'p' && global_name[1] == ':') {
            if (has_written_name) {
                total_length += 1;  /* 命令之间补一个空格 */
            }
            total_length += t_slen(global_name + 2); /* 仅输出 p: 之后的 proc 名称 */
            has_written_name = 1;   /* 标记结果中已有命令名 */
        }
        global_var_offset = global_var->next; /* 继续向后遍历 */
    }
    tcl_u32 list_offset = tcl_alc_p(context, total_length); /* 为完整命令列表分配空间 */
    if (list_offset == TCL_NULL) {
        return TCL_NULL;
    }
    tcl_u8 *write_ptr = TO_PTR(context, list_offset); /* 获取写入缓冲区 */
    has_written_name = 0;           /* 第二遍写回时重新计数 */
    command_index = 0;
    while (command_index < cmd_count) {
        const tcl_u8 *command_name = cmd_table[command_index].name;
        if (has_written_name) {
            *write_ptr++ = ' ';
        }
        tcl_u32 command_name_length = t_slen(command_name);
        t_mcpy(write_ptr, command_name, command_name_length);
        write_ptr += command_name_length;
        has_written_name = 1;
        command_index++;
    }
    global_var_offset = context->g_vars;
    while (global_var_offset != TCL_NULL) {
        TclVar *global_var = TO_PTR(context, global_var_offset);
        const tcl_u8 *global_name = TO_PTR(context, global_var->name);
        if (global_name[0] == 'p' && global_name[1] == ':') {
            if (has_written_name) {
                *write_ptr++ = ' ';
            }
            tcl_u32 proc_name_length = t_slen(global_name + 2);
            t_mcpy(write_ptr, global_name + 2, proc_name_length);
            write_ptr += proc_name_length;
            has_written_name = 1;
        }
        global_var_offset = global_var->next;
    }
    *write_ptr = 0;
    return list_offset;
}

static tcl_i32 tcl_cmd_info_commands_core(TclCtx *context, tcl_i32 arg_count, tcl_u32 *arg_values) {
    if (arg_count < 2) {            /* 参数检查：无参数时返回全部命令列表 */
        tcl_u32 list_offset = tcl_info_commands_collect_all(context);
        if (list_offset == TCL_NULL) {
            return TCL_ERROR;
        }
        context->result = list_offset;
        return TCL_OK;
    }
    const tcl_u8 *search_name = TO_PTR(context, arg_values[1]); /* 获取查询名称 */
    /* 第一步：在 C 原子命令注册表中线性查找 */
    for (tcl_i32 idx = 0; idx < cmd_count; idx++) {
        if (t_scmp(search_name, cmd_table[idx].name) == 0) { /* 名称匹配 */
            context->result = arg_values[1]; /* 返回命令名称 */
            return TCL_OK;
        }
    }
    /* 第二步：在全局符号表中查找 Tcl proc（以 "p:" 前缀标识） */
    tcl_u32 var_offset = context->g_vars; /* 从全局变量表头开始遍历 */
    while (var_offset != TCL_NULL) {
        TclVar *var_ptr = TO_PTR(context, var_offset); /* 获取节点指针 */
        const tcl_u8 *vname = TO_PTR(context, var_ptr->name); /* 获取名称字符串 */
        if (vname[0] == 'p' && vname[1] == ':' &&
            t_scmp(search_name, vname + 2) == 0) { /* 匹配 "p:<name>" 格式 */
            context->result = arg_values[1]; /* 返回命令名称 */
            return TCL_OK;
        }
        var_offset = var_ptr->next; /* 移动到下一个节点 */
    }
    /* 设计：未找到命令时返回真正的空字符串对象，而非 TCL_NULL。
       TCL_NULL 代表"无值"，会导致 $var 展开失败误判为变量未定义。
       空字符串 "" 是有效的 Tcl 值，与 TCL_NULL 语义不同。 */
    tcl_u32 empty_str = tcl_alc_p(context, 1); /* 分配1字节，用于存放 null terminator */
    if (empty_str != TCL_NULL) { /* 分配成功 */
        ((tcl_u8*)TO_PTR(context, empty_str))[0] = 0; /* 写入字符串结束符，得到空字符串 */
        context->result = empty_str; /* 返回空字符串对象 */
    } /* 分配失败时 context->result 保持为 TCL_NULL（极端情况，可接受） */
    return TCL_OK;
}

/* 导出函数：向核心解释器注册所有扩展指令 */
void tcl_register_ext_cmds(TclCtx *context) {
    tcl_register_c_cmd((const tcl_u8 *)"puts", tcl_cmd_puts);
    tcl_register_c_cmd((const tcl_u8 *)"exit", tcl_cmd_exit);
    tcl_register_c_cmd((const tcl_u8 *)"append", tcl_cmd_append);
    tcl_register_c_cmd((const tcl_u8 *)"incr", tcl_cmd_incr);
    tcl_register_c_cmd((const tcl_u8 *)"lappend", tcl_cmd_lappend);
    tcl_register_c_cmd((const tcl_u8 *)"__string_core", tcl_cmd_string); /* 注册核心 C 实现 */
    tcl_register_c_cmd((const tcl_u8 *)"__info_commands_core", tcl_cmd_info_commands_core); /* 注册底层命令查询接口 */
}
