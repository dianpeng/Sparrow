#ifndef OPT_GLOBAL_EXPR_REDUCE_H_
#define OPT_GLOBAL_EXPR_REDUCE_H_

/* A sort of customized version of global value numbering for our cases.
 * It is used to perform common expression level optimization in a global
 * scope ( not within basic block ). Mainly, it will perform stuff like :
 * 1. Constant propogagtion
 * 2. Copy propogation
 * 3. CSE by global numbering
 * 4. Strength reduction
 * 5. Misc expression level optimization
 * 6. Trivial PHI elimination
 *
 * The algorithm it tries to perform is totally different with Cliff Click's
 * Global Code Motion & Global Value Numbering. The code motion will be performed
 * in separate pass
 *
 * The algorithm here just do forward cfg pass. Nothing really special here.
 */

enum OptResult Opt_GlobalExprReduce( struct IrGraph* , int debug );

#endif /* OPT_GLOBAL_EXPR_REDUCE_H_ */
