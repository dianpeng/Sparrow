#include "parser.h"
#include "lexer.h"
#include "object.h"
#include "error.h"
#include "bc.h"
#include "../util.h"

#include <stdarg.h>
#include <assert.h>
#include <stddef.h>
#include <math.h>

/* Maximum local variable size */
#define MAX_LOCVAR 256

/* Maximum unary operation on a single operand */
#define UNARYOP_MAX 128
#define LOGICOP_MAX 128

/* Loop control related */
#define BRK_STMT_MAX 32
#define CONT_STMT_MAX 32
#define BRANCH_STMT_MAX 32

/* Function/Closure argument list */
#define CLOSURE_ARG_MAX 64

#define EXPRTYPE(__) \
  __(EUNDEFINED,"undefined") \
  __(ENULL,"null") \
  __(ETRUE,"true") \
  __(EFALSE,"false") \
  __(ENUMBER,"number") \
  __(ESTRING,"string") \
  __(ELIST,"list") \
  __(EMAP,"map") \
  __(ECLOSURE,"closure") \
  __(ELOCAL,"local variable") \
  __(EUPVALUE,"upvalue") \
  __(EGLOBAL,"global variable") \
  __(EFUNCCALL,"function call") \
  __(EINTRINSIC,"intrinsic function call") \
  __(EEXPR,"expression")

enum ExprType {
#define __(A,B) A,
  EXPRTYPE(__)
  SIZE_OF_EXPRS
#undef __
};

static const char* exprtype_str( enum ExprType et ) {
  switch(et) {
#define __(A,B) case A: return B;
    EXPRTYPE(__)
    default: return NULL;
#undef __
  }
}

#define is_noneconst(EXPR) \
  ((EXPR)->tag == ELIST || (EXPR)->tag == EMAP || \
   (EXPR)->tag == ECLOSURE || (EXPR)->tag == ELOCAL || \
   (EXPR)->tag == EUPVALUE || (EXPR)->tag == EGLOBAL || \
   (EXPR)->tag == EFUNCCALL|| (EXPR)->tag == EEXPR)

#define is_const(EXPR) \
  ((EXPR)->tag == ENULL || (EXPR)->tag == ETRUE || \
   (EXPR)->tag == EFALSE|| (EXPR)->tag == ENUMBER || \
   (EXPR)->tag == ESTRING)

struct Expr {
  enum ExprType tag; /* What kind of expression tag */
  struct ObjStr* str; /* Using managed string */
  union {
    double num;
    enum ExprType lcomp; /* Used when we are parsing index component */
  } u;
  int info;
  struct Label cpos; /* code position , used on demand */
};

static const char* expr_typestr( struct Expr* expr ) {
  return exprtype_str(expr->tag);
}

struct LocalVar {
  int idx; /* Where the hack this variable is placed */
  struct CStr name; /* Name of the variable */
};

struct PClosure;
struct LexScope {
  struct LexScope* prev; /* Parent scope */
  struct ObjProto* closure; /* Which closure it is compiled against */
  struct PClosure* pclosure;  /* Beloned PClosure */
  int start_idx; /* *Start* index of this lexical scope */
  int cur_idx; /* *Next* local variable stack index */
  int var_size;/* *Size* of local variable in this lexical scope */
  /* Loop related statment : continue/break */
  struct Label bjmp[BRK_STMT_MAX]; /* Break jump */
  size_t bjmp_sz;
  struct Label cjmp[CONT_STMT_MAX]; /* Continue jump */
  size_t cjmp_sz;
  /* Flags --------------------------- */
  int is_loop: 1 ; /* Whether we the *first* level of loop */
  int is_in_loop : 1; /* Whether we are in a loop */
  int is_closure : 1; /* Whether this is the *first* level of closure */
};

/* Closure related parsing status */
struct PClosure {
  struct LexScope* cur_scp; /* If error happend, this field is undefined */
  struct PClosure* prev; /* Parent closure if have */
  struct PClosure* child;/* Enclosed closure , used when patch UPVALUE */
  struct ObjProto* closure; /* Closure objects */
  struct LocalVar var_tab[MAX_LOCVAR];
  size_t vt_size;
  /* UpValue symbol name table */
  struct CStr* upvar_arr;
  size_t upvar_size;
  size_t upvar_cap;
};

struct Parser {
  struct Lexer lex; /* Lexer */
  struct PClosure* closure; /* Current PClosure */
  struct StrBuf err; /* Error buffer */
  struct Sparrow* sparrow; /* Sparrow thread */
  struct ObjModule* module;
  int rnd_idx;
};

#define cclosure(P) ((P)->closure)
#define objclosure(P) (cclosure(P)->closure)

#define is_postfix(T) ((T) == ELOCAL || \
    (T) == EUPVALUE || (T) == EGLOBAL || \
    (T) == ELIST || (T) == EMAP || \
    (T) == ECLOSURE || (T) == EEXPR)

#define perr(EC,...) \
  do { \
    ReportError(&(p->err),p->module->source_path.str,p->lex.line, \
        p->lex.ccnt,EC,##__VA_ARGS__); \
  } while(0)

/* Misc helpers */
#define codebuf(P) &((P)->closure->closure->code_buf)
#define cbA(OP,A) CodeBufferEmitA(codebuf(p),OP,A,p->lex.line,p->lex.ccnt)
#define cbOP(OP) CodeBufferEmitOP(codebuf(p),OP,p->lex.line,p->lex.ccnt)
#define cbputA() CodeBufferPutA(codebuf(p))
#define cbputOP() CodeBufferPutOP(codebuf(p))
#define cbpatchA(POS,OP,A) CodeBufferPatchA(codebuf(p),POS,OP, \
    A,p->lex.line,p->lex.ccnt)
#define cbpatchOP(POS,OP) CodeBufferPatchOP(codebuf(p),POS,OP, \
    p->lex.line,p->lex.ccnt)

#define is_unaryop(P) TokenIsUnaryOP(LexerToken(&((P)->lex)))
#define is_factorop(P) TokenIsFactorOP(LexerToken(&((P)->lex)))
#define is_termop(P) TokenIsTermOP(LexerToken(&((P)->lex)))
#define is_arithop(P) TokenIsArithOP(LexerToken(&((P)->lex)))
#define is_compop(P) TokenIsCompOP(LexerToken(&((P)->lex)))
#define is_logicop(P) TokenIsLogicOP(LexerToken(&((P)->lex)))

#define LOCVAR_FAILED -2
#define LOCVAR_NEW -1

static int def_locvar( struct Parser* p , struct CStr* var , int own ) {
  struct PClosure* pc = cclosure(p);
  struct LexScope* scp = cclosure(p)->cur_scp;
  int i = (size_t)(pc->vt_size - 1); /* reversely search the table */
  for( ; i >= 0 ; --i ) {
    if(pc->var_tab[i].idx < scp->start_idx) {
      break; /* Done */
    }
    if( CStrEq(&(pc->var_tab[i].name),var) ) {
      perr(PERR_DECL_DUP);
      if(own) CStrDestroy(var);
      return LOCVAR_FAILED;
    }
  }
  if(pc->vt_size == MAX_LOCVAR-1) {
    perr(PERR_TOO_MANY_LOCAL_VARIABLES);
    if(own) CStrDestroy(var);
    return LOCVAR_FAILED;
  }
  if(own)
    pc->var_tab[pc->vt_size].name = *var;
  else
    pc->var_tab[pc->vt_size].name = CStrDupCStr(var);
  pc->var_tab[pc->vt_size].idx = pc->cur_scp->cur_idx;
  ++pc->cur_scp->var_size;
  ++pc->cur_scp->cur_idx;
  ++pc->vt_size;
  return LOCVAR_NEW;
}

/* define random var */
static int def_rndvar( struct Parser* p , const char* postfix ) {
  struct CStr nname = CStrPrintF("@%d_%s",p->rnd_idx,postfix);
  ++p->rnd_idx;
  return def_locvar(p,&nname,1);
}

static int
_get_locvar( struct PClosure* pc , const struct ObjStr* var ) {
  int sz = (int)(pc->vt_size);
  int i;
  /* reverse searching */
  for( i = sz - 1; i>= 0 ; --i ) {
    if(ObjStrCmpCStr(var,&(pc->var_tab[i].name))==0) {
      return pc->var_tab[i].idx;
    }
  }
  return LOCVAR_FAILED;
}

static int
get_locvar( struct Parser* p , const struct ObjStr* var ) {
  return _get_locvar(cclosure(p),var);
}

static void initialize_pclosure( struct PClosure* pc ,
    struct LexScope* cscp ,
    struct PClosure* prev ,
    struct ObjProto* cls ) {
  pc->cur_scp = cscp;
  pc->prev = prev;
  pc->child = NULL;
  pc->closure = cls;
  pc->vt_size = 0;
  pc->upvar_arr = NULL;
  pc->upvar_size = 0;
  pc->upvar_cap = 0;
}

static void destroy_pclosure( struct PClosure* pc ) {
  size_t i;
  for( i = 0 ; i < pc->vt_size ; ++i ) {
    CStrDestroy(&(pc->var_tab[i].name));
  }
  for( i = 0 ; i < pc->upvar_size; ++i ) {
    CStrDestroy(pc->upvar_arr+i);
  }
  free(pc->upvar_arr);
}

