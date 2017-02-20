#include "bc-ir-builder.h"
#include "ir.h"
#include "../bc.h"
#include "../object.h"

/* Forward =========================> */
struct LoopBuilder;
struct BytecodeIrBuilder;

static int build_bytecode( struct Sparrow* , struct BytecodeIrBuilder* );
static int build_if( struct Sparrow* , struct BytecodeIrBuilder* );
static int build_loop(struct Sparrow* , struct BytecodeIrBuilder* );
static int build_branch( struct Sparrow* , struct BytecodeIrBuilder* );

/* Stack ================================= */
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
  SPARROW_ASSERT(num <= ss->stk_size);
  ss->stk_size -= num;
}

static struct IrNode* ss_top( struct StackStats* ss, size_t num ) {
  SPARROW_ASSERT(num < ss->stk_size);
  return ss->stk_arr[ (ss->stk_size - 1) - num ];
}

static struct IrNode* ss_bot( struct StackStats* ss, size_t index ) {
  SPARROW_ASSERT(index < ss->stk_size);
  return ss->stk_arr[index];
}

static struct IrNode* ss_replace( struct StackStats* ss ,
    struct IrNode* node ) {
  SPARROW_ASSERT(ss->stk_size > 0);
  ss->stk_arr[ (ss->stk_size-1) ] = node;
  return node;
}

static void ss_place( struct StackStats* ss ,
    size_t index ,
    struct IrNode* node ) {
  SPARROW_ASSERT( index < ss->stk_size );
  ss->stk_arr[index] = node;
}

static void ss_clone( const struct StackStats* src , struct StackStats* dest ) {
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

struct LoopBuilder {
  struct IrNode* pre_if;      /* The very first IF node to test loop variant */
  struct IrNode* pre_if_true; /* The very first IF node's if_true branch */
  struct IrNode* pre_if_false;/* The very first IF node's if_false branch */
  struct IrNode* loop;        /* The loop body . Potentailly we could have many
                               * loop body due to placement of break and continue */
  struct IrNode* loop_exit;   /* Loop exit node */
  struct IrNode* loop_exit_true; /* The loop exit node's true node */
  struct IrNode* loop_exit_false;/* The loop exit node's false node */
};

struct BytecodeIrBuilder {
  const struct ObjProto* proto; /* Target protocol, same value as closure->proto */
  struct IrGraph* graph;        /* Targeted graph */
  struct StackStats stack;      /* Current stack */
  struct IrNode* region;        /* Current region node */
  size_t code_pos;              /* Current codei postion */

  /* An array of existed merged region. In our code, any time a branch
   * merged region will be created only if it is not created already.
   * Since our merge can have multiple phase, we need to put it into an
   * array to look up later on */
  struct MergedRegionList* mregion;

  /* Loop builder , if we are in the loop, then this field will be set */
  struct LoopBuilder* loop;
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
    struct IrNode* left_node = left->stack.stk_arr[i];
    struct IrNode* right_node= right->stack.stk_arr[i];
    if(left_node != right_node) {
      left->stack.stk_arr[i] = IrNodeNewPhi(left->graph,
                                            left->stack.stk_arr[i],
                                            right->stack.stk_arr[i]);
    }
  }
  builder_destroy(right);
}

