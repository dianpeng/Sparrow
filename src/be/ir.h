#ifndef IR_H_
#define IR_H_

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
 * is easy to generate 1) control flow graph and also 2) expression tree just by
 * using this single IR.
 *
 * Additionally information regarding IR is stored parallel in other bundled
 * data structure. Like dominator tree and liveless map are stored in an object
 * called JitContext. This object is per module based and serve as the central
 * data structure for any jitting method.
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
 * type of the IR. All IR node has a common header IRBase , inside of the IRBase
 * we have a type field to help user *cast* the IRBase pointer to correct sub
 * type. It is really just traditional way to do single inheritance in C.
 */

/* IR operand list */

/* Due to the fact that our bytecode is *typed* , so we will not see any IR
 * graph that *only* contain high level op. High level op is mostly used when
 * the bytecode doesn't provide any type information, ie `a = b + c`; The b
 * and c are both variables , so bytecode doesn't have any information to tell.
 * The IR for this add will be H_ADD, which means high level ADD. Later on, when
 * optimization algorithm do type lowering, assume if figure out b is integer,
 * then the H_ADD will becomes M_ADD_IV , which means mid-ir that trys to add
 * an integer b with variable c. This instruction is mostly speculative. */

#define HIGH_IROP_LIST(X)







#endif /* IR_H_ */
