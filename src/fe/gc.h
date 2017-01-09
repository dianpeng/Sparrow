#ifndef GC_H_
#define GC_H_
#include "../conf.h"
#include "object.h"
#include <stdio.h>

#ifndef SPARROW_DEFAULT_GC_THRESHOLD
#define SPARROW_DEFAULT_GC_THRESHOLD 100000
#endif /* SPARROW_DEFAULT_GC_THRESHOLD */

#ifndef SPARROW_DEFAULT_GC_RATIO
#define SPARROW_DEFAULT_GC_RATIO 0.5
#endif /* SPARROW_DEFAULT_GC_RATIO */

#ifndef SPARROW_DEFAULT_GC_PENALTY_RATIO
#define SPARROW_DEFAULT_GC_PENALTY_RATIO 0.3
#endif /* SPARROW_DEFAULT_GC_PENALTY_RATIO */

/* We have pretty rookie GC and we will optimize it later on.
 * The GC is a stop-world mark&swap GC and have a very simple
 * and straitforward implementation */

/* helper function to *finalize* an GC manged object. Do not use it if you
 * don't know what it is */
void GCFinalizeObj( struct Sparrow* , struct GCRef* );

/* try to trigger a GC. If return is negative number, means no GC is triggered.
 * otherwise a postive number returned to represent how many objects has been
 * correctly touched */
int GCTry( struct Sparrow* );

/* Same as try but just force it to trigger anyway. So it will always return
 * a positive number */
void GCForce( struct Sparrow* );

/* Mark routine used for user to do customize cooperative GC in user data */
void GCMark ( Value value );
void GCMarkString( struct ObjStr* );
void GCMarkList( struct ObjList* );
void GCMarkMap ( struct ObjMap* );
void GCMarkUdata( struct ObjUdata* );
void GCMarkMethod(struct ObjMethod*);
void GCMarkModule(struct ObjModule*);
void GCMarkClosure( struct ObjClosure* );
void GCMarkComponent( struct ObjComponent* );
void GCMarkProto ( struct ObjProto* );

#endif /* GC_H_ */