static struct IrNode*
builder_get_or_create_merged_region( const struct BytecodeIrBuilder* builder ,
                                     uint32_t pos,
                                     struct IrNode* if_true,
                                     struct IrNode* if_false ) {
  size_t i;
  struct MergedRegion new_node;
  struct IrNode* node = NULL;
  SPARROW_ASSERT(builder->mregion);
  for( i = 0 ; i < builder->mregion->mregion_size ; ++i ) {
    if(pos == builder->mregion->mregion_arr[i].pos) {
      node = builder->mregion->mregion_arr[i].node;
      break;
    }
  }
  if(!node) {
    new_node.pos = pos;
    new_node.node = IrNodeNewMerge(builder->graph,if_true,if_false);
    DynArrPush(builder->mregion,mregion,new_node);
    node = new_node.node;
  } else {
    IrNodeAddOutput(builder->graph,node,if_true);
    IrNodeAddOutput(builder->graph,node,if_false);
  }

  return node;
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
 *  |  IfTrue | --------------------|
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
        ss_top(&(builder->stack),0),/* predicate/condition */
        builder->region             /* predecessor */
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
  SPARROW_ASSERT(code_buf->buf[builder->code_pos] == BC_IF);

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
    if(!build_if_block(sparrow,&true_builder,if_false_pos))
      return -1;

    /* Update the if_true region node */
    if_true = true_builder.region;
  }

  /* 2. Transform the IfFalse region or branch */
  {
    if_false = IrNodeNewIfFalse(graph,if_header);

    /* Now check if we do have none-trivial IfFalse block */
    if( code_buf->buf[true_builder.code_pos] == BC_ENDIF ) {

      SPARROW_ASSERT( if_false_pos > true_builder.code_pos );

      /* Generate a false_builder based on the existed builder */
      builder_clone(&false_builder,builder,if_false);

      /* Point to where the false branch begain */
      false_builder.code_pos = if_false_pos ;

      /* Now get where the merge region starts by get argument of instruction
       * BC_ENDIF . We must have a ENDIF here */
      merge_pos = CodeBufferDecodeArg(code_buf,true_builder.code_pos+1);

      /* Build the IfFalse block */
      if(build_if_block(sparrow,&false_builder,merge_pos))
        return -1;

      /* After this build_if_block call, we should always see :
       * SPARROW_ASSERT opr == false_builder.code_pos since the branching one should
       * already be correctly nested inside of the IfFalse branch */
      SPARROW_ASSERT(merge_pos == false_builder.code_pos);

      /* Update the if_false region node */
      if_false = false_builder.region;

      /* Place the PHI nodes on the stack */
      builder_place_phi( &true_builder, &false_builder );

      ss_move(&(builder->stack) , &(true_builder.stack));
      builder_destroy(&true_builder);
    } else {
      /* Place the PHI nodes on the stack */
      builder_place_phi( builder, &true_builder );

      /* Merge pos is the current position where the code halts */
      merge_pos = true_builder.code_pos;
    }
  }

  /* 3. Generate the merge region and join the branch nodes */
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

/* Loop IR graph building =============================================
 *
 * Our loop's bytecode is pretty much decroated so it is easy for
 * us to detect a loop. A loop is always starts with bytecode BC_FORPREP
 * which implicitly tells where to JUMP if the loop variant is out of
 * scope. The loop variant is the TOS element. A loop is closed by
 * BC_FORLOOP instruction which tells to jump back to the loop header.
 *
 * The loop will be compiled into a relative complicated graph:
 *
 *        --------------
 *    --- |  IfNode    |--------
 *    |   --------------       |
 *    |                        |
 *    |                        |
 * ----------              -------------
 * | IfTrue |              | IfFalse   |--------------|
 * ----------              -------------              |
 *    |                                               |
 *    |                                               |
 *    |                                               |
 *    |                                               |
 * --------------                                     |
 * | Loop       |<---------------|                    |
 * --------------                |                    |
 *    |                          |                ------------
 *    |                          |                | LoopMerge|
 *    |                          |                ------------
 * --------------           -------------             |
 * | LoopExit   |---------->| IfTrue    |             |
 * --------------           -------------             |
 *    |                                               |
 *    |                                               |
 *    |                                               |
 *    |             --------------                    |
 *    |------------>| IfFalse    |--------------------|
 *                  --------------
 *  This makes our life easier since we could easily correctly place the
 *  BREAK and CONTINUE jump to the correct node
 *
 */

static void setup_loop_builder( struct Sparrow* sparrow ,
    struct BytecodeIrBuilder* builder ) {
  (void)sparrow;
  struct IrNode* loop_variant;

  SPARROW_ASSERT(builder->loop);

  loop_variant = IrNodeNewIterTest(builder->graph,
                                   ss_top(&(builder->stack),0),
                                   builder->region);

  ss_replace(&(builder->stack),loop_variant);

  builder->loop->pre_if = IrNodeNewIf(
      builder->graph,
      loop_variant,
      builder->region);

  builder->loop->pre_if_true = IrNodeNewIfTrue(
      builder->graph,
      builder->loop->pre_if);

  builder->loop->pre_if_false= IrNodeNewIfFalse(
      builder->graph,
      builder->loop->pre_if);

  builder->loop->loop= IrNodeNewLoop(
      builder->graph,
      builder->loop->pre_if_true);

  /* NOTES: the loop exit node is not linked with graph. The link will be
   * done *after* everything is done */
  builder->loop->loop_exit = IrNodeNewLoopExit(
      builder->graph,
      loop_variant);

  builder->loop->loop_exit_true = IrNodeNewIfTrue(
      builder->graph,
      builder->loop->loop_exit);

  builder->loop->loop_exit_false = IrNodeNewIfFalse(
      builder->graph,
      builder->loop->loop_exit);

  /* backedge of the loop */
  IrNodeAddOutput(builder->graph,
                  builder->loop->loop_exit_true,
                  builder->loop->loop);
}

