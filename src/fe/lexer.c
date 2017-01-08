#include "lexer.h"
#include <stdarg.h>
#include <assert.h>
#include <stdlib.h>
#include <errno.h>
#include <math.h>
#include <string.h>
#include <limits.h>

const char* TokenGetName( enum Token tk ) {
#define __(A,B) case A : return B;
  switch(tk) {
    TOKEN(__)
    default:
      return NULL;
  }
#undef __
}

void LexerInit( struct Lexer* lex , const char* src , size_t pos ) {
  lex->lexeme.tk = TK_ERROR;
  lex->lexeme.tk_len = 0;
  StrBufInit(&(lex->lexeme.str),128);
  lex->src = src;
  lex->pos = pos;
  lex->line = 0;
  lex->ccnt = 0;
}

void LexerDestroy( struct Lexer* lex ) {
  StrBufDestroy(LexerLexemeStr(lex));
}

#define yield(TK,LEN) \
  do { \
    lex->lexeme.tk = (TK); \
    lex->lexeme.tk_len = (LEN); \
    return LexerLexeme(lex); \
  } while(0)

static struct Lexeme*
_pred1( struct Lexer* lex , char lookahead ,
    enum Token tk1, enum Token tk2 ) {
  char achar = lex->src[lex->pos+1];
  if(achar == lookahead) {
    yield( tk1 , 2 );
  } else {
    yield( tk2 , 1 );
  }
}
#define pred1 return _pred1

static struct Lexeme*
_pred1err( struct Lexer* lex , char lookahead ,
    enum Token tk1 , enum Token tk2 , const char* fmt , ... ) {
  if(_pred1(lex,lookahead,tk1,tk2)->tk == TK_ERROR) {
    va_list vl;
    va_start(vl,fmt);
    StrBufVPrintF(LexerLexemeStr(lex),fmt,vl);
  }
  return &(lex->lexeme);
}
#define pred1err return _pred1err

static void
skip_comment( struct Lexer* lex ) {
  int c;
  assert(lex->src[lex->pos] == '/');
  assert(lex->src[lex->pos+1] == '/');
  lex->pos+=2;
  while((c = lex->src[lex->pos])) {
    if(c == '\n') break;
    ++lex->pos;
  }
  /* Skip the last line break if we have so,
   * otherwise we end up in <EOF> which we need
   * to put it back to stream for next token */
  if(c) {
    /* Last line is a comment , we should not have any
     * error later on so we just *DO NOT* update line
     * and ccount number */
    ++lex->pos;
    lex->ccnt = 1;
    lex->line++;
  }
}

static struct Lexeme*
lex_string( struct Lexer* lex ) {
  const char* start = lex->src + lex->pos;
  int c;
  StrBufClear(&(lex->lexeme.str));
  assert(*start == '\"'); ++start;
  while((c = *start)) {
    if(c == '\\') {
      /* Check if we have correct escape strings */
      int nc = *(start+1);
      int ec = LexerGetStringEscapeChar(nc);
      if(ec) {
        c = ec;
        ++start;
      }
    } else if(c == '"') {
      break;
    }
    StrBufPush(LexerLexemeStr(lex),c);
    ++start;
  }

  if(!c) {
    lex->lexeme.tk = TK_ERROR;
    lex->lexeme.tk_len = 0;
    StrBufPrintF(LexerLexemeStr(lex),"The string is not closed by \"!");
    return LexerLexeme(lex);
  } else {
    lex->lexeme.tk = TK_STRING;
    lex->lexeme.tk_len = start - (lex->src+lex->pos) + 1; /* skip " */
    return LexerLexeme(lex);
  }
}

static struct Lexeme*
lex_number( struct Lexer* lex ) {
  const char* start = lex->src + lex->pos;
  char* pend;
  errno = 0;
  double d = strtod(start,&pend);
  if(errno) {
    lex->lexeme.tk = TK_ERROR;
    StrBufPrintF(LexerLexemeStr(lex),
        "Cannot convert current token to *number* due to %s",
        strerror(errno));
    return LexerLexeme(lex);
  } else {
    lex->lexeme.tk = TK_NUMBER;
    lex->lexeme.tk_len = (pend - start);
    LexerLexemeNum(lex) = d;
    return LexerLexeme(lex);
  }
}

#define MAX_KEYWORD_LEN 11

static int
kw_comp( const char* L , const char* R ) {
  int i;
  for( i = 0 ; i < MAX_KEYWORD_LEN ; ++i ) {
    char lc = L[i];
    char rc = R[i];
    if(!rc)
      return i; /* Success path , consumed *all* R string */
    else if( lc == rc )
      continue;
    else
      return -1;
  }
  assert(0); return -1;
}

static int
kw_check( struct Lexer* lex , const char* R ) {
  int i;
  if((i=kw_comp(lex->src + lex->pos+1,R)) > 0 &&
      !LexerIsIdRestChar(lex->src[1+i+lex->pos]))
    return 1;
  else
    return 0;
}

