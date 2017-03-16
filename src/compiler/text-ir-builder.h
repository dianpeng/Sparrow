#ifndef TEXT_IR_BUILDER_H_
#define TEXT_IR_BUILDER_H_

struct IrGraph;

/*
 * Here we design a textual format to represent a graph. This textual graph
 * is used to automatically test our IrGraph building is correct or not. We
 * use a different approach with Dot Format simply because the node's name is
 * hard to generate.
 * In a dot format, the node name is important since we need to make it unique.
 * This is fine with the IrGraph we generated because each node will be tagged
 * with a unique id. However we user tries to write a graph with text, we don't
 * know those ID and we cannot guess those ID from human perspectives. Mostly,
 * it is useless to guess those ID , they are supposed to be consumed by the
 * computer. In order to solve this problem, we use a different format.
 *
 * A representation splits into 2 sections. One section is nodes , the other is
 * edges. Inside of the node section, the name of each node just needs to be
 * unique, user could use whatever thing make sense to user itself. Inside of
 * the edge section, it defines how the nodes interact with each other. Since
 * the name of each node only is used inside of the textul for parsing purpose.
 * We don't validate these names . The only important things is the graph it
 * formats
 */

int TextToIrGraph( const char* text , struct IrGraph* output );


#endif /* TEXT_IR_BUILDER_H_ */
