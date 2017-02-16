#include "bc-ir-builder.h"
#include "../fe/object.h"

/* Forward =========================> */
static int build_if( struct Sparrow* , struct BytecodeIrBuilder* );
static int build_for(struct Sparrow* , struct BytecodeIrBuilder* );
static int build_branch( struct Sparrow* , struct BytecodeIrBuilder* );

struct StackStats {
  size_t stk_size;
  size_t stk_cap;
  struct IrNode** stk_arr;
};

static void ss_init( struct StackStats* ss ) {
  ss->stk_size = 0;
  ss->stk_cap = 8;
  ss->stk_arr = malloc( sizeof(struct IrNode*) * 8 );
}

static void ss_push( struct StackStats* ss, struct IrNode* node ) {
  DynArrPush(ss,stk,node);
}

static void ss_pop( struct StackStats* ss, size_t num ) {
  assert(num <= ss->stk_size);
  ss->stk_size -= num;
}

static struct IrNode* ss_top( struct StackStats* ss, size_t num ) {
  assert(num < ss->stk_size);
  return ss->stk_arr[ (ss->stk_size - 1) - num ];
}

static struct IrNode* ss_bot( struct StackStats* ss, size_t index ) {
  assert(index < ss->stk_size);
  return ss->stk_arr[index];
}

static struct IrNode* ss_replace( struct StackStats* ss ,
    struct IrNode* node ) {
  assert(ss->stk_size > 0);
  ss->stk_arr[ (ss->stk_size-1) ] = node;
}

static void ss_place( struct StackStats* ss ,
    size_t index ,
    struct IrNode* node ) {
  assert( index < ss->stk_size );
  ss->stk_arr[index] = node;
}

static void ss_clone( const struct StackStats* src , struct StackStats* dest ) {
  size_t i;
  dest->stk_size = src->stk_size;
  dest->stk_cap = src->stk_cap;
  dest->stk_arr = malloc(sizeof(struct IrNode*)*src->stk_size);
  memcpy(dest->stk_arr,src->stk_arr,dest->stk_size*sizeof(struct IrNode*));
}

static void ss_destroy( struct StackStats* ss ) {
  free(ss->stk_arr);
  ss->stk_size = ss->stk_cap = 0;
  ss->stk_arr = NULL;
}

static void ss_move( struct StackStats* left , struct StackStats* right ) {
  ss_destroy(left);
  *left = *right;
  right->stk_arr = NULL;
  right->stk_size = right->stk_cap = 0;
}

/* IrBuilder ============================================== */
struct MergedRegion {
  uint32_t pos;
  struct IrNode* node;
};

struct MergedRegionList {
  struct MergedRegion* mregion_arr;
  size_t mregion_size;
  size_t mregion_cap;
};

struct BytecodeIrBuilder {
  const struct ObjProto* proto; /* Target proto */
  struct IrGraph* graph;     /* Targeted graph */
  struct StackStats stack; /* Current stack */
  struct IrNode* region;     /* Current region node */
  size_t code_pos;           /* Current codei postion */

  /* An array of existed merged region. In our code, any time a branch
   * merged region will be created only if it is not created already.
   * Since our merge can have multiple phase, we need to put it into an
   * array to look up later on */
  struct MergedRegionList* mregion;
};

static void builder_clone( const struct BytecodeIrBuilder* old_builder ,
                          struct BytecodeIrBuilder* new_builder ,
                          struct IrNode* new_region ) {
  new_builder->proto = old_builder->proto;
  new_builder->graph = old_builder->graph;
  ss_clone(&(old_builder->stack),&(new_builder->stack));
  new_builder->region = new_region;
  new_builder->code_pos = old_builder->code_pos;
}

static void builder_destroy( struct BytecodeIrBuilder* builder ) {
  builder->proto = NULL;
  builder->graph = NULL;
  builder->region = NULL;
  builder->code_pos = 0;
  builder->mregion = NULL;
  ss_destroy(&(builder->stack));
}

/* Merge the *right* bytecode_ir_builder into *left* builder . And the right
 * hand side will be destroyed */