static void enter_lexscope( struct Parser* p ,
    struct LexScope* scp , int isloop ) {
  struct LexScope* cscp = cclosure(p)->cur_scp;
  scp->prev = cscp;
  scp->pclosure = cclosure(p);
  scp->closure = objclosure(p);
  scp->cur_idx = (cscp == NULL) ? 0 : cscp->cur_idx;
  scp->start_idx = (cscp == NULL) ? 0 : cscp->cur_idx;
  scp->var_size = 0;
  scp->is_loop = isloop;
  if(isloop) scp->is_in_loop = 1;
  else {
    if(cscp && (cscp->is_loop || cscp->is_in_loop))
      scp->is_in_loop = 1;
    else
      scp->is_in_loop = 0;
  }
  scp->is_closure = 0;
  scp->bjmp_sz = scp->cjmp_sz = 0;
  cclosure(p)->cur_scp = scp;
}

static void leave_lexscope( struct Parser* p ) {
  struct LexScope* scp = cclosure(p)->cur_scp;
  /* pop all local variable defined in this freaking scope */
  if(scp->var_size) cbA(BC_POP,(int)scp->var_size);
  cclosure(p)->cur_scp = scp->prev;
}

/* used when a closure leave its scope */
static void exit_lexscope( struct Parser* p ) {
  struct LexScope* scp = cclosure(p)->cur_scp;
  cclosure(p)->cur_scp = scp->prev;
}

static struct LexScope* find_loopscp( struct Parser* p ) {
  struct LexScope* scp = cclosure(p)->cur_scp;
  assert(cclosure(p)->cur_scp->is_loop ||
         cclosure(p)->cur_scp->is_in_loop);
  while(!(scp->is_loop)) {
    scp = scp->prev;
    assert(scp);
  }
  return scp;
}

#define is_inloop(P) (cclosure(p)->cur_scp->is_loop || \
    cclosure(p)->cur_scp->is_in_loop)

/* Upvalue helpers */
static int
upvar_add(struct PClosure* pc , const struct ObjStr* var ,
    int idx , int state ) {
  struct ObjProto* oc = pc->closure;
  struct UpValueIndex uv;
  struct CStr dupvar = CStrDupLen(var->str,var->len);
  uv.state = state;
  uv.idx = idx;
  DynArrPush(oc,uv,uv);
  DynArrPush(pc,upvar,dupvar);
  assert( oc->uv_size == pc->upvar_size );
  return (int)(oc->uv_size-1);
}

static int
resolve_upvar_as_local( struct PClosure* pc , const struct ObjStr* var ) {
  size_t i;
  for( i = 0 ; i< pc->vt_size ; ++i ) {
    if(ObjStrCmpCStr(var,&(pc->var_tab[i].name))==0) {
      return pc->var_tab[i].idx;
    }
  }
  return -1;
}

static int
resolve_upvar( struct Parser* p , const struct ObjStr* str ) {
  struct PClosure* start = cclosure(p);
  start->child = NULL;
  start = start->prev;
  if(start) start->child = cclosure(p);
  while(start) {
    int idx = resolve_upvar_as_local(start,str);
    if(idx >=0) {
      /* Collapsing upvalue table */
      struct PClosure* c = start->child;
      assert(c);
      idx = upvar_add(c,str,idx,UPVALUE_INDEX_EMBED);
      c = c->child;
      while(c) {
        /* Since the upvar is found in upvalue table, we just
         * need to get it from upvar and it is *closed* */
        idx = upvar_add(c,str,idx,UPVALUE_INDEX_DETACH);
        c = c->child;
      }
      return idx;
    }
    if(start->prev) start->prev->child = start;
    start = start->prev;
  }
  return -1;
}

// Try to find the *upvar* in upvalue table
static int
find_upvar( struct Parser* p , const struct ObjStr* str ) {
  struct PClosure* start = cclosure(p);
  if(start) start->child = NULL;
  while(start) {
    size_t i;
    int idx;
    for( i = 0 ; i < start->upvar_size ; ++i ) {
      if(ObjStrCmpCStr(str,&(start->upvar_arr[i]))==0) {
        struct PClosure* c = start->child;
        idx = (int)(i);
        while(c) {
          idx = upvar_add(c,str,idx,UPVALUE_INDEX_DETACH);
          c = c->child;
        }
        return idx;
      }
    }
    if(start->prev) start->prev->child = start;
    start = start->prev;
  }
  return -1;
}

static int
handle_upvar( struct Parser* p , const struct ObjStr* str ) {
  int idx = find_upvar(p,str);
  if(idx <0) {
    return resolve_upvar(p,str);
  }
  return idx;
}

/* Expr helpers */
#define is_convnum(EXPR) ((EXPR)->tag == ENUMBER || \
   (EXPR)->tag == ETRUE || \
   (EXPR)->tag == EFALSE)

static void expr2num( struct Expr* expr ) {
  assert(is_convnum(expr));
  switch(expr->tag) {
    case ENUMBER:
      break;
    case ETRUE:
      expr->u.num = 1;
      expr->tag = ENUMBER;
      break;
    case EFALSE:
      expr->u.num = 0;
      expr->tag = ENUMBER;
      break;
    default:
      assert(!"unreachable!");
      break;
  }
}

static int expr_index( struct Parser* p , struct Expr* expr ) {
  switch(expr->tag) {
    case ENUMBER:
      return ConstAddNumber(objclosure(p),expr->u.num);
    case ESTRING:
      return ConstAddString(objclosure(p),expr->str);
    default:
      assert(!"unreachable!"); return -1;
  }
}

/* Emitter helpers */
static int emit_loadnum( struct Parser* p , double num ) {
  int ipart;
  int ret = ConvNum(num,&ipart);
  if(ret || ipart > 5 || ipart < -5 ) {
    int idx = ConstAddNumber(objclosure(p),num);
    if(idx <0) {
      perr(PERR_TOO_MANY_NUMBER_LITERALS);
      return -1;
    }
    cbA(BC_LOADN,idx);
  } else {
    switch(ipart) {
      case -5:cbOP(BC_LOADNN5); break;
      case -4:cbOP(BC_LOADNN4); break;
      case -3:cbOP(BC_LOADNN3); break;
      case -2:cbOP(BC_LOADNN2); break;
      case -1:cbOP(BC_LOADNN1); break;
      case 0: cbOP(BC_LOADN0); break;
      case 1: cbOP(BC_LOADN1); break;
      case 2: cbOP(BC_LOADN2); break;
      case 3: cbOP(BC_LOADN3); break;
      case 4: cbOP(BC_LOADN4); break;
      case 5: cbOP(BC_LOADN5); break;
      default: assert(!"unreachable!"); break;
    }
  }
  return 0;
}

static int emit_loadstr( struct Parser* p , struct ObjStr* str ) {
  int idx = ConstAddString(objclosure(p),str);
  if(idx <0) {
    perr(PERR_TOO_MANY_STRING_LITERALS);
    return -1;
  }
  cbA(BC_LOADS,idx);
  return 0;
}

#define emit_none(P) cbOP(BC_LOADNULL)
#define emit_true(P) cbOP(BC_LOADTRUE)
#define emit_false(P) cbOP(BC_LOADFALSE)

static int emit_readvar( struct Parser* p , struct Expr* expr ) {
  int idx = get_locvar(p,expr->str);
  if(idx == LOCVAR_FAILED) {
    idx = handle_upvar(p,expr->str); /* Resolve it as upvalue */
    if(idx<0) {
      int sidx;
      expr->tag = EGLOBAL;
      /* Add string into closure const table */
      sidx = ConstAddString( p->closure->closure , expr->str );
      if(sidx <0) {
        perr(PERR_TOO_MANY_STRING_LITERALS);
        return -1;
      }
      cbA(BC_GGET,sidx);
    } else {
      expr->tag = EUPVALUE;
      cbA(BC_UGET,idx);
    }
  } else {
    expr->tag = ELOCAL;
    cbA(BC_LOADV,idx);
  }
  return 0;
}

/* Helper function to try emit an expression if we need to */
static int tryemit_expr( struct Parser* p , struct Expr* expr ) {
  int ret;
  switch(expr->tag) {
    case ETRUE: return emit_true(p);
    case EFALSE:return emit_false(p);
    case ENULL: return emit_none(p);
    case ESTRING:
      ret = emit_loadstr(p,expr->str);
      return ret;
    case ENUMBER:
      return emit_loadnum(p,expr->u.num);
    default: /* do nothing */
      return 0;
  }
}

/* emit arithmatic operation */
#define _emit(L,R) \
  do { \
    if(lexpr == const_expr) { \
      cbA(L,idx); \
    } else { \
      cbA(R,idx); \
    } \
  } while(0)

