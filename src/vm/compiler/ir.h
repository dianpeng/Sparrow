#ifndef IR_H_
#define IR_H_
#include "../../conf.h"
#include "../util.h"
#include "../bc.h"

struct IrNode;
struct IrLink;
struct IrGraph;

/* IR ---> Intermediate Representation
 * IR in Sparrow is model with Cliff Click's doctor thesis, ie sea-of-nodes.
 * I personally like the simplicity of this IR, especially there's no 2 layer
 * architecutures like in traditional CFG. Same as most of sea-of-nodes
 * mplementation, we have all layer of IR ( HIR , MIR , LIR ) in one single
 * graph. And the graph is enough for all stages of optimization.
 *
 * The IR is basically a SSA form and it is derived directly from bytecode.
 * We will have a bytecode ir builder , and also a high-level assembly like
 * ir builder which helps us to write stub assembly , similar with V8's interpreter.
 *
 * All the IR is composed of node and edge. Node carries all the information and
 * have different type/category. Edge is *unlabled* but impose constraints. It
 * is easy to generate 1) control flow graph and also 2) visit expression just by
 * using this single IR.
 *
 * Additionally information regarding IR is stored parallel in other bundled
 * data structure. Like dominator tree and liveless map are stored in an object
 * called JitContext. This object is per module based and serve as the central
 * data structure for any methods are been jitting .
 *
 *
 * IR Lowering is inpace. The optimization algorithm will just modify the IR
 * in place . Since IR just a loosely connected graph, modify the graph is just
 * changing some pointers.
 *
 * For debug purpose, a IR serializer will be provided to serialize the IR into
 * dot format, then user is able to use graphviz to visualize it.
 *
 * IR doesn't have a fixed format but could have *multiple* information based on
 * type of the IR. All IR node has a common header IrNode , inside of the IrNode
 * we have a type field to help user *cast* the IrNode pointer to correct sub
 * type. It is really just traditional way to do single inheritance in C.
 */

#define CONTROL_IR_LIST(X) \
  X(CTL_START,"ctl_start") \
  /* loop header region */ \
  X(CTL_LOOP_HEADER,"ctl_loop_header") \
  X(CTL_LOOP_BODY,"ctl_loop_body") \
  X(CTL_LOOP_EXIT,"ctl_loop_exit") \
  /* place holder for a bunch of value */ \
  X(CTL_REGION,"ctl_region") \
  /* branch node */ \
  X(CTL_IF,"ctl_if") \
  X(CTL_TRUE,"ctl_true") \
  X(CTL_FALSE,"ctl_false") \
  X(CTL_RET,"ctl_ret") \
  X(CTL_END,"ctl_end")


/* Shared IR list . IR here is shared throughout the optimization stage */
#define SHARED_IR_LIST(X) \
  X(CTL_PHI,"c_phi")

/* Constant IR list. */
#define CONSTANT_IR_LIST(X) \
  X(CONST_INT32,"const_int32") \
  X(CONST_INT64,"const_int64") \
  X(CONST_REAL32,"const_real32") \
  X(CONST_REAL64,"const_real64") \
  X(CONST_STRING,"const_string") \
  X(CONST_BOOLEAN,"const_boolean") \
  X(CONST_NULL,"const_null")

/* Primitive IR list */
#define PRIMITIVE_IR_LIST(X) \
  X(PRIMITIVE_LIST,"primitive_list") \
  X(PRIMITIVE_MAP,"primitive_map")

/* High level IR , the IR is mostly a one-2-one mapping of the bytecode and with
 * some simplicity. It is the IR that is used to build IR graph */