static void builder_place_phi( struct BytecodeIrBuilder* left ,
                               struct BytecodeIrBuilder* right ) {
  size_t i;
  const size_t len = MIN(left->stack.stk_size,right->stack.stk_size);
  /* The merge really just means place PHI node. Our phi node is simply a
   * node that takes 2 operands. Since we simply generate multiple branch
   * as nested if else branch then our PHI node should be simply nested
   * as well. We only care about the stack slot up to a point that left
   * has it. For rest of the stack slot we will just discard them */
  for( i = 0 ; i < len ; ++i ) {
    left->stack.stk_arr[i] = IrNodeNewPhi(left->graph,left->stack.stk_arr[i],
                                                      right->stack.stk_arr[i]);
  }
  builder_destroy(right);
}

static struct IrNode*
builder_get_or_create_merged_region( const struct BytecodeIrBuilder* builder ,
                                     uint32_t pos,
                                     struct IrNode* if_true,
                                     struct IrNode* if_false );

static int build_bytecode( struct Sparrow* sparrow ,
                           struct BytecodeIrBuilder* builder ) {
  size_t code_pos = builder->code_pos;
  const struct ObjProto* proto = builder->proto;
  struct IrGraph* graph = builder->graph;
  struct IrNode* region = builder->region;
  struct StackStats* stack = &(builder->stack);
  uint8_t op;
  uint32_t opr;
  const struct CodeBuffer* code_buffer = &(proto->code_buf);

  /* Here we just use a switch case , no need to threading the code */
#define DECODE_ARG() \
  do { \
    opr = CodeBufferDecodeArg(code_buffer,code_pos); \
    code_pos += 3; \
  } while(0)

#define CASE(X) case X:

#define DISPATCH() break

  op = code_buffer->buf[code_pos++];
  switch(op) {

    CASE(BC_ADDNV) {
      struct IrNode* bin;
      DECODE_ARG();
      bin = IrNodeNewBinary( graph , IR_H_ADD ,
          IrNodeNewConstNumber(graph,opr,proto),
          ss_top(stack,0),
          region);
      ss_replace(stack,bin);
      DISPATCH();
    }

    CASE(BC_ADDVN) {
      struct IrNode* bin;
      DECODE_ARG();
      bin = IrNodeNewBinary( graph , IR_H_ADD ,
          ss_top(stack,0),
          IrNodeNewConstNumber(graph,opr,proto),
          region);
      ss_replace(stack,bin);
      DISPATCH();
    }

    CASE(BC_ADDSV) {
      struct IrNode* bin;
      DECODE_ARG();
      bin = IrNodeNewBinary(graph,IR_H_ADD,
          ss_top(stack,0),
          IrNodeNewConstString(graph,opr,proto),
          region);
      ss_replace(stack,bin);
      DISPATCH();
    }

    CASE(BC_ADDVS) {
      struct IrNode* bin;
      DECODE_ARG();
      bin = IrNodeNewBinary(graph,IR_H_ADD,
          IrNodeNewConstString(graph,opr,proto),
          ss_top(stack,0),
          region);
      ss_replace(stack,bin);
      DISPATCH();
    }

    CASE(BC_ADDVV) {
      struct IrNode* bin = IrNodeNewBinary(graph,IR_H_ADD,
          ss_top(stack,1),
          ss_top(stack,0),
          region);
      ss_pop(stack,2);ss_push(stack,bin);
      DISPATCH();
    }

    CASE(BC_SUBNV) {
      struct IrNode* bin;
      DECODE_ARG();
      bin = IrNodeNewBinary(graph,IR_H_ADD,
          IrNodeNewConstNumber(graph,opr,proto),
          ss_top(stack,0),
          region);
      ss_replace(stack,bin);
      DISPATCH();
    }

    CASE(BC_SUBVN) {
      struct IrNode* bin;
      DECODE_ARG();
      bin = IrNodeNewBinary(graph,IR_H_ADD,
          IrNodeNewConstNumber(graph,opr,proto),
          ss_top(stack,0),
          region);
      ss_replace(stack,bin);
      DISPATCH();
    }

    CASE(BC_SUBVV) {
      struct IrNode* bin;
      bin = IrNodeNewBinary(graph,IR_H_ADD,
          ss_top(stack,1),
          ss_top(stack,0),
          region);
      ss_pop(stack,2); ss_push(stack,bin);
      DISPATCH();
    }

    CASE(BC_MULNV) {
      struct IrNode* bin;
      DECODE_ARG();
      bin = IrNodeNewBinary(graph,IR_H_ADD,
          IrNodeNewConstNumber(graph,opr,proto),
          ss_top(stack,0),
          region);
      ss_replace(stack,bin);
      DISPATCH();
    }

    CASE(BC_MULVN) {
      struct IrNode* bin;
      DECODE_ARG();
      bin = IrNodeNewBinary(graph,IR_H_ADD,
          ss_top(stack,0),
          IrNodeNewConstNumber(graph,opr,proto),
          region);
      ss_replace(stack,bin);
      DISPATCH();
    }

    CASE(BC_MULVV) {
      struct IrNode* bin;
      bin = IrNodeNewBinary(graph,IR_H_ADD,
          ss_top(stack,1),
          ss_top(stack,0),
          region);
      ss_pop(stack,2);
      ss_push(stack,bin);
      DISPATCH();
    }

    CASE(BC_DIVNV) {
      struct IrNode* bin;
      DECODE_ARG();
      bin = IrNodeNewBinary(graph,IR_H_ADD,
          IrNodeNewConstNumber(graph,opr,proto),
          ss_top(stack,0),
          region);
      ss_replace(stack,bin);
      DISPATCH();
    }

    CASE(BC_DIVVN) {
      struct IrNode* bin;
      DECODE_ARG();
      bin = IrNodeNewBinary(graph,IR_H_ADD,
          ss_top(stack,0),
          IrNodeNewConstNumber(graph,opr,proto),
          region);
      ss_replace(stack,bin);
      DISPATCH();
    }

    CASE(BC_DIVVV) {
      struct IrNode* bin = IrNodeNewBinary(graph,IR_H_ADD,
          ss_top(stack,1),
          ss_top(stack,0),
          region);
      ss_pop(stack,2); ss_push(stack,bin);
      DISPATCH();
    }

    CASE(BC_MODVN) {
      struct IrNode* bin;
      DECODE_ARG();
      bin = IrNodeNewBinary(graph,IR_H_ADD,
          IrNodeNewConstNumber(graph,opr,proto),
          ss_top(stack,0),
          region);
      ss_replace(stack,bin);
      DISPATCH();
    }

    CASE(BC_MODNV) {
      struct IrNode* bin;
      DECODE_ARG();
      bin = IrNodeNewBinary(graph,IR_H_ADD,
          ss_top(stack,0),
          IrNodeNewConstNumber(graph,opr,proto),
          region);
      ss_replace(stack,bin);
      DISPATCH();
    }

    CASE(BC_MODVV) {
      struct IrNode* bin = IrNodeNewBinary(graph,IR_H_ADD,
          ss_top(stack,1),
          ss_top(stack,0),
          region);
      ss_pop(stack,2); ss_push(stack,bin);
      DISPATCH();
    }

    CASE(BC_POWNV) {
      struct IrNode* bin;
      DECODE_ARG();
      bin = IrNodeNewBinary(graph,IR_H_ADD,
          IrNodeNewConstNumber(graph,opr,proto),
          ss_top(stack,0),
          region);
      ss_replace(stack,bin);
      DISPATCH();
    }

    CASE(BC_POWVN) {
      struct IrNode* bin;
      DECODE_ARG();
      bin = IrNodeNewBinary(graph,IR_H_ADD,
          ss_top(stack,0),
          IrNodeNewConstNumber(graph,opr,proto),
          region);
      ss_replace(stack,bin);
      DISPATCH();
    }

    CASE(BC_POWVV) {
      struct IrNode* bin;
      bin = IrNodeNewBinary(graph,IR_H_ADD,
          ss_top(stack,1),
          ss_top(stack,0),
          region);
      ss_pop(stack,2); ss_push(stack,bin);
      DISPATCH();
    }

    CASE(BC_NEG) {
      struct IrNode* una = IrNodeNewUnary(graph,IR_H_NEG,
          ss_top(stack,0),
          region);
      ss_replace(stack,una);
      DISPATCH();
    }

    CASE(BC_NOT) {
      struct IrNode* una = IrNodeNewUnary(graph,IR_H_NOT,
          ss_top(stack,0),
          region);
      ss_replace(stack,una);
      DISPATCH();
    }

    CASE(BC_TEST) {
      struct IrNode* una = IrNodeNewUnary(graph,IR_H_TEST,
          ss_top(stack,0),
          region);
      ss_replace(stack,una);
      DISPATCH();
    }

    /* Constant loading instructions. For constant node we don't link
     * it to the region node since we don't need to. If this node's value
     * is not a dead value, then it will somehow be used in certain
     * expression. Then obviously it will linked the graph later. Otherwise
     * automatically it is DECed */
    CASE(BC_LOADN) {
      struct IrNode* n;
      DECODE_ARG();
      n = IrNodeNewConstNumber(graph,opr,proto);
      ss_push(stack,n);
      DISPATCH();
    }

#define DO(N) \
    CASE(BC_LOADN##N) {  \
      struct IrNode* n = IrNodeGetConstNumber(graph,N); \
      ss_push(stack,n); \
      DISPATCH(); \
    }

    /* BC_LOADN0 */
    DO(0)

    /* BC_LOADN1 */
    DO(1)

    /* BC_LOADN2 */
    DO(2)

    /* BC_LOADN3 */
    DO(3)

    /* BC_LOADN4 */
    DO(4)

    /* BC_LOADN5 */
    DO(5)

#undef DO /* DO */

#define DO(N) \
    CASE(BC_LOADNN##N) { \
      struct IrNode* n = IrNodeGetConstNumber(graph,-(N)); \
      ss_push(stack,n); \
      DISPATCH(); \
    }

    /* BC_LOADNN1 */
    DO(1)

    /* BC_LOADNN2 */
    DO(2)

    /* BC_LOADNN3 */
    DO(3)

    /* BC_LOADNN4 */
    DO(4)

    /* BC_LOADNN5 */
    DO(5)

#undef DO /* DO */

    CASE(BC_LOADS) {
      struct IrNode* n;
      DECODE_ARG();
      n = IrNodeNewConstString(graph,opr,proto);
      ss_push(stack,n);
      DISPATCH();
    }

    CASE(BC_LOADV) {
      DECODE_ARG();
      ss_push(stack,ss_bot(stack,opr));
      DISPATCH();
    }

    CASE(BC_LOADNULL) {
      ss_push(stack,IrNodeNewConstNull(graph));
      DISPATCH();
    }

    CASE(BC_LOADTRUE) {
      ss_push(stack,IrNodeNewConstTrue(graph));
      DISPATCH();
    }

    CASE(BC_LOADFALSE) {
      ss_push(stack,IrNodeNewConstFalse(graph));
      DISPATCH();
    }

    CASE(BC_MOVE) {
      /* This instruction will introduce *new* definition of a variable,
       * but we implicit rename all the varialbe by always pointed to the
       * latest definition */
      struct IrNode* tos = ss_top(stack,0);
      DECODE_ARG();
      ss_place(stack,opr,tos);
      ss_pop(stack,1);
      DISPATCH();
    }

    CASE(BC_MOVETRUE) {
      DECODE_ARG();
      ss_place(stack,opr,IrNodeNewConstTrue(graph));
      DISPATCH();
    }

    CASE(BC_MOVEFALSE) {
      DECODE_ARG();
      ss_place(stack,opr,IrNodeNewConstFalse(graph));
      DISPATCH();
    }

    CASE(BC_MOVENULL) {
      DECODE_ARG();
      ss_place(stack,opr,IrNodeNewConstNull(graph));
      DISPATCH();
    }

#define DO(N) \
    CASE(BC_MOVEN##N) { \
      DECODE_ARG(); \
      ss_place(stack,opr,IrNodeGetConstNumber(graph,N)); \
      DISPATCH(); \
    }

    /* BC_MOVEN0 */
    DO(0)

    /* BC_MOVEN1 */
    DO(1)

    /* BC_MOVEN2 */
    DO(2)

    /* BC_MOVEN3 */
    DO(3)

    /* BC_MOVEN4 */
    DO(4)

    /* BC_MOVEN5 */
    DO(5)

#undef DO /* DO */

#define DO(N) \
    CASE(BC_MOVENN##N) { \
      DECODE_ARG(); \
      ss_place(stack,opr,IrNodeGetConstNumber(graph,-(N))); \
      DISPATCH(); \
    }

    /* BC_MOVENN1 */
    DO(1)

    /* BC_MOVENN2 */
    DO(2)

    /* BC_MOVENN3 */
    DO(3)

    /* BC_MOVENN4 */
    DO(4)

    /* BC_MOVENN5 */
    DO(5)

    /* Comparison operators ============================== */
    CASE(BC_LTNV) {
      struct IrNode* bin;
      DECODE_ARG();
      bin = IrNodeNewBinary( graph , IR_H_LT ,
          IrNodeNewConstNumber(graph,opr,proto),
          ss_top(stack,0),
          region);
      ss_replace(stack,bin);
      DISPATCH();
    }

    CASE(BC_LTVN) {
      struct IrNode* bin;
      DECODE_ARG();
      bin = IrNodeNewBinary( graph , IR_H_LT ,
          ss_top(stack,0),
          IrNodeNewConstNumber(graph,opr,proto),
          region);
      ss_replace(stack,bin);
      DISPATCH();
    }

    CASE(BC_LTSV) {
      struct IrNode* bin;
      DECODE_ARG();
      bin = IrNodeNewBinary( graph , IR_H_LT ,
          IrNodeNewConstString(graph,opr,proto),
          ss_top(stack,0),
          region);
      ss_replace(stack,bin);
      DISPATCH();
    }

    CASE(BC_LTVS) {
      struct IrNode* bin;
      DECODE_ARG();
      bin = IrNodeNewBinary( graph , IR_H_LT ,
          ss_top(stack,0),
          IrNodeNewConstString(graph,opr,proto),
          region);
      ss_replace(stack,bin);
      DISPATCH();
    }

    CASE(BC_LTVV) {
      struct IrNode* bin;
      bin = IrNodeNewBinary( graph , IR_H_LT ,
          ss_top(stack,1),
          ss_top(stack,0),
          region);
      ss_pop(stack,2); ss_push(stack,bin);
      DISPATCH();
    }

    CASE(BC_LENV) {
      struct IrNode* bin;
      DECODE_ARG();
      bin = IrNodeNewBinary( graph , IR_H_LE ,
          IrNodeNewConstNumber(graph,opr,proto),
          ss_top(stack,0),
          region);
      ss_replace(stack,bin);
      DISPATCH();
    }

    CASE(BC_LEVN) {
      struct IrNode* bin;
      DECODE_ARG();
      bin = IrNodeNewBinary( graph , IR_H_LE ,
          ss_top(stack,0),
          IrNodeNewConstNumber(graph,opr,proto),
          region);
      ss_replace(stack,bin);
      DISPATCH();
    }

    CASE(BC_LESV) {
      struct IrNode* bin;
      DECODE_ARG();
      bin = IrNodeNewBinary( graph , IR_H_LE ,
          IrNodeNewConstString(graph,opr,proto),
          ss_top(stack,0),
          region);
      ss_replace(stack,bin);
      DISPATCH();
    }

    CASE(BC_LEVS) {
      struct IrNode* bin;
      DECODE_ARG();
      bin = IrNodeNewBinary( graph , IR_H_LE ,
          ss_top(stack,0),
          IrNodeNewConstString(graph,opr,proto),
          region);
      ss_replace(stack,bin);
      DISPATCH();
    }

    CASE(BC_LEVV) {
      struct IrNode* bin = IrNodeNewBinary( graph , IR_H_LE ,
          ss_top(stack,1),
          ss_top(stack,0),
          region);
      ss_pop(stack,2); ss_push(stack,bin);
      DISPATCH();
    }

    CASE(BC_GTNV) {
      struct IrNode* bin;
      DECODE_ARG();
      bin = IrNodeNewBinary( graph , IR_H_GT ,
          IrNodeNewConstNumber(graph,opr,proto),
          ss_top(stack,0),
          region);
      ss_replace(stack,bin);
      DISPATCH();
    }

    CASE(BC_GTVN) {
      struct IrNode* bin;
      DECODE_ARG();
      bin = IrNodeNewBinary( graph , IR_H_GT ,
          ss_top(stack,0),
          IrNodeNewConstNumber(graph,opr,proto),
          region);
      ss_replace(stack,bin);
      DISPATCH();
    }

    CASE(BC_GTSV) {
      struct IrNode* bin;
      DECODE_ARG();
      bin = IrNodeNewBinary( graph , IR_H_GT ,
          IrNodeNewConstString(graph,opr,proto),
          ss_top(stack,0),
          region);
      ss_replace(stack,bin);
      DISPATCH();
    }

    CASE(BC_GTVS) {
      struct IrNode* bin;
      DECODE_ARG();
      bin = IrNodeNewBinary( graph , IR_H_GT ,
          ss_top(stack,0),
          IrNodeNewConstString(graph,opr,proto),
          region);
      ss_replace(stack,bin);
      DISPATCH();
    }

    CASE(BC_GTVV) {
      struct IrNode* bin = IrNodeNewBinary( graph , IR_H_GT ,
          ss_top(stack,1),
          ss_top(stack,0),
          region);
      ss_pop(stack,2); ss_push(stack,bin);
      DISPATCH();
    }

    CASE(BC_GENV) {
      struct IrNode* bin;
      DECODE_ARG();
      bin = IrNodeNewBinary( graph , IR_H_GE ,
          IrNodeNewConstNumber(graph,opr,proto),
          ss_top(stack,0),
          region);
      ss_replace(stack,bin);
      DISPATCH();
    }

    CASE(BC_GEVN) {
      struct IrNode* bin;
      DECODE_ARG();
      bin = IrNodeNewBinary( graph , IR_H_GE ,
          ss_top(stack,0),
          IrNodeNewConstNumber(graph,opr,proto),
          region);
      ss_replace(stack,bin);
      DISPATCH();
    }

    CASE(BC_GESV) {
      struct IrNode* bin;
      DECODE_ARG();
      bin = IrNodeNewBinary( graph , IR_H_GE ,
          IrNodeNewConstString(graph,opr,proto),
          ss_top(stack,0),
          region);
      ss_replace(stack,bin);
      DISPATCH();
    }

    CASE(BC_GEVS) {
      struct IrNode* bin;
      DECODE_ARG();
      bin = IrNodeNewBinary( graph , IR_H_GE ,
          ss_top(stack,0),
          IrNodeNewConstString(graph,opr,proto),
          region);
      ss_replace(stack,bin);
      DISPATCH();
    }

    CASE(BC_GEVV) {
      struct IrNode* bin;
      DECODE_ARG();
      bin = IrNodeNewBinary( graph , IR_H_GE ,
          ss_top(stack,1),
          ss_top(stack,0),
          region);
      ss_pop(stack,2); ss_push(stack,bin);
      DISPATCH();
    }

    CASE(BC_EQNV) {
      struct IrNode* bin;
      DECODE_ARG();
      bin = IrNodeNewBinary( graph , IR_H_EQ ,
          IrNodeNewConstNumber(graph,opr,proto),
          ss_top(stack,0),
          region);
      ss_replace(stack,bin);
      DISPATCH();
    }

    CASE(BC_EQVN) {
      struct IrNode* bin;
      DECODE_ARG();
      bin = IrNodeNewBinary( graph , IR_H_EQ ,
          ss_top(stack,0),
          IrNodeNewConstNumber(graph,opr,proto),
          region);
      ss_replace(stack,bin);
      DISPATCH();
    }

    CASE(BC_EQSV) {
      struct IrNode* bin;
      DECODE_ARG();
      bin = IrNodeNewBinary( graph , IR_H_EQ ,
          IrNodeNewConstString(graph,opr,proto),
          ss_top(stack,0),
          region);
      ss_replace(stack,bin);
      DISPATCH();
    }

    CASE(BC_EQVNULL) {
      struct IrNode* bin;
      bin = IrNodeNewBinary( graph , IR_H_EQ ,
          ss_top(stack,0),
          IrNodeNewConstNull(graph),
          region);
      ss_replace(stack,bin);
      DISPATCH();
    }

    CASE(BC_EQNULLV) {
      struct IrNode* bin;
      bin = IrNodeNewBinary(graph, IR_H_EQ ,
          IrNodeNewConstNull(graph),
          ss_top(stack,0),
          region);
      ss_replace(stack,bin);
      DISPATCH();
    }

    CASE(BC_EQVV) {
      struct IrNode* bin = IrNodeNewBinary(graph,IR_H_EQ,
          ss_top(stack,1),
          ss_top(stack,0),
          region);
      ss_pop(stack,2);ss_push(stack,bin);
      DISPATCH();
    }

    CASE(BC_NENV) {
      struct IrNode* bin;
      DECODE_ARG();
      bin = IrNodeNewBinary(graph,IR_H_NE,
          IrNodeNewConstNumber(graph,opr,proto),
          ss_top(stack,0),
          region);
      ss_replace(stack,bin);
      DISPATCH();
    }

    CASE(BC_NEVN) {
      struct IrNode* bin;
      DECODE_ARG();
      bin = IrNodeNewBinary(graph,IR_H_NE,
          ss_top(stack,0),
          IrNodeNewConstNumber(graph,opr,proto),
          region);
      ss_replace(stack,bin);
      DISPATCH();
    }

    CASE(BC_NESV) {
      struct IrNode* bin;
      DECODE_ARG();
      bin = IrNodeNewBinary(graph,IR_H_NE,
          IrNodeNewConstString(graph,opr,proto),
          ss_top(stack,0),
          region);
      ss_replace(stack,bin);
      DISPATCH();
    }

    CASE(BC_NEVS) {
      struct IrNode* bin;
      DECODE_ARG();
      bin = IrNodeNewBinary(graph,IR_H_NE,
          ss_top(stack,0),
          IrNodeNewConstString(graph,opr,proto),
          region);
      ss_replace(stack,bin);
      DISPATCH();
    }

    CASE(BC_NENULLV) {
      struct IrNode* bin = IrNodeNewBinary(graph,IR_H_NE,
          IrNodeNewConstNull(graph),
          ss_top(stack,0),
          region);
      ss_replace(stack,bin);
      DISPATCH();
    }

    CASE(BC_NEVNULL) {
      struct IrNode* bin = IrNodeNewBinary(graph,IR_H_NE,
          ss_top(stack,0),
          IrNodeNewConstNull(graph),
          region);
      ss_replace(stack,bin);
      DISPATCH();
    }

    CASE(BC_NEVV) {
      struct IrNode* bin = IrNodeNewBinary(graph,IR_H_NE,
          ss_top(stack,1),
          ss_top(stack,0),
          region);
      ss_pop(stack,2);ss_push(stack,bin);
      DISPATCH();
    }
  }

  /* bump the code pointer */
  builder->code_pos = code_pos;
  return 0;
}

