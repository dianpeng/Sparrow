#ifndef BC_H_
#define BC_H_
#include <inttypes.h>
#include <stddef.h>
#include <stdio.h>
#include <shared/util.h>

/* Bytecode
 * We have a *none-fixed* length bytecode configuration. Each bytecode
 * occupies one single bytes and optionally it could have a 3 bytes
 * arguments
 *
 *     +====+
 *     | OP |
 *     +====+====================+
 *     | OP |  3 bytes arg       |
 *     +=========================+
 *
 * N number literal
 * S string literal
 * V variable
 *
 */

#define MAX_ARG_VALUE 0x00ffffff
#define BCARG_NULL 0
#define BCARG_TRUE 1
#define BCARG_FALSE 2

/* For testing purpose */
#ifndef CODE_BUFFER_INITIAL_SIZE
#define CODE_BUFFER_INITIAL_SIZE 32
#endif /* CODE_BUFFER_INITIAL_SIZE */

/* Order matters, don't adjust order !! */
#define BYTECODE(__) \
  /* Intrinsic functions , must be at very first to start \
   * index at 0 */ \
  __(BC_ICALL_TYPEOF,"c_typeof",1) \
  __(BC_ICALL_ISBOOLEAN,"c_is_boolean",1) \
  __(BC_ICALL_ISSTRING,"c_is_string",1) \
  __(BC_ICALL_ISNUMBER,"c_is_number",1) \
  __(BC_ICALL_ISNULL,"c_is_null",1) \
  __(BC_ICALL_ISLIST,"c_is_list",1) \
  __(BC_ICALL_ISMAP,"c_is_map",1) \
  __(BC_ICALL_ISCLOSURE,"c_is_closure",1) \
  __(BC_ICALL_TOSTRING,"c_to_string",1) \
  __(BC_ICALL_TONUMBER,"c_to_number",1) \
  __(BC_ICALL_TOBOOLEAN,"c_to_boolean",1) \
  __(BC_ICALL_PRINT,"c_print",1) \
  __(BC_ICALL_ERROR,"c_error",1) \
  __(BC_ICALL_ASSERT,"c_assert",1) \
  __(BC_ICALL_IMPORT,"c_import",1) \
  __(BC_ICALL_SIZE,"c_size",1) \
  __(BC_ICALL_RANGE,"c_range",1) \
  __(BC_ICALL_LOOP,"c_loop",1) \
  __(BC_ICALL_RUNSTRING,"c_run_string",1) \
  __(BC_ICALL_MIN,"c_min",1) \
  __(BC_ICALL_MAX,"c_max",1) \
  __(BC_ICALL_SORT,"c_sort",1) \
  __(BC_ICALL_SET,"c_set",1) \
  __(BC_ICALL_GET,"c_get",1) \
  __(BC_ICALL_EXIST,"c_exist",1) \
  __(BC_ICALL_MSEC,"c_msec",1) \
  /* Arithmatic operators */ \
  __(BC_ADDNV,"addnv",1) \
  __(BC_ADDVN,"addvn",1) \
  __(BC_ADDVV,"addvv",0) \
  __(BC_ADDSV,"addsv",1) \
  __(BC_ADDVS,"addvs",1) \
  __(BC_SUBNV,"subnv",1) \
  __(BC_SUBVN,"subvn",1) \
  __(BC_SUBVV,"subvv",0) \
  __(BC_MULNV,"mulnv",1) \
  __(BC_MULVN,"mulvn",1) \
  __(BC_MULVV,"mulvv",0) \
  __(BC_DIVNV,"divnv",1) \
  __(BC_DIVVN,"divvn",1) \
  __(BC_DIVVV,"divvv",0) \
  __(BC_POWVN,"powvn",1) \
  __(BC_POWNV,"pownv",1) \
  __(BC_POWVV,"powvv",0) \
  __(BC_MODVN,"modvn",1) \
  __(BC_MODNV,"modnv",1) \
  __(BC_MODVV,"modvv",0) \
  /* Unary */ \
  __(BC_NEG,"neg",0) \
  __(BC_NOT,"not",0) \
  __(BC_TEST,"test",0) \
  /* Load/Store destination is always specified */ \
  __(BC_LOADN,"loadn",1) \
  __(BC_LOADS,"loads",1) \
  __(BC_LOADV,"loadv",1) \
  __(BC_LOADNULL,"loadnull",0) \
  __(BC_LOADTRUE,"loadtrue",0) \
  __(BC_LOADFALSE,"loadfalse",0) \
  /* Move instructions */ \
  __(BC_MOVE,"move",1) \
  /* Optimized MOVE */ \
  __(BC_MOVETRUE,"movetrue",1) \
  __(BC_MOVEFALSE,"movefalse",1) \
  __(BC_MOVENULL,"movenull",1) \
  __(BC_MOVENN5,"movenn5",1) \
  __(BC_MOVENN4,"movenn4",1) \
  __(BC_MOVENN3,"movenn3",1) \
  __(BC_MOVENN2,"movenn2",1) \
  __(BC_MOVENN1,"movenn1",1) \
  __(BC_MOVEN0,"moven0",1) \
  __(BC_MOVEN1,"moven1",1) \
  __(BC_MOVEN2,"moven2",1) \
  __(BC_MOVEN3,"moven3",1) \
  __(BC_MOVEN4,"moven4",1) \
  __(BC_MOVEN5,"moven5",1) \
  /* Comparison */ \
  __(BC_LTNV,"ltnv",1) \
  __(BC_LTVN,"ltvn",1) \
  __(BC_LTSV,"ltsv",1) \
  __(BC_LTVS,"ltvs",1) \
  __(BC_LTVV,"ltvv",0) \
  __(BC_LENV,"lenv",1) \
  __(BC_LEVN,"levn",1) \
  __(BC_LESV,"lesv",1) \
  __(BC_LEVS,"levs",1) \
  __(BC_LEVV,"levv",0) \
  __(BC_GTNV,"gtnv",1) \
  __(BC_GTVN,"gtvn",1) \
  __(BC_GTSV,"gtsv",1) \
  __(BC_GTVS,"gtvs",1) \
  __(BC_GTVV,"gtvv",0) \
  __(BC_GENV,"genv",1) \
  __(BC_GEVN,"gevn",1) \
  __(BC_GESV,"gesv",1) \
  __(BC_GEVS,"gevs",1) \
  __(BC_GEVV,"gevv",0) \
  __(BC_EQNV,"eqnv",1) \
  __(BC_EQVN,"eqvn",1) \
  __(BC_EQSV,"eqsv",1) \
  __(BC_EQVS,"eqvs",1) \
  __(BC_EQVNULL,"eqvnull",0) \
  __(BC_EQNULLV,"eqnullv",0) \
  __(BC_EQVV,"eqvv",0) \
  __(BC_NENV,"nenv",1) \
  __(BC_NEVN,"nevn",1) \
  __(BC_NESV,"nesv",1) \
  __(BC_NEVS,"nevs",1) \
  __(BC_NEVNULL,"nevnull",0) \
  __(BC_NENULLV,"nenullv",0) \
  __(BC_NEVV,"nevv",0) \
  /* Control flow JUMP instructions */ \
  /* Alias JUMP specialized for break and continue statements , ease the \
   * pain when we do IR generation . The BC_BRK and BC_CONT is the same here. \
   * we don't have forward jump and backward jump difference */ \
  __(BC_BRK,"brk",1) \
  __(BC_CONT,"cont",1) \
  __(BC_IF,"if",1) \
  /* ENDIF is just an alias of JMP but it only generated in branch body. \
   * The backend can easily know this is a jump jump to the merge node */ \
  __(BC_ENDIF,"endif",1) \
  __(BC_BRT,"brt",1) \
  __(BC_BRF,"brf",1) \
  /* Call */ \
  __(BC_CALL,"call",1) \
  __(BC_CALL0,"call0",0) \
  __(BC_CALL1,"call1",0) \
  __(BC_CALL2,"call2",0) \
  __(BC_CALL3,"call3",0) \
  __(BC_CALL4,"call4",0) \
  /* New */ \
  __(BC_NEWL0,"newl0",0) \
  __(BC_NEWL1,"newl1",0) \
  __(BC_NEWL2,"newl2",0) \
  __(BC_NEWL3,"newl3",0) \
  __(BC_NEWL4,"newl4",0) \
  __(BC_NEWL,"newl",1) \
  __(BC_NEWM0,"newm0",0) \
  __(BC_NEWM1,"newm1",0) \
  __(BC_NEWM2,"newm2",0) \
  __(BC_NEWM3,"newm3",0) \
  __(BC_NEWM4,"newm4",0) \
  __(BC_NEWM,"newm",1) \
  /* Attributes/Index Get */ \
  __(BC_AGETS,"agets",1) \
  __(BC_AGETN,"agetn",1) \
  __(BC_AGETI,"ageti",1) \
  __(BC_AGET,"aget",0) \
  /* Upvalue */ \
  __(BC_UGET,"uget",1) \
  __(BC_USET,"uset",1) \
  __(BC_USETTRUE,"usettrue",1) \
  __(BC_USETFALSE,"usetfalse",1) \
  __(BC_USETNULL,"usetnull",1) \
  /* Global value */ \
  __(BC_GGET,"gget",1) \
  __(BC_GSET,"gset",1) \
  __(BC_GSETTRUE,"gsettrue",1) \
  __(BC_GSETFALSE,"gsetfalse",1) \
  __(BC_GSETNULL,"gsetnull",1) \
  /* Iterator */ \
  __(BC_IDREFK,"idrefk",0) \
  __(BC_IDREFKV,"idrefkv",0) \
  __(BC_FORPREP,"forprep",1) \
  __(BC_FORLOOP,"forloop",1) \
  /* Stack pop */ \
  __(BC_POP,"pop",1) \
  /* Return */ \
  __(BC_RET,"ret",0) \
  __(BC_RETN,"retn",1) \
  __(BC_RETS,"rest",1) \
  __(BC_RETT,"rett",0) \
  __(BC_RETF,"retf",0) \
  __(BC_RETN0,"retn0",0) \
  __(BC_RETN1,"retn1",0) \
  __(BC_RETNN1,"retnn1",0) \
  __(BC_RETNULL,"retnull",0) \
  /* Attribute set */ \
  __(BC_ASETN,"asetn",1) \
  __(BC_ASETS,"asets",1) \
  __(BC_ASET,"aset",0) \
  __(BC_ASETI,"aseti",0) \
  /* Load closure */ \
  __(BC_LOADCLS,"loadcls",1) \
  /* Optimization */ \
  __(BC_LOADNN5,"loadnn5",0) \
  __(BC_LOADNN4,"loadnn4",0) \
  __(BC_LOADNN3,"loadnn3",0) \
  __(BC_LOADNN2,"loadnn2",0) \
  __(BC_LOADNN1,"loadnn1",0) \
  __(BC_LOADN0,"loadn0",0) \
  __(BC_LOADN1,"loadn1",0) \
  __(BC_LOADN2,"loadn2",0) \
  __(BC_LOADN3,"loadn3",0) \
  __(BC_LOADN4,"loadn4",0) \
  __(BC_LOADN5,"loadn5",0) \
  /* JIT tag */ \
  __(BC_LOOP,"loop",1) \
  __(BC_CLOSURE,"closure",1) \
  /* Debug */ \
  __(BC_OP,"<OP>",0) \
  __(BC_A,"<A>",1) \
  __(BC_NOP,"nop",0)

