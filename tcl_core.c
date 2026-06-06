/*
 * Tclsh.v2 - Ultra-compact, stackless, Libc-free Tcl interpreter
 */

typedef unsigned char      tcl_u8;
typedef signed char        tcl_i8;
typedef unsigned short     tcl_u16;
typedef signed short       tcl_i16;
typedef unsigned int       tcl_u32;
typedef signed int         tcl_i32;
#if defined(__x86_64__) || defined(__aarch64__)
typedef unsigned long long tcl_ptr;
#else
typedef unsigned int       tcl_ptr;
#endif

#define TCL_NULL (0xFFFFFFFF)
#define MAX_ARGS 32
#define tcl_i32_MAX 2147483647

#ifdef TCL_DEBUG
#define TCL_LOG(s) tcl_hal_puts((const tcl_u8 *)s)
#else
#define TCL_LOG(s)
#endif

typedef struct {
    tcl_u8  *arena;
    tcl_u32  size;
    tcl_u32  p_top;
    tcl_u32  t_bot;
    tcl_u32  g_vars;
    tcl_u32  result;
    tcl_i32  status;
    tcl_u32 curr_f;
    tcl_u32 tmp_roots[16];
} TclCtx;

typedef struct {
    tcl_u32 name; tcl_u32 val; tcl_u32 next; tcl_u8 flags;
} TclVar;

#define VAR_LINK 0x01

typedef struct {
    tcl_u32 script; tcl_u32 pc; tcl_u32 vars; tcl_u32 parent; tcl_u32 result;
    tcl_u32 cond; tcl_u32 body; tcl_u32 argv[MAX_ARGS];
    tcl_u8  argc; tcl_u8 state; tcl_u8 flags; tcl_u8 exp_idx;
} TclFrame;

#define FRAME_SHARE_SCOPE 1
#define FRAME_IS_PROC     2

#define TCL_OK 0
#define TCL_ERROR 1
#define TCL_RETURN 2
#define TCL_BREAK 3
#define TCL_CONTINUE 4
#define TCL_EXIT 5
#define TCL_YIELD 6

#define ST_TOKENIZE 0
#define ST_EXPAND 1
#define ST_EXECUTE 2
#define ST_RESUME 3
#define ST_COND 6
#define ST_LOOP 7
#define ST_IF_COND 8
#define ST_IF_BODY 9
#define ST_CATCH 10
#define ST_CATCH_END 11

#define TO_PTR(ctx, offset) ((offset) == TCL_NULL ? 0 : (void *)((ctx)->arena + (offset)))

static tcl_u32 t_slen(const tcl_u8 *s) { tcl_u32 l=0; while(s&&s[l])l++; return l; }
static void t_mcpy(void *d, const void *s, tcl_u32 n) { tcl_u8 *dd=(tcl_u8*)d; const tcl_u8 *ss=(const tcl_u8*)s; while(n--)*dd++=*ss++; }
static tcl_i32 t_scmp(const tcl_u8 *s1, const tcl_u8 *s2) { if(!s1||!s2) return s1==s2?0:(s1?-1:1); while(*s1&&(*s1==*s2)){s1++;s2++;} return *(tcl_u8*)s1 - *(tcl_u8*)s2; }
static tcl_i32 t_atoi(const tcl_u8 *s) { tcl_i32 r=0; tcl_i32 sign=1; if(*s=='-'){sign=-1;s++;} while(*s>='0'&&*s<='9') r=r*10+(*s++-'0'); return r*sign; }
static void t_itoa(tcl_i32 n, tcl_u8 *s) {
    tcl_i32 i=0; if(n==0){s[i++]='0';s[i]=0;return;}
    if(n<0){s[i++]='-';n=-n;}
    tcl_u8 b[12]; tcl_i32 j=0; while(n > 0){b[j++]=(tcl_u8)((n % 10)+'0');n/=10;}
    while(j>0)s[i++]=b[--j]; s[i]=0;
}

static tcl_u32 tcl_get_var(TclCtx *ctx, tcl_u32 f_off, const tcl_u8 *name);
static tcl_i32 tcl_set_var(TclCtx *ctx, tcl_u32 f_off, tcl_u32 name_off, tcl_u32 val_off);
const tcl_u8 *tcl_get_result(TclCtx *ctx);
void tcl_hal_puts(const tcl_u8 *s);

typedef struct { tcl_u32 size_and_flags; tcl_u32 forward; } ObjHeader;
#define OBJ_MARK_BIT 0x80000000
#define OBJ_VAR_BIT  0x40000000
#define OBJ_SIZE(h) ((h)->size_and_flags & ~(OBJ_MARK_BIT|OBJ_VAR_BIT))

#define HS ((sizeof(TclCtx) + 15) & ~15)

static void mark_obj(TclCtx *ctx, tcl_u32 offset) {
    if (offset == TCL_NULL) return;
    if (offset < HS || offset >= ctx->p_top) return;
    ObjHeader *h = (ObjHeader*)TO_PTR(ctx, offset - sizeof(ObjHeader));
    if (h->size_and_flags & OBJ_MARK_BIT) return;
    h->size_and_flags |= OBJ_MARK_BIT;
    if (h->size_and_flags & OBJ_VAR_BIT) {
        TclVar *v = (TclVar*)TO_PTR(ctx, offset);
        mark_obj(ctx, v->name); mark_obj(ctx, v->val); mark_obj(ctx, v->next);
    }
}