/* TODO:: Add strength reduction for trivial cases */
static int emit_arithcomp( struct Parser* p ,
    struct Expr* lexpr,
    struct Expr* rexpr,
    enum Token tk ) {
  assert(is_noneconst(lexpr) || is_noneconst(rexpr));
  if(is_const(lexpr) || is_const(rexpr)) {
    struct Expr* const_expr;
    const_expr = is_const(lexpr) ? lexpr : rexpr;
    if(lexpr->tag != ENULL && rexpr->tag != ENULL) {
      int idx;
      if(const_expr->tag == ESTRING) {
        idx = expr_index(p,const_expr);
        if(idx<0) {
          perr(PERR_TOO_MANY_STRING_LITERALS);
          return -1;
        }
        if(tk!=TK_ADD && !TokenIsCompOP(tk)) {
          perr(PERR_STRING_ARITHMATIC);
          return -1;
        } else {
          switch(tk) {
            case TK_ADD: _emit(BC_ADDSV,BC_ADDVS); break;
            case TK_LT : _emit(BC_LTSV,BC_LTVS); break;
            case TK_LE : _emit(BC_LESV,BC_LEVS); break;
            case TK_GT : _emit(BC_GTSV,BC_GTVS); break;
            case TK_GE : _emit(BC_GESV,BC_GEVS); break;
            case TK_EQ : _emit(BC_EQSV,BC_EQVS); break;
            case TK_NE : _emit(BC_NESV,BC_NEVS); break;
            default: assert(!"unreachable!"); break;
          }
        }
      } else {
        assert(is_convnum(const_expr));
        expr2num(const_expr);
        idx = expr_index(p,const_expr);
        if(idx<0) {
          perr(PERR_TOO_MANY_NUMBER_LITERALS);
          return -1;
        }
        switch(tk) {
          case TK_ADD: _emit(BC_ADDNV,BC_ADDVN); break;
          case TK_SUB: _emit(BC_SUBNV,BC_SUBVN); break;
          case TK_MUL: _emit(BC_MULNV,BC_MULVN); break;
          case TK_DIV: _emit(BC_DIVNV,BC_DIVVN); break;
          case TK_POW: _emit(BC_POWNV,BC_POWVN); break;
          case TK_MOD: _emit(BC_MODNV,BC_MODVN); break;
          case TK_LE : _emit(BC_LENV,BC_LEVN); break;
          case TK_LT : _emit(BC_LTNV,BC_LTVN); break;
          case TK_GE : _emit(BC_GENV,BC_GEVN); break;
          case TK_GT : _emit(BC_GTNV,BC_GTVN); break;
          case TK_EQ : _emit(BC_EQNV,BC_EQVN); break;
          case TK_NE : _emit(BC_NENV,BC_NEVN); break;
          default: assert(!"unreachable!"); break;
        }
      }
    } else {
      /* generate code for NULL comparison */
      switch(tk) {
        case TK_EQ:
          if(const_expr == lexpr) {
            cbOP(BC_EQNULLV);
          } else {
            cbOP(BC_EQVNULL);
          }
          break;
        case TK_NE:
          if(const_expr == lexpr) {
            cbOP(BC_NENULLV);
          } else {
            cbOP(BC_NEVNULL);
          }
          break;
        default:
          perr(PERR_NULL_ARITH);
          return -1;
      }
    }
  } else {
    /* both sides are noneconst expression */
    switch(tk) {
      case TK_ADD: cbOP(BC_ADDVV); break;
      case TK_SUB: cbOP(BC_SUBVV); break;
      case TK_MUL: cbOP(BC_MULVV); break;
      case TK_DIV: cbOP(BC_DIVVV); break;
      case TK_POW: cbOP(BC_POWVV); break;
      case TK_MOD: cbOP(BC_MODVV); break;
      case TK_LE : cbOP(BC_LEVV) ; break;
      case TK_LT : cbOP(BC_LTVV) ; break;
      case TK_GE : cbOP(BC_GEVV) ; break;
      case TK_GT : cbOP(BC_GTVV) ; break;
      case TK_EQ : cbOP(BC_EQVV) ; break;
      case TK_NE : cbOP(BC_NEVV) ; break;
      default: assert(!"unreachable!"); break;
    }
  }
  lexpr->tag = EEXPR;
  return 0;
}
#undef _emit

/* Constant folding routine , move it to VM module ??? */
enum {
  FOLD,
  UNFOLD,
  PERROR
};

static int
_tryfold_unary( struct Parser* p , enum Token tk , struct Expr* expr ) {
  switch(expr->tag) {
    case ETRUE:
      if(tk == TK_SUB) {
        expr->tag = ENUMBER;
        expr->u.num = -1;
      } else if(tk == TK_NOT) {
        expr->tag = EFALSE;
      } else assert(!"unreachable!");
      return 0;
    case EFALSE:
      if(tk == TK_SUB) {
        expr->tag = ENUMBER;
        expr->u.num = 0;
      } else if(tk == TK_NOT) {
        expr->tag = ETRUE;
      } else assert (0);
      return 0;
    case ENUMBER:
      if(tk == TK_SUB) {
        expr->u.num = -(expr->u.num);
      } else if(tk == TK_NOT) {
        expr->tag = (expr->u.num ? EFALSE : ETRUE);
      } else assert(!"unreachable!");
      return 0;
    case ENULL:
      if(tk == TK_SUB) {
        perr(PERR_NULL_UNARY);
        return -1;
      } else if(tk == TK_NOT) {
        expr->tag = ETRUE;
      } else assert(!"unreachable!");
      return 0;
    case ESTRING:
      if(tk == TK_SUB) {
        perr(PERR_STRING_UNARY);
        return -1;
      } else if(tk == TK_NOT) {
        expr->tag = EFALSE;
      } else assert(!"unreachable!");
      return 0;
    default:
      assert(!"unreachable!"); return -1;
  }
}

static int
tryfold_unary( struct Parser* p , enum Token* tkarr ,
    size_t sz , struct Expr* expr ) {
  int i;
  int isz = (int)sz;

  switch(expr->tag) {
    case ETRUE:
    case EFALSE:
    case ENULL:
    case ENUMBER:
    case ESTRING:
      break;
    default:
      return UNFOLD;
  }
  /* reverse order , inner operator takes effect first */
  for( i = isz - 1 ; i >= 0 ; --i ) {
    if(_tryfold_unary(p,tkarr[i],expr)) return PERROR;
  }
  return FOLD;
}

static int
tryfold_arithcomp( struct Parser* p ,
    struct Expr* l , struct Expr* r , enum Token op ) {
  if(is_convnum(l) && is_convnum(r)) {
    double lnum , rnum;
    expr2num(l); expr2num(r);
    lnum = l->u.num; rnum = r->u.num;
    l->tag = ENUMBER;
    switch(op) {
      case TK_ADD : l->u.num = lnum+rnum; break;
      case TK_SUB : l->u.num = lnum-rnum; break;
      case TK_MUL : l->u.num = lnum*rnum; break;
      case TK_DIV :
        if(!rnum) {
          perr(PERR_DIVIDE_ZERO);
          return PERROR;
        }
        l->u.num = lnum/rnum;
        break;
      case TK_POW:
        l->u.num = pow(lnum,rnum);
        break;
      case TK_MOD:
        if(!rnum) {
          perr(PERR_DIVIDE_ZERO);
          return PERROR;
        }
        l->u.num = NumMod(lnum,rnum);
        break;
      case TK_LT:
        l->tag = (lnum < rnum ? ETRUE : EFALSE);
        break;
      case TK_LE:
        l->tag = (lnum <=rnum ? ETRUE : EFALSE);
        break;
      case TK_GT:
        l->tag = (lnum > rnum ? ETRUE : EFALSE);
        break;
      case TK_GE:
        l->tag = (lnum >=rnum ? ETRUE : EFALSE);
        break;
      case TK_EQ:
        l->tag = (lnum ==rnum ? ETRUE : EFALSE);
        break;
      case TK_NE:
        l->tag = (lnum !=rnum ? ETRUE : EFALSE);
        break;
      default:
        assert(!"unreachable!"); break;
    }
    return FOLD;
  } else {
    /* Check if it is string */
    if(l->tag == ESTRING && r->tag == ESTRING) {
      if(op == TK_ADD || TokenIsCompOP(op)) {
        switch(op) {
          case TK_ADD:
            l->tag = ESTRING;
            l->str = ObjStrCatNoGC(p->sparrow,l->str,r->str);
            break;
          case TK_LT:
            l->tag = ((ObjStrCmp(l->str,r->str)<0) ? ETRUE:EFALSE);
            break;
          case TK_LE:
            l->tag = ((ObjStrCmp(l->str,r->str)<=0)? ETRUE:EFALSE);
            break;
          case TK_GT:
            l->tag = ((ObjStrCmp(l->str,r->str)>0) ? ETRUE:EFALSE);
            break;
          case TK_GE:
            l->tag = ((ObjStrCmp(l->str,r->str)>=0)? ETRUE:EFALSE);
            break;
          case TK_EQ:
            l->tag = ((ObjStrEqual(l->str,r->str))? ETRUE:EFALSE);
            break;
          case TK_NE:
            l->tag = ((ObjStrEqual(l->str,r->str))? ETRUE:EFALSE);
            break;
          default:
            assert(!"unreachable!"); break;
        }
        return FOLD;
      } else {
        perr(PERR_STRING_ARITHMATIC);
        return PERROR;
      }
    } else if(l->tag == ENULL && r->tag == ENULL) {
      switch(op) {
        case TK_EQ: l->tag = ETRUE; break;
        case TK_NE: l->tag = EFALSE;break;
        default:
          perr(PERR_NULL_ARITH,LexerGetTokenName(&(p->lex)));
          return PERROR;
      }
      return FOLD;
    } else if(!is_noneconst(l) && !is_noneconst(r)) {
      perr(PERR_CONST_ARITHMATIC,expr_typestr(l),expr_typestr(r));
      return PERROR;
    }
  }
  assert( is_noneconst(l) || is_noneconst(r) );
  return UNFOLD;
}

