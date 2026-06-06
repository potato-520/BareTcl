/*
 * extcmd.c - Extension commands for Tclsh.v2
 */

static int tcl_cmd_puts(TclCtx *ctx, tcl_i32 argc, tcl_u32 *argv) {
    if (argc < 2) return TCL_ERROR;
    tcl_hal_puts(TO_PTR(ctx, argv[1]));
    tcl_hal_puts((const tcl_u8 *)"\n");
    return TCL_OK;
}

static int tcl_cmd_exit(TclCtx *ctx, tcl_i32 argc, tcl_u32 *argv) {
    return TCL_EXIT;
}

static int tcl_cmd_append(TclCtx *ctx, tcl_i32 argc, tcl_u32 *argv) {
    if(argc<2) return TCL_ERROR;
    tcl_u32 vo=tcl_get_var(ctx,ctx->curr_f,TO_PTR(ctx,argv[1]));
    ctx->tmp_roots[10] = vo; // existing val
    tcl_u32 cur_l = vo==TCL_NULL?0:t_slen(TO_PTR(ctx,vo));
    tcl_u32 add_l = 0; for(tcl_i32 i=2; i<argc; i++) add_l += t_slen(TO_PTR(ctx,argv[i]));
    tcl_u32 no = tcl_alc_p(ctx, cur_l + add_l + 1); 
    if(no==TCL_NULL) { ctx->tmp_roots[10]=TCL_NULL; return TCL_ERROR; }
    ctx->tmp_roots[11] = no; // new block
    tcl_u8 *d = TO_PTR(ctx, ctx->tmp_roots[11]); 
    if(ctx->tmp_roots[10]!=TCL_NULL) { t_mcpy(d, TO_PTR(ctx,ctx->tmp_roots[10]), cur_l); d+=cur_l; }
    for(tcl_i32 i=2; i<argc; i++) { 
        tcl_u32 l = t_slen(TO_PTR(ctx,argv[i])); 
        t_mcpy(d, TO_PTR(ctx,argv[i]), l); d+=l; 
    }
    *d=0; 
    if (tcl_set_var(ctx, ctx->curr_f, argv[1], ctx->tmp_roots[11]) != TCL_OK) {
        ctx->tmp_roots[10] = ctx->tmp_roots[11] = TCL_NULL; return TCL_ERROR;
    }
    ctx->result = ctx->tmp_roots[11]; 
    ctx->tmp_roots[10] = ctx->tmp_roots[11] = TCL_NULL; 
    return TCL_OK;
}

static int tcl_cmd_scmp(TclCtx *ctx, tcl_i32 argc, tcl_u32 *argv) {
    if(argc<3) return TCL_ERROR;
    tcl_i32 res = t_scmp(TO_PTR(ctx, argv[1]), TO_PTR(ctx, argv[2]));
    tcl_u32 r=tcl_alc_p(ctx,12); if(r!=TCL_NULL){ t_itoa(res,TO_PTR(ctx,r)); ctx->result=r; } return TCL_OK;
}

void tcl_register_ext_cmds(TclCtx *ctx) {
    tcl_register_c_cmd((const tcl_u8 *)"puts", tcl_cmd_puts);
    tcl_register_c_cmd((const tcl_u8 *)"exit", tcl_cmd_exit);
    tcl_register_c_cmd((const tcl_u8 *)"append", tcl_cmd_append);
    tcl_register_c_cmd((const tcl_u8 *)"t_scmp", tcl_cmd_scmp);
}