/* This directive basically counts that at most 11 special bytecode to
 * load number directly (no need to get an arg of opcode from constant table ).
 * The 11 comes from (-1 -- -5 + 0 + 1 -- 5 ) */
#define BC_SPECIAL_NUMBER_SIZE 11

/* Intrinsic function table */
#define INTRINSIC_FUNCTION(__) \
  __(TYPEOF,TypeOf,"typeof") \
  __(ISBOOLEAN,IsBoolean,"is_boolean") \
  __(ISSTRING,IsString,"is_string") \
  __(ISNUMBER,IsNumber,"is_number") \
  __(ISNULL,IsNull,"is_null") \
  __(ISLIST,IsList,"is_list") \
  __(ISMAP,IsMap,"is_map") \
  __(ISCLOSURE,IsClosure,"is_closure") \
  __(TOSTRING,ToString,"to_string") \
  __(TONUMBER,ToNumber,"to_number") \
  __(TOBOOLEAN,ToBoolean,"to_boolean") \
  __(PRINT,Print,"print") \
  __(ERROR,Error,"error") \
  __(ASSERT,Assert,"assert") \
  __(IMPORT,Import,"import") \
  __(SIZE,Size,"size") \
  __(RANGE,Range,"range") \
  __(LOOP,Loop,"loop") \
  __(RUNSTRING,RunString,"run_string") \
  __(MIN,Min,"min") \
  __(MAX,Max,"max") \
  __(SORT,Sort,"sort") \
  __(SET,Set,"set") \
  __(GET,Get,"get") \
  __(EXIST,Exist,"exist") \
  __(MSEC,MSec,"msec")

