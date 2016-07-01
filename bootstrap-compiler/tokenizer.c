#include "common.h"


typedef struct {
	str_t        source;
	size_t       pos;
	token_list_p tokens;
	FILE*        error_stream;
	size_t       error_count;
} tokenizer_ctx_t, *tokenizer_ctx_p;



//
// Private tokenizer support functions
//

static int peek_at_offset(tokenizer_ctx_p ctx, size_t offset) {
	if ((ssize_t)ctx->pos + offset >= ctx->source.len)
		return EOF;
	return ctx->source.ptr[ctx->pos + offset];
}

static int peek1(tokenizer_ctx_p ctx) {
	return peek_at_offset(ctx, 0);
}

static int peek2(tokenizer_ctx_p ctx) {
	return peek_at_offset(ctx, 1);
}

static int peek3(tokenizer_ctx_p ctx) {
	return peek_at_offset(ctx, 1);
}

static token_t new_token(tokenizer_ctx_p ctx, token_type_t type, int chars_to_consume) {
	if ((ssize_t)ctx->pos + chars_to_consume > ctx->source.len) {
		fprintf(ctx->error_stream, "Tried to consume a char beyond EOF!\n");
		abort();
	}
	
	token_t token = (token_t){
		.type   = type,
		.source = str_from_mem(ctx->source.ptr + ctx->pos, chars_to_consume)
	};
	ctx->pos += chars_to_consume;
	return token;
}

static void consume_into_token(tokenizer_ctx_p ctx, token_p token, int chars_to_consume) {
	if ((ssize_t)ctx->pos + chars_to_consume > ctx->source.len) {
		fprintf(ctx->error_stream, "Tried to consume a char beyond EOF!\n");
		abort();
	} else if (ctx->source.ptr + ctx->pos != token->source.ptr + token->source.len) {
		fprintf(ctx->error_stream, "Tried to put a char into a token thats end isn't the current tokenizer position!\n");
		abort();
	}
	
	ctx->pos          += chars_to_consume;
	token->source.len += chars_to_consume;
}

static void append_token(tokenizer_ctx_p ctx, token_t token) {
	list_append(&ctx->tokens, token);
}

// append_token_str, replace with str_putc(&token->str_val, c);

static void make_into_error_token(token_p token, char* message) {
	token->type = T_ERROR;
	token->str_val = str_from_c(message);
}

static void new_error_token(tokenizer_ctx_p ctx, ssize_t start_offset, size_t length, char* message) {
	if ( (ssize_t)ctx->pos + start_offset < 0 || (ssize_t)ctx->pos + start_offset + length > ctx->source.len ) {
		fprintf(ctx->error_stream, "The error token contains bytes outside of the source string!\n");
		abort();
	}
	
	return (token_t){
		.type   = T_ERROR,
		.source = str_from_mem(ctx->source.ptr + ctx->pos + start_offset, length)
	};
}



//
// Tokenizer
//

static bool next_token(tokenizer_ctx_p ctx);
static void tokenize_one_line_comment(tokenizer_ctx_p ctx);
static void tokenize_nested_multiline_comment(tokenizer_ctx_p ctx);
static void tokenize_string(tokenizer_ctx_p ctx);

size_t tokenize(str_t source, token_list_p tokens, FILE* error_stream) {
	tokenizer_ctx_t ctx = (tokenizer_ctx_t){
		.source = source,
		.pos    = 0,
		.tokens = tokens,
		.error_stream = error_stream,
		.error_count  = 0
	};
	
	while ( next_token(&ctx) ) { }
	
	return ctx.error_count;
}