#define HIGH_IROP_LIST(X) \
  /* Numeric Operations */ \
  X(H_ADD,"h_add") \
  X(H_SUB,"h_sub") \
  X(H_MUL,"h_mul") \
  X(H_DIV,"h_div") \
  X(H_POW,"h_pow") \
  X(H_MOD,"h_mod") \
  /* Comparison */ \
  X(H_LT,"h_lt") \
  X(H_LE,"h_le") \
  X(H_GT,"h_gt") \
  X(H_GE,"h_ge") \
  X(H_EQ,"h_eq") \
  X(H_NE,"h_he") \
  /* Upvalue */ \
  X(H_UGET,"h_uget") \
  X(H_USET,"h_uset") \
  /* Property acesss without information */ \
  X(H_PROP,"h_prop") \
  X(H_PROPI,"h_propi") \
  /* Global */ \
  X(H_GGET,"h_gget") \
  X(H_GSET,"h_gset") \
  /* Loop */ \
  X(H_ITRKV,"h_itrkv") \
  X(H_ITRK ,"h_itrk") \
  X(H_ITRN ,"h_itrn") \
  /* Unary */ \
  X(H_NEG,"h_neg") \
  X(H_NOT,"h_not") \
  X(H_TEST,"h_test") \
  /* Iterator */ \
  X(H_ITER_TEST,"h_iter_test") \
  X(H_ITER_NEW ,"h_iter_new") \
  X(H_ITER_DREF_KEY,"h_iter_dref_key") \
  X(H_ITER_DREF_VAL,"h_iter_dref_val" ) \
  /* Function */ \
  X(H_CALL,"h_call") \
  X(H_CALL_INTRINSIC,"h_call_intrinsic")

#define ALL_IRS(X) \
  CONTROL_IR_LIST(X) \
  SHARED_IR_LIST(X) \
  CONSTANT_IR_LIST(X) \
  PRIMITIVE_IR_LIST(X) \
  HIGH_IROP_LIST(X)

/* Define each IR list */
#define X(A,B) IR_##A
enum {
  CONTROL_IR__START = 0xff,
  CONTROL_IR_LIST(X)
  CONTROL_IR__END
};

enum {
  SHARED_IR_LIST__START = 0x01ff,
  SHARED_IR_LIST(X)
  SHARED_IR__END
};

enum {
  CONSTANT_IR_LIST__START = 0x02ff,
  CONSTANT_IR_LIST(X)
  CONTANT_IR__END
};

enum {
  PRIMITIVE_IR_LIST__START = 0x03ff,
  PRIMITIVE_IR_LIST(X)
  PRIMITIVE_IR__END
};

enum {
  HIGH_IROP_LIST__START = 0x04ff,
  HIGH_IROP_LIST(X)
  HIGH_IROP_LIST__END
};
#undef X /* X */

enum IrKind {
  IRKIND_CONTROL,
  IRKIND_SHARED,
  IRKIND_CONSTANT,
  IRKIND_PRIMITIVE,
  IRKIND_HIGH_IROP
};

/* Function to check whether a ir opcode belongs to a certain type */
static SPARROW_INLINE int IrGetKindCode( int opcode ) {
  return ((opcode & 0xff00)>>8);
}

static SPARROW_INLINE int IrIsControl( int opcode ) {
  return IrGetKindCode(opcode) == IRKIND_CONTROL;
}

static SPARROW_INLINE int IrIsStatment(int opcode ) {
  return !IrIsControl(opcode);
}

static SPARROW_INLINE int IrIsShared ( int opcode ) {
  return IrGetKindCode(opcode) == IRKIND_SHARED;
}

static SPARROW_INLINE int IrIsConstant(int opcode ) {
  return IrGetKindCode(opcode) == IRKIND_CONSTANT;
}

static SPARROW_INLINE int IrIsPrimitive(int opcode) {
  return IrGetKindCode(opcode) == IRKIND_PRIMITIVE;
}

static SPARROW_STATIC_ASSERT int IrIsHighIR(int opcode) {
  return IrGetKindCode(opcode) == IRKIND_HIGH_IROP;
}

/* IrUse. A use structure represents the def-use/use-def chain element. Since
 * the chain will be modified during IrGraph construction. We use a double
 * linked list here to ease the pain for removing a certain use from the IR */