static int build_loop_body( struct Sparrow* sparrow ,
    struct BytecodeIrBuilder* builder ) {
  const struct ObjProto* proto = builder->proto;
  const struct CodeBuffer* code_buf = &(proto->code_buf);
  uint8_t op;

  (void)sparrow;

  SPARROW_ASSERT(builder->region == builder->loop->loop);

  do {
    op = code_buf->buf[builder->code_pos];
    if(op == BC_FORLOOP) {
      /* Leave the last BC_FORLOOP unconsumed */
      break;
    } else {
      if(build_bytecode(sparrow,builder)) return -1;
    }
  } while(1);
  return 0;
}

static int build_loop( struct Sparrow* sparrow ,
    struct BytecodeIrBuilder* builder ) {
  const struct ObjProto* proto = builder->proto;
  const struct CodeBuffer* code_buf = &(proto->code_buf);
  struct LoopBuilder loop;
  struct LoopBuilder* saved_loop_builder;
  struct BytecodeIrBuilder loop_body_builder;
  uint32_t merge_pos;

  /* Saved the current builder's loop when we are in nested loop */
  saved_loop_builder = builder->loop;
  builder->loop = &loop;

  SPARROW_ASSERT(code_buf->buf[builder->code_pos] == BC_FORPREP);

  /* Get the merge region position */
  merge_pos = CodeBufferDecodeArg(code_buf,builder->code_pos+1);
  builder->code_pos += 3;

  /* 0. Setup all the loop builder context */
  setup_loop_builder(sparrow,builder);

  /* 1.  Phase1 : Generate code for the loop body */
  {
    /* TOS is the iterator */
    builder_clone(&loop_body_builder,builder,loop.loop);

    /* Build the loop body until we finish the current loop */
    if(build_loop_body(sparrow,&loop_body_builder)) return -1;

    /* Link the region in current builder to the loop_exit */
    IrNodeAddOutput(builder->graph,builder->region,loop.loop_exit);
  }

  /* 2. Phase2 : Generate PHI and modify all the node that reference the PHI
   * node */
  {
    const size_t len = MIN(builder->stack.stk_size,
                           loop_body_builder.stack.stk_size);
    size_t i;

    for( i = 0 ; i < len ; ++i ) {
      struct IrNode* left = builder->stack.stk_arr[i];
      struct IrNode* right= loop_body_builder.stack.stk_arr[i];

      if(left != right) {
        /* If we reach here we know there is a *new* phi node is introduced
         * because in the loop , that stack slot is modified. After we add
         * this new PHI ,all the statement that reference to this PHI needs
         * to be modified as well.
         * We could know those used node by inspecting the def-use chain */
        struct IrNode* phi = IrNodeNewPhi(builder->graph,left,right);
        struct IrUse* start= IrNodeOutputBegin(right);

        while(start != IrNodeOutputEnd(right)) {
          struct IrNode* victim = start->node;
          if(victim != phi) {
            struct IrUse* use_place = IrNodeFindInput(victim,right);
            SPARROW_ASSERT(use_place);

            /* patched the used place to use new phi node */
            use_place->node = phi;

            /* add the victim node into phi's use chain */
            IrNodeAddOutput(builder->graph,phi,victim);

            /* Remove this use since we modify it */
            start = IrNodeRemoveOutput(right,start);
          } else {
            start = start->next; /* Not a potential victim */
          }
        }

        /* Place the PHI node in the stack slot */
        builder->stack.stk_arr[i] = phi;
      }
    }
  }

  /* 3. Add the merge node */
  {
    struct IrNode* merge = builder_get_or_create_merged_region(
        builder,
        merge_pos,
        loop.loop_exit_true,
        loop.loop_exit_false
        );
    builder->region = merge;

    /* SANITY CHECK ************************** */
    SPARROW_ASSERT( code_buf->buf[ loop_body_builder.code_pos ] == BC_FORLOOP );
    SPARROW_ASSERT( loop_body_builder.code_pos + 4 == merge_pos );

    builder->code_pos = merge_pos;
  }

  /* 4. Final cleanup */
  builder->loop = saved_loop_builder;
  builder_destroy(&loop_body_builder);

  return 0;
}