void tcl_gc(TclCtx *ctx) {
    TCL_LOG("-- GC START --\n");
    tcl_u32 old_top = ctx->p_top;
    mark_obj(ctx, ctx->result); mark_obj(ctx, ctx->g_vars);
    for (tcl_i32 i=0; i<16; i++) mark_obj(ctx, ctx->tmp_roots[i]);
    tcl_u32 fo = ctx->curr_f;
    while (fo != TCL_NULL) {
        TclFrame *f = TO_PTR(ctx, fo);
        mark_obj(ctx, f->script); mark_obj(ctx, f->vars); mark_obj(ctx, f->cond); mark_obj(ctx, f->body);
        for (tcl_i32 i=0; i<f->argc; i++) mark_obj(ctx, f->argv[i]);
        fo = f->parent;
    }
    tcl_u32 curr = HS, fp = HS;
    while (curr < ctx->p_top) {
        ObjHeader *h = (ObjHeader*)TO_PTR(ctx, curr);
        tcl_u32 sz = OBJ_SIZE(h);
        if (h->size_and_flags & OBJ_MARK_BIT) { h->forward = fp; fp += sz; }
        curr += sz;
    }
    #define UP(ptr) do { if ((ptr) != TCL_NULL && (ptr) >= HS && (ptr) < ctx->p_top) { ObjHeader *oh = (ObjHeader*)TO_PTR(ctx, (ptr) - sizeof(ObjHeader)); if (oh->size_and_flags & OBJ_MARK_BIT) (ptr) = oh->forward + sizeof(ObjHeader); else (ptr) = TCL_NULL; } } while(0)
    UP(ctx->result); UP(ctx->g_vars); for (tcl_i32 i=0; i<16; i++) UP(ctx->tmp_roots[i]);
    fo = ctx->curr_f;
    while (fo != TCL_NULL) {
        TclFrame *f = TO_PTR(ctx, fo);
        UP(f->script); UP(f->vars); UP(f->cond); UP(f->body);
        for (tcl_i32 i=0; i<f->argc; i++) UP(f->argv[i]);
        fo = f->parent;
    }
    curr = HS;
    while (curr < ctx->p_top) {
        ObjHeader *h = (ObjHeader*)TO_PTR(ctx, curr);
        tcl_u32 sz = OBJ_SIZE(h);
        if ((h->size_and_flags & OBJ_MARK_BIT) && (h->size_and_flags & OBJ_VAR_BIT)) {
            TclVar *v = (TclVar*)TO_PTR(ctx, curr + sizeof(ObjHeader));
            UP(v->name); UP(v->val); UP(v->next);
        }
        curr += sz;
    }
    curr = HS; fp = HS;
    while (curr < ctx->p_top) {
        ObjHeader *h = (ObjHeader*)TO_PTR(ctx, curr);
        tcl_u32 sz = OBJ_SIZE(h);
        if (h->size_and_flags & OBJ_MARK_BIT) {
            h->size_and_flags &= ~OBJ_MARK_BIT;
            if (curr != h->forward) {
                t_mcpy(TO_PTR(ctx, h->forward), TO_PTR(ctx, curr), sz);
            }
            fp += sz;
        }
        curr += sz;
    }
    ctx->p_top = fp;
    TCL_LOG("-- GC END --\n");
}

static tcl_u32 tcl_alc_p(TclCtx *ctx, tcl_u32 n) {
    tcl_u32 tot = (n + sizeof(ObjHeader) + 7) & ~7;
    if(ctx->p_top + tot > ctx->t_bot) {
        tcl_gc(ctx);
        if(ctx->p_top + tot > ctx->t_bot) { ctx->status=TCL_ERROR; return TCL_NULL; }
    }
    tcl_u32 a = ctx->p_top; ObjHeader *h = (ObjHeader*)TO_PTR(ctx, a); h->size_and_flags = tot; ctx->p_top += tot;
    tcl_u8 *d = TO_PTR(ctx, a + sizeof(ObjHeader));
    for (tcl_u32 i=0; i<n; i++) d[i] = 0;
    return a + sizeof(ObjHeader);
}
static tcl_u32 tcl_alc_t(TclCtx *ctx, tcl_u32 n) { 
    n=(n+7)&~7; 
    if(ctx->p_top+n>ctx->t_bot) {
        tcl_gc(ctx);
        if(ctx->p_top+n>ctx->t_bot) return TCL_NULL;
    }
    ctx->t_bot-=n; 
    for(tcl_u32 i=0; i<n; i++) ctx->arena[ctx->t_bot+i]=0;
    return ctx->t_bot; 
}

static tcl_u32 tcl_find_var_node(TclCtx *ctx, tcl_u32 fo, const tcl_u8 *name) {
    while(fo!=TCL_NULL){
        TclFrame *f=TO_PTR(ctx,fo); tcl_u32 vo=f->vars;
        while(vo!=TCL_NULL){ TclVar *v=TO_PTR(ctx,vo); if(t_scmp(name,TO_PTR(ctx,v->name))==0) return vo; vo=v->next; }
        if (f->flags & FRAME_SHARE_SCOPE) fo=f->parent; else break;
    }
    tcl_u32 vo=ctx->g_vars; while(vo!=TCL_NULL){ TclVar *v=TO_PTR(ctx,vo); if(t_scmp(name,TO_PTR(ctx,v->name))==0) return vo; vo=v->next; }
    return TCL_NULL;
}

static tcl_u32 tcl_get_var(TclCtx *ctx, tcl_u32 fo, const tcl_u8 *name) {
    tcl_u32 vo = tcl_find_var_node(ctx, fo, name);
    if (vo == TCL_NULL) return TCL_NULL;
    TclVar *v = TO_PTR(ctx, vo);
    if (v->flags & VAR_LINK) { TclVar *lv = TO_PTR(ctx, v->val); return lv->val; }
    return v->val;
}

static tcl_i32 tcl_set_var(TclCtx *ctx, tcl_u32 fo, tcl_u32 no, tcl_u32 vlo) {
    ctx->tmp_roots[4] = no; ctx->tmp_roots[5] = vlo;
    tcl_u32 vo = tcl_find_var_node(ctx, fo, TO_PTR(ctx, ctx->tmp_roots[4]));
    if (vo != TCL_NULL) {
        ctx->tmp_roots[6] = vo;
        if (ctx->tmp_roots[5] == TCL_NULL) {
            TclVar *v = TO_PTR(ctx, ctx->tmp_roots[6]);
            if (v->flags & VAR_LINK) v = TO_PTR(ctx, v->val);
            v->val = TCL_NULL; ctx->tmp_roots[4] = ctx->tmp_roots[5] = ctx->tmp_roots[6] = TCL_NULL; return TCL_OK;
        }
        tcl_u32 nl = t_slen(TO_PTR(ctx, ctx->tmp_roots[5])) + 1, nv = tcl_alc_p(ctx, nl);
        if (nv == TCL_NULL) { ctx->tmp_roots[4] = ctx->tmp_roots[5] = ctx->tmp_roots[6] = TCL_NULL; return TCL_ERROR; }
        t_mcpy(TO_PTR(ctx, nv), TO_PTR(ctx, ctx->tmp_roots[5]), nl);
        TclVar *v = TO_PTR(ctx, ctx->tmp_roots[6]);
        if (v->flags & VAR_LINK) v = TO_PTR(ctx, v->val);
        v->val = nv; ctx->tmp_roots[4] = ctx->tmp_roots[5] = ctx->tmp_roots[6] = TCL_NULL; return TCL_OK;
    }
    tcl_u32 curr_fo = fo;
    while(curr_fo!=TCL_NULL){ TclFrame *pf=TO_PTR(ctx,curr_fo); if(!(pf->flags&FRAME_SHARE_SCOPE)) break; curr_fo=pf->parent; }
    ctx->tmp_roots[6] = curr_fo; // Frame where vars will be added
    
    tcl_u32 nvo = tcl_alc_p(ctx, sizeof(TclVar)); if (nvo == TCL_NULL) { ctx->tmp_roots[4] = ctx->tmp_roots[5] = ctx->tmp_roots[6] = TCL_NULL; return TCL_ERROR; }
    ctx->tmp_roots[7] = nvo;
    TclVar *nv = TO_PTR(ctx, ctx->tmp_roots[7]); nv->name = nv->val = nv->next = TCL_NULL; nv->flags = 0;
    ((ObjHeader*)TO_PTR(ctx, ctx->tmp_roots[7] - sizeof(ObjHeader)))->size_and_flags |= OBJ_VAR_BIT;
    
    tcl_u32 nno = tcl_alc_p(ctx, t_slen(TO_PTR(ctx, ctx->tmp_roots[4])) + 1);
    if (nno == TCL_NULL) { ctx->tmp_roots[4] = ctx->tmp_roots[5] = ctx->tmp_roots[6] = ctx->tmp_roots[7] = TCL_NULL; return TCL_ERROR; }
    ctx->tmp_roots[8] = nno;
    
    tcl_u32 nvo_val = TCL_NULL;
    if (ctx->tmp_roots[5] != TCL_NULL) {
        nvo_val = tcl_alc_p(ctx, t_slen(TO_PTR(ctx, ctx->tmp_roots[5])) + 1);
        if (nvo_val == TCL_NULL) { ctx->tmp_roots[4] = ctx->tmp_roots[5] = ctx->tmp_roots[6] = ctx->tmp_roots[7] = ctx->tmp_roots[8] = TCL_NULL; return TCL_ERROR; }
    }
    t_mcpy(TO_PTR(ctx, ctx->tmp_roots[8]), TO_PTR(ctx, ctx->tmp_roots[4]), t_slen(TO_PTR(ctx, ctx->tmp_roots[4])) + 1);
    if (ctx->tmp_roots[5] != TCL_NULL) t_mcpy(TO_PTR(ctx, nvo_val), TO_PTR(ctx, ctx->tmp_roots[5]), t_slen(TO_PTR(ctx, ctx->tmp_roots[5])) + 1);
    
    TclFrame *rf = (ctx->tmp_roots[6] == TCL_NULL) ? 0 : TO_PTR(ctx, ctx->tmp_roots[6]);
    tcl_u32 *h = rf ? &rf->vars : &ctx->g_vars;
    
    nv = TO_PTR(ctx, ctx->tmp_roots[7]); nv->name = ctx->tmp_roots[8]; nv->val = nvo_val; nv->next = *h; *h = ctx->tmp_roots[7];
    ctx->tmp_roots[4] = ctx->tmp_roots[5] = ctx->tmp_roots[6] = ctx->tmp_roots[7] = ctx->tmp_roots[8] = TCL_NULL; return TCL_OK;
}

