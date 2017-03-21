#include "opt-global-expr-reduce.h"

/* Most of the expression level optimization can be worked as a query engine.
 * We just need to solve 2 problems:
 * 1. At certain point, what is our knowledge base ?
 * 2. At certain point, what kind of question to ask against our KB ?
 *
 * The knowledge base basically stores all possibly utility for optimizing the
 * future code and when we reach at certain point we need to ask about each
 * question based on the expression optimization rules. So a rules engine is
 * also useful here. */