/* Branch ==========================================
 *
 * The branch here is different from IF bytecode. A IF bytecode is generated
 * from if-else chain. The branch bytecode ,ie BC_BRT and BC_BRF , is generated
 * from logic operation combination.
 * eg : var a = b || c && d;
 *
 * This will result in several BC_BRT/BC_BRF bytecode.
 *
 * Each BC_BRT/BC_BRF strictly forms a graph same as a solo "if".
 */
static struct IrNode* build_branch_header( struct Sparrow* sparrow ,
                                           struct BytecodeIrBuilder* builder ) {
  struct IrNode* predicate = IrNodeNewUnary(builder->graph,
                                            IR_H_TEST ,
                                            ss_top(&(builder->stack),0),
                                            builder->region);
  struct IrNode* header = IrNodeNewIf( builder->graph ,
                                       predicate,
                                       builder->region );
  (void)sparrow;

  /* Update the current TOS value , we are not supposed to pop this
   * value since the branch instruction optionally pop the value out
   * based on the result of the TOS evaluation */
  ss_push(&(builder->stack),predicate);
  return header;
}

static int build_branch_body( struct Sparrow* sparrow ,
                              struct BytecodeIrBuilder* builder ,
                              size_t end ) {
  (void)sparrow;
  while( builder->code_pos < end ) {
    if(build_bytecode(sparrow,builder)) return -1;
  }
  return 0;
}

static int build_branch( struct Sparrow* sparrow ,
    struct BytecodeIrBuilder* builder ) {
  const struct ObjProto* proto = builder->proto;
  const struct CodeBuffer* code_buf = &(proto->code_buf);
  struct IrNode* header = build_branch_header( sparrow , builder );
  struct IrNode* ft_branch;
  struct IrNode* jump_branch;
  uint8_t op;
  uint32_t jump_pos;

  op = code_buf->buf[builder->code_pos];
  SPARROW_ASSERT(op == BC_BRF || op == BC_BRT);
  jump_pos = CodeBufferDecodeArg(code_buf,builder->code_pos+1);

  /* 0. Setup fallthrough branch */
  {
    struct IrNode* saved_region;

    /* Here we don't need a new builder/environment simply because
     * the fallthrough branch of the BRX bytecode will never introduce
     * kill to existed slot. The code generated here is *in* expression,
     * it is a subexpression.
     * The fallthrough branch will never need to modify its stack stats
     * simply because the TOS is actually used in that branch */
    ft_branch = (op == BC_BRF)  ?
                 IrNodeNewIfFalse( builder->graph , header ) :
                 IrNodeNewIfTrue ( builder->graph , header );


    saved_region = builder->region;

    builder->region = ft_branch;

    if(build_branch_body(sparrow,builder,jump_pos)) return -1;

    builder->region = saved_region;
  }

  /* 1. Setup jump branch which is *empty* , trivial branch */
  {
    jump_branch = (op == BC_BRF) ?
                  IrNodeNewIfTrue( builder->graph , header  ) :
                  IrNodeNewIfFalse(builder->graph , header  );
  }

  /* 2. Setup merge branch */
  {
    struct IrNode* merge = builder_get_or_create_merged_region(builder,
        jump_pos,
        ft_branch,
        jump_branch);

    /* pop the TOS from the stack since we are done with the fallthrough
     * branch . The merged region don't need any TOS value */
    ss_pop(&(builder->stack),1);
    builder->region = merge;
    builder->code_pos = jump_pos;
  }

  return 0;
}

static int build_list( struct Sparrow* sparrow ,
                       struct BytecodeIrBuilder* builder ) {

  uint32_t arg;
  struct IrNode* list = IrNodeNewList( builder->graph );
  int size;
  int i;
  const struct CodeBuffer* code_buf = &(builder->proto->code_buf);
  uint8_t op = code_buf->buf[builder->code_pos];

  (void)sparrow;

  switch(op) {
    case BC_NEWL0: builder->code_pos++ ; size = 0; break;
    case BC_NEWL1: builder->code_pos++ ; size = 1; break;
    case BC_NEWL2: builder->code_pos++ ; size = 2; break;
    case BC_NEWL3: builder->code_pos++ ; size = 3; break;
    case BC_NEWL4: builder->code_pos++ ; size = 4; break;
    default:
      SPARROW_ASSERT(op == BC_NEWL);
      arg = CodeBufferDecodeArg(code_buf,builder->code_pos+1);
      size = (int)arg;
      builder->code_pos+=4;
      break;
  }
  list = IrNodeNewList( builder->graph );

  for( i = size - 1 ; i >= 0 ; --i ) {
    IrNodeListAddArgument(builder->graph,
                       list,
                       ss_top(&(builder->stack),i));
  }

  IrNodeListSetRegion(builder->graph,list,builder->region);

  ss_pop(&(builder->stack),size);
  ss_push(&(builder->stack),list);
  return 0;
}