/* Helper macros */
#define CONSUME(TK) \
  do { \
    if(LexerToken(&(p->lex))!= (TK)) { \
      perr(PERR_UNEXPECTED_TOKEN,TokenGetName(TK)); \
      return -1; \
    } \
    LexerNext(&(p->lex)); \
  } while(0)

#define TRY(TK) \
  do { \
    const struct Lexeme* __lexeme = LexerNext(&(p->lex)); \
    if(__lexeme->tk != (TK)) { \
      perr(PERR_UNEXPECTED_TOKEN,TokenGetName(TK)); \
      return -1; \
    } \
  } while(0)

#define NEXT() \
  do { \
    LexerNext(&(p->lex)); \
  } while(0)

static int pexpr_atom( struct Parser* p , struct Expr* );
static int pexpr_list( struct Parser* p , struct Expr* );
static int pexpr_map ( struct Parser* p , struct Expr* );
/* Helper for generating code for prefix expression, leave last
 * instruction un-emitted for the caller, since this may be a store
 * or a read */
static int _pexpr_pfixcomp( struct Parser* p  , struct Expr* );
static int _pexpr_pfix( struct Parser* p ,
    struct Expr* lexpr , struct Expr* rexpr );
static int pexpr_funccall( struct Parser* p , struct Expr* );
static int pexpr_rpfix( struct Parser* p , struct Expr* expr );
static int pexpr( struct Parser* , struct Expr* );
static int parse_closure( struct Parser* p );

/* Arithmatic operations */
static int pexpr_unary( struct Parser* p ,struct Expr* expr) {
  enum Token oparr[UNARYOP_MAX];
  size_t sz =0;
  size_t i;
  /* check unary operators */
  int ret;
  while(is_unaryop(p)) {
    if( sz == UNARYOP_MAX ) {
      perr(PERR_TOO_MANY_UNARY_OPERATORS);
      return -1;
    }
    oparr[sz] = LexerToken(&(p->lex));
    ++sz;
    NEXT();
  }
  if(pexpr_rpfix(p,expr)) return -1;
  /* try to do simple unary unfold */
  ret = tryfold_unary(p,oparr,sz,expr);
  switch(ret) {
    case FOLD: return 0;
    case PERROR: return -1;
    default:
      break;
  }
  /* generate this much of unary operators */
  for( i = 0 ; i < sz ; ++i ) {
    switch(oparr[i]) {
      case TK_SUB: cbOP(BC_NEG); break;
      case TK_NOT: cbOP(BC_NOT); break;
      default: assert(!"unreachable!"); break;
    }
  }
  return 0;
}

#define _DEFINE_PARITH(PREV,CHECKER,NAME) \
  static int pexpr_##NAME( struct Parser* p , struct Expr* expr ) { \
    struct Expr rexpr; \
    int ret; \
    enum Token tk; \
    if(PREV(p,expr)) return -1; \
    while(CHECKER(p)) { \
      tk = LexerToken(&(p->lex)); \
      NEXT(); \
      if(PREV(p,&rexpr)) return -1; \
      ret = tryfold_arithcomp(p,expr,&rexpr,tk); \
      switch(ret) { \
        case FOLD: break; \
        case PERROR: return -1; \
        default: \
          if(emit_arithcomp(p,expr,&rexpr,tk)) return -1; \
          break; \
      } \
    } \
    return 0; \
  }

/* pexpr_factor */
_DEFINE_PARITH(pexpr_unary,is_factorop,factor)

/* pexpr_term */
_DEFINE_PARITH(pexpr_factor,is_termop,term)

/* pexpr_comp */
_DEFINE_PARITH(pexpr_term,is_compop,comp)

#undef _DEFINE_PARITH

/* pexpr_logic */
#define is_booleantype(EXPR) is_const(EXPR)
#define is_nonebooleantype(EXPR) (is_noneconst(EXPR))

static int _check_tf( struct Expr* expr ) {
  if(is_booleantype(expr)) {
    if(is_convnum(expr)) {
      struct Expr temp = *expr;
      expr2num(&temp);
      return temp.u.num ? 1 : 0;
    } else if(expr->tag == ESTRING) {
      return 1;
    } else {
      return 0;
    }
  } else {
    return 2;
  }
}

/* Logical value True/False table */
enum {
  LOGIC_FALSE,
  LOGIC_TRUE,
  LOGIC_NEEDEVAL
};

/* "&&" and "||" true false table */
static const int logic_andtab[3][3] = {
  /* Left : 0 , Right : 0 , 0, 0 */
  { LOGIC_FALSE , LOGIC_FALSE , LOGIC_FALSE },
  /* Left : 1 , Right : 0 , 1 , EVAL */
  { LOGIC_FALSE , LOGIC_TRUE , LOGIC_NEEDEVAL},
  /* Left : 2 , Right : 0 , EVAL , EVAL */
  { LOGIC_FALSE , LOGIC_NEEDEVAL, LOGIC_NEEDEVAL }
};

static const int logic_ortab[3][3] = {
  /* Left : 0 , Right : 0 , 1 , EVAL */
  { LOGIC_FALSE , LOGIC_TRUE , LOGIC_NEEDEVAL },
  /* Left : 1 , Right : 1 , 1 , EVAL */
  { LOGIC_TRUE , LOGIC_TRUE , LOGIC_TRUE },
  /* Left : 2 , Right : EVAL , 1 , EVAL */
  { LOGIC_NEEDEVAL , LOGIC_TRUE , LOGIC_NEEDEVAL }
};

struct LogicJump {
  struct Label pos;
  enum Bytecode instr;
};

#define _push_jmp() \
  do { \
    if(sz == LOGICOP_MAX) { \
      perr(PERR_TOO_MANY_LOGIC_OP); \
      return -1; \
    } \
    jmptb[sz].pos = cbputA(); \
    jmptb[sz].instr= (tk == TK_AND ? BC_BRF : BC_BRT); \
    ++sz; \
  } while(0)

static int pexpr_logic( struct Parser* p , const int (*tftab)[3],
                                           enum Token oper,
                                           int (*prev)( struct Parser* , struct Expr* ),
                                           struct Expr* expr ) {
  struct Expr rexpr;
  struct LogicJump jmptb[LOGICOP_MAX];
  size_t sz = 0;
  enum Token tk;
  expr->cpos = CodeBufferGetLabel(codebuf(p));
  if(prev(p,expr)) return -1;
  tk = LexerToken(&(p->lex));
  if(tk == oper && is_nonebooleantype(expr)) _push_jmp();
  while(tk == oper) {
    int ltf,rtf;
    int result;
    NEXT();
    rexpr.cpos = CodeBufferGetLabel(codebuf(p));
    if(prev(p,&rexpr)) goto fail;
    ltf = _check_tf(expr); rtf = _check_tf(&rexpr);
    result = tftab[ltf][rtf];
    switch(result) {
      case LOGIC_TRUE:
        if(is_nonebooleantype(expr)) {
          assert(is_booleantype(&rexpr));
          CodeBufferSetToLabel(codebuf(p),expr->cpos);
        }
        if(is_nonebooleantype(&rexpr)) {
          assert(is_booleantype(expr));
          CodeBufferSetToLabel(codebuf(p),rexpr.cpos);
        }
        expr->tag = ETRUE;
        expr->cpos = CodeBufferGetLabel(codebuf(p));
        break;
      case LOGIC_FALSE:
        if(is_nonebooleantype(expr)) {
          assert(is_booleantype(&rexpr));
          CodeBufferSetToLabel(codebuf(p),expr->cpos);
        }
        if(is_nonebooleantype(&rexpr)) {
          assert(is_booleantype(expr));
          CodeBufferSetToLabel(codebuf(p),expr->cpos);
        }
        expr->tag = EFALSE;
        expr->cpos = CodeBufferGetLabel(codebuf(p));
        break;
      default:
        if(is_nonebooleantype(&rexpr)) {
          expr->cpos = rexpr.cpos;
          if(LexerToken(&(p->lex)) != oper) {
            /* generate last *test* instruction , this is used
             * to rewrite the *last* component which doesn't have
             * a brf/brt instruction , to true or false value */
            cbOP(BC_TEST);
          } else {
            _push_jmp();
          }
        } else {
          if(tryemit_expr(p,&rexpr)) return -1;
          if((LexerToken(&(p->lex)) != oper) &&
             (rexpr.tag != ETRUE && rexpr.tag != EFALSE)) {
            cbOP(BC_TEST); /* Generate a test to rewrite last value */
          }
        }
        expr->tag = EEXPR;
        break;
    }
    tk = LexerToken(&(p->lex));
  }
  /* Fix jump table , ignore the *last* jump! */
  if(sz && expr->tag == EEXPR) {
    size_t i; size_t target = CodeBufferPos(codebuf(p));
    for(i = 0; i < sz ; ++i) {
      cbpatchA(jmptb[i].pos,jmptb[i].instr,(int32_t)(target));
    }
  }
  return 0;

fail:
  return -1;
}

