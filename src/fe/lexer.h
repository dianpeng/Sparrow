#ifndef LEX_H_
#define LEX_H_
#include "../util.h"
#include <stddef.h>
#include <ctype.h>

#define TOKEN(__) \
  __(TK_ADD,"+") \
  __(TK_SUB,"-") \
  __(TK_MUL,"*") \
  __(TK_DIV,"/") \
  __(TK_MOD,"%") \
  __(TK_POW,"^") \
  __(TK_NOT,"!") \
  __(TK_LT,"<") \
  __(TK_LE,"<=") \
  __(TK_GT,">") \
  __(TK_GE,">=") \
  __(TK_EQ,"==") \
  __(TK_NE,"!=") \
  __(TK_AND,"&&") \
  __(TK_OR,"||") \
  __(TK_ASSIGN,"=")  \
  __(TK_DOT,".") \
  __(TK_COLON,":") \
  __(TK_COMMA,",") \
  __(TK_SEMICOLON,";") \
  __(TK_LPAR,"(") \
  __(TK_RPAR,")") \
  __(TK_LBRA,"{") \
  __(TK_RBRA,"}") \
  __(TK_LSQR,"[") \
  __(TK_RSQR,"]") \
  __(TK_IF,"if") \
  __(TK_ELIF,"elif") \
  __(TK_ELSE,"else") \
  __(TK_FOR,"for") \
  __(TK_IN,"in") \
  __(TK_WHILE,"while") \
  __(TK_BREAK,"break") \
  __(TK_CONTINUE,"continue") \
  __(TK_FUNCTION,"function") \
  __(TK_RETURN,"return") \
  __(TK_VAR,"var") \
  __(TK_VARIABLE,"<variable>") \
  __(TK_STRING,"<string>") \
  __(TK_NUMBER,"<number>") \
  __(TK_TRUE,"<true>") \
  __(TK_FALSE,"<false>") \
  __(TK_NULL,"<null>") \
  __(TK_EOF,"<eof>") \
  __(TK_ERROR,"<error>")

enum Token {
#define __(A,B) A ,
  TOKEN(__)
  SIZE_OF_TOKENS
#undef __
};

#define TokenIsFactorOP(TK) \
  ((TK) == TK_MUL || (TK) == TK_DIV || \
   (TK) == TK_POW || (TK) == TK_MOD)

#define TokenIsTermOP(TK) \
  ((TK) == TK_ADD || (TK) == TK_SUB)

#define TokenIsUnaryOP(TK) \
  ((TK) == TK_NOT || (TK) == TK_SUB)

#define TokenIsArithOP(TK) \
  (TokenIsFactorOP(TK) || TokenIsTermOP(TK))

#define TokenIsCompOP(TK) \
  ((TK) == TK_LT || (TK) == TK_LE || \
   (TK) == TK_GT || (TK) == TK_GE || \
   (TK) == TK_EQ || (TK) == TK_NE)

#define TokenIsLogicOP(TK) \
  ((TK) == TK_AND || (TK) == TK_OR)


const char* TokenGetName( enum Token );

struct Lexeme {
  enum Token tk;
  struct StrBuf str;
  double num;
  size_t tk_len;
};

struct Lexer {
  struct Lexeme lexeme;
  const char* src;
  size_t pos;
  /* Coordinates of lexer */
  size_t line;
  size_t ccnt;
};

void LexerInit( struct Lexer* , const char* , size_t );
void LexerDestroy( struct Lexer* );
struct Lexeme* LexerNext( struct Lexer* );
#define LexerGetTokenName(LEX) TokenGetName((LEX)->lexeme.tk)

#define LexerIsIdRestChar(X) (isalnum((X)) || (X) == '_')
#define LexerIsIdInitChar(X) ((X) == '_' || isalpha((X)))

static SPARROW_INLINE
char LexerGetStringEscapeChar( int c ) {
  switch(c) {
    case 't': return '\t';
    case 'v': return '\v';
    case 'b': return '\b';
    case 'n': return '\n';
    case '\\':return '\\';
    case '\"':return '\"';
    default: return 0;
  }
}

static SPARROW_INLINE
char LexerGetStringUnescapeChar( int c ) {
  switch(c) {
    case '\t': return 't';
    case '\v': return 'v';
    case '\b': return 'b';
    case '\n': return 'n';
    case '\\': return '\\';
    case '"': return '"';
    default: return 0;
  }
}

void LexerUnescapeStringLiteral( struct StrBuf* sbuf , const char* str ,
    size_t length );

#define LexerLexeme(LEX) (&((LEX)->lexeme))
#define LexerLexemeStr(LEX) (&((LEX)->lexeme.str))
#define LexerLexemeNum(LEX) ((LEX)->lexeme.num)
#define LexerToken(LEX) ((LEX)->lexeme.tk)
#define LexerPosition(LEX) ((LEX)->pos)

#endif /* LEX_H_ */
