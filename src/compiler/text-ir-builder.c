#include <compiler/text-ir-builder.h>

/* Grammar:
 * @@start := graph.
 * graph := GRAPH STRING LBRA node-section edge-section RBRA
 * node-section := NODE LBRA node-decalaration-list RBRA
 * node-declaration-list := EMPTY | (node-declaration)+
 * node-declaration := string LSQR attr-list RSQR
 * attr-list := EMPTY | attr*
 * attr := ( OPCODE ASSIGN STRING ) |
 *         ( EFFECT ASSIGN INTEGER) |
 *         ( PROP_EFFECT ASSIGN INTEGER) |
 *         ( DEAD ASSIGN INTEGER ) |
 *         ( INPUT_SIZE ASSIGN INTEGER ) |
 *         ( INPUT_MAX ASSIGN INTEGER ) |
 *         ( OUTPUT_SIZE ASSIGN INTEGER ) |
 *         ( OUTPUT_MAX ASSIGN INTEGER ) |
 *         ( BOUNDED ASSIGN INTEGER ) |
 *         ( VARIABLE ASSIGN value )
 *
 * edge-section := edge LBRA edge-declaration-list RBRA
 * edge-declaration-list := EMPTY | edge-decalaration*
 * edge-declaration := STRING arrow STRING
 * arrow := INPUT_ARROW | OUTPUT_ARROW
 * value := STRING | INTEGER
 */

#define IG_TOKEN_LIST(__) \
  __(GRAPH,"graph") \
  __(LBRA,"{" ) \
  __(RBRA,"}" ) \
  __(NODE,"node") \
  __(LSQR,"[") \
  __(RSQR,"]") \
  __(OPCODE,"opcode") \
  __(EFFECT,"effect") \
  __(PROP_EFFECT,"prop_effect") \
  __(DEAD,"dead") \
  __(INPUT_SIZE,"input_size") \
  __(OUTPUT_SIZE,"output_size") \
  __(INPUT_MAX,"input_max") \
  __(OUTPUT_MAX,"output_max") \
  __(BOUNDED,"bouded") \
  __(VARIABLE,"<variable>") \
  __(ASSIGN,"=") \
  __(INTEGER,"<integer>") \
  __(STRING,"<string>") \
  __(INPUT_ARROW,"<-") \
  __(OUTPUT_ARROW,"->") \
  __(ERROR,"<error>") \
  __(END,"<end>")

enum ig_token {
#define __(A,B) A,
  IG_TOKEN_LIST(__)
  SIZE_OF_TOKENS
#undef __ /* __ */
};

struct ig_lexeme {
  int int_value;
  const char* str_value;
  enum ig_token tk;
  size_t tk_len;
};

struct ig_lexer {
  const char* source;
  size_t pos;
  struct ig_lexeme lexeme;
};

static void ig_lexer_init( struct ig_lexer* lexer , const char* source ) {
  lexer->source = source;
  lexer->pos = 0;
  lexer->lexeme.tk = TK_ERROR;
  lexer->lexeme.tk_len = 0;
}

static int ig_lexer_next( struct ig_lexer* lexer ) {
  lexer->pos += lexer->lexeme.tk_len;
  do {
    int c = lexer->source[lexer->pos];
    switch(c) {
    }
  } while(1);
}










































