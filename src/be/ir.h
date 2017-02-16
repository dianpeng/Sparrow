#ifndef IR_H_
#define IR_H_
#include "../util.h"
#include "../conf.h"

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
  /* FuncCall */ \
  X(H_CALL ,"h_call") \
  X(H_CALLI,"h_calli") \
  X(H_NEG,"h_neg") \
  X(H_NOT,"h_not") \
  X(H_TEST,"h_test") \
  /* Iterator */ \
  X(H_ITER_TEST,"h_iter_test") \
  X(H_ITER_NEW ,"h_iter_new")

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

/* IrLink is just a dynamic array that holds pointer to other inputs.
 * It will only be used when the local size is not enough */
struct IrLink {
  IrNode** ir_arr;
  uint32_t ir_size;
  uint32_t ir_cap;
};

/* IrLink size must be 2 pointer length */
SPARROW_STATIC_ASSERT(sizeof(struct IrLink) == sizeof(struct IrNode*)*2,\
    IrLinkMustBe2);

/* Node . A node just represents a node in an IR graph. It can have more
 * information based on the node tag . We will have fixed size of inline
 * storage of IrNode when a Node is allocated. But during the optimization
 * phase , we may want to store more information to a node in place, so
 * the information carried with IrNode should be able to grow but not impact
 * those node that *linked* to the IrNode.
 *
 * Let's see an IrNode layout.
 * Any IrNode will have a IrNode as header.
 * ------------
 * | irop :16 |
 * | kind : 2 |
 * | ....     |
 * ------------         ------------------------------
 * |          |         |                            |
 * | Data/Ptr | ------->|   Out Of Line Storage      |
 * |          |         |                            |
 * ------------         ------------------------------
 * The data set can also be *converted* to a pointer points to another
 * out of index storage. This is only needed when certain optimization want
 * to put *more* information into the area. The user could tell which kind
 * of storage it is using by checking local bitflag. If local is 1, then
 * the cast will treat memory is local ; otherwise the data set will be
 * treated as a pointer
 */
struct IrNode {
  uint16_t op;
  uint16_t local: 1;
  uint16_t immutable : 1;    /* Whether this node is immutable or not */
  uint16_t input_size : 2;   /* 1. 0 means no input
                                2. 1 means 1 input
                                3. 2 means 2 input
                                4. 3 means ooi storage
                              */
  uint16_t mark_state:2 ;    /* 2 bits mark state for traversal of the graph */
  /* Misc data field for helping us identify IrNode */
  uint32_t id : 24;          /* Monotonic ID */
  union {
    struct IrNode* local[2]; /* Assume a generally 2 local maximum */
    struct IrLink  ooi;      /* OutOfIndex storage */
  } input;
};

/* Size constarints for IrNode */
SPARROW_STATIC_ASSERT(sizeof(IrNode)==24,IrNodeSize);

/* Misc helper functions for IrNode */

/* Get IR's raw data */
static SPARROW_INLINE void* IrNodeGetRawData( struct IrNode* node ) {
  if(node->local) {
    return (char*)(node) + sizeof(*node);
  } else {
    return *(void**)((char*)(node) + sizeof(*node));
  }
}

#define IrCastData(IR,TYPE) ((TYPE)(IrNodeGetRawData(IR)))

static SPARROW_INLINE uint32_t IrNodeGetInputSize( struct IrNode* node ) {
  if(node->input_size == 3) {
    return input.ooi.ir_sz;
  } else {
    assert(node->input_size > 2);
    return node->input_size;
  }
}

void IrNodeAddInput( struct IrNode* node , struct IrNode* input_node );

static SPARROW_INLINE
struct IrNode* IrNodeIndexInput( struct IrNode* node , size_t index ) {
  if(index < 2) {
    assert( index < node->input_size );
    return node->input.local[index];
  } else {
    assert(node->input_size == 3);
    assert(node->input.ooi.ir_sz > index);
    return node->input.ooi.ir_arr[index];
  }
}

/* Resize an IrNode's data part to a out of line storage provided by allocator
 * with size *length*.
 * User is supposed to initialize data returned from IrNode */
void* IrNodeResize( struct IrNode* , struct IrGraph* , size_t );

/* General purpose IrNode creation */
struct IrNode* IrNodeNew( struct IrGraph* , size_t data_size , struct IrNode* );

struct IrNode* IrNodeNewControlFlow( struct IrGraph* , int op , struct IrNOde* );

struct IrNode* IrNodeNewBinary( struct IrGraph*, int op , struct IrNode* left ,
                                                          struct IrNode* right,
                                                          struct IrNode* region);

struct IrNode* IrNodeNewUnary ( struct IrGraph* , int op, struct IrNode* operand,
                                                          struct IrNode* region);

/* Function to get intrinsice number constant node. Number must be in range
 * [-5,5] */
struct IrNode* IrNodeGetConstNumber( struct IrGraph* , int32_t number );

struct IrNode* IrNodeNewConstNumber( struct IrGraph* , uint32_t index ,
                                                       const struct ObjProto*);
struct IrNode* IrNodeNewConstString( struct IrGraph* , uint32_t index ,
                                                       const struct ObjProto*);
struct IrNode* IrNodeNewConstBoolean(struct IrGraph* , int value );

#define IrNodeNewConstTrue(GRAPH) IrNodeNewConstBoolean(GRAPH,1)

#define IrNodeNewConstFalse(GRAPH) IrNodeNewConstBoolean(GRAPH,0)

struct IrNode* IrNodeNewConstNull( struct IrGraph* );

/* Control flow node */
struct IrNode* IrNodeNewIf( struct IrGraph* , struct IrNode* cond, struct IrNode* pred );
struct IrNode* IrNodeNewIfTrue(struct IrGraph*, struct IrNode* pred );
struct IrNode* IrNodeNewIfFalse(struct IrGraph*, struct IrNode* pred );
struct IrNode* IrNodeNewLoopHeader(struct IrGraph* , struct IrNode* cond );
struct IrNode* IrNodeNewLoop(struct IrGraph* , struct IrNode* pred );
struct IrNode* IrNodeNewLoopExit(struct IrGraph* , struct IrNode* pred );
struct IrNode* IrNodeNewMerge(struct IrGraph* , struct IrNode* if_true , struct IrNode* if_false );

/* Iterator */
struct IrNode* IrNodeNewIterTest(struct IrGraph*, struct IrNode* value, struct IrNode* region);
struct IrNode* IrNodeNewIterNew (struct IrGraph*, struct IrNode* value, struct IrNode* region);

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

  /* Tables for those node that is immutable */
  struct IrNode** num_table; /* Constant Number table, in our bytecode, we
                              * have specialized IR to load small integer,
                              * those integer will not be here, but instead
                              * they will be directly read it from the field */

  /* Special number table, hold number ranging at [-5,+5] */
  struct IrNode* spnum_table[BC_SPECIAL_NUMBER_SIZE];

  /* Other primitive node */
  struct IrNode* true_node;
  struct IrNode* false_node;
  struct IrNode* null_node;

  /* String IR node */
  struct IrNode** str_table; /* Constant string table */
};

/* Initialize a IrGraph object with regards to a specific protocol object */
void IrGraphInit( struct IrGraph* , const struct ObjProto* , struct Sparrow* );

#endif /* IR_H_ */
