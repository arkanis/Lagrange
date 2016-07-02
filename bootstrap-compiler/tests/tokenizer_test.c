#include <string.h>
#include "../common.h"

#define SLIM_TEST_IMPLEMENTATION
#include "slim_test.h"


struct { char* code; size_t tokens_len; token_p tokens_ptr; } samples[] = {
	// Empty string
	{ "", 1, (token_t[]){
		{ .type = T_EOF,    .source = { 0, "" } }
	} },
	
	// Test normal one char and simple white space tokens
	{ "{ } ( ) , =", 12, (token_t[]){
		{ .type = T_CBO,    .source = { 1, "{" } },
		{ .type = T_WS,     .source = { 1, " " } },
		{ .type = T_CBC,    .source = { 1, "}" } },
		{ .type = T_WS,     .source = { 1, " " } },
		{ .type = T_RBO,    .source = { 1, "(" } },
		{ .type = T_WS,     .source = { 1, " " } },
		{ .type = T_RBC,    .source = { 1, ")" } },
		{ .type = T_WS,     .source = { 1, " " } },
		{ .type = T_COMMA,  .source = { 1, "," } },
		{ .type = T_WS,     .source = { 1, " " } },
		{ .type = T_ASSIGN, .source = { 1, "=" } },
		{ .type = T_EOF,    .source = { 0, "", } }
	} },
	
	// Test tab and new line (possible EOS) white space tokens
	{ " \t ( \n ) \t ", 6, (token_t[]){
		{ .type = T_WS,     .source = { 3, " \t " } },
		{ .type = T_RBO,    .source = { 1, "("    } },
		{ .type = T_WSNL,   .source = { 3, " \n " } },
		{ .type = T_RBC,    .source = { 1, ")"    } },
		{ .type = T_WS,     .source = { 3, " \t " } },
		{ .type = T_EOF,    .source = { 0, ""     } }
	} },
	
	// Integer literals
	{ "12345", 2, (token_t[]){
		{ .type = T_INT, .source = { 5, "12345" }, .int_val = 12345 },
		{ .type = T_EOF, .source = { 0, ""      }  }
	} },
	{ " 12345 ", 4, (token_t[]){
		{ .type = T_WS,  .source = { 1, " "     } },
		{ .type = T_INT, .source = { 5, "12345" }, .int_val = 12345 },
		{ .type = T_WS,  .source = { 1, " "     } },
		{ .type = T_EOF, .source = { 0, ""      } }
	} },
	
	// One line comments
	{ "// foo ", 2, (token_t[]){
		{ .type = T_COMMENT, .source = { 7, "// foo " } },
		{ .type = T_EOF,     .source = { 0, ""        } }
	} },
	{ " // foo \n ", 4, (token_t[]){
		{ .type = T_WS,      .source = { 1, " ",      } },
		{ .type = T_COMMENT, .source = { 7, "// foo " } },
		{ .type = T_WSNL,    .source = { 2, "\n ",    } },
		{ .type = T_EOF,     .source = { 0, "",       } }
	} },
	
