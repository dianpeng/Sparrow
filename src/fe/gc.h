#ifndef GC_H_
#define GC_H_
#include "../conf.h"
#include "object.h"
#include <stdio.h>

/* We just have a stop-the-world GC, but have a good trigger mechanism . In
 * generaly , the GC is teaked to avoid potential useless GC try. A useless
 * GC try means a GC is kicked in but end up without collecting anything.
 * We have a penalty system to avoid such GC trigger and also we have other
 * mechanism to avoid GC trigger becomes too lazy which means GC never tries
 * to kicks in at anytime */

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
