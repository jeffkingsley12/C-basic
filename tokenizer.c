#include "tokenizer.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

/* Offset by +1 when storing TokenType in void* so enum value 0 is distinguishable from NULL */
#define BOX_TOKEN_TYPE(t)   ((void*)((uintptr_t)(t) + 1u))
#define UNBOX_TOKEN_TYPE(p) ((TokenType)((uintptr_t)(p) - 1u))

Value make_int(int64_t v)    { return (Value){VAL_INT, .as.integer = v}; }
Value make_float(double v)   { return (Value){VAL_FLOAT, .as.number = v}; }
Value make_bool(bool v)      { return (Value){VAL_BOOLEAN, .as.boolean = v}; }
Value make_none(void)        { return (Value){VAL_NONE, .as.integer = 0}; }
Value make_string(String *v) { return (Value){VAL_STRING, .as.string = v}; }

void value_release(Value *v) {
    if (v && v->type == VAL_STRING && v->as.string) {
        string_unref(v->as.string);
        v->as.string = NULL;
    }
}

static void token_destroy_cb(void *item, Allocator *alloc) {
    Token *token = (Token*)item;
    if (!token) return;
    value_release(&token->literal);
    string_unref(token->lexeme);
    alloc->alloc(token, 0, alloc->context);
}

static bool emit_token(List *stream, TokenType type, String *lexeme, 
                       Value literal, int line, int col, Allocator *alloc) {
    Token *token = (Token*)alloc->alloc(NULL, sizeof(Token), alloc->context);
    if (!token) {
        string_unref(lexeme);
        value_release(&literal);
        return false;
    }
    
    token->type = type;
    token->lexeme = lexeme;
    token->literal = literal;
    token->line = line;
    token->column = col;
    
    if (!list_append(stream, token)) {
        token_destroy_cb(token, alloc);
        return false;
    }
    return true;
}

