#ifndef BC_IR_BUILDER_H_
#define BC_IR_BUILDER_H_
#include "ir.h"
#include "../fe/bc.h"

struct IrGraph;
struct Sparrow;

/* This BytecodeIrBuilder will convert Bytecode sequence to HIGH level IR
 * graph.
 *
 *
 * Detail:
 *
 * To generate a SSA format of graph from a stack based bytecode we need to
 * simulate stack operation by approximately interpret the bytecode sequence.
 *
 * 1. Basic Block
 *   We don't have a BB, here I just mean the instruction that is executed
 *   linearly. For discussion purpose, let's assume all the instructions are
 *   in some sort of BB. To generate sea-of-nodes from BB is simply a matter
 *   of interpretation of each bytecode and also correctly maintain the stack
 *   status with its corresponding node.
 *
 *   The BC_MOVE will always be a kill for existed variable, so it will
 *   introduce renaming of each stack slots.
 *   Additionally there're several bytecode will simply be ignored, mostly
 *   POP operation . The POP operation will still take effect on stack slots
 *   but we won't generate IrNode for these bytecodes.
 *
 * 2. Branch
 *   In our bytecode, there're several places could generate branch. To generate
 *   branch IR, we need walk all possible branch's BB. And then we insert a
 *   region for *merge/join*. In the merge/join node, it is the good place to
 *   introduce PHI node. We will always *eagerly* introduce PHI node right in
 *   the merge node , simply because if we do it in a lazy fasion , insert PHI
 *   node when it is actually used , we will need to maintain separate stack for
 *   each branch. It is just too complicated to do it. So we will eagerly do the
 *   stack merge for PHI node and insert it. Later on the optimization phase will
 *   just remove those unreferenced PHI node.
 *
 * 3. Loop
 *   In our bytecode, we have a clear bytecode to indicate , hey this is the
 *   loop header which is BC_FORPREP. The FORPREP instruction will just notify
 *   us to insert a loop header here. However the effect brought by FOREPREP is
 *   not in the loop body but before the loop header. Since we don't have goto,
 *   our loop will always be natural loop and graph is always reducible. The
 *   complication comes from the back edge introduce by the loop. The natural
 *   interpretation order will be loop header ---> loop body ---> back edge
 *
 *   But loop variant is a PHI , up to the point to generate IR graph, we haven't
 *   evaluated any instruction after where the PHI should go to , so the operand
 *   for PHI is not clear.
 *
 *   var j = 0;                        j1 = 0;
 *   for( _ in forever() ) {
 *     j = j + 1;                      j2 = PHI(j1,j3)
 *                                     j3 = j2 + 1
 *   }
 *
 *   As you can see, the correct PHI node relies on j3 which is not shown in
 *   the context when j2 should be introduced, even we *don't* know whether we
 *   should insert a PHI node there. This kind of complication is something we
 *   should deal with.
 *   To address this issue, we need a 2 pass algorithm. At first pass, we just
 *   do normal renaming as if there is not back-edge.
 *
 *   Phase 1:
 *
 *   var j = 0;                          j1 = 0
 *   for( _ in forever() ) {
 *     j = j +1;              =====>     j2 = j1 + 1
 *   }
 *
 *   Then we will find a back edge and also we introduce a if region,Following,
 *   we need to insert a PHI node there because of separate stack.
 *
 *   The phi node introduced will be j3 = PHI(j1,j2). So when we introduce the
 *   PHI node we need to patch all the existed reference to j1 simply because
 *   the new PHI node makes old j1 wrong. So we re-visit each statement in the
 *   loop body and patch any node reference to the j1 node to j3. Then we will
 *   have a correct loop isntallment.
 *
 *
 * Stack simluation maintain and renaming.
 *
 *   The key is that we need to approximately simluate the bytecode to get the
 *   correct stack states. The bytecode LOAD/PUSH and POP will be interpreted
 *   to make stack state correct. Additionally, when we start to analyze
 *   branch or loop, multiple stack must be kept , and those stack cannot be
 *   discarded until we finish merge.
 *
 *   To address this issue, we need to have a chain based stack tree instead of
 *   a linear stack. This stack tree will be used to maintain multiple parallel
 *   stack frame while we start to visit different branch.
 *
 *   To explicitly maintain such stack tree is complicated since a stack frame
 *   should be associated with a region/control flow node instead of chain to
 *   the root of the stack. Since we iterate each nodes along with the control
 *   flow node not stack. To ease the pain for building these stuff, we will
 *   need a sparse array to track the stack that region node need it. The sparse
 *   array is stored *out of index* and it is indexed by the node's id. Currently
 *   the sparse array is implemented as an array plus a linear search. We don't
 *   believe we will have huge amount of branches.
 */

int BytecodeIrBuilderBuild( struct Sparrow* , struct IrGraph* ,
                                              struct ObjProto* );




#endif /* BC_IR_BUILDER_H_ */
