#include <compiler/ir.h>
#include <shared/debug.h>
#include <math.h>

/* Linked list related stuff */
#define link_node( GRAPH , IRNODE , ELEMENT , TYPE ) \
  do { \
    struct IrUse* use_node = ArenaAllocatorAlloc(((GRAPH)->arena),sizeof(*use_node)); \
    struct IrUse* tail_node  = (struct IrUse*)(&(IRNODE)->TYPE##_tail); \
    use_node->node = (ELEMENT); \
    use_node->prev = tail_node->prev; \
    tail_node->prev->next = use_node; \
    tail_node->prev = use_node; \
    use_node->next = tail_node; \
    ++((IRNODE)->TYPE##_size); \
  } while(0)

#define remove_node(IRNODE,USE,TYPE) \
  do { \
    (USE)->prev->next = (USE)->next; \
    (USE)->next->prev = (USE)->prev; \
    --((IRNODE)->TYPE##_size); \
  } while(0)

#define init_node(IRNODE,TYPE) \
  do { \
    struct IrUse* tail = (struct IrUse*)(&((IRNODE)->TYPE##_tail)); \
    tail->next = tail; \
    tail->prev = tail; \
    (IRNODE)->TYPE##_size = 0; \
  } while(0)

#define IRNODE_GET_DATA(IRNODE,TYPE) (TYPE)IrNodeGetData(IRNODE)

struct opcode_name {
  int op;
  const char* name;
};

static struct opcode_name OPCODE_NAME[] = {
#define __(A,B) { IR_##A , B },
  ALL_IRS(__)
  { -1 , NULL }
#undef __ /* __ */
};

const char* IrGetName( int opcode ) {
#define __(A,B) case IR_##A : return B ;
  switch(opcode) {
    ALL_IRS(__)
    default: return NULL;
#undef __ /* __ */
  }
}

#define IsDeadRegion(REGION) ((REGION)->op == IR_CTL_JUMP)

static void
add_region( struct IrGraph* graph , struct IrNode* region , struct IrNode* node ) {
  SPARROW_ASSERT(IrIsControl(region->op));
  SPARROW_ASSERT(node->bounded_node == NULL);
  link_node(graph,region,node,input);
  node->bounded_node = region;
}

static void
remove_region( struct IrNode* node ) {
  if(node->bounded_node) {
    SPARROW_ASSERT(IrIsControl(node->bounded_node->op));
    SPARROW_ASSERT(IrNodeHasEffect(node));
    {
      struct IrUse* use = IrNodeFindInput(node->bounded_node,node);
      SPARROW_ASSERT(use);
      remove_node(node->bounded_node,use,input);
      node->bounded_node = NULL;
    }
  }
}

static void add_def_use( struct IrGraph* graph ,
                         struct IrNode* use ,
                         struct IrNode* def ) {
  SPARROW_ASSERT( use->input_max < 0 || use->input_size < use->input_max );
  /* Add the def to Use-Def chain , which is the input chain */
  link_node(graph,use,def,input);
  /* Add the use to Def-Use chain , which is the output chain */
  link_node(graph,def,use,output);
}

void IrNodeAddInput( struct IrGraph* graph , struct IrNode* node , struct IrNode* target ) {
  if( IrNodeFindInput(node,target) == NULL ) {
    SPARROW_ASSERT(node->input_max <0 || node->input_size < node->input_max);
    SPARROW_ASSERT(node != target);
    link_node(graph,node,target,input);
  }
}

void IrNodeAddOutput(struct IrGraph* graph , struct IrNode* node , struct IrNode* target ) {
  if( IrNodeFindOutput(node,target) == NULL ) {
    SPARROW_ASSERT(node->output_max <0 || node->output_size < node->output_max);
    SPARROW_ASSERT(node != target);
    link_node(graph,node,target,output);
  }
}

struct IrUse* IrNodeFindInput(struct IrNode* node , struct IrNode* target) {
  struct IrUse* begin = IrNodeInputBegin(node);
  for( ; begin  != IrNodeInputEnd(node) ; begin = begin->next ) {
    if(target == begin->node) return begin;
  }
  return NULL;
}

struct IrUse* IrNodeFindOutput(struct IrNode* node , struct IrNode* target) {
  struct IrUse* begin = IrNodeOutputBegin(node);
  for( ; begin != IrNodeOutputEnd(node) ; begin = begin->next ) {
    if(target == begin->node) return begin;
  }
  return NULL;
}

struct IrUse* IrNodeRemoveInput(struct IrNode* node , struct IrUse* use) {
  struct IrUse* ret = use->next;
  SPARROW_ASSERT( IrNodeFindInput(node,use->node) == use );
  remove_node(node,use,input);
  return ret;
}

struct IrUse* IrNodeRemoveOutput(struct IrNode* node , struct IrUse* use ) {
  struct IrUse* ret = use->next;
  SPARROW_ASSERT( IrNodeFindOutput(node,use->node) == use  );
  remove_node(node,use,output);
  return ret;
}

void IrNodeClearInput( struct IrNode* node ) {
  init_node(node,input);
}

void IrNodeClearOutput(struct IrNode* node) {
  init_node(node,output);
}

static struct IrNode* new_node( struct IrGraph* graph , int op ,
                                                 int effect,
                                                 int prop_effect,
                                                 int dead ,
                                                 int input_max ,
                                                 int output_max ,
                                                 size_t extra ) {
  struct IrNode* bin = ArenaAllocatorAlloc((graph->arena), sizeof(*bin) + extra);
  bin->op = op;
  bin->effect = effect;
  bin->prop_effect = prop_effect;
  bin->dead = dead;
  bin->mark = 0;
  bin->id = graph->node_id++;
  bin->input_max = input_max;
  bin->output_max= output_max;
  bin->bounded_node = NULL;
  init_node(bin,input);
  init_node(bin,output);
  return bin;
}

struct IrNode* IrNodeNewBinary( struct IrGraph* graph , int op , struct IrNode* left ,
                                                                 struct IrNode* right,
                                                                 struct IrNode* region ) {
  struct IrNode* bin;
  SPARROW_ASSERT(IrIsHighIrBinary(op));
  bin = new_node(graph,op,
                       0,
                       IrNodeHasEffect(left) || IrNodeHasEffect(right),
                       IsDeadRegion(region),
                       2,
                       -1,
                       0);

  add_def_use(graph,bin,left);
  add_def_use(graph,bin,right);

  remove_region(left);
  remove_region(right);

  if(IrNodeHasEffect(bin)) {
    add_region(graph,region,bin);
  }

  return bin;
}

struct IrNode* IrNodeNewUnary( struct IrGraph* graph , int op , struct IrNode* operand ,
                                                                struct IrNode* region ) {
  struct IrNode* bin;
  SPARROW_ASSERT(IrIsHighIrUnary(op));
  bin = new_node(graph,op,
                       0,
                       IrNodeHasEffect(operand),
                       IsDeadRegion(region),
                       1,
                       -1,
                       0);

  add_def_use(graph,bin,operand);

  remove_region(operand);

  if(IrNodeHasEffect(bin)) {
    add_region(graph,region,bin);
  }
  return bin;
}

static
struct IrNode* new_number_node( struct IrGraph* graph , double num , struct IrNode* region ) {
  int op = IrNumberGetType(num);
  struct IrNode* bin;
  size_t size;

  switch(op) {
    case IR_CONST_INT32:
      size = 4;
      break;
    case IR_CONST_INT64:
      size = 8;
      break;
    case IR_CONST_REAL64:
      size = 8;
      break;
    default:
      SPARROW_UNREACHABLE(); return NULL;
  }

  bin = new_node(graph,op, 0, 0, IsDeadRegion(region),0, -1, size);

  switch(op) {
    case IR_CONST_INT32:
      *IRNODE_GET_DATA(bin,int32_t*) = (int32_t)(num);
      break;
    case IR_CONST_INT64:
      *IRNODE_GET_DATA(bin,int64_t*) = (int64_t)(num);
      break;
    case IR_CONST_REAL64:
      *IRNODE_GET_DATA(bin,double*)= num;
      break;
    default:
      SPARROW_UNREACHABLE(); return NULL;
  }

  return bin;
}

struct IrNode* IrNodeGetConstNumber( struct IrGraph* graph , int32_t number ,
    struct IrNode* region ) {
  struct IrNode* bin = new_node(graph,IR_CONST_INT32,0,0,IsDeadRegion(region),0,-1,sizeof(int32_t));
  *IRNODE_GET_DATA(bin,int32_t*) = number;
  return bin;
}

struct IrNode* IrNodeNewConstNumber( struct IrGraph* graph , uint32_t index ,
                                                             const struct ObjProto* proto,
                                                             struct IrNode* region ) {
  double num = proto->num_arr[index];
  return new_number_node(graph,num,region);
}

struct IrNode* IrNodeNewConstString( struct IrGraph* graph , uint32_t index ,
                                                             const struct ObjProto* proto,
                                                             struct IrNode* region ) {
  struct IrNode* node = new_node(graph,IR_CONST_STRING,0,0,IsDeadRegion(region),0,-1,sizeof(void*));
  *IRNODE_GET_DATA(node,struct ObjStr**) = proto->str_arr[index];
  return node;
}

struct IrNode* IrNodeNewConstBoolean( struct IrGraph* graph , int value , struct IrNode* region ) {
  struct IrNode* node = new_node(graph,IR_CONST_BOOLEAN,0,0,IsDeadRegion(region),0,-1,sizeof(int));
  *IRNODE_GET_DATA(node,int*) = value;
  return node;
}

struct IrNode* IrNodeNewConstNull( struct IrGraph* graph , struct IrNode* region ) {
  return new_node(graph,IR_CONST_NULL,0,0,IsDeadRegion(region),0,-1,0);
}

struct IrNode* IrNodeNewList( struct IrGraph* graph ) {
  return new_node( graph , IR_PRIMITIVE_LIST , 0 , 0 , 0 , -1, -1 , 0 );
}

void IrNodeListAddArgument( struct IrGraph* graph , struct IrNode* list ,
                                                    struct IrNode* argument ) {
  add_def_use(graph,list,argument);
  list->prop_effect = IrNodeHasEffect(argument);
  remove_region(argument);
}

void IrNodeListSetRegion( struct IrGraph* graph , struct IrNode* list ,
                                                  struct IrNode* region ) {
  if(IrNodeHasEffect(list)) {
    add_region(graph,list,region);
  }
  list->dead = IsDeadRegion(region);
}

struct IrNode* IrNodeNewMap( struct IrGraph* graph ) {
  return new_node(graph,IR_PRIMITIVE_MAP,0,0,0,-1,-1,0);
}

void IrNodeMapAddArgument( struct IrGraph* graph , struct IrNode* map ,
                                                   struct IrNode* key ,
                                                   struct IrNode* val ) {
  struct IrNode* pair;
  pair = new_node( graph , IR_PRIMITIVE_PAIR , 0 ,
                                               IrNodeHasEffect(key) || IrNodeHasEffect(val),
                                               0,
                                               -1,
                                               -1,
                                               0);
  map->prop_effect = pair->prop_effect;
  add_def_use(graph,pair,key);
  add_def_use(graph,pair,val);
  add_def_use(graph,map,pair);
  remove_region(key);
  remove_region(val);
}

void IrNodeMapSetRegion( struct IrGraph* graph , struct IrNode* map ,
                                                 struct IrNode* region ) {
  if(IrNodeHasEffect(map)) {
    add_region(graph,region,map);
  }
  map->dead = IsDeadRegion(region);
}

struct IrNode* IrNodeNewClosure( struct IrGraph* graph , const struct ObjProto* proto
                                                       , size_t upcnt ,
                                                         struct IrNode* region ) {
  struct IrNode* bin;
  bin = new_node( graph , IR_PRIMITIVE_CLOSURE , 0 , 0 ,
                                                 IsDeadRegion(region),
                                                 upcnt,
                                                 -1 ,
                                                 sizeof(void*));
  *IRNODE_GET_DATA(bin,const struct ObjProto**) = proto;
  return bin;
}

void IrNodeClosureAddUpvalueEmbed( struct IrGraph* graph , struct IrNode* closure ,
                                                           struct IrNode* upvalue ,
                                                           struct IrNode* region ) {
  (void)region;
  add_def_use(graph,closure,upvalue);
}

void IrNodeClosureAddUpvalueDetach(struct IrGraph* graph , struct IrNode* closure ,
                                                           uint32_t index ,
                                                           struct IrNode* region ) {
  struct IrNode* detach = new_node(graph,IR_PRIMITIVE_UPVALUE_DETACH,
                                       0,
                                       0,
                                       IsDeadRegion(region),
                                       0,
                                       -1,
                                       sizeof(uint32_t));
  *IRNODE_GET_DATA(detach,uint32_t*) = index;
  add_def_use(graph,closure,detach);
}

struct IrNode* IrNodeNewCall( struct IrGraph* graph , struct IrNode* function ,
                                                      struct IrNode* region ) {
  struct IrNode* bin = new_node( graph , IR_H_CALL ,
                                         1 ,
                                         0 ,
                                         IsDeadRegion(region),
                                         -1 ,
                                         -1 ,
                                         0 );
  remove_region(function);
  add_def_use(graph,bin,function);
  add_region(graph,region,bin);
  return bin;
}

struct IrNode* IrNodeNewArgument(struct IrGraph* graph , uint32_t index ) {
  struct IrNode* bin = new_node( graph , IR_PRIMITIVE_ARGUMENT ,
                                         0 , 0 , 0, 0 , -1, sizeof(uint32_t));
  *IRNODE_GET_DATA(bin,uint32_t*) = index;
  return bin;
}

void IrNodeCallAddArg( struct IrGraph* graph , struct IrNode* call ,
                                               struct IrNode* arg  ,
                                               struct IrNode* region ) {
  (void)region;
  remove_region(arg);
  IrNodeAddInput(graph,call,arg);
}

struct IrNode* IrNodeNewCallIntrinsic( struct IrGraph* graph , enum IntrinsicFunction func ,
                                                               struct IrNode* region ) {
  struct IrNode* bin = new_node( graph , IR_H_CALL_INTRINSIC , 1 ,
                                                               0 ,
                                                               IsDeadRegion(region),
                                                              -1 ,
                                                              -1 ,
                                                              sizeof(enum IntrinsicFunction));
  *IRNODE_GET_DATA(bin,enum IntrinsicFunction*) = func;
  add_region(graph,region,bin);
  return bin;
}

struct IrNode* IrNodeNewAGet( struct IrGraph* graph , struct IrNode* tos,
                                                      struct IrNode* component ,
                                                      struct IrNode* region ) {
  struct IrNode* bin = new_node(graph,IR_H_AGET,1,0,IsDeadRegion(region),2,-1,0);
  add_def_use(graph,bin,tos);
  add_def_use(graph,bin,component);
  remove_region(tos);
  remove_region(component);
  add_region(graph,region,bin);
  return bin;
}

struct IrNode* IrNodeNewASet( struct IrGraph* graph , struct IrNode* tos,
                                                      struct IrNode* component,
                                                      struct IrNode* value,
                                                      struct IrNode* region ) {
  struct IrNode* bin = new_node(graph,IR_H_ASET,1,0,IsDeadRegion(region),3,0,0);
  add_def_use(graph,bin,tos);
  add_def_use(graph,bin,component);
  add_def_use(graph,bin,value);
  remove_region(tos);
  remove_region(component);
  remove_region(value);
  add_region(graph,region,bin);
  return bin;
}

struct IrNode* IrNodeNewAGetIntrinsic( struct IrGraph* graph , struct IrNode* tos,
                                                               enum IntrinsicAttribute attr ,
                                                               struct IrNode* region ) {
  struct IrNode* bin = new_node(graph,IR_H_AGET_INTRINSIC,1,0,IsDeadRegion(region),
      1,-1,sizeof(enum IntrinsicAttribute));
  add_def_use(graph,bin,tos);
  *IRNODE_GET_DATA(bin,enum IntrinsicAttribute*) = attr;
  remove_region(tos);
  add_region(graph,region,bin);
  return bin;
}

struct IrNode* IrNodeNewASetIntrinsic( struct IrGraph* graph , struct IrNode* tos ,
                                                               enum IntrinsicAttribute attr ,
                                                               struct IrNode* value ,
                                                               struct IrNode* region ) {
  struct IrNode* bin = new_node(graph, IR_H_ASET_INTRINSIC,1,0,IsDeadRegion(region),
      2,-1,sizeof(enum IntrinsicAttribute));
  add_def_use(graph,bin,tos);
  add_def_use(graph,bin,value);
  *IRNODE_GET_DATA(bin,enum IntrinsicAttribute*) = attr;
  remove_region(tos);
  remove_region(value);
  add_region(graph,region,bin);
  return bin;
}

struct IrNode* IrNodeNewUGet( struct IrGraph* graph , uint32_t index ,
                                                      struct IrNode* region ) {
  struct IrNode* bin = new_node(graph, IR_H_UGET , 0 , 0 , IsDeadRegion(region),
      0 , -1 , sizeof(uint32_t));
  *IRNODE_GET_DATA(bin,uint32_t*) = index;
  (void)region;
  return bin;
}

struct IrNode* IrNodeNewUSet( struct IrGraph* graph , uint32_t index ,
                                                      struct IrNode* value ,
                                                      struct IrNode* region ) {
  struct IrNode* bin = new_node(graph , IR_H_USET , 1 , 0 , IsDeadRegion(region),
      1 , 0 , sizeof(uint32_t));
  *IRNODE_GET_DATA(bin,uint32_t*) = index;
  add_def_use(graph,bin,value);
  remove_region(value);
  add_region(graph,region,bin);
  return bin;
}

struct IrNode* IrNodeNewGGet( struct IrGraph* graph , struct IrNode* name ,
                                                      struct IrNode* region ) {
  struct IrNode* bin = new_node(graph , IR_H_GGET , 1 , 0 , IsDeadRegion(region),
      1 , 0 , 0 );
  add_def_use(graph,bin,name);
  remove_region(name);
  add_region(graph,region,bin);
  return bin;
}

struct IrNode* IrNodeNewGSet( struct IrGraph* graph , struct IrNode* name ,
                                                      struct IrNode* value ,
                                                      struct IrNode* region ) {
  struct IrNode* bin = new_node(graph , IR_H_GSET , 1 , 0 , IsDeadRegion(region) ,
      2 , 0 , 0 );
  add_def_use(graph,bin,name);
  add_def_use(graph,bin,value);
  remove_region(name);
  remove_region(value);
  add_region(graph,region,bin);
  return bin;
}

struct IrNode* IrNodeNewBranch( struct IrGraph* graph , int op ) {
  struct IrNode* bin;
  SPARROW_ASSERT( op == IR_H_BRTRUE || op == IR_H_BRFALSE );
  bin = new_node(graph , op , 1 , 0 , 0 , 2 , 0 , 0 );
  return bin;
}

void IrNodeBranchAdd( struct IrGraph* graph , struct IrNode* node ,
                                              struct IrNode* arg ) {
  SPARROW_ASSERT( node->op == IR_H_BRTRUE || node->op == IR_H_BRFALSE );
  node->prop_effect = IrNodeHasEffect(arg);
  add_def_use(graph,node,arg);
  remove_region(arg);
}

void IrNodeBranchSetRegion( struct IrGraph* graph , struct IrNode* node ,
                                                    struct IrNode* region ) {
  SPARROW_ASSERT( node->op == IR_H_BRTRUE || node->op == IR_H_BRFALSE );
  if(node->prop_effect) add_region(graph,region,node);
}

/* Control flow nodes ========================================== */
struct IrNode* IrNodeNewRegion( struct IrGraph* graph ) {
  return new_node(graph , IR_CTL_REGION , 0 , 0 , 0 , -1 , 1 , 0 );
}

struct IrNode* IrNodeNewJump  ( struct IrGraph* graph , struct IrNode* parent ,
                                                        struct IrNode* next ) {
  struct IrNode* node = new_node(graph , IR_CTL_JUMP , 0 , 0 , 0 , -1 , 2 , 0 );
  link_node(graph,parent,node,output);
  link_node(graph,node,next,output);
  return node;
}

struct IrNode* IrNodeNewMerge( struct IrGraph* graph , struct IrNode* if_true ,
                                                       struct IrNode* if_false ) {
  struct IrNode* bin = new_node( graph , IR_CTL_MERGE , 0 , 0 , 0 , -1 , 1 , 0 );
  SPARROW_ASSERT(if_true->op == IR_CTL_IF_TRUE || IR_CTL_IF_FALSE);
  SPARROW_ASSERT(if_false->op == IR_CTL_IF_FALSE || IR_CTL_IF_TRUE);
  link_node(graph,if_true,bin,output);
  link_node(graph,if_false,bin,output);
  return bin;
}

struct IrNode* IrNodeNewIf( struct IrGraph* graph , struct IrNode* predicate ,
                                                    struct IrNode* parent ) {
  struct IrNode* node = new_node( graph , IR_CTL_IF , 0 , 0 , 0 , 1 , 2 , 0 );
  /* link the control flow chain */
  link_node(graph,parent,node,output);
  /* link the expression that is bounded in this node */
  link_node(graph,node,predicate,output);
  /* remove the predicate's bounded region */
  remove_region(predicate);
  return node;
}

struct IrNode* IrNodeNewIfTrue( struct IrGraph* graph , struct IrNode* parent ) {

  struct IrNode* node = new_node( graph , IR_CTL_IF_TRUE , 0 , 0 , 0 , -1 , 1 , 0 );
  link_node(graph,parent,node,output);
  return node;
}

struct IrNode* IrNodeNewIfFalse(struct IrGraph* graph , struct IrNode* parent ) {
  struct IrNode* node = new_node( graph , IR_CTL_IF_FALSE , 0 , 0 , 0 , -1 , 1 , 0 );
  link_node(graph,parent,node,output);
  return node;
}

struct IrNode* IrNodeNewLoop( struct IrGraph* graph , struct IrNode* parent ) {
  struct IrNode* node = new_node( graph , IR_CTL_LOOP , 0 , 0 , 0 , -1 , 1 , 0 );
  link_node(graph,parent,node,output);
  return node;
}

struct IrNode* IrNodeNewLoopExit( struct IrGraph* graph ) {
  struct IrNode* node = new_node( graph , IR_CTL_LOOP_EXIT , 0 , 0 , 0 , -1 , 2 , 0 );
  return node;
}

struct IrNode* IrNodeNewReturn( struct IrGraph* graph, struct IrNode* value ,
                                                       struct IrNode* parent,
                                                       struct IrNode* end ) {
  struct IrNode* node = new_node( graph , IR_CTL_RET , 0 , 0 , 0 , 1 , 1 , 0 );
  link_node(graph,parent,node,output);
  link_node(graph,node,value,input);
  link_node(graph,node,end,output);
  remove_region(value);
  return node;
}

/* Iterator ============================== */
struct IrNode* IrNodeNewIterTest( struct IrGraph* graph , struct IrNode* value ,
                                                          struct IrNode* region) {
  struct IrNode* node = new_node( graph , IR_H_ITER_TEST , 1 ,
                                                           0 ,
                                                           IsDeadRegion(region),
                                                           1 ,
                                                           -1,
                                                           0 );
  add_def_use(graph,node,value);
  remove_region(value);
  add_region(graph,region,node);
  return node;
}

struct IrNode* IrNodeNewIter   ( struct IrGraph* graph , struct IrNode* value ,
                                                          struct IrNode* region) {
  struct IrNode* node = new_node( graph , IR_H_ITER_NEW , 1 ,
                                                          0 ,
                                                          IsDeadRegion(region),
                                                          1 ,
                                                          -1,
                                                          0 );
  add_def_use(graph,node,value);
  remove_region(value);
  add_region(graph,region,node);
  return node;
}

struct IrNode* IrNodeNewIterDref( struct IrGraph* graph , struct IrNode* iter ,
                                                          struct IrNode* region ) {
  struct IrNode* node = new_node( graph , IR_H_ITER_DREF, 1 ,
                                                          0 ,
                                                          IsDeadRegion(region),
                                                          1 ,
                                                          -1,
                                                          0);
  add_def_use(graph,node,iter);
  remove_region(iter);
  add_region(graph,region,node);
  return node;
}

/* Misc ================================== */
struct IrNode* IrNodeNewPhi( struct IrGraph* graph , struct IrNode* left ,
                                                     struct IrNode* right,
                                                     struct IrNode* region ) {
  struct IrNode* node = new_node( graph , IR_PHI , 0 ,
                                                   IrNodeHasEffect(left) || IrNodeHasEffect(right) ,
                                                   IsDeadRegion(region),
                                                   2 ,
                                                   -1,
                                                   sizeof(struct IrNodePhiData));
  struct IrNodePhiData* phi_data = IRNODE_GET_DATA(node,struct IrNodePhiData*);

  SPARROW_ASSERT(region->op == IR_CTL_MERGE);

  /* order matters !!! */
  add_def_use(graph,node,left);
  add_def_use(graph,node,right);
  remove_region(left);
  remove_region(right);
  if(IrNodeHasEffect(node)) add_region(graph,region,node);
  phi_data->merge_node = region;
  return node;
}

struct IrNode* IrNodeNewProjection( struct IrGraph* graph , struct IrNode* target,
                                                            uint32_t index ,
                                                            struct IrNode* region ) {
  struct IrNode* node = new_node( graph , IR_PROJECTION , 0 ,
                                                          IrNodeHasEffect(target),
                                                          IsDeadRegion(region),
                                                          1 ,
                                                          -1 ,
                                                          sizeof(uint32_t));
  add_def_use(graph,node,target);
  remove_region(target);
  *IRNODE_GET_DATA(node,uint32_t*) = index;
  if(node->prop_effect) add_region(graph,region,node);
  return node;
}

/* IrNode RAW ========================================================== */
int IrNodeNameToOpcode( const char* opcode_name ) {
  size_t i;
  for( i = 0 ; i < SPARROW_ARRAY_SIZE(OPCODE_NAME) ; ++i ) {
    if(strcmp(opcode_name,OPCODE_NAME[i].name) ==0)
      return OPCODE_NAME[i].op;
  }
  return -1;
}

struct IrNode* IrNodeNewRaw( struct IrGraph* graph , int opcode ,
                                                     int effect ,
                                                     int prop_effect ,
                                                     int dead ) {
  return new_node( graph , opcode , effect , prop_effect , dead , -1, -1, 0);
}

int IrNodeRawAddBound( struct IrGraph* graph , struct IrNode* node ,
                                               struct IrNode* region ) {
  if(!IrIsControl(region->op)) {
    SPARROW_DBG(ERROR,"region %s is not control flow node!",IrGetName(region->op));
    return -1;
  }
  if(IrIsControl(node->op)) {
    SPARROW_DBG(ERROR,"node %s is a control flow node!",IrGetName(node->op));
    return -1;
  }
  if(node->bounded_node) {
    SPARROW_DBG(ERROR,"node %s has already been bounded to a node %s!",
        IrGetName(node->op),
        IrGetName(node->bounded_node->op));
    return -1;
  }
  add_region(graph,region,node);
  return 0;
}

int IrNodeRawAddInput( struct IrGraph* graph , struct IrNode* node ,
                                               struct IrNode* another ) {
  if(IrIsControl(node->op)) {
    if(IrIsControl(another->op)) {
      SPARROW_DBG(ERROR,"node %s is a control flow node, and tries to add as input "
                        "to another control flow node %s!",
                        IrGetName(another->op),IrGetName(node->op));
      return -1;
    }
  } else {
    if(IrIsControl(another->op)) {
      SPARROW_DBG(ERROR,"tries to add node %s , which is a control flow node, "
          "to a node %s which is an expression node!",
          IrGetName(another->op),
          IrGetName(node->op));
      return -1;
    }
  }
  {
    struct IrUse* use = IrNodeFindInput(node,another);
    if(use) {
      SPARROW_DBG(ERROR,"node %s has already been added to node %s!",
          IrGetName(another->op),
          IrGetName(node->op));
      return -1;
    }
    link_node(graph,node,another,input);
  }
  return 0;
}

int IrNodeRawAddOutput( struct IrGraph* graph , struct IrNode* node ,
                                                struct IrNode* another ) {
  if(IrIsControl(node->op)) {
    if(!IrIsControl(another->op)) {
      SPARROW_DBG(ERROR,"node %s is a control flow node, and tries to add an output "
          "node %s which is *not* a control flow node!",
          IrGetName(node->op),
          IrGetName(another->op));
      return -1;
    }
  } else {
    if(IrIsControl(another->op)) {
      SPARROW_DBG(ERROR,"node %s is not a control flow node, and tries to add an "
          "output node %s which is a control flow node!",
          IrGetName(node->op),
          IrGetName(another->op));
      return -1;
    }
  }

}

/* IrGraph ======================================== */
void IrGraphInit( struct IrGraph* graph , const struct ObjModule* module,
                                          const struct ObjProto* protocol ,
                                          struct Sparrow* sparrow ) {
  graph->sparrow = sparrow;
  graph->mod = module;
  graph->proto = protocol;
  graph->arena = ArenaAllocatorCreate(1024,1024*1024);
  graph->node_id = 0;
  graph->start = new_node( graph , IR_CTL_START , 0 , 0 , 0 , 0 , 1 , 0 );
  graph->end   = new_node( graph , IR_CTL_END   , 0 , 0 , 0 , 0 , 0 , 0 );
  graph->clean_state = 0;
}

void IrGraphInitForInline( struct IrGraph* new_graph ,
                           struct IrGraph* old_graph ,
                           uint32_t protocol_index ) {
  new_graph->sparrow = old_graph->sparrow;
  new_graph->mod = old_graph->mod;
  new_graph->proto = old_graph->mod->cls_arr[protocol_index];
  new_graph->arena = old_graph->arena;
  new_graph->start = IrNodeNewRegion( new_graph );
  new_graph->end   = IrNodeNewRegion( new_graph );
}

void IrGraphDestroy( struct IrGraph* graph ) {
  ArenaAllocatorDestroy(graph->arena);
}