struct IrUse {
  struct IrUse* prev;
  struct IrUse* next;
  struct IrNode* node;
};

/* Node . A node just represents a node in an IR graph. It is the single element
 * that is used to build IR graph. Node are labeled with OPCODE which indicates
 * the semantic of the node itself. Each node has builtin storage that indicate
 * its 1) use-def 2) def-use chains. Apart from that any other information is
 * supposed to be stored inside of a out-of-index array by unique id of the node.
 * A node will get its unique ID when it is created and it is a handy identity
 * for user to use node to index to certain side information .
 *
 *
 * For a statment IrNode , (opcode != CONTROL_FLOW) , the IrNode's input and
 * output will always pointed to use-def and def-use chain. For a control flow
 * node. The Input and Output will be used as control follow sequences for
 * forward and backward purpose.
 *
 * For expression that is related to a certain region node. It is represented
 * by its private data structure which *only* used in ControlFlow node */

struct IrNode {
  uint16_t op;
  uint16_t immutable : 1;    /* Whether this node is immutable or not */
  uint16_t effect : 1;       /* This is a coarsed side effect analyzing while
                              * the bytecode is translated into the IrGraph.
                              * This will impact whether the node will be added
                              * into the use chain of its belonging control flow
                              * node */
  uint16_t mark_state:2 ;    /* 2 bits mark state for traversal of the graph */

  uint32_t id;               /* Ir Unique ID . User could use it to index to
                              * a side array for associating information that
                              * is local to a certain optimization pass */

  /* The following use-def and def-use chain's memory is owned by the Arena
   * allocator inside of the IrGrpah */

  /* Input chain or use def chain */
  struct IrUse input_tail;
  uint32_t input_size;

  /* Output chain or def use chain */
  struct IrUse output_tail;
  uint32_t output_size;
};

void IrNodeAddInput (struct IrGraph* , struct IrNode* node , struct IrNode* input_node );
void IrNodeAddOutput(struct IrGraph* , struct IrNode* node , struct IrNode* output_node);

struct IrUse* IrNodeFindInput(struct IrNode* , struct IrNode* );
struct IrUse* IrNodeFindOutput(struct IrNode*, struct IrNode* );

struct IrUse* IrNodeRemoveInput(struct IrNode*,struct IrUse*);
struct IrUse* IrNodeRemoveOutput(struct IrNode*,struct IrUse*);

void IrNodeAddControlFlow(struct IrGraph* , struct IrNode* pred , struct IrNode* succ );

/* Helper macro for iterating the def-use/use-def chains */
#define IrNodeInputEnd(IRNODE) (&((IRNODE)->input_tail))
#define IrNodeOutputEnd(IRNODE) (&((IRNODE)->output_tail))
#define IrNodeInputBegin(IRNODE) (((IRNODE)->input_tail).next)
#define IrNodeOutputBegin(IRNODE) (((IRNODE)->output_tail).next)

/* General purpose IrNode creation */
struct IrNode* IrNodeNewBinary( struct IrGraph*, int op , struct IrNode* left ,
                                                          struct IrNode* right,
                                                          struct IrNode* region);

struct IrNode* IrNodeNewUnary ( struct IrGraph* , int op, struct IrNode* operand,
                                                          struct IrNode* region);

/* Function to get intrinsice number constant node. Number must be in range
 * [-5,5] */
struct IrNode* IrNodeGetConstNumber( struct IrGraph* , int32_t number );
struct IrNode* IrNodeNewConstNumber( struct IrGraph* , uint32_t index , const struct ObjProto* );
struct IrNode* IrNodeNewConstString( struct IrGraph* , uint32_t index , const struct ObjProto* );
struct IrNode* IrNodeNewConstBoolean(struct IrGraph* , int value );
#define IrNodeNewConstTrue(GRAPH) IrNodeNewConstBoolean(GRAPH,1)
#define IrNodeNewConstFalse(GRAPH) IrNodeNewConstBoolean(GRAPH,0)
struct IrNode* IrNodeNewConstNull( struct IrGraph* );