/* If-Else branch building. In our code base the if is always compiled
 * into byte code BC_IF.
 *
 * The if-else branch can have following 3 types:
 *
 * 1. A solo "if" , eg: if(condition) { body }
 * 2. A if-else pair, eg: if(condition) { body } else { body }
 * 3. A if-else if-else chain, eg: if(cond1) { body } else if(cond2) { body } else { body }
 *
 * In generated bytecode, all if else chain always starts with a BC_IF instruction.
 * And optionally we will see BC_ENDIF instruction to *jump* to the merge region.
 *
 *
 * We will only see BC_ENDIF if the current branch body doesn't naturally fall through
 * to merge body. Obviously a solo "if" branch will never generate BC_ENDIF since it
 * naturally fallthrough to merge body.
 * Since we know IF is a branch indication, we can use to compile if branch code.
 *
 * The transformed graph will be like this:
 *
 * 1. A solo if:
 *
 *  ----------
 *  |        |       -----------
 *  |  If    |-------| IfFalse |----|
 *  |        |       -----------    |
 *  ----------                      |
 *      |                           |
 *      |                       -----------------
 *      |                       | fall through  |
 *      |                       -----------------
 *   ----------                     |
 *  |         |                     |
 *  |  IfTrue | --------------------|
 *  |         |
 *  -----------
 *
 *  The IfFalse branch will be *trivial* which means no statement will be linked
 *  to that node.
 *
 *  We don't have a IfFalse region node in such case. The IfTrue region node
 *  directly merges to the fallthrough region node.
 *
 *  We could distinguish this situation by checking until we hit the if's false
 *  jump target, we didn't see a BC_ENDIF instruction. Then we are sure that it
 *  is this kind of graph.
 *
 *
 * 2. A if-else will be like the traditional binary branch. An if node splits
 * into 2 branch node , ie IfTrue and IfFalse , then merge into a region node.
 *
 * 3. A if-else if-else will be flatten into multiple layered IfElse branch.
 */

