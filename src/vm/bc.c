#include <vm/bc.h>
#include <shared/debug.h>

#define __(A,B,C) C,
static int DEBUG_TABLE[SIZE_OF_BYTECODE+1] = {
  BYTECODE(__)
  -1
};
#undef __

static const char* IFUNCTABLE[] = {
#define __(A,B,C) C,
  INTRINSIC_FUNCTION(__)
#undef __
  NULL
};

static const char* IATTRTABLE[] = {
#define __(A,B) B,
  INTRINSIC_ATTRIBUTE(__)
#undef __
  NULL
};

enum Bytecode IFuncGetBytecode( const char* name ) {
  size_t i;
  for( i = 0 ; i < SIZE_OF_IFUNC ; ++i ) {
    if(strcmp(name,IFUNCTABLE[i])==0) {
      return (enum Bytecode)(i);
    }
  }
  return BC_NOP;
}

const char* IFuncGetName( enum IntrinsicFunction ifunc ) {
#define __(A,B,C) case IFUNC_##A: return C;
  switch(ifunc) {
    INTRINSIC_FUNCTION(__)
    default: SPARROW_UNREACHABLE(); return NULL;
  }
#undef __
}

enum IntrinsicAttribute IAttrGetIndex( const char* iattr ) {
  size_t i;
  for( i = 0 ; i < SIZE_OF_IATTR ; ++i ) {
    if(strcmp(iattr,IATTRTABLE[i])==0) {
      return (enum IntrinsicAttribute)(i);
    }
  }
  return SIZE_OF_IATTR;
}

const char* IAttrGetName( enum IntrinsicAttribute iattr ) {
  switch(iattr) {
#define __(A,B) case IATTR_##A: return B;
    INTRINSIC_ATTRIBUTE(__)
    default: return NULL;
#undef __
  }
}

static void patchA( enum Bytecode op , uint32_t A ,
    uint8_t* location ) {
  uint32_t c1,c2,c3;
  SPARROW_ASSERT(op >= 0 && op < SIZE_OF_BYTECODE);
  SPARROW_ASSERT(A < MAX_ARG_VALUE);
  c1 = (uint8_t)(A & 0xff);
  c2 = (uint8_t)((A & 0xff00)>>8);
  c3 = (uint8_t)((A & 0xff0000)>>16);
  location[0] = (uint8_t)op;
  location[1] = c1;
  location[2] = c2;
  location[3] = c3;
}

static void patchOP( enum Bytecode op , uint8_t* location ) {
  SPARROW_ASSERT(op >= 0 && op < SIZE_OF_BYTECODE);
  *location = op;
}

static void encodeA( struct CodeBuffer* cb ,
    enum Bytecode op , uint32_t A ) {
  patchA(op,A,cb->pos+cb->buf);
  cb->pos += 4;
}

static void encodeOP( struct CodeBuffer* cb ,
    enum Bytecode op ) {
  patchOP(op,cb->buf+cb->pos);
  cb->pos++;
}

void CodeBufferInit( struct CodeBuffer* cb ) {
  cb->buf = malloc(CODE_BUFFER_INITIAL_SIZE);
  cb->cap = CODE_BUFFER_INITIAL_SIZE;
  cb->pos = 0;
  cb->dbg_arr = NULL;
  cb->dbg_size= 0;
  cb->dbg_cap = 0;
  cb->ins_size = 0;
}

void CodeBufferDestroy( struct CodeBuffer* cb ) {
  free(cb->buf);
  free(cb->dbg_arr);
  cb->buf = NULL;
  cb->cap = cb->pos = 0;
  cb->dbg_arr = NULL;
  cb->dbg_size = cb->dbg_cap = 0;
  cb->ins_size = 0;
}

struct Label CodeBufferPutOP( struct CodeBuffer* cb ) {
  struct Label ret;
  struct InstrDebugInfo dbg = {0,0};
  if(cb->pos == cb->cap) {
    /* Add more spaces */
    MemGrow((void**)&(cb->buf),&(cb->cap),1);
  }
  encodeOP(cb,BC_OP);
  /* Add debug entry for this instructions */
  DynArrPush(cb,dbg,dbg);
  ++cb->ins_size;
  SPARROW_ASSERT(cb->ins_size == cb->dbg_size);
  ret.dbg_pos = cb->dbg_size -1;
  ret.code_pos= cb->pos-1;
  return ret;
}