static tcl_i32 tcl_cmd_set(TclCtx *ctx, tcl_i32 argc, tcl_u32 *argv) {
    if(argc<2) return TCL_ERROR;
    if(argc==2){ tcl_u32 v=tcl_get_var(ctx,ctx->curr_f,TO_PTR(ctx,argv[1])); if(v==TCL_NULL) return TCL_ERROR; ctx->result=v; }
    else { if(tcl_set_var(ctx,ctx->curr_f,argv[1],argv[2])!=TCL_OK) return TCL_ERROR; ctx->result=argv[2]; }
    return TCL_OK;
}
static tcl_i32 tcl_cmd_proc(TclCtx *ctx, tcl_i32 argc, tcl_u32 *argv) {
    if(argc<4) return TCL_ERROR;
    ctx->tmp_roots[0]=tcl_alc_p(ctx,64); if(ctx->tmp_roots[0]==TCL_NULL) return TCL_ERROR;
    tcl_u8 *pn=TO_PTR(ctx,ctx->tmp_roots[0]); pn[0]='p';pn[1]=':'; const tcl_u8 *n=TO_PTR(ctx,argv[1]); tcl_i32 i=0; while(n[i]&&i<60){pn[i+2]=n[i];i++;} pn[i+2]=0;
    if(tcl_set_var(ctx,TCL_NULL,ctx->tmp_roots[0],argv[3])!=TCL_OK){ ctx->tmp_roots[0]=TCL_NULL; return TCL_ERROR; }
    ctx->tmp_roots[1]=tcl_alc_p(ctx,64); if(ctx->tmp_roots[1]==TCL_NULL){ ctx->tmp_roots[0]=TCL_NULL; return TCL_ERROR; }
    tcl_u8 *an=TO_PTR(ctx,ctx->tmp_roots[1]); t_mcpy(an,TO_PTR(ctx,ctx->tmp_roots[0]),64); an[0]='a';
    tcl_i32 res = tcl_set_var(ctx,TCL_NULL,ctx->tmp_roots[1],argv[2]);
    ctx->tmp_roots[0]=ctx->tmp_roots[1]=TCL_NULL; return res;
}
static tcl_i32 tcl_cmd_if(TclCtx *ctx, tcl_i32 argc, tcl_u32 *argv) { if(argc<3) return TCL_ERROR; TclFrame *f=TO_PTR(ctx,ctx->curr_f); f->cond=argv[1]; f->body=argv[2]; f->state=ST_IF_COND; return TCL_YIELD; }
static tcl_i32 tcl_cmd_while(TclCtx *ctx, tcl_i32 argc, tcl_u32 *argv) { if(argc<3) return TCL_ERROR; TclFrame *f=TO_PTR(ctx,ctx->curr_f); f->cond=argv[1]; f->body=argv[2]; f->state=ST_COND; return TCL_YIELD; }

static tcl_i32 tcl_cmd_expr(TclCtx *ctx, tcl_i32 argc, tcl_u32 *argv) {
    if(argc<2) return TCL_ERROR;
    tcl_u8 *s = TO_PTR(ctx, argv[1]);
    if (s[0] == '{') {
        tcl_u32 l = t_slen(s);
        ctx->tmp_roots[0] = tcl_alc_p(ctx, l); if (ctx->tmp_roots[0] == TCL_NULL) return TCL_ERROR;
        tcl_u8 *d = TO_PTR(ctx, ctx->tmp_roots[0]); t_mcpy(d, s + 1, l - 2); d[l - 2] = 0;
        tcl_u32 fo = tcl_alc_t(ctx, sizeof(TclFrame)); if (fo == TCL_NULL) return TCL_ERROR;
        TclFrame *f = TO_PTR(ctx, fo); f->script = ctx->tmp_roots[0]; f->pc = 0; f->vars = TCL_NULL; f->parent = ctx->curr_f; f->state = ST_TOKENIZE; f->flags = FRAME_SHARE_SCOPE;
        f->cond = f->body = f->result = TCL_NULL; f->argc = f->exp_idx = 0;
        ctx->tmp_roots[0] = TCL_NULL; ((TclFrame*)TO_PTR(ctx, ctx->curr_f))->state = ST_RESUME;
        ctx->curr_f = fo; return TCL_YIELD;
    }
    if(argc==4){
        tcl_i32 v1=t_atoi(TO_PTR(ctx,argv[1])), v2=t_atoi(TO_PTR(ctx,argv[3])); const tcl_u8 *op=TO_PTR(ctx,argv[2]); tcl_i32 res=0;
        if(t_scmp(op,(tcl_u8*)"==")==0) res=(v1==v2); else if(t_scmp(op,(tcl_u8*)"!=")==0) res=(v1!=v2); else if(t_scmp(op,(tcl_u8*)">")==0) res=(v1>v2); else if(t_scmp(op,(tcl_u8*)"<")==0) res=(v1<v2); else if(t_scmp(op,(tcl_u8*)"+")==0) res=(v1+v2); else if(t_scmp(op,(tcl_u8*)"-")==0) res=(v1-v2); else if(t_scmp(op,(tcl_u8*)"*")==0) res=(v1*v2); else if(t_scmp(op,(tcl_u8*)"/")==0) res=(v2?v1/v2:0); else if(t_scmp(op,(tcl_u8*)"%")==0) res=(v2?v1%v2:0);
        tcl_u32 r=tcl_alc_p(ctx,12); if(r!=TCL_NULL){ t_itoa(res,TO_PTR(ctx,r)); ctx->result=r; }
    } else ctx->result=argv[1]; return TCL_OK;
}