static bool next_token(tokenizer_ctx_p ctx) {
	int c = peek1(ctx);
	int c2 = peek2(ctx);
	int c3 = peek3(ctx);
	switch(c) {
		case EOF:  append_token(ctx, new_token(ctx, T_EOF, 0));     return;
		case '{':  append_token(ctx, new_token(ctx, T_CBO, 1));     return;
		case '}':  append_token(ctx, new_token(ctx, T_CBC, 1));     return;
		case '(':  append_token(ctx, new_token(ctx, T_RBO, 1));     return;
		case ')':  append_token(ctx, new_token(ctx, T_RBC, 1));     return;
		case ',':  append_token(ctx, new_token(ctx, T_COMMA, 1));   return;
		case '.':  append_token(ctx, new_token(ctx, T_PERIOD, 1));  return;
		case '~':  append_token(ctx, new_token(ctx, T_COMPL, 1));   return;
		case '"':  tokenize_string(ctx);  return;
		
		case '+':
			switch(c2) {
				case '=':  append_token(ctx, new_token(ctx, T_ADD_ASSIGN, 2));  return;
				default:   append_token(ctx, new_token(ctx, T_ADD, 1));         return;
			}
		case '-':
			switch(c2) {
				case '=':  append_token(ctx, new_token(ctx, T_SUB_ASSIGN, 2));  return;
				default:   append_token(ctx, new_token(ctx, T_SUB, 1));         return;
			}
		case '*':
			switch(c2) {
				case '=':  append_token(ctx, new_token(ctx, T_MUL_ASSIGN, 2));  return;
				default:   append_token(ctx, new_token(ctx, T_MUL, 1));         return;
			}
		case '/':
			switch(c2) {
				case '/':  tokenize_one_line_comment(ctx);                      return;
				case '*':  tokenize_nested_multiline_comment(ctx);              return;
				case '=':  append_token(ctx, new_token(ctx, T_DIV_ASSIGN, 2));  return;
				default:   append_token(ctx, new_token(ctx, T_DIV, 1));         return;
			}
		case '%':
			switch(c2) {
				case '=':  append_token(ctx, new_token(ctx, T_MOD_ASSIGN, 2);   return;
				default:   append_token(ctx, new_token(ctx, T_MOD, 1);          return;
			}
		
		case '<':
			switch(c2) {
				case '<':
					switch(c3) {
						case '=':  append_token(ctx, new_token(ctx, T_SL_ASSIGN, 3));  return;
						default:   append_token(ctx, new_token(ctx, T_SL, 2));         return;
					}
				case '=':  append_token(ctx, new_token(ctx, T_LE, 2));  return;
				default:   append_token(ctx, new_token(ctx, T_LT, 1));  return;
			}
		case '>':
			switch(c2) {
				case '>':
					switch(c3) {
						case '=':  append_token(ctx, new_token(ctx, T_SR_ASSIGN, 3));  return;
						default:   append_token(ctx, new_token(ctx, T_SR, 2));         return;
					}
				case '=':  append_token(ctx, new_token(ctx, T_GE, 2));  return;
				default:   append_token(ctx, new_token(ctx, T_GT, 1));  return;
			}
		
		case '&':
			switch(c2) {
				case '&':  append_token(ctx, new_token(ctx, T_AND, 2));             return;
				case '=':  append_token(ctx, new_token(ctx, T_BIN_AND_ASSIGN, 2));  return;
				default:   append_token(ctx, new_token(ctx, T_BIN_AND, 1));         return;
			}
		case '|':
			switch(c2) {
				case '|':  append_token(ctx, new_token(ctx, T_OR, 2));             return;
				case '=':  append_token(ctx, new_token(ctx, T_BIN_OR_ASSIGN, 2));  return;
				default:   append_token(ctx, new_token(ctx, T_BIN_OR, 1));         return;
			}
		case '^':
			switch(c2) {
				case '=':  append_token(ctx, new_token(ctx, T_BIN_XOR_ASSIGN, 2));  return;
				default:   append_token(ctx, new_token(ctx, T_BIN_XOR, 1));         return;
			}
		
		case '=':
			switch(c2) {
				case '=':  append_token(ctx, new_token(ctx, T_EQ, 2));      return;
				default:   append_token(ctx, new_token(ctx, T_ASSIGN, 1));  return;
			}
		case '!':
			switch(c2) {
				case '=':  append_token(ctx, new_token(ctx, T_NEQ, 2));  return;
				default:   append_token(ctx, new_token(ctx, T_NOT, 1));  return;
			}
	}
	
	if ( isspace(c) ) {
		token_t t = new_token(ctx, (c == '\n') ? T_WSNL : T_WS, 1);
		
		while ( isspace(c = peek1(ctx)) ) {
			consume_into_token(ctx, &t, 1);
			// If a white space token contains a new line it becomes a possible end of statement
			if (c == '\n')
				t.type = T_WSNL;
		}
		
		append_token(ctx, t);
		return;
	}
	
	// TODO: Handle numbers starting with a "0" prefix (e.g. 0b... 0o... 0x...)
	// But also "0" itself.
	if ( isdigit(c) ) {
		token_t t = new_token(ctx, T_INT, 1);
		
		int value = c - '0';
		while ( isdigit(c = peek1(ctx)) ) {
			consume_into_token(ctx, &t, 1);
			value = value * 10 + (c - '0');
		}
		
		t.int_val = value;
		append_token(ctx, t);
		return;
	}
	
	if ( isalpha(c) || c == '_' ) {
		token_t t = new_token(ctx, T_ID, 1);
		
		while ( c = peek1(ctx), (isalnum(c) || c == '_') )
			consume_into_token(ctx, &t, 1);
		
		for(size_t i = 0; i < sizeof(keywords) / sizeof(keywords[0]); i++) {
			if ( strncmp(t.src.ptr, keywords[i].keyword, t.src.len) == 0 && (int)strlen(keywords[i].keyword) == t.src.len ) {
				t.type = keywords[i].type;
				break;
			}
		}
		
		append_token(ctx, t);
		return;
	}
	
	// Abort on any unknown char. Ignoring them will just get us surprised...
	token_t t = new_token(ctx, T_ERROR, 1);
	make_into_error_token(&t, "stray character in source code");
	append_token(ctx, t);
	return;
}

// Function is called when "//" was peeked. So it's safe to consume 2 chars
// right away.
static void tokenize_one_line_comment(tokenizer_ctx_p ctx) {
	token_t t = new_token(ctx, T_COMMENT, 2);
	
	int c;
	while ( c = peek1(ctx), !(c == '\n' || c == EOF) )
		consume_into_token(ctx, &t, 1);
	
	append_token(ctx, t);
	return;
}

// Function is called when "/*" was peeked. So it's safe to consume 2 chars
// right away.
static void tokenize_nested_multiline_comment(tokenizer_ctx_p ctx) {
	token_t t = new_token(ctx, T_COMMENT, 2);
	
	int nesting_level = 1;
	while ( nesting_level > 0 ) {
		if ( peek1(ctx) == '*' && peek2(ctx) == '/' ) {
			nesting_level--;
			consume_into_token(ctx, &t, 2);
		} else if ( peek1(ctx) == '/' && peek2(ctx) == '*' ) {
			nesting_level++;
			consume_into_token(ctx, &t, 2);
		} else if ( peek1(ctx) == EOF ) {
			make_into_error_token(&t, "unterminated multiline comment");
			break;
		} else {
			consume_into_token(ctx, &t, 1);
		}
	}
	
	append_token(ctx, t);
	return;
}

// Function is called when '"' was peeked. So it's safe to consume one char
// right away.
static void tokenize_string(tokenizer_ctx_p ctx) {
	token_t t = new_token(ctx, T_STR, 1);
	
	while (true) {
		int c = peek1(ctx);
		consume_into_token(ctx, &t, 1);
		
		switch(c) {
			case '"':
				append_token(ctx, t);
				return;
			default:
				str_putc(&t->str_val, c);
				break;
				
			case EOF:
				make_into_error_token(&t, "unterminated string");
				append_token(ctx, t);
				return;
			case '\\':
				c = peek1(ctx);
				consume_into_token(ctx, &t, 1);
				switch(c) {
					case EOF:
						make_into_error_token(&t, "unterminated escape code in string");
						append_token(ctx, t);
						return;
					case '\\':
						str_putc(&t->str_val, '\\');
						break;
					case '"':
						str_putc(&t->str_val, '"');
						break;
					case 'n':
						str_putc(&t->str_val, '\n');
						break;
					case 't':
						str_putc(&t->str_val, '\t');
						break;
					default:
						// Just report the invalid escape code as error token
						append_token(ctx, new_error_token(ctx, -2, 2, "unknown escape code in string"));
						break;
				}
				break;
		}
	}
}



//
// Utility functions
//

void token_free(token_p token) {
	token_p t = token;
	switch(token->type) {
		#define _
		#define TOKEN(id, free_expr) case id: free_expr; break;
		#include "token_spec.h"
		#undef TOKEN
		#undef _
	}
}

int token_line(module_p module, token_p token) {
	int line = 1;
	for(char* c = token->source.ptr; c >= module->source.ptr; c--) {
		if (*c == '\n')
			line++;
	}
	return line;
}

int token_col(module_p module, token_p token) {
	int col = 0;
	for(char* c = token->source.ptr; *c != '\n' && c >= module->source.ptr; c--)
		col++;
	return col;
}