	// Multiline comments
	{ "/* foo */", 2, (token_t[]){
		{ .type = T_COMMENT, .source = { 9, "/* foo */" } },
		{ .type = T_EOF,     .source = { 0, ""          } }
	} },
	{ "/**/", 2, (token_t[]){
		{ .type = T_COMMENT, .source = { 4, "/**/"      } },
		{ .type = T_EOF,     .source = { 0, ""          } }
	} },
	{ "/***/", 2, (token_t[]){
		{ .type = T_COMMENT, .source = { 5, "/***/"     } },
		{ .type = T_EOF,     .source = { 0, ""          } }
	} },
	{ "/****/", 2, (token_t[]){
		{ .type = T_COMMENT, .source = { 6, "/****/"    } },
		{ .type = T_EOF,     .source = { 0, ""          } }
	} },
	{ "/* s1 /* s2 /* foo */ e2 */ m1 /* s2 /* foo */ e2 */ m1 /*/*/*/*****/*/*/*/ e1  */", 2, (token_t[]){
		{ .type = T_COMMENT, .source = { 82, "/* s1 /* s2 /* foo */ e2 */ m1 /* s2 /* foo */ e2 */ m1 /*/*/*/*****/*/*/*/ e1  */" } },
		{ .type = T_EOF,     .source = { 0, ""          } }
	} },
	{ "/*", 2, (token_t[]){
		{ .type = T_ERROR,   .source = { 2, "/*"        } },
		{ .type = T_EOF,     .source = { 0, ""          } }
	} },
	{ " /**  /*", 3, (token_t[]){
		{ .type = T_WS,      .source = { 1, " "         } },
		{ .type = T_ERROR,   .source = { 7, "/**  /*"   } },
		{ .type = T_EOF,     .source = { 0, ""          } }
	} },
	{ " /* foo */ ", 4, (token_t[]){
		{ .type = T_WS,      .source = { 1, " "         } },
		{ .type = T_COMMENT, .source = { 9, "/* foo */" } },
		{ .type = T_WS,      .source = { 1, " "         } },
		{ .type = T_EOF,     .source = { 0, ""          } }
	} },
	{ " /***/ ", 4, (token_t[]){
		{ .type = T_WS,      .source = { 1, " "         } },
		{ .type = T_COMMENT, .source = { 5, "/***/"     } },
		{ .type = T_WS,      .source = { 1, " "         } },
		{ .type = T_EOF,     .source = { 0, ""          } }
	} },
	
	// Strings
	{ "\"foo\"", 2, (token_t[]){
		{ .type = T_STR, .source = { 5, "\"foo\""  }, .str_val = { 3, "foo" } },
		{ .type = T_EOF, .source = { 0, ""         } }
	} },
	{ "\"\\\\\"", 2, (token_t[]){
		{ .type = T_STR, .source = { 4, "\"\\\\\"" }, .str_val = { 1, "\\" } },
		{ .type = T_EOF, .source = { 0, ""         } }
	} },
	{ "\"\\t\"", 2, (token_t[]){
		{ .type = T_STR, .source = { 4, "\"\\t\""  }, .str_val = { 1, "\t" } },
		{ .type = T_EOF, .source = { 0, ""         } }
	} },
	{ "\"\\n\"", 2, (token_t[]){
		{ .type = T_STR, .source = { 4, "\"\\n\""  }, .str_val = { 1, "\n" } },
		{ .type = T_EOF, .source = { 0, ""         } }
	} },
	{ "\"\\\"\"", 2, (token_t[]){
		{ .type = T_STR, .source = { 4, "\"\\\"\"" }, .str_val = { 1, "\"" } },
		{ .type = T_EOF, .source = { 0, ""         } }
	} },
	{ "\"foo", 2, (token_t[]){
		{ .type = T_ERROR, .source = { 4, "\"foo"  } },
		{ .type = T_EOF,   .source = { 0, ""       } }
	} },
	{ "\"x\\", 2, (token_t[]){
		{ .type = T_ERROR, .source = { 3, "\"x\\"  } },
		{ .type = T_EOF,   .source = { 0, ""       } }
	} },
	/*
	{ "\"foo\\hbar\"", 2, (token_t[]){
		{ .type = T_ERROR, .source = { x, "\"foo\\hbar\"",  .source.len = 9, .str_len = 29, .str_val = "unknown escape code in string" },
		{ .type = T_EOF,   .source = { x, "",              .source.len = 0 }
	} },
	*/
	
