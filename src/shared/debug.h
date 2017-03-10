#ifndef DEBUG_H_
#define DEBUG_H_
#include <stdio.h>

/* Logger interface globally for Sparrow */
enum {
  SPARROW_LOGGER_SEVERITY_INFO,
  SPARROW_LOGGER_SEVERITY_WARN,
  SPARROW_LOGGER_SEVERITY_ERROR,
  SPARROW_LOGGER_SEVERITY_FATAL
};

void SparrowSetLoggerOutputFile( FILE* );

#define SparrowSetLoggerOutputStderr SparrowSetLoggerOutputFile(stderr)
#define SparrowSetLoggerOutputStdout SparrowSetLoggerOutputFile(stdout)

void SparrowLoggerWrite( int severity , const char* file , int line  ,
                                                           const char* format ,
                                                           ... );

int SparrowLoggerAssert( const char* file , int line , const char* expr ,
                                                       const char* format ,
                                                       ... );

#ifdef SPARROW_DEBUG
#define SPARROW_ASSERT(X) \
  (void)(!!(X) || SparrowLoggerAssert(__FILE__,__LINE__,#X,NULL))

#define SPARROW_ASSERT_INFO(X,...) \
  (void)(!!(X) || SparrowLoggerAssert(__FILE__,__LINE__,#X,__VA__ARGS__))

#define SPARROW_DBG(SEVERITY,FMT,...) \
  SparrowLoggerWrite(SPARROW_LOGGER_SEVERITY_##SEVERITY,__FILE__,__LINE__,FMT,__VA_ARGS__)

#else
#define SPARROW_ASSERT(X) (void)(X)

#define SPARROW_ASSERT_INFO(X,...) (void)(X)

#define SPARROW_DBG(SEVERITY,FMT,...) (void)SEVERITY

#endif /* SPARROW_DEBUG */

#define SPARROW_VERIFY(X) \
  (void)(!!(X) || SparrowLoggerAssert( __FILE__ , __LINE__ , #X , NULL ))

#define SPARROW_VERIFY_INFO(X,...) \
  (void)(!!(X) || SparrowLoggerAssert( __FILE__ , __LINE__ , #X , __VA_ARGS__ ))

#define SPARROW_UNREACHABLE() \
  SparrowLoggerAssert(__FILE__,__LINE__,"<unreachable>!",NULL)

#define SPARROW_UNIMPLEMENTED() \
  SparrowLoggerAssert(__FILE__,__LINE__,"<unimplemented!>",NULL)

#define SPARROW_DUMP(SEVERITY,FMT,...) \
  SparrowLoggerWrite(SPARROW_LOGGER_SEVERITY_##SEVERITY,__FILE__,__LINE__,FMT,__VA_ARGS__)

#endif /* DEBUG_H_ */