/* Intrinsic Attribute */
#define INTRINSIC_ATTRIBUTE(__) \
  __(EXTEND,"extend") \
  __(PUSH,"push") \
  __(POP,"pop") \
  __(SIZE,"size") \
  __(RESIZE,"resize") \
  __(EMPTY,"empty") \
  __(CLEAR,"clear") \
  __(SLICE,"slice") \
  __(EXIST,"exist")

enum IntrinsicFunction {
#define __(A,B,C) IFUNC_##A,
  INTRINSIC_FUNCTION(__)
#undef __
  SIZE_OF_IFUNC
};

enum IntrinsicAttribute {
#define __(A,B) IATTR_##A,
  INTRINSIC_ATTRIBUTE(__)
#undef __
  SIZE_OF_IATTR
};

enum Bytecode {
#define __(A,B,C) A,
  BYTECODE(__)
  SIZE_OF_BYTECODE
#undef __
};

const char* GetBytecodeName( enum Bytecode );

enum Bytecode IFuncGetBytecode( const char* name );
const char* IFuncGetName( enum IntrinsicFunction );

enum IntrinsicAttribute IAttrGetIndex( const char* name );
const char* IAttrGetName( enum IntrinsicAttribute );

/* Emitters for bytecode */
struct InstrDebugInfo {
  size_t line;
  size_t ccnt;
};

