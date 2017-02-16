#ifndef PARSER_H_
#define PARSER_H_

struct CodeBuffer;
struct CStr;
struct Sparrow;

struct ObjModule* Parse( struct Sparrow* ,
    const char* fpath ,
    const char* source ,
    struct CStr* err );

#endif /* PARSER_H_ */