static int build_map( struct Sparrow* sparrow ,
                      struct BytecodeIrBuilder* builder ) {
  uint8_t op;
  uint32_t arg;
  struct IrNode* map;
  int size;
  int i;
  const struct CodeBuffer* code_buf = &(builder->proto->code_buf);

  (void)sparrow;

  op = code_buf->buf[builder->code_pos];
  switch(op) {
    case BC_NEWM0 : builder->code_pos++ ; size = 0; break;
    case BC_NEWM1 : builder->code_pos++ ; size = 1; break;
    case BC_NEWM2 : builder->code_pos++ ; size = 2; break;
    case BC_NEWM3 : builder->code_pos++ ; size = 3; break;
    case BC_NEWM4 : builder->code_pos++ ; size = 4; break;
    default:
      SPARROW_ASSERT(op == BC_NEWM);
      arg = CodeBufferDecodeArg(code_buf,builder->code_pos+1);
      size = (int)arg;
      builder->code_pos+=4;
      break;
  }

  map = IrNodeNewMap( builder->graph );

  for( i = 2 * ( size - 1 ) ; i >= 0 ; i -= 2 ) {
    IrNodeMapAddArgument(builder->graph,
                      map,
                      ss_top(&(builder->stack),i+1),
                      ss_top(&(builder->stack),i));
  }

  IrNodeMapSetRegion(builder->graph,map,builder->region);

  ss_pop(&(builder->stack),size*2);
  ss_push(&(builder->stack),map);
  return 0;
}

static int build_call( struct Sparrow* sparrow ,
                       struct BytecodeIrBuilder* builder ) {
  uint8_t op;
  int i;
  int arg_count;
  struct IrNode* function;
  struct IrNode* call;
  const struct CodeBuffer* code_buf = &(builder->proto->code_buf);

  (void)sparrow;

  op = code_buf->buf[ builder->code_pos ];
  switch(op) {
    case BC_CALL0:
      arg_count = 0;
      function = ss_top(&(builder->stack),0);
      builder->code_pos++;
      break;
    case BC_CALL1:
      arg_count = 1;
      function = ss_top(&(builder->stack),1);
      builder->code_pos++;
      break;
    case BC_CALL2:
      arg_count = 2;
      function = ss_top(&(builder->stack),2);
      builder->code_pos++;
      break;
    case BC_CALL3:
      arg_count = 3;
      function = ss_top(&(builder->stack),3);
      builder->code_pos++;
      break;
    case BC_CALL4:
      arg_count = 4;
      function = ss_top(&(builder->stack),4);
      builder->code_pos++;
      break;
    default:
      SPARROW_ASSERT(op == BC_CALL);
      arg_count = (int)CodeBufferDecodeArg(code_buf,builder->code_pos+1);
      function = ss_top(&(builder->stack),arg_count);
      builder->code_pos+=4;
      break;
  }

  call = IrNodeNewCall( builder->graph , function , builder->region );

  for( i = arg_count - 1 ; i >= 0 ; --i ) {
    IrNodeCallAddArg(builder->graph, call, ss_top(&(builder->stack),i));
  }

  ss_pop(&(builder->stack),arg_count+1);
  return 0;
}