/* Primitive */
struct IrNode* IrNodeNewList( struct IrGraph* );

/* For adding input into List/Map , do not use IrNodeAddInput/Output, but use
 * following functions which takes care of the effect */
void IrNodeListAddInput( struct IrGraph* ,
                         struct IrNode*  ,
                         struct IrNode*  ,
                         struct IrNode* );

struct IrNode* IrNodeNewMap( struct IrGraph* );

void IrNodeMapAddInput( struct IrGraph* ,
                        struct IrNode* map ,
                        struct IrNode* key ,
                        struct IrNode* val ,
                        struct IrNode* region );

/* Function call */
struct IrNode* IrNodeNewCall( struct IrGraph* , struct IrNode* function ,
                                                struct IrNode* region );

struct IrNode* IrNodeNewCallIntrinsic( struct IrGraph* , enum IntrinsicFunction func ,
                                                         struct IrNode* region );

/* Control flow node.
 *
 * Each control flow node can have *unbounded* Input node but fixed number of
 * output node. EG: a binary branch will have to force you to add a If node
 * since it is the only node that is able have 2 branches/output */
struct IrNode* IrNodeNewRegion(struct IrGraph*);
struct IrNode* IrNodeNewIf( struct IrGraph* , struct IrNode* cond, struct IrNode* pred );
struct IrNode* IrNodeNewIfTrue(struct IrGraph*, struct IrNode* pred );
struct IrNode* IrNodeNewIfFalse(struct IrGraph*, struct IrNode* pred );
struct IrNode* IrNodeNewLoop(struct IrGraph* , struct IrNode* pred );
struct IrNode* IrNodeNewLoopExit(struct IrGraph* , struct IrNode* pred );
/* This function will link the Return node to end node in IrGraph automatically */
struct IrNode* IrNodeNewReturn(struct IrGraph* ,
                               struct IrNode* value ,
                               struct IrNode* pred);

size_t IrNodeControlFlowUseCount( struct IrNode* );
#define IrNodeControlFlowUseEmpty(IRNODE) ((IrNodeControlFlowUseCount(IRNODE)) ==0)
struct IrNode* IrNodeControFlowIndexUse( struct IrNode* , size_t index );
void IrNodeControlFlowAddUse ( struct IrGraph*, struct IrNode* , struct IrNode* );

/* Iterator */
struct IrNode* IrNodeNewIterTest(struct IrGraph*, struct IrNode* value, struct IrNode* region);
struct IrNode* IrNodeNewIterNew (struct IrGraph*, struct IrNode* value, struct IrNode* region);
struct IrNode* IrNodeNewIterDrefKey(struct IrGraph* , struct IrNode* , struct IrNode* );
struct IrNode* IrNodeNewIterDrefVal(struct IrGraph* , struct IrNode* , struct IrNode* );

/* Misc */
struct IrNode* IrNodeNewPhi( struct IrGraph* , struct IrNode* left , struct IrNode* right );

/* IR graph ================================================================
 * IR graph is really just a high level name of a bundle that has lots of
 * information stored inside of it. It is just a piece of central data
 * structure to store information like , liveness analyze result , dominator
 * tree result or other temporary analyze result. Also user is able to find
 * the Start and End node inside of the IR graph as well.
 */
struct IrGraph {
  struct Sparrow* sparrow;     /* Sparrow instance */
  struct ArenaAllocator arena; /* Allocator for IR nodes */
  int node_id;                 /* Current node ID */
  struct IrNode* start;        /* Start of the Node */
  struct IrNode* end  ;        /* End of the Node */
};

/* Initialize a IrGraph object with regards to a specific protocol object */
void IrGraphInit( struct IrGraph* , const struct ObjProto* , struct Sparrow* );

#endif /* IR_H_ */