static struct Lexeme*
lex_var( struct Lexer* lex ) {
  StrBufClear(LexerLexemeStr(lex)); /* Clear the lexeme buffer */
  const char* start = lex->src + lex->pos;
  if(!LexerIsIdInitChar(*start)) {
    StrBufPrintF(LexerLexemeStr(lex),
        "Unrecognized token here , with character %c!",*start);
    lex->lexeme.tk = TK_ERROR;
    lex->lexeme.tk_len = 0;
    return LexerLexeme(lex);
  }

  while( LexerIsIdRestChar(*start) )
    StrBufPush(LexerLexemeStr(lex),*(start++));

  lex->lexeme.tk = TK_VARIABLE;
  lex->lexeme.tk_len = ( start - (lex->src + lex->pos) );
  return LexerLexeme(lex);
}

static struct Lexeme*
lex_var_or_kw( struct Lexer* lex ) {
  int c = lex->src[lex->pos];
  switch(c) {
    case 'b':
      if(kw_check(lex,"reak"))
        yield(TK_BREAK,5);
      else
        return lex_var(lex);
    case 'c':
      if(kw_check(lex,"ontinue"))
        yield(TK_CONTINUE,8);
      else
        return lex_var(lex);
    case 'e':
      if(kw_check(lex,"lif"))
        yield(TK_ELIF,4);
      else if(kw_check(lex,"lse"))
        yield(TK_ELSE,4);
      else
        return lex_var(lex);
    case 'f':
      if(kw_check(lex,"alse"))
        yield(TK_FALSE,5);
      else if(kw_check(lex,"unction"))
        yield(TK_FUNCTION,8);
      else if(kw_check(lex,"or"))
        yield(TK_FOR,3);
      else
        return lex_var(lex);
    case 'n':
      if(kw_check(lex,"ull"))
        yield(TK_NULL,4);
      else
        return lex_var(lex);
    case 'i':
      if(kw_check(lex,"f"))
        yield(TK_IF,2);
      else if(kw_check(lex,"n"))
        yield(TK_IN,2);
      else
        return lex_var(lex);
    case 'r':
      if(kw_check(lex,"eturn"))
        yield(TK_RETURN,6);
      else
        return lex_var(lex);
    case 't':
      if(kw_check(lex,"rue"))
        yield(TK_TRUE,4);
      else
        return lex_var(lex);
    case 'v':
      if(kw_check(lex,"ar"))
        yield(TK_VAR,3);
      else
        return lex_var(lex);
    case 'w':
      if(kw_check(lex,"hile"))
        yield(TK_WHILE,5);
      else
        return lex_var(lex);
    default:
      return lex_var(lex);
  }
}

struct Lexeme* LexerNext( struct Lexer* lex ) {
  lex->ccnt += lex->lexeme.tk_len;
  lex->pos += lex->lexeme.tk_len;
  do {
    switch(lex->src[lex->pos]) {
      case '+': yield(TK_ADD,1);
      case '-': yield(TK_SUB,1);
      case '*': yield(TK_MUL,1);
      case '/':
        {
          /* Probing for the comments */
          char nchar = lex->src[lex->pos+1];
          if(nchar == '/') {
            skip_comment(lex);
            continue;
          } else {
            yield(TK_DIV,1);
          }
        }
      case '^': yield(TK_POW,1);
      case '%': yield(TK_MOD,1);
      case '.': yield(TK_DOT,1);
      case ':': yield(TK_COLON,1);
      case ',': yield(TK_COMMA,1);
      case ';': yield(TK_SEMICOLON,1);
      case '<': pred1( lex , '=' , TK_LE , TK_LT );
      case '>': pred1( lex , '=' , TK_GE , TK_GT );
      case '=': pred1( lex , '=' , TK_EQ , TK_ASSIGN );
      case '!': pred1( lex , '=' , TK_NE , TK_NOT );
      case '&': pred1err( lex , '&' , TK_AND , TK_ERROR ,
                    "You specify character \"&\" which is not a valid token,"
                    "do you mean \"&&\"?");
      case '|': pred1err( lex , '|' , TK_OR , TK_ERROR ,
                    "You specify character \"|\" which is not a valid token,"
                    "do you mean \"||\"?");
      case '(': yield(TK_LPAR,1);
      case ')': yield(TK_RPAR,1);
      case '[': yield(TK_LSQR,1);
      case ']': yield(TK_RSQR,1);
      case '{': yield(TK_LBRA,1);
      case '}': yield(TK_RBRA,1);
      case '0':case '1':case '2':case '3':case '4':
      case '5':case '6':case '7':case '8':case '9':
        return lex_number(lex);
      case '\"':
        return lex_string(lex);
      case 0: yield(TK_EOF,0);
      case ' ':case '\t':case '\b':case '\v':
        ++lex->pos;
        ++lex->ccnt;
        continue;
      case '\n':
        ++lex->pos;
        ++lex->line; lex->ccnt = 1;
        continue;
      default:
        return lex_var_or_kw(lex);
    }
  } while(1);
  assert(0); return NULL;
}