struct Label CodeBufferPutA( struct CodeBuffer* cb ) {
  int i;
  struct InstrDebugInfo dbg = {0,0};
  struct Label ret;
  for( i = 0 ; i < 2 ; ++i ) {
    if(cb->pos +4 > cb->cap) {
      MemGrow((void**)&(cb->buf),&(cb->cap),1);
    } else break;
  }
  SPARROW_ASSERT(cb->pos+4 <= cb->cap);
  encodeA(cb,BC_A,0);
  DynArrPush(cb,dbg,dbg);
  ++cb->ins_size;
  SPARROW_ASSERT(cb->ins_size == cb->dbg_size);
  ret.dbg_pos = cb->dbg_size - 1;
  ret.code_pos = cb->pos - 4;
  return ret;
}

int CodeBufferEmitA( struct CodeBuffer* cb ,
    enum Bytecode op, uint32_t A,
    size_t line, size_t ccnt ) {
  int i;
  struct InstrDebugInfo dbg;
  for( i = 0 ; i < 2 ; ++i ) {
    if(cb->pos + 4 > cb->cap) {
      MemGrow((void**)&(cb->buf),&(cb->cap),1);
    }
  }
  SPARROW_ASSERT(cb->pos+4 <= cb->cap);
  SPARROW_ASSERT(A < MAX_ARG_VALUE);
  SPARROW_ASSERT(DEBUG_TABLE[op]);
  encodeA(cb,op,A);
  dbg.line = line;
  dbg.ccnt = ccnt;
  DynArrPush(cb,dbg,dbg);
  ++cb->ins_size;
  SPARROW_ASSERT(cb->ins_size == cb->dbg_size);
  return 0;
}

int CodeBufferEmitOP( struct CodeBuffer* cb ,
    enum Bytecode op ,
    size_t line , size_t ccnt ) {
  struct InstrDebugInfo dbg;
  if(cb->pos == cb->cap) {
    MemGrow((void**)&(cb->buf),&(cb->cap),1);
  }
  SPARROW_ASSERT(!DEBUG_TABLE[op]);
  encodeOP(cb,op);
  dbg.line = line;
  dbg.ccnt = ccnt;
  DynArrPush(cb,dbg,dbg);
  ++cb->ins_size;
  SPARROW_ASSERT(cb->ins_size == cb->dbg_size);
  return 0;
}

void CodeBufferPatchOP( struct CodeBuffer* cb,
    struct Label l,
    enum Bytecode op,
    size_t line, size_t ccnt ) {
  SPARROW_ASSERT(l.code_pos < cb->pos);
  SPARROW_ASSERT(cb->buf[l.code_pos] == BC_OP);
  patchOP(op,cb->buf + l.code_pos);
  cb->dbg_arr[l.dbg_pos].line = line;
  cb->dbg_arr[l.dbg_pos].ccnt = ccnt;
}

void CodeBufferPatchA( struct CodeBuffer* cb,
    struct Label l,
    enum Bytecode op, uint32_t A,
    size_t line, size_t ccnt ) {
  SPARROW_ASSERT(l.code_pos < cb->pos);
  SPARROW_ASSERT(cb->buf[l.code_pos] == BC_A);
  SPARROW_ASSERT(A < MAX_ARG_VALUE);
  patchA(op,A,cb->buf + l.code_pos);
  cb->dbg_arr[l.dbg_pos].line = line;
  cb->dbg_arr[l.dbg_pos].ccnt = ccnt;
}

void CodeBufferDump( const struct CodeBuffer* cb,
    FILE* output , const char* prefix ) {
  size_t pos = 0;
  size_t nins= 0;
  if(prefix)
    fprintf(output,"Code buffer dump(%s):\n",prefix);
  else
    fprintf(output,"Code buffer dump\n");
  fprintf(output,"Instruction count: %zu\n"
                 "Code buffer size : %zu\n",
                 cb->ins_size,cb->pos);
  while(pos < cb->pos){
    uint8_t op = cb->buf[pos];

#define __(A,B,C) \
    case A: \
      if(C == 1) { \
        uint32_t opr = CodeBufferDecodeArg(cb,pos+1); \
        fprintf(output,"%zu. %zu(4)    %10s(%d) @(%zu,%zu)\n",nins+1,pos,B,opr, \
            cb->dbg_arr[nins].line , \
            cb->dbg_arr[nins].ccnt); \
        pos += 4; \
      }  else { \
        fprintf(output,"%zu. %zu(1)    %10s @(%zu,%zu)\n",nins+1,pos,B,\
            cb->dbg_arr[nins].line , \
            cb->dbg_arr[nins].ccnt); \
        pos += 1; \
      } \
      ++nins; break;

    switch(op) {
      BYTECODE(__)
      default:
        SPARROW_UNREACHABLE();
        return;
    }

#undef __
  }
  fflush(output);
}
