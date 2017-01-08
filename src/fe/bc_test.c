#define CODE_BUFFER_INITIAL_SIZE 24
#include "bc.h"

struct check_result {
  int argcount; /* Argument count */
  uint32_t operand; /* Operand of instructions */
  size_t line; /* Line number */
  size_t ccnt; /* Char count */
};

#define __(A,B,C) C,

static int ARG_COUNT[SIZE_OF_BYTECODE+1] = {
  BYTECODE(__)
  -1
};

#undef __

static void test_bc_basic() {
  {
    struct CodeBuffer cb;
    CodeBufferInit(&cb);
    size_t line = 1;
    size_t ccnt = 1;
    size_t nins = 1;

#define __(A,B,C) \
    do { \
      if(C == 1) { \
        CodeBufferEmitA(&cb,A,nins,line,ccnt); \
      } else { \
        CodeBufferEmitOP(&cb,A,line,ccnt); \
      } \
      ++line; ++ccnt; ++nins; \
    } while(0);

    BYTECODE(__)

#undef __

    {
      int i = 0;
      int pos = 0;
      struct check_result res[255];
      for( ; i < SIZE_OF_BYTECODE ; ++i ) {
        res[i].argcount = ARG_COUNT[i];
        res[i].operand = (uint32_t)(i+1);
        res[i].line = (uint32_t)(i+1);
        res[i].ccnt = (uint32_t)(i+1);
      }
      i = 0;
      do {
        uint8_t op = cb.buf[pos];
        assert(op == i);
        if(ARG_COUNT[op]) {
          assert( res[i].operand == CodeBufferDecodeArg(&cb,pos+1) );
          assert( res[i].line == cb.dbg_arr[i].line );
          assert( res[i].ccnt == cb.dbg_arr[i].ccnt );
          pos += 4;
        } else {
          assert( res[i].line == cb.dbg_arr[i].line );
          assert( res[i].ccnt == cb.dbg_arr[i].ccnt );
          pos += 1;
        }
        ++i;
      } while(pos < cb.pos);
    }
    CodeBufferDestroy(&cb);
  }
}

int main() {
  fprintf(stderr,"Current OP number:%d\n",SIZE_OF_BYTECODE);
  test_bc_basic();
  return 0;
}