static tcl_i32 tcl_cmd_return(TclCtx *ctx, tcl_i32 argc, tcl_u32 *argv) { if(argc==2) ctx->result=argv[1]; return TCL_RETURN; }
static tcl_i32 tcl_cmd_break(TclCtx *ctx, tcl_i32 argc, tcl_u32 *argv) { return TCL_BREAK; }
static tcl_i32 tcl_cmd_continue(TclCtx *ctx, tcl_i32 argc, tcl_u32 *argv) { return TCL_CONTINUE; }
static tcl_i32 tcl_cmd_error(TclCtx *ctx, tcl_i32 argc, tcl_u32 *argv) { if(argc>1) ctx->result=argv[1]; return TCL_ERROR; }

static tcl_i32 tcl_cmd_eval(TclCtx *ctx, tcl_i32 argc, tcl_u32 *argv) {
    if(argc<2) return TCL_ERROR;
    tcl_u32 fo=tcl_alc_t(ctx,sizeof(TclFrame)); if(fo==TCL_NULL) return TCL_ERROR;
    TclFrame *f=TO_PTR(ctx,fo); f->script=argv[1]; f->pc=0; f->vars=TCL_NULL; f->parent=ctx->curr_f; f->state=ST_TOKENIZE; f->flags=FRAME_SHARE_SCOPE;
    f->cond=f->body=f->result=TCL_NULL; f->argc=f->exp_idx=0;
    ctx->curr_f=fo; return TCL_YIELD;
}

static tcl_i32 tcl_cmd_catch(TclCtx *ctx, tcl_i32 argc, tcl_u32 *argv) {
    if(argc<2) return TCL_ERROR;
    ctx->tmp_roots[8] = argv[1]; ctx->tmp_roots[9] = argc>2?argv[2]:TCL_NULL;
    tcl_u32 fo=tcl_alc_t(ctx,sizeof(TclFrame)); if(fo==TCL_NULL) return TCL_ERROR;
    TclFrame *f=TO_PTR(ctx,ctx->curr_f); f->cond=ctx->tmp_roots[8]; f->body=ctx->tmp_roots[9]; f->state=ST_CATCH_END;
    TclFrame *nf=TO_PTR(ctx,fo); nf->script=ctx->tmp_roots[8]; nf->pc=0; nf->vars=TCL_NULL; nf->parent=ctx->curr_f; nf->state=ST_TOKENIZE; nf->flags=FRAME_SHARE_SCOPE;
    nf->cond=nf->body=nf->result=TCL_NULL; nf->argc=nf->exp_idx=0;
    ctx->tmp_roots[8]=ctx->tmp_roots[9]=TCL_NULL; ctx->curr_f=fo; return TCL_YIELD;
}

static tcl_i32 tcl_cmd_upvar(TclCtx *ctx, tcl_i32 argc, tcl_u32 *argv) {
    if (argc < 3) return TCL_ERROR;
    tcl_i32 level = 1; tcl_i32 arg_idx = 1;
    if (argc > 3 && ((tcl_u8*)TO_PTR(ctx, argv[1]))[0] != '#' && !(((tcl_u8*)TO_PTR(ctx, argv[1]))[0] >= '0' && ((tcl_u8*)TO_PTR(ctx, argv[1]))[0] <= '9')) { /* Not a level */ }
    else if (argc > 3) { level = t_atoi(TO_PTR(ctx, argv[1])); arg_idx = 2; }
    tcl_u32 p = ctx->curr_f; for (tcl_i32 i=0; i<level && p!=TCL_NULL; i++) p = ((TclFrame*)TO_PTR(ctx, p))->parent;
    ctx->tmp_roots[12] = argv[arg_idx]; // otherVar name
    ctx->tmp_roots[13] = argv[arg_idx + 1]; // myVar name
    tcl_u32 target_vo = tcl_find_var_node(ctx, p, TO_PTR(ctx, ctx->tmp_roots[12]));
    if (target_vo == TCL_NULL) {
        if (tcl_set_var(ctx, p, ctx->tmp_roots[12], TCL_NULL) != TCL_OK) { ctx->tmp_roots[12]=ctx->tmp_roots[13]=TCL_NULL; return TCL_ERROR; }
        target_vo = tcl_find_var_node(ctx, p, TO_PTR(ctx, ctx->tmp_roots[12]));
    }
    ctx->tmp_roots[14] = target_vo;
    tcl_u32 nvo = tcl_alc_p(ctx, sizeof(TclVar)); if (nvo == TCL_NULL) { ctx->tmp_roots[12]=ctx->tmp_roots[13]=ctx->tmp_roots[14]=TCL_NULL; return TCL_ERROR; }
    ctx->tmp_roots[15] = nvo;
    
    TclVar *nv = TO_PTR(ctx, ctx->tmp_roots[15]); nv->name = nv->val = nv->next = TCL_NULL; nv->flags = VAR_LINK;
    ((ObjHeader*)TO_PTR(ctx, ctx->tmp_roots[15] - sizeof(ObjHeader)))->size_and_flags |= OBJ_VAR_BIT;
    
    tcl_u32 nl_no = tcl_alc_p(ctx, t_slen(TO_PTR(ctx, ctx->tmp_roots[13])) + 1);
    if (nl_no == TCL_NULL) { ctx->tmp_roots[12]=ctx->tmp_roots[13]=ctx->tmp_roots[14]=ctx->tmp_roots[15]=TCL_NULL; return TCL_ERROR; }
    t_mcpy(TO_PTR(ctx, nl_no), TO_PTR(ctx, ctx->tmp_roots[13]), t_slen(TO_PTR(ctx, ctx->tmp_roots[13])) + 1);
    
    TclFrame *cf = TO_PTR(ctx, ctx->curr_f);
    nv = TO_PTR(ctx, ctx->tmp_roots[15]); nv->name = nl_no; nv->val = ctx->tmp_roots[14]; nv->next = cf->vars; cf->vars = ctx->tmp_roots[15];
    ctx->tmp_roots[12]=ctx->tmp_roots[13]=ctx->tmp_roots[14]=ctx->tmp_roots[15]=TCL_NULL;
    return TCL_OK;
}

