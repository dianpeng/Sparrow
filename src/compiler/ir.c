#include <compiler/ir.h>
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

const char* IrGetName( int opcode ) {
#define __(A,B) case IR_##A : return B ;
  switch(opcode) {
    ALL_IRS(__)
    default: return NULL;
#undef __ /* __ */
  }
}

static void
add_region( struct IrGraph* graph , struct IrNode* region , struct IrNode* node ) {
  SPARROW_ASSERT(IrIsControl(region->op));
  SPARROW_ASSERT(node->bounded == 0);
  IrNodeAddInput(graph,region,node);
  node->bounded = 1;
}

static void
remove_region( struct IrNode* region , struct IrNode* node ) {
  if(node->bounded == 1) {
    SPARROW_ASSERT(IrIsControl(region->op));
    SPARROW_ASSERT(IrNodeHasEffect(node));
    {
      struct IrUse* use = IrNodeFindInput(region,node);
      SPARROW_ASSERT(use);
      remove_node(region,use,input);
    }
    node->bounded = 0;
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
                                                 int input_max ,
                                                 int output_max ,
                                                 size_t extra ) {
  struct IrNode* bin = ArenaAllocatorAlloc((graph->arena), sizeof(*bin) + extra);
  bin->op = op;
  bin->effect = effect;
  bin->prop_effect = prop_effect;
  bin->bounded = 0;
  bin->mark = 0;
  bin->id = graph->node_id++;
  bin->input_max = input_max;
  bin->output_max= output_max;
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
                       2,
                       -1,
                       0);

  add_def_use(graph,bin,left);
  add_def_use(graph,bin,right);

  remove_region(region,left);
  remove_region(region,right);

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
                       1,
                       -1,
                       0);

  add_def_use(graph,bin,operand);

  remove_region(region,operand);

  if(IrNodeHasEffect(bin)) {
    add_region(graph,region,bin);
  }
  return bin;
}

static
struct IrNode* new_number_node( struct IrGraph* graph , double num ) {
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