	// IDs
	{ "foo", 2, (token_t[]){
		{ .type = T_ID,  .source = { 3, "foo" } },
		{ .type = T_EOF, .source = { 0, ""    } }
	} },
	{ "_12foo34", 2, (token_t[]){
		{ .type = T_ID,  .source = { 8, "_12foo34" } },
		{ .type = T_EOF, .source = { 0, ""         } }
	} },
	{ " foo ", 4, (token_t[]){
		{ .type = T_WS,  .source = { 1, " "   } },
		{ .type = T_ID,  .source = { 3, "foo" } },
		{ .type = T_WS,  .source = { 1, " "   } },
		{ .type = T_EOF, .source = { 0, ""    } }
	} },
	{ "foo bar", 4, (token_t[]){
		{ .type = T_ID,  .source = { 3, "foo" } },
		{ .type = T_WS,  .source = { 1, " "   } },
		{ .type = T_ID,  .source = { 3, "bar" } },
		{ .type = T_EOF, .source = { 0, ""    } }
	} },
	{ "+", 2, (token_t[]){
		{ .type = T_ADD, .source = { 1, "+"   } },
		{ .type = T_EOF, .source = { 0, ""    } }
	} },
	{ "foo+bar", 4, (token_t[]){
		{ .type = T_ID,  .source = { 3, "foo" } },
		{ .type = T_ADD, .source = { 1, "+"   } },
		{ .type = T_ID,  .source = { 3, "bar" } },
		{ .type = T_EOF, .source = { 0, ""    } }
	} },
	{ "-a+b*c/d", 9, (token_t[]){
		{ .type = T_SUB, .source = { 1, "-"   } },
		{ .type = T_ID,  .source = { 1, "a"   } },
		{ .type = T_ADD, .source = { 1, "+"   } },
		{ .type = T_ID,  .source = { 1, "b"   } },
		{ .type = T_MUL, .source = { 1, "*"   } },
		{ .type = T_ID,  .source = { 1, "c"   } },
		{ .type = T_DIV, .source = { 1, "/"   } },
		{ .type = T_ID,  .source = { 1, "d"   } },
		{ .type = T_EOF, .source = { 0, ""    } }
	} },
	
	// Unknown char error
	{ " $ ", 4, (token_t[]){
		{ .type = T_WS,    .source = { 1, " " } },
		{ .type = T_ERROR, .source = { 1, "$" } },
		{ .type = T_WS,    .source = { 1, " " } },
		{ .type = T_EOF,   .source = { 0, ""  } }
	} },
};

void test_samples() {
	for(size_t i = 0; i < sizeof(samples) / sizeof(samples[0]); i++) {
		char* code = samples[i].code;
		printf("test: %s\n", code);
		
		token_list_t tokens = { 0 };
		tokenize(str_from_c(code), &tokens, stderr);
		
		st_check_int(tokens.len, samples[i].tokens_len);
		for(size_t j = 0; j < samples[i].tokens_len; j++) {
			token_p actual_token = &tokens.ptr[j];
			token_p expected_token = &samples[i].tokens_ptr[j];
			
			st_check_int(actual_token->type, expected_token->type);
			st_check_int(actual_token->source.len, expected_token->source.len);
			st_check_strn(actual_token->source.ptr, expected_token->source.ptr, expected_token->source.len);
			if (actual_token->type == T_INT) {
				st_check(actual_token->int_val == expected_token->int_val);
			} else if (actual_token->type == T_ERROR) {
				// Check that an error message is present, exact content doesn't matter, will change anyway
				st_check(actual_token->str_val.len > 0);
				st_check_not_null(actual_token->str_val.ptr);
			} else if (expected_token->str_val.ptr != NULL) {
				st_check_int(actual_token->str_val.len, expected_token->str_val.len);
				st_check_strn(actual_token->str_val.ptr, expected_token->str_val.ptr, expected_token->str_val.len);
			} else {
				st_check_null(actual_token->str_val.ptr);
				st_check_int(actual_token->str_val.len, 0);
			}
		}
		
		for(size_t j = 0; j < tokens.len; j++)
			token_free(&tokens.ptr[j]);
		list_destroy(&tokens);
	}
}


int main() {
	st_run(test_samples);
	return st_show_report();
}