static tcl_i32 tcl_cmd_uplevel(TclCtx *ctx, tcl_i32 argc, tcl_u32 *argv) {
    if(argc<2) return TCL_ERROR;
    tcl_u32 p = ctx->curr_f; tcl_i32 level = 1; tcl_i32 arg_idx = 1;
    if (argc > 2 && ((tcl_u8*)TO_PTR(ctx, argv[1]))[0] != '{' && ((tcl_u8*)TO_PTR(ctx, argv[1]))[0] != '[') { level = t_atoi(TO_PTR(ctx, argv[1])); arg_idx = 2; }
    for (tcl_i32 i=0; i<level && p!=TCL_NULL; i++) p = ((TclFrame*)TO_PTR(ctx, p))->parent;
    tcl_u32 fo=tcl_alc_t(ctx,sizeof(TclFrame)); if(fo==TCL_NULL) return TCL_ERROR;
    TclFrame *f=TO_PTR(ctx,fo); f->script=argv[arg_idx]; f->pc=0; f->vars=TCL_NULL; f->parent=p; f->state=ST_TOKENIZE; f->flags=FRAME_SHARE_SCOPE;
    f->cond=f->body=f->result=TCL_NULL; f->argc=f->exp_idx=0;
    ctx->curr_f=fo; return TCL_YIELD;
}

static tcl_i32 tcl_cmd_list(TclCtx *ctx, tcl_i32 argc, tcl_u32 *argv) {
    tcl_u32 total_l = 0; for(tcl_i32 i=1; i<argc; i++) total_l += t_slen(TO_PTR(ctx,argv[i])) + 3;
    tcl_u32 no = tcl_alc_p(ctx, total_l + 1); if(no==TCL_NULL) return TCL_ERROR;
    tcl_u8 *d = TO_PTR(ctx, no);
    for(tcl_i32 i=1; i<argc; i++) {
        const tcl_u8 *s = TO_PTR(ctx, argv[i]); tcl_i32 space=0; for(tcl_i32 j=0; s[j]; j++) if(s[j]==' '||s[j]=='\t'||s[j]=='\n') space=1;
        if(space) *d++='{'; t_mcpy(d, s, t_slen(s)); d+=t_slen(s); if(space) *d++='}';
        if(i<argc-1) *d++=' ';
    }
    *d=0; ctx->result = no; return TCL_OK;
}

static tcl_i32 tcl_cmd_llength(TclCtx *ctx, tcl_i32 argc, tcl_u32 *argv) {
    if(argc<2) return TCL_ERROR;
    const tcl_u8 *s = TO_PTR(ctx, argv[1]); tcl_i32 count=0, d=0;
    while(*s){
        while(*s==' '||*s=='\t'||*s=='\n') s++; if(!*s) break;
        count++; if(*s=='{'){ d=1; s++; while(*s && d>0){ if(*s=='{')d++; else if(*s=='}')d--; s++; } }
        else while(*s && *s!=' ' && *s!='\t' && *s!='\n') s++;
    }
    tcl_u32 r=tcl_alc_p(ctx,12); if(r!=TCL_NULL){ t_itoa(count,TO_PTR(ctx,r)); ctx->result=r; } return TCL_OK;
}

static tcl_i32 tcl_cmd_lindex(TclCtx *ctx, tcl_i32 argc, tcl_u32 *argv) {
    if(argc<3) return TCL_ERROR;
    const tcl_u8 *s = TO_PTR(ctx, argv[1]); tcl_i32 idx = t_atoi(TO_PTR(ctx, argv[2])), count=0, d=0;
    while(*s){
        while(*s==' '||*s=='\t'||*s=='\n') s++; if(!*s) break;
        const tcl_u8 *st = s; tcl_i32 len=0;
        if(*s=='{'){ s++; st=s; d=1; while(*s && d>0){ if(*s=='{')d++; else if(*s=='}')d--; s++; } len=s-st-1; }
        else { while(*s && *s!=' ' && *s!='\t' && *s!='\n') s++; len=s-st; }
        if(count==idx){ tcl_u32 no=tcl_alc_p(ctx,len+1); if(no==TCL_NULL) return TCL_ERROR; t_mcpy(TO_PTR(ctx,no),st,len); ((tcl_u8*)TO_PTR(ctx,no))[len]=0; ctx->result=no; return TCL_OK; }
        count++;
    }
    ctx->result = TCL_NULL; return TCL_OK;
}

static tcl_i32 tcl_cmd_lrange(TclCtx *ctx, tcl_i32 argc, tcl_u32 *argv) {
    if (argc < 4) return TCL_ERROR;
    tcl_i32 first = t_atoi(TO_PTR(ctx, argv[2])), last = tcl_i32_MAX;
    if (t_scmp(TO_PTR(ctx, argv[3]), (tcl_u8*)"end") != 0) last = t_atoi(TO_PTR(ctx, argv[3]));
    tcl_u32 res_list = tcl_alc_p(ctx, t_slen(TO_PTR(ctx, argv[1])) + 1); if (res_list == TCL_NULL) return TCL_ERROR;
    tcl_u8 *d = TO_PTR(ctx, res_list); const tcl_u8 *s = TO_PTR(ctx, argv[1]); tcl_i32 count = 0;
    while (*s) {
        while (*s == ' ' || *s == '\t' || *s == '\n') s++; if (!*s) break;
        const tcl_u8 *st = s; tcl_i32 d_cnt = 0;
        if (*s == '{') { s++; st = s; d_cnt = 1; while (*s && d_cnt > 0) { if (*s == '{') d_cnt++; else if (*s == '}') d_cnt--; s++; } }
        else while (*s && *s != ' ' && *s != '\t' && *s != '\n') s++;
        if (count >= first && (last == tcl_i32_MAX || count <= last)) {
            if (d != TO_PTR(ctx, res_list)) *d++ = ' ';
            t_mcpy(d, st, s - st - (d_cnt?1:0)); d += (s - st - (d_cnt?1:0));
        }
        count++;
    }
    *d = 0; ctx->result = res_list; return TCL_OK;
}

static tcl_i32 tcl_cmd_unset(TclCtx *ctx, tcl_i32 argc, tcl_u32 *argv) {
    if (argc < 2) return TCL_ERROR;
    const tcl_u8 *name = TO_PTR(ctx, argv[1]);
    tcl_u32 fo = ctx->curr_f;
    while (fo != TCL_NULL) {
        TclFrame *f = TO_PTR(ctx, fo); tcl_u32 *vo_p = &f->vars, vo = *vo_p;
        while (vo != TCL_NULL) {
            TclVar *v = TO_PTR(ctx, vo);
            if (t_scmp(name, TO_PTR(ctx, v->name)) == 0) { *vo_p = v->next; return TCL_OK; }
            vo_p = &v->next; vo = *vo_p;
        }
        if (f->flags & FRAME_SHARE_SCOPE) fo = f->parent; else break;
    }
    tcl_u32 *vo_p = &ctx->g_vars, vo = *vo_p;
    while (vo != TCL_NULL) {
        TclVar *v = TO_PTR(ctx, vo);
        if (t_scmp(name, TO_PTR(ctx, v->name)) == 0) { *vo_p = v->next; return TCL_OK; }
        vo_p = &v->next; vo = *vo_p;
    }
    return TCL_OK;
}