static struct IrNode* build_if_header( struct Sparrow* sparrow,
                                       struct BytecodeIrBuilder* builder ) {
  struct IrNode* ret = IrNodeNewIf(
        builder->graph,             /* graph */
        builder->region,            /* predecessor */
        ss_top(&(builder->stack),0) /* predicate/condition */
        );

  (void)sparrow;

  /* pop the predicate */
  ss_pop(&(builder->stack),0);
  return ret;
}

/* This function stop adding instruction at end or a ENDIF instruction is met,
 * whichever comes first */
static int build_if_block( struct Sparrow* sparrow,
                           struct BytecodeIrBuilder* builder ,
                           size_t end ) {
  const struct ObjProto* proto  = builder->proto;
  const struct CodeBuffer* code_buf = &(proto->code_buf);
  while( builder->code_pos < end ) {
    uint8_t op = code_buf->buf[builder->code_pos];
    if(op == BC_ENDIF) {
      return 0;
    } else {
      if(build_bytecode(sparrow,builder)) return -1;
    }
  }
  return 0;
}

static int build_if( struct Sparrow* sparrow , struct BytecodeIrBuilder* builder ) {
  const struct ObjProto* proto = builder->proto;
  const struct CodeBuffer* code_buf = &(proto->code_buf);
  struct IrGraph* graph = builder->graph;
  uint32_t merge_pos;
  uint32_t if_false_pos;
  struct IrNode* if_header;
  struct IrNode* if_true;
  struct IrNode* if_false;
  struct BytecodeIrBuilder true_builder;
  struct BytecodeIrBuilder false_builder;

  (void)sparrow;
  assert(code_buf->buf[builder->code_pos] == BC_IF);

  /* 0. Initialize the branch header */
  if_header = build_if_header(sparrow,builder);

  /* 1. Transform the IfTrue region or branch */
  {
    if_true = IrNodeNewIfTrue(graph,if_header);

    /* Get a new builder */
    builder_clone(&true_builder,builder,if_true);

    /* Decode the argument of the BC_IF instruction */
    if_false_pos = CodeBufferDecodeArg(code_buf,builder->code_pos+1);
    builder->code_pos +=4;

    /* Build the IfTrue block */
    if(!build_if_block(sparrow,&true_builder,if_false_pos)) return false;

    /* Update the if_true region node */
    if_true = true_builder.region;
  }

  /* 1. Transform the IfFalse region or branch */
  {
    if_false = IrNodeNewIfFalse(graph,if_header);

    /* Now check if we do have none-trivial IfFalse block */
    if( code_buf->buf[true_builder.code_pos] == BC_ENDIF ) {

      assert( if_false_pos > true_builder.code_pos );

      /* Generate a false_builder based on the existed builder */
      builder_clone(&false_builder,builder,if_false);

      /* Point to where the false branch begain */
      false_builder.code_pos = if_false_pos ;

      /* Now get where the merge region starts by get argument of instruction
       * BC_ENDIF . We must have a ENDIF here */
      merge_pos = CodeBufferDecodeArg(code_buf,true_builder.code_pos+1);

      /* Build the IfFalse block */
      if(build_if_block(sparrow,&false_builder,merge_pos)) return false;

      /* After this build_if_block call, we should always see :
       * assert opr == false_builder.code_pos since the branching one should
       * already be correctly nested inside of the IfFalse branch */
      assert(merge_pos == false_builder.code_pos);

      /* Update the if_false region node */
      if_false = false_builder.region;

      /* Place the PHI nodes on the stack */
      builder_place_phi( &true_builder, &false_builder );

      ss_move(&(builder->stack) , &(true_builder.stack));
      builder_destroy(&true_builder);
    } else {
      /* Place the PHI nodes on the stack */
      builder_place_phi( builder, &true_builder );
    }
  }

  /* 2. Generate the merge region and join the branch nodes */
  {
    struct IrNode* merge = builder_get_or_create_merged_region(builder,
        merge_pos,
        if_true,
        if_false);
    builder->region = merge;
    builder->code_pos = merge_pos;
  }

  return 0;
}