  bin = new_node(graph,op, 0, 0, 0, -1, size);

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

struct IrNode* IrNodeGetConstNumber( struct IrGraph* graph , int32_t number ) {
  struct IrNode* bin = new_node(graph,IR_CONST_INT32,0,0,0,-1,sizeof(int32_t));
  *IRNODE_GET_DATA(bin,int32_t*) = number;
  return bin;
}

struct IrNode* IrNodeNewConstNumber( struct IrGraph* graph , uint32_t index ,
                                                             const struct ObjProto* proto ) {
  double num = proto->num_arr[index];
  return new_number_node(graph,num);
}

struct IrNode* IrNodeNewConstString( struct IrGraph* graph , uint32_t index ,
                                                             const struct ObjProto* proto ) {
  struct IrNode* node = new_node(graph,IR_CONST_STRING,0,0,0,-1,sizeof(void*));
  *IRNODE_GET_DATA(node,struct ObjStr**) = proto->str_arr[index];
  return node;
}

struct IrNode* IrNodeNewConstBoolean( struct IrGraph* graph , int value ) {
  struct IrNode* node = new_node(graph,IR_CONST_BOOLEAN,0,0,0,-1,sizeof(int));
  *IRNODE_GET_DATA(node,int*) = value;
  return node;
}

struct IrNode* IrNodeNewConstNull( struct IrGraph* graph ) {
  return new_node(graph,IR_CONST_NULL,0,0,0,-1,0);
}

struct IrNode* IrNodeNewList( struct IrGraph* graph ) {
  return new_node( graph , IR_PRIMITIVE_LIST , 0 , 0 , -1, -1 , 0 );
}

void IrNodeListAddArgument( struct IrGraph* graph , struct IrNode* list ,
                                                    struct IrNode* argument ,
                                                    struct IrNode* region ) {
  add_def_use(graph,list,argument);
  list->prop_effect = IrNodeHasEffect(argument);
  remove_region(region,argument);
}

void IrNodeListSetRegion( struct IrGraph* graph , struct IrNode* list ,
                                                  struct IrNode* region ) {
  if(IrNodeHasEffect(list)) {
    add_region(graph,list,region);
  }
}

struct IrNode* IrNodeNewMap( struct IrGraph* graph ) {
  return new_node(graph,IR_PRIMITIVE_MAP,0,0,-1,-1,0);
}

void IrNodeMapAddArgument( struct IrGraph* graph , struct IrNode* map ,
                                                   struct IrNode* key ,
                                                   struct IrNode* val ,
                                                   struct IrNode* region ) {
  struct IrNode* pair;
  pair = new_node( graph , IR_PRIMITIVE_PAIR , 0 ,
                                               IrNodeHasEffect(key) || IrNodeHasEffect(val),
                                               -1,
                                               -1,
                                               0);
  map->prop_effect = pair->prop_effect;
  add_def_use(graph,pair,key);
  add_def_use(graph,pair,val);
  add_def_use(graph,map,pair);
  remove_region(region,key);
  remove_region(region,val);
}

void IrNodeMapSetRegion( struct IrGraph* graph , struct IrNode* map ,
                                                 struct IrNode* region ) {
  if(IrNodeHasEffect(map)) {
    add_region(graph,region,map);
  }
}

struct IrNode* IrNodeNewClosure( struct IrGraph* graph , const struct ObjProto* proto
                                                       , size_t upcnt ) {
  struct IrNode* bin;
  bin = new_node( graph , IR_PRIMITIVE_CLOSURE , 0 , 0 ,upcnt, -1 , sizeof(void*));
  *IRNODE_GET_DATA(bin,const struct ObjProto**) = proto;
  return bin;
}

void IrNodeClosureAddUpvalueEmbed( struct IrGraph* graph , struct IrNode* closure ,
                                                           struct IrNode* upvalue ) {
  add_def_use(graph,closure,upvalue);
}

void IrNodeClosureAddUpvalueDetach(struct IrGraph* graph , struct IrNode* closure ,
                                                           uint32_t index ) {
  struct IrNode* detach = new_node(graph,IR_PRIMITIVE_UPVALUE_DETACH,0,0,0,-1,sizeof(uint32_t));
  *IRNODE_GET_DATA(detach,uint32_t*) = index;
  add_def_use(graph,closure,detach);
}

struct IrNode* IrNodeNewCall( struct IrGraph* graph , struct IrNode* function ,
                                                      struct IrNode* region ) {
  struct IrNode* bin = new_node( graph , IR_H_CALL , 1 , 0 , -1 , -1 , 0 );
  add_def_use(graph,bin,function);
  add_region(graph,region,bin);
  return bin;
}

struct IrNode* IrNodeNewArgument(struct IrGraph* graph , uint32_t index ) {
  struct IrNode* bin = new_node( graph , IR_PRIMITIVE_ARGUMENT ,
                                         0 , 0 , 0 , -1, sizeof(uint32_t));
  *IRNODE_GET_DATA(bin,uint32_t*) = index;
  return bin;
}

void IrNodeCallAddArg( struct IrGraph* graph , struct IrNode* call ,
                                               struct IrNode* arg  ,
                                               struct IrNode* region ) {
  remove_region(region,arg);
  IrNodeAddInput(graph,call,arg);
}

struct IrNode* IrNodeNewCallIntrinsic( struct IrGraph* graph , enum IntrinsicFunction func ,
                                                               struct IrNode* region ) {
  struct IrNode* bin =new_node( graph , IR_H_CALL_INTRINSIC , 1 ,
                                                              0 ,
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
  struct IrNode* bin = new_node(graph,IR_H_AGET,1,0,2,-1,0);
  add_def_use(graph,bin,tos);
  add_def_use(graph,bin,component);
  remove_region(region,tos);
  remove_region(region,component);
  add_region(graph,region,bin);
  return bin;
}

struct IrNode* IrNodeNewASet( struct IrGraph* graph , struct IrNode* tos,
                                                      struct IrNode* component,
                                                      struct IrNode* value,
                                                      struct IrNode* region ) {
  struct IrNode* bin = new_node(graph,IR_H_ASET,1,0,3,0,0);
  add_def_use(graph,bin,tos);
  add_def_use(graph,bin,component);
  add_def_use(graph,bin,value);
  remove_region(region,tos);
  remove_region(region,component);
  remove_region(region,value);
  add_region(graph,region,bin);
  return bin;
}

struct IrNode* IrNodeNewAGetIntrinsic( struct IrGraph* graph , struct IrNode* tos,
                                                               enum IntrinsicAttribute attr ,
                                                               struct IrNode* region ) {
  struct IrNode* bin = new_node(graph,IR_H_AGET_INTRINSIC,1,0,1,-1,
                                                                sizeof(enum IntrinsicAttribute));
  add_def_use(graph,bin,tos);
  *IRNODE_GET_DATA(bin,enum IntrinsicAttribute*) = attr;
  remove_region(region,tos);
  add_region(graph,region,bin);
  return bin;
}

struct IrNode* IrNodeNewASetIntrinsic( struct IrGraph* graph , struct IrNode* tos ,
                                                               enum IntrinsicAttribute attr ,
                                                               struct IrNode* value ,
                                                               struct IrNode* region ) {
  struct IrNode* bin = new_node(graph, IR_H_ASET_INTRINSIC,1,0,2,-1,
                                                                 sizeof(enum IntrinsicAttribute));
  add_def_use(graph,bin,tos);
  add_def_use(graph,bin,value);
  *IRNODE_GET_DATA(bin,enum IntrinsicAttribute*) = attr;
  remove_region(region,tos);
  remove_region(region,value);
  add_region(graph,region,bin);
  return bin;
}

struct IrNode* IrNodeNewUGet( struct IrGraph* graph , uint32_t index ,
                                                      struct IrNode* region ) {
  struct IrNode* bin = new_node(graph, IR_H_UGET , 0 , 0 , 0 , -1 , sizeof(uint32_t));
  *IRNODE_GET_DATA(bin,uint32_t*) = index;
  (void)region;
  return bin;
}

struct IrNode* IrNodeNewUSet( struct IrGraph* graph , uint32_t index ,
                                                      struct IrNode* value ,
                                                      struct IrNode* region ) {
  struct IrNode* bin = new_node(graph , IR_H_USET , 1 , 0 , 1 , 0 , sizeof(uint32_t));
  *IRNODE_GET_DATA(bin,uint32_t*) = index;
  add_def_use(graph,bin,value);
  remove_region(region,value);
  add_region(graph,region,bin);
  return bin;
}

struct IrNode* IrNodeNewGGet( struct IrGraph* graph , struct IrNode* name ,
                                                      struct IrNode* region ) {
  struct IrNode* bin = new_node(graph , IR_H_GGET , 1 , 0 , 1 , 0 , 0 );
  add_def_use(graph,bin,name);
  remove_region(region,name);
  add_region(graph,region,bin);
  return bin;
}

struct IrNode* IrNodeNewGSet( struct IrGraph* graph , struct IrNode* name ,
                                                      struct IrNode* value ,
                                                      struct IrNode* region ) {
  struct IrNode* bin = new_node(graph , IR_H_GSET , 1 , 0 , 2 , 0 , 0 );
  add_def_use(graph,bin,name);
  add_def_use(graph,bin,value);
  remove_region(region,name);
  remove_region(region,value);
  add_region(graph,region,bin);
  return bin;
}

/* Control flow nodes ========================================== */
struct IrNode* IrNodeNewRegion( struct IrGraph* graph ) {
  return new_node(graph , IR_CTL_REGION , 0 , 0 , -1 , 1 , 0 );
}

struct IrNode* IrNodeNewMerge( struct IrGraph* graph , struct IrNode* if_true ,
                                                       struct IrNode* if_false ) {
  struct IrNode* bin = new_node( graph , IR_CTL_MERGE , 0 , 0 , -1 , 1 , 0 );
  SPARROW_ASSERT(if_true->op == IR_CTL_IF_TRUE);
  SPARROW_ASSERT(if_false->op == IR_CTL_IF_FALSE);
  IrNodeAddOutput(graph,if_true,bin);
  IrNodeAddOutput(graph,if_false,bin);
  return bin;
}

struct IrNode* IrNodeNewIf( struct IrGraph* graph , struct IrNode* predicate ,
                                                    struct IrNode* parent ) {
  struct IrNode* node = new_node( graph , IR_CTL_IF , 0 , 0 , 1 , 2 , 0 );
  /* link the control flow chain */
  IrNodeAddOutput(graph,parent,node);
  /* link the expression that is bounded in this node */
  IrNodeAddInput (graph,node,predicate);
  return node;
}

struct IrNode* IrNodeNewIfTrue( struct IrGraph* graph , struct IrNode* parent ) {

