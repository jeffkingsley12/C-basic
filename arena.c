#ifndef TOKENIZER_H
#define TOKENIZER_H

#include <stdbool.h>
#include <stdint.h>
#include "ce_types.h"
#include "string_lib.h"
#include "string_pool.h"
#include "vector.h"
#include "list.h"
#include "dict.h"

typedef enum { VAL_INT, VAL_FLOAT, VAL_STRING, VAL_BOOLEAN, VAL_NONE } ValueType;

typedef struct {
    ValueType type;
    union {
        int64_t integer;
        double number;
        String *string;
        bool boolean;
    } as;
} Value;

typedef enum {
    TOKEN_NUMBER = 0,
    TOKEN_STRING,
    TOKEN_IDENTIFIER,
    TOKEN_DEF,
    TOKEN_IF,
    TOKEN_RETURN,
    TOKEN_EOF,
    TOKEN_ERROR
} TokenType;

typedef struct {
    TokenType type;
    String *lexeme;
    Value literal;
    int line;
    int column;
} Token;

typedef struct {
    const char *source;
    size_t source_len;
    size_t start;
    size_t current;
    int line;
    int column;
    StringPool *pool;
    Dictionary *keywords;
} Lexer;

static inline void lexer_init(Lexer *lexer, const char *source, size_t source_len,
                              StringPool *pool, Dictionary *keywords) {
    if (!lexer) return;
    lexer->source = source;
    lexer->source_len = source_len;
    lexer->start = 0;
    lexer->current = 0;
    lexer->line = 1;
    lexer->column = 0;
    lexer->pool = pool;
    lexer->keywords = keywords;
}

Value make_int(int64_t v);
Value make_float(double v);
Value make_string(String *v);
Value make_bool(bool v);
Value make_none(void);
void value_release(Value *v);
List *tokenize(Lexer *lexer);
void token_stream_free(List *stream);
Dictionary *keywords_create(StringPool *pool);

#endif /* TOKENIZER_H */
