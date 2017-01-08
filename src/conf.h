#ifndef CONF_H_
#define CONF_H_
#include <inttypes.h>
#include <assert.h>
#include <limits.h>

/* We only worked on a 64 bits machine */
#ifdef __x86_64__
#define ARCH_64
#else
#define ARCH_32
#endif /* __x86_64__ */

#if defined(__GNUC__) || defined(__clang__)
/* Likely unlikely stuff for static branch predication.
 * Right now it is nearly useless for most of the new
 * architecture since they all only supports dynamic predication.
 * But it forces compiler to generate code in a certain order */
#define SP_LIKELY(X) __builtin_expect((X),1)
#define SP_UNLIKELY(X) __builtin_expect((X),0)

/* Inline flag , mostly used to help when we port to some sick
 * compiler suit like MSVC or older version of c */
#define SPARROW_INLINE inline
#else
#error "Compiler not supported!"
#endif /* __GNUC__ || __clang__ */


/* Data type size boundary limitation define */

#define SPARROW_SIZE_MAX UINT_MAX

#define SPARROW_INT_MAX INT_MAX

#define SPARROW_INT_MIN INT_MIN

#endif /* CONF_H_ */
