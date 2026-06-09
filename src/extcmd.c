/*
 * extcmd.c - Tclsh.v2 扩展指令集实现
 */

/* puts 指令实现：输出字符串至 HAL 层串口并换行 */
static int tcl_cmd_puts(TclCtx *context, tcl_i32 arg_count, tcl_u32 *arg_values) {
    if (arg_count < 2) {        /* 检查参数数量 */
        return TCL_ERROR;       /* 报错 */
    }
    tcl_hal_puts(TO_PTR(context, arg_values[1])); /* 调用 HAL 层物理输出 */
    tcl_hal_puts((const tcl_u8 *)"\n");           /* 附加换行符 */
    return TCL_OK;              /* 成功 */
}

/* exit 指令实现：触发解释器退出状态机 */
static int tcl_cmd_exit(TclCtx *context, tcl_i32 arg_count, tcl_u32 *arg_values) {
    return TCL_EXIT;            /* 返回终止状态码 */
}

/* append 指令实现：追加字符串至已有变量 */
static int tcl_cmd_append(TclCtx *context, tcl_i32 arg_count, tcl_u32 *arg_values) {
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

/* string 指令实现：支持标准 Tcl 的 string compare 等操作 */
static int tcl_cmd_string(TclCtx *context, tcl_i32 arg_count, tcl_u32 *arg_values) {
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
    return TCL_ERROR;           /* 不支持的 string 子指令 */
}

/* 导出函数：向核心解释器注册所有扩展指令 */
void tcl_register_ext_cmds(TclCtx *context) {
    tcl_register_c_cmd((const tcl_u8 *)"puts", tcl_cmd_puts);
    tcl_register_c_cmd((const tcl_u8 *)"exit", tcl_cmd_exit);
    tcl_register_c_cmd((const tcl_u8 *)"append", tcl_cmd_append);
    tcl_register_c_cmd((const tcl_u8 *)"__string_core", tcl_cmd_string); /* 注册核心 C 实现 */
}