typedef tcl_i32 (*Tcl_CmdProc)(TclCtx *ctx, tcl_i32 argc, tcl_u32 *argv);
typedef struct { const tcl_u8 *name; Tcl_CmdProc proc; } TclCmd;
static TclCmd cmd_table[64]; static tcl_i32 cmd_count=0;
void tcl_register_c_cmd(const tcl_u8 *n, Tcl_CmdProc p) { if(cmd_count<64){ cmd_table[cmd_count].name=n; cmd_table[cmd_count].proc=p; cmd_count++; } }

tcl_i32 tcl_eval(TclCtx *ctx, const tcl_u8 *script) {
    tcl_u32 sl=t_slen(script)+1, so=tcl_alc_p(ctx,sl); if(so==TCL_NULL) return TCL_ERROR; t_mcpy(TO_PTR(ctx,so),script,sl);
    ctx->tmp_roots[0]=so;
    tcl_u32 fo=tcl_alc_t(ctx,sizeof(TclFrame)); if(fo==TCL_NULL) { ctx->tmp_roots[0]=TCL_NULL; return TCL_ERROR; }
    so=ctx->tmp_roots[0]; ctx->tmp_roots[0]=TCL_NULL;
    TclFrame *f=TO_PTR(ctx,fo); f->script=so; f->pc=0; f->vars=TCL_NULL; f->parent=ctx->curr_f; f->state=ST_TOKENIZE; f->flags=0; f->exp_idx=0; f->argc=0;
    f->cond=f->body=f->result=TCL_NULL; ctx->curr_f=fo;
    while(ctx->curr_f!=TCL_NULL){
        f=TO_PTR(ctx,ctx->curr_f); const tcl_u8 *s=TO_PTR(ctx,f->script);
        switch(f->state){
            case ST_TOKENIZE:
                f->argc=0; f->exp_idx=0;
                while(s[f->pc]&&(s[f->pc]==';'||s[f->pc]=='\n'||s[f->pc]=='\r'||s[f->pc]==' '||s[f->pc]=='\t')) f->pc++;
                if(s[f->pc]=='#') { while(s[f->pc]&&s[f->pc]!='\n'&&s[f->pc]!='\r') f->pc++; continue; }
                if(!s[f->pc]) { tcl_u32 p=f->parent; ctx->t_bot+=((sizeof(TclFrame)+7)&~7); ctx->curr_f=p; break; }
                while(s[f->pc]&&f->argc<MAX_ARGS){
                    while(s[f->pc]&&(s[f->pc]==' '||s[f->pc]=='\t')) f->pc++;
                    if(!s[f->pc]||s[f->pc]==';'||s[f->pc]=='\n'||s[f->pc]=='\r') break;
                    tcl_u32 st=f->pc;
                    if(s[f->pc]=='{'){
                        tcl_i32 d=1; f->pc++; st=f->pc; while(s[f->pc]&&d>0){ if(s[f->pc]=='{')d++; else if(s[f->pc]=='}')d--; if(d>0)f->pc++; }
                        tcl_u32 l=f->pc-st, a=tcl_alc_p(ctx,l+1); if(a!=TCL_NULL){ f=TO_PTR(ctx,ctx->curr_f); s=TO_PTR(ctx,f->script); t_mcpy(TO_PTR(ctx,a),s+st,l); ((tcl_u8*)TO_PTR(ctx,a))[l]=0; f->argv[f->argc++]=a; }
                        if(s[f->pc]=='}') f->pc++;
                    } else if(s[f->pc]=='['){
                        tcl_i32 d=1; f->pc++; st=f->pc-1; while(s[f->pc]&&d>0){ if(s[f->pc]=='[')d++; else if(s[f->pc]==']')d--; f->pc++; }
                        tcl_u32 l=f->pc-st, a=tcl_alc_p(ctx,l+1); if(a!=TCL_NULL){ f=TO_PTR(ctx,ctx->curr_f); s=TO_PTR(ctx,f->script); t_mcpy(TO_PTR(ctx,a),s+st,l); ((tcl_u8*)TO_PTR(ctx,a))[l]=0; f->argv[f->argc++]=a; }
                    } else {
                        while(s[f->pc]&&s[f->pc]!=' '&&s[f->pc]!='\t'&&s[f->pc]!='\n'&&s[f->pc]!='\r'&&s[f->pc]!=';'&&s[f->pc]!=']') f->pc++;
                        tcl_u32 l=f->pc-st, a=tcl_alc_p(ctx,l+1); if(a!=TCL_NULL){ f=TO_PTR(ctx,ctx->curr_f); s=TO_PTR(ctx,f->script); t_mcpy(TO_PTR(ctx,a),s+st,l); ((tcl_u8*)TO_PTR(ctx,a))[l]=0; f->argv[f->argc++]=a; }
                    }
                }
                while(s[f->pc]&&(s[f->pc]==';'||s[f->pc]=='\n'||s[f->pc]=='\r'||s[f->pc]==' '||s[f->pc]=='\t')) f->pc++;
                f->state=ST_EXPAND; break;
            case ST_EXPAND:
                while(f->exp_idx<f->argc){
                    tcl_u32 ao = f->argv[f->exp_idx]; if(ao==TCL_NULL){ f->exp_idx++; continue; }
                    tcl_u8 *arg=TO_PTR(ctx,ao);
                    if(arg[0]=='$'){
                        tcl_u32 v=tcl_get_var(ctx,ctx->curr_f,arg+1);
                        if(v!=TCL_NULL) f->argv[f->exp_idx]=v;
                        else { ctx->status=TCL_ERROR; ctx->curr_f=TCL_NULL; return TCL_ERROR; }
                    } else if(arg[0]=='['){
                        tcl_u32 nfo=tcl_alc_t(ctx,sizeof(TclFrame)); if(nfo==TCL_NULL) { ctx->status=TCL_ERROR; ctx->curr_f=TCL_NULL; return TCL_ERROR; }
                        f=TO_PTR(ctx,ctx->curr_f); tcl_u8 *arg2=TO_PTR(ctx,f->argv[f->exp_idx]); tcl_u32 sl2=t_slen(arg2);
                        ctx->tmp_roots[0] = tcl_alc_p(ctx,sl2); if(ctx->tmp_roots[0]==TCL_NULL){ctx->status=TCL_ERROR;return TCL_ERROR;}
                        tcl_u32 sco=ctx->tmp_roots[0]; f=TO_PTR(ctx,ctx->curr_f); arg2=TO_PTR(ctx,f->argv[f->exp_idx]);
                        t_mcpy(TO_PTR(ctx,sco),arg2+1,sl2-2); ((tcl_u8*)TO_PTR(ctx,sco))[sl2-2]=0;
                        TclFrame *nf=TO_PTR(ctx,nfo); nf->script=sco; nf->pc=0; nf->vars=TCL_NULL; nf->parent=ctx->curr_f; nf->state=ST_TOKENIZE; nf->flags=FRAME_SHARE_SCOPE;
                        nf->cond=nf->body=nf->result=TCL_NULL; nf->argc=nf->exp_idx=0;
                        ctx->tmp_roots[0]=TCL_NULL; f->state=ST_RESUME; ctx->curr_f=nfo; goto next_loop;
                    }
                    f->exp_idx++;
                }
                f->state=ST_EXECUTE; break;
            case ST_EXECUTE: {
                const tcl_u8 *cmd=TO_PTR(ctx,f->argv[0]); tcl_i32 found=0;
                for(tcl_i32 i=0;i<cmd_count;i++){ if(t_scmp(cmd,cmd_table[i].name)==0){ ctx->status=cmd_table[i].proc(ctx,f->argc,f->argv); f=TO_PTR(ctx,ctx->curr_f); found=1; break; } }
                if(!found){
                    tcl_u32 vo=ctx->g_vars, body=TCL_NULL, args_list=TCL_NULL;
                    while(vo!=TCL_NULL){ TclVar *v=TO_PTR(ctx,vo); const tcl_u8 *vn=TO_PTR(ctx,v->name); if(vn[0]=='p'&&vn[1]==':'&&t_scmp(cmd,vn+2)==0) body=v->val; if(vn[0]=='a'&&vn[1]==':'&&t_scmp(cmd,vn+2)==0) args_list=v->val; vo=v->next; }
                    if(body!=TCL_NULL){
                        tcl_u32 nfo=tcl_alc_t(ctx,sizeof(TclFrame)); if(nfo==TCL_NULL) ctx->status=TCL_ERROR;
                        else {
                            TclFrame *nf=TO_PTR(ctx,nfo); nf->script=body; nf->pc=0; nf->vars=TCL_NULL; nf->parent=ctx->curr_f; nf->state=ST_TOKENIZE; nf->flags=FRAME_IS_PROC;
                            nf->cond=nf->body=nf->result=TCL_NULL; nf->argc=nf->exp_idx=0;
                            ctx->tmp_roots[0] = args_list; tcl_i32 ai = 1; tcl_u32 al_off = 0;
                            while (ai < f->argc) {
                                const tcl_u8 *al = (const tcl_u8*)TO_PTR(ctx, ctx->tmp_roots[0]) + al_off;
                                while (*al == ' ' || *al == '\t') { al++; al_off++; } if (!*al) break;
                                tcl_u32 st_off = al_off; while (*al && *al != ' ' && *al != '\t') { al++; al_off++; }
                                tcl_i32 len = al_off - st_off; ctx->tmp_roots[1] = tcl_alc_p(ctx, len+1); if(ctx->tmp_roots[1]==TCL_NULL) { ctx->status=TCL_ERROR; break; }
                                f=TO_PTR(ctx,ctx->curr_f); t_mcpy(TO_PTR(ctx,ctx->tmp_roots[1]), (const tcl_u8*)TO_PTR(ctx, ctx->tmp_roots[0]) + st_off, len);
                                ((tcl_u8*)TO_PTR(ctx,ctx->tmp_roots[1]))[len] = 0;
                                tcl_set_var(ctx, nfo, ctx->tmp_roots[1], f->argv[ai++]);
                                ctx->tmp_roots[1]=TCL_NULL; f=TO_PTR(ctx,ctx->curr_f);
                            }
                            ctx->tmp_roots[0]=TCL_NULL; f->state=ST_RESUME; ctx->curr_f=nfo; goto next_loop;
                        }
                    } else { ctx->status=TCL_ERROR; }
                }
                if(ctx->status==TCL_EXIT) { ctx->curr_f=TCL_NULL; break; }
                if(ctx->status!=TCL_OK && ctx->status!=TCL_YIELD){
                    tcl_i32 s = ctx->status; tcl_u32 r = ctx->result;
                    while(f){
                        tcl_u8 fl=f->flags; tcl_u32 p=f->parent; ctx->t_bot+=((sizeof(TclFrame)+7)&~7); ctx->curr_f=p;
                        if(p==TCL_NULL){ f=0; break; } f=TO_PTR(ctx,p);
                        if(f->state==ST_CATCH_END || ((s==TCL_BREAK||s==TCL_CONTINUE) && (f->state==ST_LOOP || f->state==ST_COND))) break;
                        if(s==TCL_RETURN && (fl&FRAME_IS_PROC)) break;
                    }
                    ctx->status=s; ctx->result=r; if(!f) ctx->curr_f=TCL_NULL;
                } else if(ctx->status==TCL_OK) f->state=ST_TOKENIZE;
                break;
            }
            case ST_IF_COND: {
                tcl_u32 nfo=tcl_alc_t(ctx,sizeof(TclFrame)); if(nfo==TCL_NULL){ ctx->status=TCL_ERROR; ctx->curr_f=TCL_NULL; break; }
                TclFrame *nf=TO_PTR(ctx,nfo); nf->script=f->cond; nf->pc=0; nf->vars=TCL_NULL; nf->parent=ctx->curr_f; nf->state=ST_TOKENIZE; nf->flags=FRAME_SHARE_SCOPE;
                nf->cond=nf->body=nf->result=TCL_NULL; nf->argc=nf->exp_idx=0; f->state=ST_IF_BODY; ctx->curr_f=nfo; break;
            }
            case ST_IF_BODY: {
                if(ctx->status!=TCL_OK){ tcl_i32 s = ctx->status; tcl_u32 r = ctx->result; tcl_u32 p=f->parent; ctx->t_bot+=((sizeof(TclFrame)+7)&~7); ctx->curr_f=p; ctx->status=s; ctx->result=r; break; }
                const tcl_u8 *res=tcl_get_result(ctx);
                if(res[0] && res[0]!='0'){
                    tcl_u32 nfo=tcl_alc_t(ctx,sizeof(TclFrame)); if(nfo==TCL_NULL){ ctx->status=TCL_ERROR; ctx->curr_f=TCL_NULL; break; }
                    TclFrame *nf=TO_PTR(ctx,nfo); nf->script=f->body; nf->pc=0; nf->vars=TCL_NULL; nf->parent=ctx->curr_f; nf->state=ST_TOKENIZE; nf->flags=FRAME_SHARE_SCOPE;
                    nf->cond=nf->body=nf->result=TCL_NULL; nf->argc=nf->exp_idx=0; f->state=ST_TOKENIZE; ctx->curr_f=nfo;
                } else f->state=ST_TOKENIZE; break;
            }
            case ST_COND: {
                if(ctx->status==TCL_BREAK){ ctx->status=TCL_OK; f->state=ST_TOKENIZE; break; }
                if(ctx->status==TCL_CONTINUE) ctx->status=TCL_OK;
                tcl_u32 nfo=tcl_alc_t(ctx,sizeof(TclFrame)); if(nfo==TCL_NULL){ ctx->status=TCL_ERROR; ctx->curr_f=TCL_NULL; break; }
                TclFrame *nf=TO_PTR(ctx,nfo); nf->script=f->cond; nf->pc=0; nf->vars=TCL_NULL; nf->parent=ctx->curr_f; nf->state=ST_TOKENIZE; nf->flags=FRAME_SHARE_SCOPE;
                nf->cond=nf->body=nf->result=TCL_NULL; nf->argc=nf->exp_idx=0; f->state=ST_LOOP; ctx->curr_f=nfo; break;
            }
            case ST_LOOP: {
                if(ctx->status==TCL_BREAK){ ctx->status=TCL_OK; f->state=ST_TOKENIZE; break; }
                if(ctx->status==TCL_CONTINUE) { ctx->status=TCL_OK; f->state=ST_COND; break; }
                if(ctx->status!=TCL_OK){ tcl_i32 s = ctx->status; tcl_u32 r = ctx->result; tcl_u32 p=f->parent; ctx->t_bot+=((sizeof(TclFrame)+7)&~7); ctx->curr_f=p; ctx->status=s; ctx->result=r; break; }
                const tcl_u8 *res=tcl_get_result(ctx);
                if(res[0] && res[0]!='0'){
                    tcl_u32 nfo=tcl_alc_t(ctx,sizeof(TclFrame)); if(nfo==TCL_NULL){ ctx->status=TCL_ERROR; ctx->curr_f=TCL_NULL; break; }
                    TclFrame *nf=TO_PTR(ctx,nfo); nf->script=f->body; nf->pc=0; nf->vars=TCL_NULL; nf->parent=ctx->curr_f; nf->state=ST_TOKENIZE; nf->flags=FRAME_SHARE_SCOPE;
                    nf->cond=nf->body=nf->result=TCL_NULL; nf->argc=nf->exp_idx=0; f->state=ST_COND; ctx->curr_f=nfo;
                } else f->state=ST_TOKENIZE; break;
            }
            case ST_CATCH_END: {
                ctx->tmp_roots[0] = ctx->result; ctx->tmp_roots[1] = f->body;
                tcl_u32 s_off=tcl_alc_p(ctx,12); if(s_off==TCL_NULL) { ctx->tmp_roots[0]=ctx->tmp_roots[1]=TCL_NULL; ctx->status=TCL_ERROR; break; }
                t_itoa(ctx->status,TO_PTR(ctx,s_off)); ctx->tmp_roots[2] = s_off; f=TO_PTR(ctx,ctx->curr_f);
                if(ctx->tmp_roots[1]!=TCL_NULL) tcl_set_var(ctx, f->parent, ctx->tmp_roots[1], ctx->tmp_roots[0]);
                ctx->result=ctx->tmp_roots[2]; ctx->status=TCL_OK; f=TO_PTR(ctx,ctx->curr_f); f->state=ST_TOKENIZE; 
                ctx->tmp_roots[0]=ctx->tmp_roots[1]=ctx->tmp_roots[2]=TCL_NULL; break;
            }
            case ST_RESUME: {
                f = TO_PTR(ctx, ctx->curr_f);
                if (ctx->status != TCL_OK && ctx->status != TCL_RETURN) {
                    tcl_i32 s = ctx->status; tcl_u32 r = ctx->result;
                    while(f){
                        tcl_u8 fl=f->flags; tcl_u32 p=f->parent; ctx->t_bot+=((sizeof(TclFrame)+7)&~7); ctx->curr_f=p;
                        if(p==TCL_NULL){ f=0; break; } f=TO_PTR(ctx,p);
                        if(f->state==ST_CATCH_END || ((s==TCL_BREAK||s==TCL_CONTINUE) && (f->state==ST_LOOP || f->state==ST_COND))) break;
                        if(s==TCL_RETURN && (fl&FRAME_IS_PROC)) break;
                    }
                    ctx->status=s; ctx->result=r; if(!f) ctx->curr_f=TCL_NULL; break;
                }
                if (ctx->status == TCL_RETURN) ctx->status = TCL_OK;
                if(f->exp_idx < f->argc){ f->argv[f->exp_idx]=ctx->result; f->exp_idx++; f->state=ST_EXPAND; }
                else f->state=ST_TOKENIZE; break;
            }
            default: ctx->curr_f=TCL_NULL; break;
        }
        next_loop:;
    }
    tcl_i32 final_status = ctx->status; ctx->status = TCL_OK; return final_status;
}