List* tokenize(Lexer *lexer) {
    if (!lexer || !lexer->pool || !lexer->keywords) return NULL;
    if (lexer->source_len > 0 && !lexer->source) return NULL;
    
    Allocator *alloc = lexer->pool->allocator;
    List *token_stream = list_create(0, alloc, NULL);
    if (!token_stream) return NULL;

    while (lexer->current < lexer->source_len) {
        lexer->start = lexer->current;
        char c = lexer->source[lexer->current];

        // 1. Whitespace
        if (isspace((unsigned char)c)) {
            if (c == '\n') { 
                lexer->line++; 
                lexer->column = 0; 
            } else { 
                lexer->column++; 
            }
            lexer->current++;
            continue;
        }

        // 2. String Literals ("...")
        if (c == '"') {
            int start_col = lexer->column;
            lexer->current++;
            lexer->column++;
            size_t str_start = lexer->current;

            while (lexer->current < lexer->source_len && lexer->source[lexer->current] != '"') {
                if (lexer->source[lexer->current] == '\n') {
                    lexer->line++;
                    lexer->column = 0;
                } else {
                    lexer->column++;
                }
                lexer->current++;
            }

            if (lexer->current >= lexer->source_len) {
                // Unterminated string literal -> emit error token
                String *err_str = string_create_with_len(&lexer->source[lexer->start], lexer->current - lexer->start, alloc);
                if (!err_str) goto oom_failure;
                String *interned_err = string_pool_intern(lexer->pool, err_str);
                if (!interned_err) { string_unref(err_str); goto oom_failure; }
                if (!emit_token(token_stream, TOKEN_ERROR, interned_err, make_none(), lexer->line, start_col, alloc))
                    goto oom_failure;
                break;
            }

            size_t str_len = lexer->current - str_start;
            lexer->current++; // Consume closing quote
            lexer->column++;

            String *raw_str = string_create_with_len(&lexer->source[str_start], str_len, alloc);
            if (!raw_str) goto oom_failure;

            String *interned = string_pool_intern(lexer->pool, raw_str);
            if (!interned) {
                string_unref(raw_str);
                goto oom_failure;
            }

            string_ref(interned);
            if (!emit_token(token_stream, TOKEN_STRING, interned, make_string(interned), lexer->line, start_col, alloc)) {
                string_unref(interned);
                goto oom_failure;
            }
            continue;
        }

        // 3. Numbers (Integer & Float)
        if (isdigit((unsigned char)c)) {
            int start_col = lexer->column;
            
            while (lexer->current < lexer->source_len && isdigit((unsigned char)lexer->source[lexer->current])) {
                lexer->current++;
                lexer->column++;
            }
            
            bool is_float = false;
            if (lexer->current < lexer->source_len && lexer->source[lexer->current] == '.') {
                if (lexer->current + 1 < lexer->source_len && isdigit((unsigned char)lexer->source[lexer->current + 1])) {
                    is_float = true;
                    lexer->current++;
                    lexer->column++;
                    while (lexer->current < lexer->source_len && isdigit((unsigned char)lexer->source[lexer->current])) {
                        lexer->current++;
                        lexer->column++;
                    }
                }
            }
            
            size_t len = lexer->current - lexer->start;
            String *raw_str = string_create_with_len(&lexer->source[lexer->start], len, alloc);
            if (!raw_str) goto oom_failure;
            
            String *interned = string_pool_intern(lexer->pool, raw_str);
            if (!interned) {
                string_unref(raw_str);
                goto oom_failure;
            }
            
            /* Parse from null-terminated interned string to prevent over-reading */
            const char *num_cstr = string_c_str(interned);
            Value lit = is_float ? make_float(strtod(num_cstr, NULL)) 
                                 : make_int(strtoll(num_cstr, NULL, 10));

            if (!emit_token(token_stream, TOKEN_NUMBER, interned, lit, lexer->line, start_col, alloc)) 
                goto oom_failure;
                
            continue;
        }

        // 4. Identifiers & Keywords
        if (isalpha((unsigned char)c) || c == '_') {
            int start_col = lexer->column;

            while (lexer->current < lexer->source_len && 
                  (isalnum((unsigned char)lexer->source[lexer->current]) || lexer->source[lexer->current] == '_')) {
                lexer->current++;
                lexer->column++;
            }

            size_t len = lexer->current - lexer->start;
            String *raw_str = string_create_with_len(&lexer->source[lexer->start], len, alloc);
            if (!raw_str) goto oom_failure;
            
            String *interned = string_pool_intern(lexer->pool, raw_str);
            if (!interned) {
                string_unref(raw_str);
                goto oom_failure;
            }
            
            TokenType type = TOKEN_IDENTIFIER;
            void *keyword_box = dict_get_interned_string(lexer->keywords, interned);
            if (keyword_box) {
                type = UNBOX_TOKEN_TYPE(keyword_box);
            }
            
            if (!emit_token(token_stream, type, interned, make_none(), lexer->line, start_col, alloc)) 
                goto oom_failure;
                
            continue;
        }

        // 5. Single-character symbols / fallback (Errors)
        int start_col = lexer->column;
        String *err_str = string_create_with_len(&lexer->source[lexer->start], 1, alloc);
        if (!err_str) goto oom_failure;
        
        String *interned_err = string_pool_intern(lexer->pool, err_str);
        if (!interned_err) {
            string_unref(err_str);
            goto oom_failure;
        }
        
        if (!emit_token(token_stream, TOKEN_ERROR, interned_err, make_none(), lexer->line, start_col, alloc)) 
            goto oom_failure;
            
        lexer->current++;
        lexer->column++;
    }

    // 6. Explicit Sentinel Stream Termination (EOF)
    int start_col = lexer->column;
    String *eof_str = string_create_with_len("", 0, alloc);
    if (!eof_str) goto oom_failure;
    
    String *interned_eof = string_pool_intern(lexer->pool, eof_str);
    if (!interned_eof) {
        string_unref(eof_str);
        goto oom_failure;
    }
    
    if (!emit_token(token_stream, TOKEN_EOF, interned_eof, make_none(), lexer->line, start_col, alloc)) {
        goto oom_failure;
    }

    return token_stream;

oom_failure:
    token_stream_free(token_stream);
    return NULL;
}

void token_stream_free(List *stream) {
    if (!stream) return;
    Allocator *alloc = stream->allocator;
    for (size_t i = 0; i < list_length(stream); ++i) {
        Token *t = (Token*)list_get(stream, i);
        if (t) {
            token_destroy_cb(t, alloc);
        }
    }
    list_free(stream);
}

Dictionary* keywords_create(StringPool *pool) {
    if (!pool || !pool->allocator) return NULL;
    Allocator *alloc = pool->allocator;
    Dictionary *kw = dict_create(32, alloc, NULL, 0); 
    if (!kw) return NULL;

    #define ADD_KW(kw_str, kw_type) \
        do { \
            String *s = string_create_with_len(kw_str, sizeof(kw_str) - 1, alloc); \
            if (!s) goto initialization_failure; \
            String *interned = string_pool_intern(pool, s); \
            if (!interned) { string_unref(s); goto initialization_failure; } \
            if (!dict_set_string_key(kw, interned, BOX_TOKEN_TYPE(kw_type))) { \
                string_unref(interned); \
                goto initialization_failure; \
            } \
            string_unref(interned); \
        } while(0)

    ADD_KW("def", TOKEN_DEF);
    ADD_KW("if", TOKEN_IF);
    ADD_KW("return", TOKEN_RETURN);
    
    #undef ADD_KW
    return kw;

initialization_failure:
    dict_free(kw);
    return NULL;
}