#undef _push_jmp

static int pexpr_logicand( struct Parser* p , struct Expr* expr ) {
  return pexpr_logic(p,logic_andtab,TK_AND,pexpr_comp,expr);
}

static int pexpr_logicor( struct Parser* p , struct Expr* expr ) {
  return pexpr_logic(p,logic_ortab,TK_OR,pexpr_logicand,expr);
}

/* *MUST* be none constant expression */
#define is_pfixexpr(TAG) \
  ((TAG) == ELIST || (TAG) == EMAP || \
   (TAG) == ECLOSURE || (TAG) == ELOCAL || \
   (TAG) == EGLOBAL || (TAG) == EUPVALUE|| \
   (TAG) == EFUNCCALL)

#define is_pfix_tk(P) \
  (LexerToken(&((P)->lex)) == TK_DOT  || \
   LexerToken(&((P)->lex)) == TK_LSQR  || \
   LexerToken(&((P)->lex)) == TK_LPAR)

static int pexpr_funccall( struct Parser* p , struct Expr* expr ) {
  assert( LexerToken(&(p->lex)) == TK_LPAR );
  expr->tag = EFUNCCALL;
  NEXT();
  if(LexerToken(&(p->lex)) == TK_RPAR) { /* empty funccall */
    NEXT(); cbOP(BC_CALL0);
  } else {
    struct Expr aexpr;
    int acnt = 1; /* arg count */
    do {
      if(pexpr(p,&aexpr)) return -1;
      if(tryemit_expr(p,&aexpr)) return -1;
      if(LexerToken(&(p->lex)) == TK_COMMA) {
        NEXT();
      } else if(LexerToken(&(p->lex)) == TK_RPAR) {
        NEXT(); break;
      } else {
        perr(PERR_FUNCCALL_BRACKET);
        return -1;
      }
      ++acnt;
    } while(1);

    switch(acnt) {
      case 1: cbOP(BC_CALL1); break;
      case 2: cbOP(BC_CALL2); break;
      case 3: cbOP(BC_CALL3); break;
      case 4: cbOP(BC_CALL4); break;
      default: cbA(BC_CALL,acnt); break;
    }
  }
  return 0;
}

static int _pexpr_pfixcomp( struct Parser* p , struct Expr* expr ) {
  switch(LexerToken(&(p->lex))) {
    case TK_DOT:
      TRY(TK_VARIABLE);
      expr->str = StrBufToObjStrNoGC(p->sparrow,LexerLexemeStr(&(p->lex)));
      expr->tag = ESTRING;
      if((expr->info = ConstAddString(objclosure(p),expr->str))<0)
        return -1;
      NEXT();
      return 0;
    case TK_LSQR:
      NEXT(); /* Skip [ */
      if(pexpr(p,expr)) return -1;
      /* handle other index type */
      switch(expr->tag) {
        case ETRUE:
          expr->tag = ENUMBER; expr->u.num = 1; break;
        case EFALSE:
          expr->tag = ENUMBER; expr->u.num = 0; break;
        case ENULL: perr(PERR_NULL_INDEX); return -1;
        default:
          break;
      }
      if(expr->tag == ENUMBER) {
        if((expr->info =
              ConstAddNumber(objclosure(p),expr->u.num))<0)
          return -1;
      } else if(expr->tag == ESTRING) {
        if((expr->info = ConstAddString(objclosure(p),expr->str))<0)
          return -1;
      }
      CONSUME(TK_RSQR);
      return 0;
    case TK_LPAR:
      return pexpr_funccall(p,expr);
    default:
      assert(!"unreachable!"); return -1;
  }
}

static int _pexpr_pfix( struct Parser* p , struct Expr* lexpr ,
    struct Expr* rexpr ) {
  rexpr->tag = EUNDEFINED;
  if(is_pfixexpr(lexpr->tag)) {
    if(is_pfix_tk(p)) {
      if(_pexpr_pfixcomp(p,rexpr)) return -1;
      while(is_pfix_tk(p)) {
        switch(rexpr->tag) {
          case ENUMBER: cbA(BC_AGETN,rexpr->info); break;
          case ESTRING:
          {
            enum IntrinsicAttribute iattr = IAttrGetIndex(rexpr->str->str);
            if(iattr == SIZE_OF_IATTR) {
              cbA(BC_AGETS,rexpr->info);
            } else {
              cbA(BC_AGETI,iattr); /* intrinsic attribute */
            }
          }
          break;
          case EFUNCCALL: break;
          default: cbOP(BC_AGET); break;
        }
        lexpr->tag = EEXPR;
        if(_pexpr_pfixcomp(p,rexpr)) return -1;
      }
    }
    return 0;
  } else {
    return 0;
  }
}

static int pexpr_rpfix( struct Parser* p , struct Expr* expr ) {
  struct Expr rexpr;
  int ret;
  if(pexpr_atom(p,expr)) return -1;
  ret = _pexpr_pfix(p,expr,&rexpr);
  if(rexpr.tag != EUNDEFINED) {
    switch(rexpr.tag) {
      case ENUMBER: cbA(BC_AGETN,rexpr.info); break;
      case ESTRING: cbA(BC_AGETS,rexpr.info); break;
      case EFUNCCALL: break;
      default: cbOP(BC_AGET); break;
    }
  }
  return ret;
}

static int
pexpr_list( struct Parser* p , struct Expr* expr ) {
  assert(LexerToken(&(p->lex)) == TK_LSQR);
  NEXT();
  if(LexerToken(&(p->lex)) == TK_RSQR) {
    NEXT();
    cbOP(BC_NEWL0);
  } else {
    struct Expr eexpr;
    size_t ecnt = 1;
    do {
      if(pexpr(p,&eexpr)) return -1;
      if(tryemit_expr(p,&eexpr)) return -1;
      if(LexerToken(&(p->lex)) == TK_COMMA) {
        NEXT();
      } else if(LexerToken(&(p->lex)) == TK_RSQR) {
        NEXT(); break;
      }
      ++ecnt;
    } while(1);
    switch(ecnt) {
      case 0: assert(!"unreachable!"); break;
      case 1: cbOP(BC_NEWL1); break;
      case 2: cbOP(BC_NEWL2); break;
      case 3: cbOP(BC_NEWL3); break;
      case 4: cbOP(BC_NEWL4); break;
      default: cbA(BC_NEWL,ecnt); break;
    }
  }
  expr->tag = ELIST;
  return 0;
}

/* a simple key test routine */
static int
is_map_key( struct Parser* p , struct Expr* expr ) {
  switch(expr->tag) {
    case ENULL:
    case ETRUE:
    case EFALSE:
    case ENUMBER:
    case ELIST:
    case EMAP:
      perr(PERR_INVALID_MAP_KEY,expr_typestr(expr));
      return -1;
    default:
      return 0;
  }
}

static int
pexpr_map( struct Parser* p , struct Expr* expr ) {
  assert(LexerToken(&(p->lex)) == TK_LBRA);
  NEXT();
  if(LexerToken(&(p->lex)) == TK_RBRA) {
    NEXT();
    cbOP(BC_NEWM0);
  } else {
    struct Expr kexpr , vexpr;
    size_t ecnt = 1;
    do {
      if(pexpr(p,&kexpr)) return -1;
      if(is_map_key(p,&kexpr)) return -1;
      if(tryemit_expr(p,&kexpr)) return -1;
      CONSUME(TK_COLON);
      if(pexpr(p,&vexpr)) return -1;
      if(tryemit_expr(p,&vexpr)) return -1;
      if(LexerToken(&(p->lex)) == TK_COMMA) {
        NEXT();
      } else if(LexerToken(&(p->lex)) == TK_RBRA) {
        NEXT(); break;
      }
      ++ecnt;
    } while(1);
    switch(ecnt) {
      case 0: assert(!"unreachable!"); break;
      case 1: cbOP(BC_NEWM1); break;
      case 2: cbOP(BC_NEWM2); break;
      case 3: cbOP(BC_NEWM3); break;
      case 4: cbOP(BC_NEWM4); break;
      default: cbA(BC_NEWM,ecnt); break;
    }
  }
  expr->tag = EMAP;
  return 0;
}

/* This function is used to try to compile shortcuts of
 * global functions. This function picks up function call
 * has simple format , like foo(). Those function's name
 * will be checked internally to see whather they are matched
 * with intrinsic function names. Intrinsic function are
 * executed by virtual machine in a *fast call* manner */