const tcl_u8 *tcl_get_result(TclCtx *ctx) { return ctx->result==TCL_NULL?(tcl_u8*)"" : TO_PTR(ctx,ctx->result); }

#include "tcllib.c"

tcl_i32 tcl_load_bootstrap(TclCtx *ctx) {
    return tcl_eval(ctx, (const tcl_u8 *)tcl_bootstrap);
}

void tcl_init(void *arena, tcl_i32 size) {
    TclCtx *ctx=(TclCtx*)arena; for(tcl_u32 i=0;i<sizeof(TclCtx);i++)((tcl_u8*)arena)[i]=0;
    ctx->arena=(tcl_u8*)arena; ctx->size=(tcl_u32)size; ctx->p_top=HS; ctx->t_bot=ctx->size; ctx->g_vars=TCL_NULL; ctx->result=TCL_NULL; ctx->status=TCL_OK; ctx->curr_f=TCL_NULL;
    cmd_count=0; 
    tcl_register_c_cmd((tcl_u8*)"set",tcl_cmd_set); tcl_register_c_cmd((tcl_u8*)"proc",tcl_cmd_proc); tcl_register_c_cmd((tcl_u8*)"if",tcl_cmd_if); 
    tcl_register_c_cmd((tcl_u8*)"expr",tcl_cmd_expr); tcl_register_c_cmd((tcl_u8*)"while",tcl_cmd_while); tcl_register_c_cmd((tcl_u8*)"return",tcl_cmd_return); 
    tcl_register_c_cmd((tcl_u8*)"break",tcl_cmd_break); tcl_register_c_cmd((tcl_u8*)"continue",tcl_cmd_continue); tcl_register_c_cmd((tcl_u8*)"error",tcl_cmd_error); 
    tcl_register_c_cmd((tcl_u8*)"eval",tcl_cmd_eval); tcl_register_c_cmd((tcl_u8*)"catch",tcl_cmd_catch); tcl_register_c_cmd((tcl_u8*)"uplevel",tcl_cmd_uplevel); 
    tcl_register_c_cmd((tcl_u8*)"upvar",tcl_cmd_upvar); tcl_register_c_cmd((tcl_u8*)"list",tcl_cmd_list); tcl_register_c_cmd((tcl_u8*)"llength",tcl_cmd_llength); 
    tcl_register_c_cmd((tcl_u8*)"lindex",tcl_cmd_lindex); tcl_register_c_cmd((tcl_u8*)"lrange",tcl_cmd_lrange); tcl_register_c_cmd((tcl_u8*)"unset",tcl_cmd_unset);
}