static int build_call_intrinsic( struct Sparrow* sparrow ,
                                 struct BytecodeIrBuilder* builder ,
                                 enum IntrinsicFunction func ) {
  int i;
  int arg_count;
  struct IrNode* call;
  const struct CodeBuffer* code_buf = &(builder->proto->code_buf);
  (void)sparrow;
  arg_count = (int)CodeBufferDecodeArg(code_buf,builder->code_pos+1);
  call = IrNodeNewCallIntrinsic(builder->graph,func,builder->region);
  for( i = arg_count -1 ; i >= 0 ; --i ) {
    IrNodeCallAddArg(builder->graph,call,ss_top(&(builder->stack),i));
  }

  ss_pop(&(builder->stack),arg_count);
  builder->code_pos += 4;
  return 0;
}

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

  (void)sparrow;

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

    CASE(BC_LOADCLS) {
      DECODE_ARG();
      ss_push(stack,IrNodeNewClosure(builder->graph,opr));
      DISPATCH();
    }

    CASE(BC_POP) {
      DECODE_ARG();
      ss_pop(stack,opr);
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

#undef DO /* DO */

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

    /* If branch */
    CASE(BC_IF) {
      return build_if(sparrow,builder);
    }

    /* Loop */
    CASE(BC_FORPREP) {
      return build_loop(sparrow,builder);
    }

    /* Loop Control */
    CASE(BC_BRK) {
      struct LoopBuilder* loop = builder->loop;
      SPARROW_ASSERT(loop);

      /* Link the current region to the loop exit node's if_false node */
      IrNodeAddOutput(builder->graph,builder->region,loop->loop_exit_false);

      /* Currently for simplicity we just add a new region node here and
       * this node will be DCE . We could really just mark anything that
       * is after a unconditional jump as dead while we construct our
       * ir graph. Maybe something to do in the future */
      builder->region = IrNodeNewRegion(builder->graph);
      DISPATCH();
    }

    CASE(BC_CONT) {
      struct LoopBuilder* loop = builder->loop;
      SPARROW_ASSERT(loop);

      /* Link the current region to the loop_exit node. */
      IrNodeAddOutput(builder->graph,builder->region,loop->loop_exit);

      /* Same as BREAK , add a region node after the continue and it will
       * be DCEed. */
      builder->region = IrNodeNewRegion(builder->graph);
      DISPATCH();
    }

    /* Iterator related */
    CASE(BC_IDREFK) {
      struct IrNode* n = IrNodeNewProjection(
          builder->graph,
          IrNodeNewIterDref(builder->graph, ss_top(stack,0), builder->region),
          0,
          builder->region);
      ss_push(stack,n);
      DISPATCH();
    }

    CASE(BC_IDREFKV) {
      struct IrNode* kv = IrNodeNewIterDref(builder->graph,
          ss_top(stack,0),
          builder->region);
      struct IrNode* key = IrNodeNewProjection(builder->graph,
          kv,
          0,
          builder->region);
      struct IrNode* val = IrNodeNewProjection(builder->graph,
          kv,
          1,
          builder->region);
      ss_push(stack,key);
      ss_push(stack,val);
      DISPATCH();
    }

    /* Branch */
    CASE(BC_BRT) {
      return build_branch(sparrow,builder);
    }

    CASE(BC_BRF) {
      return build_branch(sparrow,builder);
    }

    /* Primtive , mostly just List/Map allocation */
    CASE(BC_NEWL)
    CASE(BC_NEWL0)
    CASE(BC_NEWL1)
    CASE(BC_NEWL2)
    CASE(BC_NEWL3)
    CASE(BC_NEWL4) {
      return build_list(sparrow,builder);
    }

    CASE(BC_NEWM)
    CASE(BC_NEWM0)
    CASE(BC_NEWM1)
    CASE(BC_NEWM2)
    CASE(BC_NEWM3)
    CASE(BC_NEWM4) {
      return build_map(sparrow,builder);
    }

    /* Function call and return */
    CASE(BC_CALL)
    CASE(BC_CALL0)
    CASE(BC_CALL1)
    CASE(BC_CALL2)
    CASE(BC_CALL3)
    CASE(BC_CALL4) {
      return build_call(sparrow,builder);
    }

    /* Return */
    CASE(BC_RET) {
      IrNodeNewReturn(builder->graph,
                      IrNodeNewConstNull(builder->graph),
                      builder->region);
      builder->region = IrNodeNewRegion( builder->graph );
      DISPATCH();
    }

    CASE(BC_RETN) {
      DECODE_ARG();
      IrNodeNewReturn(builder->graph,
          IrNodeNewConstNumber(builder->graph,opr,proto),
          builder->region);
      builder->region = IrNodeNewRegion( builder->graph );
      DISPATCH();
    }

    CASE(BC_RETS) {
      DECODE_ARG();
      IrNodeNewReturn(builder->graph,
          IrNodeNewConstString(builder->graph,opr,proto),
          builder->region);
      builder->region = IrNodeNewRegion( builder->graph );
      DISPATCH();
    }

    CASE(BC_RETT) {
      IrNodeNewReturn(builder->graph,
          IrNodeNewConstTrue(builder->graph),
          builder->region);
      builder->region = IrNodeNewRegion( builder->graph );
      DISPATCH();
    }

    CASE(BC_RETF) {
      IrNodeNewReturn(builder->graph,
          IrNodeNewConstFalse(builder->graph),
          builder->region);
      builder->region = IrNodeNewRegion( builder->graph );
      DISPATCH();
    }

    CASE(BC_RETN0) {
      IrNodeNewReturn(builder->graph,
          IrNodeGetConstNumber(builder->graph,0),
          builder->region);
      builder->region = IrNodeNewRegion( builder->graph );
      DISPATCH();
    }

    CASE(BC_RETN1) {
      IrNodeNewReturn(builder->graph,
          IrNodeGetConstNumber(builder->graph,0),
          builder->region);
      builder->region = IrNodeNewRegion( builder->graph );
      DISPATCH();
    }

    CASE(BC_RETNN1) {
      IrNodeNewReturn(builder->graph,
          IrNodeGetConstNumber(builder->graph,-1),
          builder->region);
      builder->region = IrNodeNewRegion( builder->graph );
      DISPATCH();
    }

    CASE(BC_RETNULL) {
      IrNodeNewReturn(builder->graph,
          IrNodeNewConstNull(builder->graph),
          builder->region);
      builder->region = IrNodeNewRegion( builder->graph );
      DISPATCH();
    }

#define DO(TYPE) \
    CASE(BC_ICALL_##TYPE) { \
      return build_call_intrinsic( sparrow , builder , IFUNC_##TYPE ); \
    }

    /* BC_ICALL_TYPEOF */
    DO(TYPEOF)

    /* BC_ICALL_ISBOOLEAN */
    DO(ISBOOLEAN)

    /* BC_ICALL_ISSTRING */
    DO(ISSTRING)

    /* BC_ICALL_ISNUMBER */
    DO(ISNUMBER)

    /* BC_ICALL_ISNULL */
    DO(ISNULL)

    /* BC_ICALL_ISLIST */
    DO(ISLIST)

    /* BC_ICALL_ISMAP */
    DO(ISMAP)

    /* BC_ICALL_ISCLOSURE */
    DO(ISCLOSURE)

    /* BC_ICALL_TOSTRING */
    DO(TOSTRING)

    /* BC_ICALL_TONUMBER */
    DO(TONUMBER)

    /* BC_ICALL_TOBOOLEAN */
    DO(TOBOOLEAN)

    /* BC_ICALL_PRINT */
    DO(PRINT)

    /* BC_ICALL_ERROR */
    DO(ERROR)

    /* BC_ICALL_ASSERT */
    DO(ASSERT)

    /* BC_ICALL_IMPORT */
    DO(IMPORT)

    /* BC_ICALL_SIZE */
    DO(SIZE)

    /* BC_ICALL_RANGE */
    DO(RANGE)

    /* BC_ICALL_LOOP */
    DO(LOOP)

    /* BC_ICALL_RUNSTRING */
    DO(RUNSTRING)

    /* BC_ICALL_MIN */
    DO(MIN)

    /* BC_ICALL_MAX */
    DO(MAX)

    /* BC_ICALL_SORT */
    DO(SORT)

    /* BC_ICALL_GET */
    DO(GET)

    /* BC_ICALL_SET */
    DO(SET)

    /* BC_ICALL_EXIST */
    DO(EXIST)

    /* BC_ICALL_MSEC */
    DO(MSEC)

#undef DO /* DO */

    /* Attribute/Upvalue/Global indexing .
     * We have effect bit in the IrNode which is iused to indicate whether
     * there will be a side effect for these operations potentially. Any
     * function call will be treated as have side effect at very first.
     * And a global value indexing and attribute indexing into a map will
     * also be treated have side effect
     *
     * A list indexing will not be treated as have side effect since we don't
     * support meta operation on list type.
     * For any other indexing/attribute get on known type , it will be treated
     * as no effect.
     *
     * Pay attention, the upvalue reference is not *effected* , but upvalue
     * set is *effected* */
    CASE(BC_AGETS) {
      struct IrNode* comp;
      struct IrNode* aget;
      DECODE_ARG();
      comp = IrNodeNewConstString( builder->graph , opr, proto );
      aget = IrNodeNewAGet( builder->graph , ss_top(stack,0) , comp , region );
      ss_replace(stack,aget);
      DISPATCH();
    }

    CASE(BC_AGETI) {
      struct IrNode* ageti;
      DECODE_ARG();
      ageti = IrNodeNewAGetIntrinsic( builder->graph , ss_top(stack,0),
                                      (enum IntrinsicAttribute)(opr),
                                      region );
      ss_replace(stack,ageti);
      DISPATCH();
    }

    CASE(BC_AGETN) {
      struct IrNode* comp;
      struct IrNode* aget;
      DECODE_ARG();
      comp = IrNodeNewConstNumber( builder->graph , opr , proto );
      aget = IrNodeNewAGet( builder->graph , ss_top(stack,0) , comp , region );
      DISPATCH();
    }

    CASE(BC_AGET) {
      struct IrNode* tos = ss_top(stack,1);
      struct IrNode* comp= ss_top(stack,0);
      struct IrNode* aget = IrNodeNewAGet( builder->graph ,
                                           tos,
                                           comp,
                                           region);
      ss_pop(stack,2); ss_push(stack,aget);
      DISPATCH();
    }

    CASE(BC_ASETN) {
      struct IrNode* comp;
      DECODE_ARG();
      comp = IrNodeNewConstNumber(builder->graph,opr,proto);
      IrNodeNewASet( builder->graph , ss_top(stack,1),
                                      comp,
                                      ss_top(stack,0),
                                      region);
      ss_pop(stack,2);
      DISPATCH();
    }

    CASE(BC_ASETS) {
      struct IrNode* comp;
      DECODE_ARG();
      comp = IrNodeNewConstString(builder->graph,opr,proto);
      IrNodeNewASet( builder->graph , ss_top(stack,1),
                                      comp,
                                      ss_top(stack,0),
                                      region);
      ss_pop(stack,2);
      DISPATCH();
    }

    CASE(BC_ASETI) {
      DECODE_ARG();
      IrNodeNewASetIntrinsic(builder->graph,ss_top(stack,1),
                                            (enum IntrinsicAttribute)(opr),
                                            ss_top(stack,0),
                                            region);
      ss_pop(stack,2);
      DISPATCH();
    }

    CASE(BC_ASET) {
      IrNodeNewASet(builder->graph,ss_top(stack,2), ss_top(stack,1),
                                                    ss_top(stack,0),
                                                    region);
      ss_pop(stack,3);
      DISPATCH();
    }

    CASE(BC_UGET) {
      struct IrNode* uget;
      DECODE_ARG();
      uget = IrNodeNewUGet( builder->graph , opr , region );
      ss_push(stack,uget);
      DISPATCH();
    }

    CASE(BC_USET) {
      DECODE_ARG();
      IrNodeNewUSet( builder->graph , opr , ss_top(stack,0) , region );
      ss_pop(stack,1);
      DISPATCH();
    }

    CASE(BC_USETTRUE) {
      DECODE_ARG();
      IrNodeNewUSet( builder->graph , opr , IrNodeNewConstTrue(builder->graph),
                                            region );
      DISPATCH();
    }

    CASE(BC_USETFALSE) {
      DECODE_ARG();
      IrNodeNewUSet( builder->graph , opr , IrNodeNewConstFalse(builder->graph),
                                            region );
      DISPATCH();
    }

    CASE(BC_USETNULL) {
      DECODE_ARG();
      IrNodeNewUSet( builder->graph , opr , IrNodeNewConstNull(builder->graph),
                                            region );
      DISPATCH();
    }

    CASE(BC_GGET) {
      DECODE_ARG();
      ss_push(stack,IrNodeNewGGet(builder->graph,IrNodeNewConstString(builder->graph,
                                                         opr,
                                                         proto),region));
      DISPATCH();
    }

    CASE(BC_GSET) {
      DECODE_ARG();
      IrNodeNewGSet(builder->graph,IrNodeNewConstString(builder->graph,
                                                        opr,
                                                        proto),
                                                        ss_top(stack,0),
                                                        region);
      ss_pop(stack,1);
      DISPATCH();
    }

    CASE(BC_GSETTRUE) {
      DECODE_ARG();
      IrNodeNewGSet(builder->graph,IrNodeNewConstString(builder->graph,opr,proto),
                                   IrNodeNewConstTrue(builder->graph),
                                   region);
      DISPATCH();
    }

    CASE(BC_GSETFALSE) {
      DECODE_ARG();
      IrNodeNewGSet(builder->graph,IrNodeNewConstString(builder->graph,opr,proto),
                                   IrNodeNewConstFalse(builder->graph),
                                   region);
      DISPATCH();
    }

    CASE(BC_GSETNULL) {
      DECODE_ARG();
      IrNodeNewGSet(builder->graph,IrNodeNewConstString(builder->graph,opr,proto),
                                   IrNodeNewConstNull(builder->graph),
                                   region);
      DISPATCH();
    }

    default:
      SPARROW_UNREACHABLE();
      DISPATCH();
  }
  /* bump the code pointer */
  builder->code_pos = code_pos;
  return 0;
}