static int
pexpr_intrinsic( struct Parser* p , struct Expr* expr ) {
  enum Bytecode bc = IFuncGetBytecode(expr->str->str);
  assert(LexerToken(&(p->lex)) == TK_LPAR);
  if(bc == BC_NOP) return 1; /* Not a intrinsic function */
  else {
    int narg = 0;
    NEXT();
    if(LexerToken(&(p->lex)) == TK_RPAR) {
      NEXT();
    } else {
      narg = 1;
      do {
        struct Expr arg;
        if(pexpr(p,&arg)) return -1;
        if(tryemit_expr(p,&arg)) return -1;
        if(LexerToken(&(p->lex)) == TK_COMMA) {
          NEXT();
        } else if(LexerToken(&(p->lex)) == TK_RPAR) {
          NEXT();
          break;
        } else {
          perr(PERR_FUNCCALL_BRACKET);
          return -1;
        }
        ++narg;
      } while(1);
    }
    cbA(bc,narg);
    expr->tag = EFUNCCALL;
    return 0;
  }
}

static int
pexpr_atom( struct Parser* p , struct Expr* expr ) {
  struct Lexeme* lexeme = &(p->lex.lexeme);
  int ret;
  switch(lexeme->tk) {
    case TK_NUMBER:
      expr->u.num = lexeme->num;
      expr->tag = ENUMBER;
      NEXT();
      return 0;
    case TK_STRING:
      expr->str = StrBufToObjStrNoGC(p->sparrow,&(lexeme->str));
      expr->tag = ESTRING;
      NEXT();
      return 0;
    case TK_VARIABLE:
      expr->str = StrBufToObjStrNoGC(p->sparrow,&(lexeme->str));
      NEXT();
      if(LexerToken(&(p->lex)) == TK_LPAR) {
        ret = pexpr_intrinsic(p,expr);
        if(ret == 1) {
          if(emit_readvar(p,expr)) return -1;
          ret = 0;
        }
      } else {
        ret = emit_readvar(p,expr);
      }
      return ret;
    case TK_TRUE:
      expr->tag = ETRUE;
      NEXT();
      return 0;
    case TK_FALSE:
      expr->tag = EFALSE;
      NEXT();
      return 0;
    case TK_NULL:
      expr->tag = ENULL;
      NEXT();
      return 0;
    case TK_LSQR:
      return pexpr_list(p,expr);
    case TK_LBRA:
      return pexpr_map(p,expr);
    case TK_LPAR:
      NEXT(); /* skip ( */
      expr->tag = EEXPR;
      if(pexpr(p,expr)) return -1;
      else {
        CONSUME(TK_RPAR);
        return 0;
      }
    case TK_FUNCTION:
      expr->tag = ECLOSURE;
      return parse_closure(p);
    default:
      perr(PERR_UNKNOWN_TOKEN);
      return -1;
  }
}

static int pexpr( struct Parser* p ,struct Expr* expr ) {
  return pexpr_logicor(p,expr);
}

/* Chunk */
static int parse_stmtorchunk( struct Parser* , int newscope );
static int parse_chunk( struct Parser* p , int newscope );

/* Control structure */
struct IfBranch {
  struct Label jmp; /* Where this branch should jump if failed */
  struct Label to;  /* Previous branch's patched jump position */
};

static int _parse_branch( struct Parser* p , enum Token tk ,
    struct IfBranch* br ) {
  struct Expr cond;
  assert(LexerToken(&(p->lex)) == tk);
  TRY(TK_LPAR); NEXT(); /* skip ( */
  /* Patch the previous branch to *this* jump if we have */
  if(tk == TK_ELIF)
    cbpatchA(br->to,BC_IF,CodeBufferPos(codebuf(p)));
  /* Parse condition , here we could do a simple DCE by evaluating
   * the condition. But it doesn't pays off since this DCE only works
   * by checking condition is constant or not. And I don't think there
   * will be many people writing code like if(false) do_something();
   * Therefore, I will just emit this number on top of the stack */
  if(pexpr(p,&cond))
    return -1;
  CONSUME(TK_RPAR); /* skip ) */
  if(tryemit_expr(p,&cond))
    return -1; /* emit the condition on stack */
  br->jmp = cbputA(); /* emit jump code stub */
  if(parse_stmtorchunk(p,1))
    return -1; /* emit branch body */
  return 0;
}

#define _push_jmp() \
  do { \
    if(jo_sz == BRANCH_STMT_MAX) { \
      perr(PERR_TOO_MANY_BRANCH); \
      return -1; \
    } else { \
      jump_out[jo_sz++] = cbputA(); \
    } \
  } while(0)

static int parse_if( struct Parser* p ) {
  struct IfBranch br;
  int has_else = 0;
  struct Label jump_out[BRANCH_STMT_MAX]; /* Jump out label */
  size_t jo_sz = 0;
  size_t i;

  assert( LexerToken(&(p->lex)) == TK_IF );
  if(_parse_branch(p,TK_IF,&br)) return -1;
  if(LexerToken(&(p->lex)) == TK_ELIF ||
     LexerToken(&(p->lex)) == TK_ELSE)
    _push_jmp();

  while(LexerToken(&(p->lex)) == TK_ELIF) {
    br.to = br.jmp;
    if(_parse_branch(p,TK_ELIF,&br)) return -1;
    if(LexerToken(&(p->lex)) == TK_ELIF ||
       LexerToken(&(p->lex)) == TK_ELSE) _push_jmp();
  }

  /* Check if we have TK_ELSE */
  if(LexerToken(&(p->lex)) == TK_ELSE) {
    NEXT(); /* Skip else */
    cbpatchA(br.jmp,BC_IF,CodeBufferPos(codebuf(p)));
    has_else = 1;
    if(parse_stmtorchunk(p,1)) return -1;
  }

  /* Patch the last if/elif branch jumpping to *here* */
  if(!has_else) cbpatchA(br.jmp,BC_IF,CodeBufferPos(codebuf(p)));

  /* Close all jump out */
  for( i = 0 ; i < jo_sz ; ++i ) {
    cbpatchA(jump_out[i],BC_ENDIF,CodeBufferPos(codebuf(p)));
  }
  return 0;
}

#undef _push_jmp

static void _close_forjump( struct Parser* p ,
    size_t cont_jmp ) {
  struct LexScope* scp = cclosure(p)->cur_scp;
  size_t i;
  int brk_jmp;
  assert(scp->is_loop); /* Must be a loop */
  /* break. All the break will jump to a position
   * which pop variables inside of loop body since
   * we jump out of the body */
  brk_jmp = CodeBufferPos(codebuf(p));
  for( i = 0 ; i < scp->bjmp_sz ; ++i ) {
    cbpatchA(scp->bjmp[i],BC_BRK,brk_jmp);
  }
  /* continue jump */
  for( i = 0 ; i < scp->cjmp_sz ; ++i ) {
    cbpatchA(scp->cjmp[i],BC_CONT,cont_jmp);
  }
}

static int parse_for( struct Parser* p ) {
  assert( LexerToken(&(p->lex)) == TK_FOR );
  TRY(TK_LPAR); NEXT(); /* Skip ( */
  if(LexerToken(&(p->lex)) == TK_VARIABLE) {
    struct LexScope scp; /* Iterator bounded scope */
    struct CStr key; /* Loop variant key */
    struct CStr val; /* Loop variant value */
    struct Expr cond;/* Loop variant condition */
    struct Label skip_body;/* Loop header test jump */
    size_t loop_hdr; /* Loop header position */
    size_t cont_jmp; /* Continue jump position */
    enter_lexscope(p,&scp,1);
    key = StrBufToCStr(&(p->lex.lexeme.str)); /* get the variable name */
    NEXT();
    if(LexerToken(&(p->lex)) == TK_COMMA) {
      NEXT();
      if(LexerToken(&(p->lex)) != TK_VARIABLE) {
        perr(PERR_FOR_LOOP_VALUE);
        CStrDestroy(&key);
        return -1;
      }
      val = StrBufToCStr(&(p->lex.lexeme.str));
      NEXT();
    } else {
      val = CStrEmpty();
    }
    CONSUME(TK_IN); /* Skip in */
    if(pexpr(p,&cond)) return -1; /* evaluate the target */
    if(tryemit_expr(p,&cond)) return -1;
    if(def_rndvar(p,"itr") != LOCVAR_NEW) { /* pin iterator to an internal variable */
      perr(PERR_TOO_MANY_LOCAL_VARIABLES);
      return -1;
    }
    CONSUME(TK_RPAR);
    /* loop prolog */
    skip_body = cbputA(); /* forprep , loop header instruction */
    loop_hdr = CodeBufferPos(codebuf(p)); /* store the loop header */
    {
      struct LexScope inner_scp; /* Loop body scope */
      enter_lexscope(p,&inner_scp,0);
      /* key reservation */
      if(def_locvar(p,&key,1) != LOCVAR_NEW) {
        perr(PERR_TOO_MANY_LOCAL_VARIABLES);
        return -1;
      }
      /* value reservation */
      if(!CStrIsEmpty(&val)) {
        if(def_locvar(p,&val,1) != LOCVAR_NEW) {
          perr(PERR_TOO_MANY_LOCAL_VARIABLES);
          return -1;
        }
        cbOP(BC_IDREFKV);
      } else {
        cbOP(BC_IDREFK);
      }
      if(parse_stmtorchunk(p,0)) return -1; /* goto inner body */
      cont_jmp = CodeBufferPos(codebuf(p)); /* Continue statment jump position */
      leave_lexscope(p); /* leave the inner loop body */
      cbA(BC_FORLOOP,loop_hdr);
    }
    /* fix all break/continue statment jump table */
    _close_forjump(p,cont_jmp);
    /* fix skip_body jump */
    cbpatchA(skip_body,BC_FORPREP,CodeBufferPos(codebuf(p)));
    leave_lexscope(p);
    return 0;
  } else {
    perr(PERR_FOR_LOOP_KEY);
    return -1;
  }
}