  struct IrNode* node = new_node( graph , IR_CTL_IF_TRUE , 0 , 0 , -1 , 1 , 0 );
  IrNodeAddOutput(graph,parent,node);
  return node;
}

struct IrNode* IrNodeNewIfFalse(struct IrGraph* graph , struct IrNode* parent ) {
  struct IrNode* node = new_node( graph , IR_CTL_IF_FALSE , 0 , 0 , -1 , 1 , 0 );
  IrNodeAddOutput(graph,parent,node);
  return node;
}

struct IrNode* IrNodeNewLoop( struct IrGraph* graph , struct IrNode* parent ) {
  struct IrNode* node = new_node( graph , IR_CTL_LOOP , 0 , 0 , -1 , 1 , 0 );
  IrNodeAddOutput(graph,parent,node);
  return node;
}

struct IrNode* IrNodeNewLoopExit( struct IrGraph* graph , struct IrNode* parent ) {
  struct IrNode* node = new_node( graph , IR_CTL_LOOP_EXIT , 0 , 0 , -1 , 2 , 0 );
  IrNodeAddOutput(graph,parent,node);
  return node;
}

struct IrNode* IrNodeNewReturn( struct IrGraph* graph, struct IrNode* value ,
                                                       struct IrNode* parent,
                                                       struct IrNode* end ) {
  struct IrNode* node = new_node( graph , IR_CTL_RET , 0 , 0 , 1 , 1 , 0 );
  IrNodeAddOutput(graph,parent,node);
  IrNodeAddInput (graph,node,value);
  IrNodeAddOutput(graph,node,end);
  return node;
}

/* Iterator ============================== */
struct IrNode* IrNodeNewIterTest( struct IrGraph* graph , struct IrNode* value ,
                                                          struct IrNode* region) {
  struct IrNode* node = new_node( graph , IR_H_ITER_TEST , 1 ,
                                                           IrNodeHasEffect(value),
                                                           1 ,
                                                           -1 ,
                                                           0 );
  add_def_use(graph,node,value);
  remove_region(region,value);
  add_region(graph,region,node);
  return node;
}

struct IrNode* IrNodeNewIterNew ( struct IrGraph* graph , struct IrNode* value ,
                                                          struct IrNode* region) {
  struct IrNode* node = new_node( graph , IR_H_ITER_NEW , 1 ,
                                                          IrNodeHasEffect(value),
                                                          1 ,
                                                          -1 ,
                                                          0 );
  add_def_use(graph,node,value);
  remove_region(region,value);
  add_region(graph,region,node);
  return node;
}

struct IrNode* IrNodeNewIterDref( struct IrGraph* graph , struct IrNode* iter ,
                                                          struct IrNode* region ) {
  struct IrNode* node = new_node( graph , IR_H_ITER_DREF, 1 ,
                                                          IrNodeHasEffect(iter),
                                                          1,
                                                          -1,
                                                          0);
  add_def_use(graph,node,iter);
  remove_region(region,iter);
  add_region(graph,region,node);
  return node;
}

/* Misc ================================== */
struct IrNode* IrNodeNewPhi( struct IrGraph* graph , struct IrNode* left ,
                                                     struct IrNode* right,
                                                     struct IrNode* region ) {
  struct IrNode* node = new_node( graph , IR_PHI , 0 ,
                                                   IrNodeHasEffect(left) || IrNodeHasEffect(right) ,
                                                   2 ,
                                                   -1,
                                                   0 );
  /* order matters !!! */
  add_def_use(graph,node,left);
  add_def_use(graph,node,right);
  remove_region(region,left);
  remove_region(region,right);
  if(IrNodeHasEffect(node)) add_region(graph,region,node);
  return node;
}

struct IrNode* IrNodeNewProjection( struct IrGraph* graph , struct IrNode* target,
                                                            uint32_t index ,
                                                            struct IrNode* region ) {
  struct IrNode* node = new_node( graph , IR_PROJECTION , 1 , 0 , 1 , -1 , sizeof(uint32_t));
  add_def_use(graph,node,target);
  *IRNODE_GET_DATA(node,uint32_t*) = index;
  add_region(graph,region,node);
  return node;
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
  graph->start = new_node( graph , IR_CTL_START , 0 , 0 , 0 , 1 , 0 );
  graph->end   = new_node( graph , IR_CTL_END   , 0 , 0 , 0 , 0 , 0 );
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

