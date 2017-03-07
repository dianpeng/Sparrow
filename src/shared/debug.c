#include <shared/debug.h>
#include <stdarg.h>
#include <assert.h>
#include <stdlib.h>

struct logger_context {
  FILE* output;
};

static struct logger_context LOGGER_CTX = {
  NULL
};

static const char* SEVERIFY[] = {
  "INFO","WARN","ERROR","FATAL","ASSERT"
};

void SparrowSetLoggerOutputFile( FILE* output ) {
  LOGGER_CTX.output = output;
}


/* TODO:: The following code is not *thread safe*. If we have threaded
 * compliation architecture. Then all the following code needs to be
 * rewritten into a thread safe manner */

void SparrowLoggerWrite( int severity , const char* file , int line ,
                                                           const char* format,
                                                           ... ) {
  va_list vl;
  assert(severity >=0 && severity <4);
  va_start(vl,format);

  if(!LOGGER_CTX.output) LOGGER_CTX.output = stderr;

  fprintf(LOGGER_CTX.output,"[%s] (%s@%d):",SEVERIFY[severity],file,line);
  vfprintf(LOGGER_CTX.output,format,vl);
  fwrite ("\n",1,1,LOGGER_CTX.output);

  if(severity != SPARROW_LOGGER_SEVERITY_INFO)
    fflush(LOGGER_CTX.output);
}

int SparrowLoggerAssert( const char* file , int line , const char* expr ,
                                                       const char* format ,
                                                       ... ) {
  va_list vl;
  va_start(vl,format);

  if(!LOGGER_CTX.output) LOGGER_CTX.output = stderr;

  fprintf(LOGGER_CTX.output,"[%s] (%s@%d) (%s):", SEVERIFY[4] ,
                                                  file,
                                                  line,
                                                  expr);
  vfprintf(LOGGER_CTX.output,format,vl);
  fwrite ("\n",1,1,LOGGER_CTX.output);
  fflush(LOGGER_CTX.output);
  abort();
  return 0;
}