/* Loop related control structure */
static int parse_continue( struct Parser* p ) {
  assert(LexerToken(&(p->lex)) == TK_CONTINUE);
  if(is_inloop(p)) {
    struct LexScope* lscp = find_loopscp(p);
    assert(lscp->is_loop);
    if(lscp->cjmp_sz == CONT_STMT_MAX) {
      perr(PERR_TOO_MANY_CONTINUE);
      return -1;
    } else {
      lscp->cjmp[lscp->cjmp_sz] = cbputA();
      ++lscp->cjmp_sz;
    }
  } else {
    perr(PERR_CONTINUE);
    return -1;
  }
  NEXT();
  return 0;
}

static int parse_break( struct Parser* p ) {
  assert(LexerToken(&(p->lex)) == TK_BREAK);
  if(is_inloop(p)) {
    struct LexScope* lscp = find_loopscp(p);
    assert(lscp->is_loop);
    if(lscp->bjmp_sz == BRK_STMT_MAX) {
      perr(PERR_TOO_MANY_BREAK);
      return -1;
    } else {
      /* Since break will break to the closet loop scope
       * but we may have sevaral local variables declaraed
       * in each nested lexical scope. We need to pop them
       * before the jump */
      int narg = p->closure->cur_scp->cur_idx - lscp->cur_idx;
      cbA(BC_POP,narg);
      lscp->bjmp[lscp->bjmp_sz] = cbputA();
      ++lscp->bjmp_sz;
    }
  } else {
    perr(PERR_BREAK);
    return -1;
  }
  NEXT();
  return 0;
}

/* Closure/Proto */
static int _parse_closureproto( struct Parser* p ,
    struct ObjProto* objc ) {
  struct CStr argarr[ CLOSURE_ARG_MAX ];
  size_t argarr_sz = 0;
  size_t i;
  struct StrBuf sbuf;

  assert(LexerToken(&(p->lex)) == TK_LPAR); NEXT(); /* Skip ( */
  if(LexerToken(&(p->lex)) == TK_RPAR) {
    NEXT();
    objc->narg = 0;
    objc->proto= CStrPrintF("()");
    return 0;
  } else {
    // (void)def_locvar(p,&VARG,0); /* cannot fail */
    do {
      if(LexerToken(&(p->lex)) == TK_VARIABLE) {
        if(argarr_sz == CLOSURE_ARG_MAX) {
          perr(PERR_TOO_MANY_CLOSURE_ARGUMENT);
          goto fail;
        }
        argarr[argarr_sz] = StrBufToCStr(LexerLexemeStr(&(p->lex)));
        ++argarr_sz;

        /* register the argument as local variables */
        if(def_locvar(p,argarr+argarr_sz-1,0) != LOCVAR_NEW) {
          perr(PERR_CLOSURE_ARGUMENT_DUP);
          goto fail;
        }

        NEXT();
        if(LexerToken(&(p->lex)) == TK_COMMA) {
          NEXT();
        } else if(LexerToken(&(p->lex)) == TK_RPAR) {
          NEXT(); break;
        } else {
          perr(PERR_UNEXPECTED_TOKEN,"\",\" or \")\"");
          goto fail;
        }
      } else {
        perr(PERR_UNKNOWN_TOKEN);
        goto fail;
      }
    } while(1);

    objc->narg = argarr_sz;

    /* Build the protocol string */
    StrBufInit(&sbuf,128);
    StrBufAppendStr(&sbuf,"(");
    for( i = 0 ; i < argarr_sz ; ++i ) {
      StrBufAppendCStr(&sbuf,argarr+i);
      CStrDestroy(argarr+i);
      if(i < argarr_sz-1) StrBufAppendStr(&sbuf,",");
    }
    StrBufAppendStr(&sbuf,")");
    objc->proto = StrBufToCStr(&sbuf);
    StrBufDestroy(&sbuf);
    return 0;
  }

fail:
  for( i = 0 ; i < argarr_sz ; ++i ) {
    CStrDestroy(argarr+i);
  }
  return -1;
}

static int parse_closure( struct Parser* p ) {
  struct ObjProto* objc = ObjNewProtoNoGC(p->sparrow,p->module);
  struct PClosure pclosure;
  struct LexScope lscope;
  int idx = objc->cls_idx;
  assert(LexerToken(&(p->lex)) == TK_FUNCTION);
  objc->start = LexerPosition(&(p->lex));

  NEXT(); /* skip function */

  /* initialize pclousre */
  initialize_pclosure(&pclosure,NULL,p->closure,objc);
  p->closure = &pclosure;

  /* initialize scope */
  enter_lexscope(p,&lscope,0);

  /* parse the freaking proto */
  if(_parse_closureproto(p,objc)) goto fail;

  /* parse the chunk */
  if(parse_chunk(p,0)) goto fail;

  /* generate a ret anyway, this code generation
   * is *right before* the leave_lexscope since if
   * an ret is emitted, the function enclosing scope
   * will know how to recover the stack frame without
   * extra byte codee */
  cbOP(BC_RETNULL);

  /* Exit the function lexical scope, not leave_scope
   * simply because exit_lexscope won't generate code
   * for poping local variable which we don't need it */
  exit_lexscope(p);

  /* exit the current closure scope */
  assert(cclosure(p)->cur_scp == NULL);
  p->closure = pclosure.prev;

  /* load closure on to stack */
  cbA(BC_LOADCLS,idx);

  /* destroy the pclosure internal gut */
  destroy_pclosure(&pclosure);

  objc->end = LexerPosition(&(p->lex));

  return 0;
fail:
  destroy_pclosure(&pclosure);
  return -1;
}

static int parse_return( struct Parser* p ) {
  assert(LexerToken(&(p->lex)) == TK_RETURN);
  NEXT(); /* Skip return */
  if(LexerToken(&(p->lex)) == TK_SEMICOLON) {
    cbOP(BC_RETNULL);
    return 0;
  } else {
    struct Expr val;
    if(pexpr(p,&val)) return -1;
    switch(val.tag) {
      case ETRUE: cbOP(BC_RETT); break;
      case EFALSE:cbOP(BC_RETF); break;
      case ENULL: cbOP(BC_RETNULL); break;
      case ENUMBER:
        {
          int ret;
          int ipart;
          ret = ConvNum(val.u.num,&ipart);
          if(ret || ipart > 1 || ipart < -1) {
            int idx = ConstAddNumber(objclosure(p),val.u.num);
            if(idx<0) {
              perr(PERR_TOO_MANY_NUMBER_LITERALS);
              return -1;
            }
            cbA(BC_RETN,idx);
          } else {
            switch(ipart) {
              case 0: cbOP(BC_RETN0); break;
              case 1: cbOP(BC_RETN1); break;
              case -1:cbOP(BC_RETNN1);break;
              default: assert(!"unreachable!"); break;
            }
          }
        }
        break;
      case ESTRING:
        {
          int idx = ConstAddString(objclosure(p),val.str);
          if(idx<0) {
            perr(PERR_TOO_MANY_STRING_LITERALS);
            return -1;
          }
          cbA(BC_RETS,idx);
        }
        break;
      default:
        if(tryemit_expr(p,&val)) return -1;
        cbOP(BC_RET);
        break;
    }
  }
  return 0;
}

static int parse_decl( struct Parser* p ) {
  assert(LexerToken(&(p->lex)) == TK_VAR);
  NEXT();
  if(LexerToken(&(p->lex)) != TK_VARIABLE) {
    perr(PERR_DECL_VARIABLE);
    return -1;
  } else {
    do {
      struct CStr cstr = StrBufToCStr(LexerLexemeStr(&(p->lex)));
      struct Expr val;
      int ret;
      NEXT();
      /* define local variable */
      ret = def_locvar(p,&cstr,1);
      if(ret != LOCVAR_NEW) return -1;
      if(LexerToken(&(p->lex)) == TK_ASSIGN) {
        NEXT();
        if(pexpr(p,&val)) {
          return -1;
        }
        if(tryemit_expr(p,&val)) return -1;
      } else {
        cbOP(BC_LOADNULL); /* load a null serve as default value */
      }
      if(LexerToken(&(p->lex)) == TK_COMMA) {
        NEXT();
      } else {
        break; /* break from *multiple* decl */
      }
    } while(1);
    return 0;
  }
}