struct CodeBuffer {
  /* Debug information */
  struct InstrDebugInfo* dbg_arr;
  size_t dbg_size;
  size_t dbg_cap;
  /* Instruction buffer */
  uint8_t* buf;
  size_t cap;
  size_t pos;

  size_t ins_size; /* Instruction size */
};

struct Label {
  size_t code_pos;
  size_t dbg_pos;
};

void CodeBufferInit( struct CodeBuffer* );
void CodeBufferDestroy( struct CodeBuffer* );

static SPARROW_INLINE
void CodeBufferGetState( struct CodeBuffer* cb,
    size_t* ins_pos,
    size_t* dbg_pos,
    size_t* ins_size) {
  if(ins_pos) *ins_pos = cb->pos;
  if(dbg_pos) *dbg_pos = cb->dbg_size;
  if(ins_size)*ins_size= cb->ins_size;
}

#define CodeBufferPos(CB) ((CB)->pos)

static SPARROW_INLINE
struct Label CodeBufferGetLabel( struct CodeBuffer* cb ) {
  struct Label ret = { cb->pos , cb->dbg_size };
  return ret;
}

static SPARROW_INLINE
void CodeBufferSetToLabel( struct CodeBuffer* cb , struct Label l ) {
  cb->pos = l.code_pos;
  cb->dbg_size = l.dbg_pos;
  cb->ins_size = l.dbg_pos;
}

struct Label CodeBufferPutA( struct CodeBuffer* );
struct Label CodeBufferPutOP(struct CodeBuffer* );

int CodeBufferEmitA  ( struct CodeBuffer* ,
    enum Bytecode op , uint32_t A ,
    size_t line , size_t ccnt );
int CodeBufferEmitOP ( struct CodeBuffer* ,
    enum Bytecode op ,
    size_t line , size_t ccnt );
void CodeBufferPatchOP( struct CodeBuffer* ,
    struct Label l,
    enum Bytecode op,
    size_t line, size_t ccnt );
void CodeBufferPatchA ( struct CodeBuffer* ,
    struct Label l,
    enum Bytecode op , uint32_t A ,
    size_t line, size_t ccnt );

/* helper function for debugging */
void CodeBufferDump( const struct CodeBuffer* cb ,
    FILE* output , const char* prefix );

/* helper decoder for A type instructions */
static SPARROW_INLINE
uint32_t CodeBufferDecodeArg( const struct CodeBuffer* b , size_t pos ) {
  int p = (int)pos;
  uint8_t b1 = b->buf[p];
  uint8_t b2 = b->buf[p+1];
  uint8_t b3 = b->buf[p+2];
  return b1 + (b2 << 8) + (b3 << 16);
}

#endif /* BC_H_ */
