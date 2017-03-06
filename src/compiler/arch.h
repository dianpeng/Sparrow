#ifndef ARCH_H_
#define ARCH_H_
#include "../conf.h"

#ifndef __x86_64__
#error "Guys, only support x86-64 now"
#endif /* __x86_64__ */

/* Architecture related low level definitions */

/* We cannot rely on C standard to get the limitation of different types
 * of value's range unless we have explicitly defined int32_t int64_t's
 * length. Currently I just hardcoded */

#define SPARROW_INT32_MAX (2147483647)
#define SPARROW_INT32_MIN (-2147483648)
#define SPARROW_INT64_MAX ((int64_t)(9223372036854775807))
#define SPARROW_INT64_MIN ((int64_t)(-9223372036854775807))

/* For floating point number, we will use SSE2 and only supports x64. So
 * we only gonna use double floating point so no need to fall back to floating
 * point number */

#endif /* ARCH_H_ */