/* parsing assignment for function call */
static int parse_assignorfunccall( struct Parser* p ) {
  struct Expr rexpr,lexpr;
  assert(LexerToken(&(p->lex)) == TK_VARIABLE);
  lexpr.str = StrBufToObjStrNoGC(p->sparrow,LexerLexemeStr(&(p->lex)));
  NEXT(); /* Skip VAR */
  if(is_pfix_tk(p)) {
    if(LexerToken(&(p->lex)) == TK_LPAR) {
      int ret = pexpr_intrinsic(p,&lexpr);
      if(ret == 1) {
        if(emit_readvar(p,&lexpr)) goto fail;
      }
    } else {
      if(emit_readvar(p,&lexpr)) goto fail;
    }
    if(_pexpr_pfix(p,&lexpr,&rexpr)) goto fail;
    switch(rexpr.tag) {
      case EUNDEFINED:
        if(lexpr.tag == EFUNCCALL)
          cbA(BC_POP,1);
        break;
      case EFUNCCALL:
        cbA(BC_POP,1);
        break;
      default: /* Require an assignment operation otherwise
                * it is a syntax error */
      {
        struct Expr val;
        if(LexerToken(&(p->lex)) != TK_ASSIGN) {
          perr(PERR_UNKNOWN_STMT);
          goto fail;
        }
        NEXT();
        if(pexpr(p,&val)) goto fail;
        if(tryemit_expr(p,&val)) goto fail;
        switch(rexpr.tag) {
          case ESTRING:
            { /* check whether the attribute is intrinsic attributes */
              enum IntrinsicAttribute iattr = IAttrGetIndex(rexpr.str->str);
              if(iattr == SIZE_OF_IATTR)
                cbA(BC_ASETS,rexpr.info);
              else
                cbA(BC_ASETI,iattr); /* intrinsic attributes */
            }
            break;
          case ENUMBER: cbA(BC_ASETN,rexpr.info); break;
          default: cbOP(BC_ASET); break;
        }
      }
      break;
    }
  } else {
    struct Expr val;
    int idx;
    CONSUME(TK_ASSIGN); /* skip = */
    if(pexpr(p,&val)) goto fail;
    if((idx = get_locvar(p,lexpr.str))<0) {
      if((idx = handle_upvar(p,lexpr.str))<0) {
        /* emit *global variable* set operation */
        idx = ConstAddString(objclosure(p),lexpr.str);
        if(idx<0) {
          perr(PERR_TOO_MANY_STRING_LITERALS);
          goto fail;
        }
        switch(val.tag) {
          case ETRUE: cbA(BC_GSETTRUE,idx); break;
          case EFALSE:cbA(BC_GSETFALSE,idx); break;
          case ENULL: cbA(BC_GSETNULL,idx); break;
          default:
            if(tryemit_expr(p,&val)) goto fail;
            cbA(BC_GSET,idx);
            break;
        }
      } else {
        /* emit as upvalue */
        switch(val.tag) {
          case ETRUE: cbA(BC_USETTRUE,idx); break;
          case EFALSE:cbA(BC_USETFALSE,idx); break;
          case ENULL: cbA(BC_USETNULL,idx); break;
          default:
            if(tryemit_expr(p,&val)) goto fail;
            cbA(BC_USET,idx);
            break;
        }
      }
    } else {
      /* emit as local variable */
      switch(val.tag) {
        case ETRUE: cbA(BC_MOVETRUE,idx); break;
        case EFALSE:cbA(BC_MOVEFALSE,idx); break;
        case ENULL: cbA(BC_MOVENULL,idx); break;
        case ENUMBER:
          {
            int ret;
            int ipart;
            ret = ConvNum(val.u.num,&ipart);
            if(ret || ipart > 5 || ipart < -5) {
              goto normal;
            } else {
              switch(ipart) {
                case 0: cbA(BC_MOVEN0,idx); break;
                case 1: cbA(BC_MOVEN1,idx); break;
                case 2: cbA(BC_MOVEN2,idx); break;
                case 3: cbA(BC_MOVEN3,idx); break;
                case 4: cbA(BC_MOVEN4,idx); break;
                case 5: cbA(BC_MOVEN5,idx); break;
                case -1:cbA(BC_MOVENN1,idx); break;
                case -2:cbA(BC_MOVENN2,idx); break;
                case -3:cbA(BC_MOVENN3,idx); break;
                case -4:cbA(BC_MOVENN4,idx); break;
                case -5:cbA(BC_MOVENN5,idx); break;
                default: assert(!"unreachable!"); break;
              }
            }
          }
          break;
        default:
normal:
          if(tryemit_expr(p,&val)) goto fail;
          cbA(BC_MOVE,idx);
          break;
      }
    }
  }
  return 0;

fail:
  return -1;
}

/* parsing the statment */
static int parse_stmt( struct Parser* p ) {
  switch(LexerToken(&(p->lex))) {
    case TK_VAR:
      if(parse_decl(p)) return -1;
      CONSUME(TK_SEMICOLON);
      return 0;
    case TK_CONTINUE:
      if(parse_continue(p)) return -1;
      CONSUME(TK_SEMICOLON);
      return 0;
    case TK_BREAK:
      if(parse_break(p)) return -1;
      CONSUME(TK_SEMICOLON);
      return 0;
    case TK_RETURN:
      if(parse_return(p)) return -1;
      CONSUME(TK_SEMICOLON);
      return 0;
    case TK_IF: return parse_if(p);
    case TK_FOR: return parse_for(p);
    case TK_VARIABLE:
      if(parse_assignorfunccall(p)) return -1;
      CONSUME(TK_SEMICOLON);
      return 0;
    default:
      perr(PERR_UNKNOWN_TOKEN);
      return -1;
  }
}

static int parse_chunk( struct Parser* p , int newscope ) {
  struct LexScope scp;
  if(newscope) enter_lexscope(p,&scp,0);
  assert(LexerToken(&(p->lex)) == TK_LBRA);
  NEXT();
  if(LexerToken(&(p->lex)) == TK_RBRA) goto done;

  do {
    if(parse_stmt(p)) return -1;
  } while(LexerToken(&(p->lex)) != TK_RBRA);

done:
  NEXT(); /* Skip } */
  if(newscope) leave_lexscope(p);
  return 0;
}

static int parse_mainchunk( struct Parser* p ) {
  struct LexScope scp;
  enter_lexscope(p,&scp,0);
  do {
    if(parse_stmt(p)) return -1;
  } while(LexerToken(&(p->lex)) != TK_EOF);
  /* lastly generate a *ret* */
  cbOP(BC_RETNULL);
  exit_lexscope(p);
  return 0;
}

static int parse_stmtorchunk( struct Parser* p , int newscope ) {
  if(LexerToken(&(p->lex)) == TK_LBRA) {
    return parse_chunk(p,newscope);
  } else {
    struct LexScope scp;
    if(newscope) enter_lexscope(p,&scp,0);
    if(parse_stmt(p)) return -1;
    if(newscope) leave_lexscope(p);
    return 0;
  }
}

/* Exported interface for parser module */
struct ObjModule* Parse( struct Sparrow* sparrow ,
    const char* fpath ,
    const char* source ,
    struct CStr* err ) {
  struct Parser p; /* Global parser object */
  struct PClosure pclosure; /* top level closure */
  struct ObjProto* objc;  /* top level closure object */
  int ret;
  struct ObjModule* mod;
  int free_src = 0;

  /* check if such file has been parsed or not */
  if(fpath) {
    mod = ObjFindModule(sparrow,fpath);
    if(mod != NULL) return mod;
  }

  /* if we don't have such file content, just read it */
  if(!source) {
    source = ReadFile( fpath ,NULL );
    if(!source) {
      struct StrBuf sbuf;
      StrBufInit(&sbuf,128);
      StrBufPrintF(&sbuf,"Cannot read file %s!",fpath);
      *err = StrBufToCStr(&sbuf);
      StrBufDestroy(&sbuf);
      return NULL;
    }
    free_src = 1;
  }
  mod = ObjNewModuleNoGC(sparrow,fpath,source);

  /* initialize parser object */
  LexerInit(&(p.lex),source,0); /* initialize lexer */
  LexerNext(&(p.lex)); /* set lexer to 1st token */
  p.closure = &pclosure;
  StrBufInit(&p.err,0);
  p.rnd_idx = 0;
  p.sparrow = sparrow;
  p.module = mod;

  /* initialize ObjProto */
  objc = ObjNewProtoNoGC( sparrow , mod );
  objc->proto = CStrPrintF("__entry__");
  objc->narg = 0;

  /* initialize pclosure */
  initialize_pclosure(&pclosure,NULL,NULL,objc);

  /* parse top level code , have fun :) */
  ret = parse_mainchunk(&p);

  /* destroy pclosure resource */
  destroy_pclosure(&pclosure);

  /* free source memory if we allocate it by ourself */
  if(free_src) free((void*)source);

  if(!ret) {
    LexerDestroy(&(p.lex));
    StrBufDestroy(&p.err);
    return mod;
  } else {
    *err = StrBufToCStr(&p.err);
    LexerDestroy(&(p.lex));
    StrBufDestroy(&p.err);
    return NULL;
  }
}
