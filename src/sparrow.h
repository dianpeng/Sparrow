#ifndef CONF_H_
#define CONF_H_
#include <inttypes.h>
#include <assert.h>
#include <limits.h>

#if defined(__GNUC__) || defined(__clang__)

/* Likely unlikely stuff for static branch predication.
 * Right now it is nearly useless for most of the new
 * architecture since they all only supports dynamic predication.
 * But it forces compiler to generate code in a certain order */
#define SPARROW_LIKELY(X) __builtin_expect((X),1)
#define SPARROW_UNLIKELY(X) __builtin_expect((X),0)
/* Inline flag , mostly used to help when we port to some sick
 * compiler suit like MSVC or older version of c */
#define SPARROW_INLINE inline
#else
#error "Compiler not supported!"
#endif /* __GNUC__ || __clang__ */

#ifndef SPARROW_REAL_FORMAT_PRECISION
#define SPARROW_REAL_FORMAT_PRECISION "4"
#endif /* SPARROW_REAL_FORMAT_PRECISION */

#ifndef SPARROW_DEFAULT_FUNCCALL_SIZE
#define SPARROW_DEFAULT_FUNCCALL_SIZE 1024
#endif /* SPARROW_DEFAULT_FUNCCALL_SIZE */

#ifndef SPARROW_DEFAULT_STACK_SIZE
#define SPARROW_DEFAULT_STACK_SIZE 1024*128
#endif /* SPARROW_DEFAULT_STACK_SIZE */

#ifndef SPARROW_DEFAULT_GC_THRESHOLD
#define SPARROW_DEFAULT_GC_THRESHOLD 25000
#endif /* SPARROW_DEFAULT_GC_THRESHOLD */

#ifndef SPARROW_DEFAULT_GC_RATIO
#define SPARROW_DEFAULT_GC_RATIO 0.5
#endif /* SPARROW_DEFAULT_GC_RATIO */

#ifndef SPARROW_DEFAULT_GC_PENALTY_RATIO
#define SPARROW_DEFAULT_GC_PENALTY_RATIO 0.3
#endif /* SPARROW_DEFAULT_GC_PENALTY_RATIO */

/* Data type size boundary limitation define */
#ifndef SPARROW_SIZE_MAX
#define SPARROW_SIZE_MAX UINT_MAX
#endif /* SPARROW_SIZE_MAX */

#ifndef SPARROW_INT_MAX_
#define SPARROW_INT_MAX INT_MAX
#endif /* SPARROW_INT_MAX */

#ifndef SPARROW_INT_MIN
#define SPARROW_INT_MIN INT_MIN
#endif /* SPARROW_INT_MIN */

/* Static assertion directives */
#define SPARROW_STATIC_ASSERT(COND,MSG) \
  typedef char static_assert_##MSG[(COND)?1:-1]

#define SPARROW_MAX(A,B) ((A) > (B) ? (A) : (B))

#define SPARROW_MIN(A,B) ((A) < (B) ? (A) : (B))

#define SPARROW_ARRAY_SIZE(X) ((sizeof(X) /sizeof(X[0])))

#define SPARROW_STRING_SIZE(X) (SPARROW_ARRAY_SIZE(X)-1)

#define SPARROW_UNUSE_ARG(X) (void)(X)

#endif /* CONF_H_ */
