#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include "parser.h"


//
// Parser state and utility stuff
//

struct parser_s {
	token_list_p list;
	size_t pos;
};


int next_filtered_token_at(parser_p parser, size_t pos, bool ignore_ws_eos) {
	size_t offset = 0;
	while (pos + offset < (size_t)parser->list->tokens_len) {
		token_type_t type = parser->list->tokens_ptr[pos + offset].type;
		// Return the offset if we found a filtered token there
		if ( !( type == T_WS || type == T_COMMENT || (ignore_ws_eos && type == T_WSNL) ) )
			return offset;
		offset++;
	}
	
	// We're either beyond the last token or found no filtered token beyond pos
	return -1;
}

#define consume(parser)          consume_impl((parser), true,  __FUNCTION__, __LINE__)
#define consume_with_eos(parser) consume_impl((parser), false, __FUNCTION__, __LINE__)
token_p consume_impl(parser_p parser, bool ignore_ws_eos, const char* caller, int line) {
	int offset = next_filtered_token_at(parser, parser->pos, ignore_ws_eos);
	// Return NULL if there are no more filtered tokens
	if (offset == -1)
		return NULL;
	
	parser->pos += offset;
	token_p current_token = &parser->list->tokens_ptr[parser->pos];
	parser->pos++;
	
	printf("%s:%d consume ", caller, line);
	token_print(stdout, current_token, TP_INLINE_DUMP);
	printf("\n");
	
	return current_token;
}

#define peek(parser)          peek_impl((parser), true,  __FUNCTION__, __LINE__)
#define peek_with_eos(parser) peek_impl((parser), false, __FUNCTION__, __LINE__)
token_p peek_impl(parser_p parser, bool ignore_ws_eos, const char* caller, int line) {
	int offset = next_filtered_token_at(parser, parser->pos, ignore_ws_eos);
	// Return NULL if there are no more filtered tokens
	if (offset == -1)
		return NULL;
	
	token_p current_token = &parser->list->tokens_ptr[parser->pos + offset];
	
	printf("%s:%d peek ", caller, line);
	token_print(stdout, current_token, TP_INLINE_DUMP);
	printf("\n");
	
	return current_token;
}

#define peek_type(parser)          peek_type_impl((parser), true,  __FUNCTION__, __LINE__)
#define peek_type_with_eos(parser) peek_type_impl((parser), false, __FUNCTION__, __LINE__)
token_type_t peek_type_impl(parser_p parser, bool ignore_ws_eos, const char* caller, int line) {
	return peek_impl(parser, ignore_ws_eos, caller, line)->type;
}

#define consume_type(parser, type) consume_type_impl((parser), (type), __FUNCTION__, __LINE__)
token_p consume_type_impl(parser_p parser, token_type_t type, const char* caller, int line) {
	token_p t = consume_impl(parser, (type != T_WSNL), caller, line);
	if (t->type == type)
		return t;
	
	token_p current_token = &parser->list->tokens_ptr[parser->pos-1];
	
	printf("%s:%d consume_type, expected %d, got ", caller, line, type);
	token_print(stdout, current_token, TP_INLINE_DUMP);
	printf("\n");
	
	abort();
}



//
// Parser rules
//

static bool is_stmt_start(token_type_t type);
static bool is_expr_start(token_type_t type);
node_p parse_expr_ex(parser_p parser, bool collect_uops);


//
// Definitions
//

node_p parse(token_list_p list, parser_rule_func_t rule) {
	parser_t parser = { list, 0 };
	return rule(&parser);
}



//
// Statements
//

static bool is_stmt_start(token_type_t type) {
	switch(type) {
		case T_SYSCALL:
			return true;
		default:
			return is_expr_start(type);
	}
}

void parse_eos(parser_p parser) {
	token_p t = consume_with_eos(parser);
	if ( !(t->type == T_WSNL || t->type == T_EOF) ) {
		printf("%s:%d:%d: expectet WSNL or EOF, got:\n", parser->list->filename, token_line(t), token_col(t));
		token_print_line(stderr, t, 0);
		abort();
	}
}

node_p parse_stmt(parser_p parser) {
	token_p t = consume(parser);
	if (t->type == T_SYSCALL) {
		node_p expr = parse_expr(parser);
		parse_eos(parser);
		
		node_p stmt = node_alloc(NT_SYSCALL);
		stmt->syscall.expr = expr;
		expr->parent = stmt;
		return stmt;
	}
	
	printf("%s:%d:%d: expectet 'syscall', got:\n", parser->list->filename, token_line(t), token_col(t));
	token_print_line(stderr, t, 0);
	abort();
	
	return NULL;
}


//
// Expressions
//

static bool is_expr_start(token_type_t type) {
	switch(type) {
		case T_ID:
		case T_INT:
		case T_STR:
		case T_RBO:
			return true;
		default:
			return false;
	}
}

node_p parse_expr_without_trailing_ops(parser_p parser) {
	node_p node = NULL;
	
	token_p t = consume(parser);
	switch(t->type) {
		case T_ID:
			node = node_alloc(NT_ID);
			node->id.name.ptr = t->src_str;
			node->id.name.len = t->src_len;
			break;
		case T_INT:
			node = node_alloc(NT_INTL);
			node->intl.value = t->int_val;
			break;
		case T_STR:
			node = node_alloc(NT_STRL);
			node->strl.value.ptr = t->str_val;
			node->strl.value.len = t->str_len;
			break;
		case T_RBO:
			// TODO: remember somehow that this node was surrounded by brackets!
			// Otherwise exact syntax reconstruction will be impossible.
			node = parse_expr(parser);
			consume_type(parser, T_RBC);
			break;
		default:
			printf("%s:%d:%d: expectet ID, INT or STR, got:\n", parser->list->filename, token_line(t), token_col(t));
			token_print_line(stderr, t, 0);
			abort();
			return NULL;
	}
	
	return node;
}

node_p parse_expr(parser_p parser) {
	node_p node = parse_expr_without_trailing_ops(parser);
	
	if (peek_type(parser) == T_ID) {
		// Got an operator, wrap everything into an uops node and collect the
		// remaining operators and expressions.
		node_p uops = node_alloc(NT_UOPS);
		node->parent = uops;
		buf_append(&uops->uops.list, node);
		
		while (peek_type(parser) == T_ID) {
			token_p id = consume_type(parser, T_ID);
			
			node_p op_node = node_alloc_append(NT_ID, uops, &uops->uops.list);
			op_node->id.name.ptr = id->src_str;
			op_node->id.name.len = id->src_len;
			
			node_p expr = parse_expr_without_trailing_ops(parser);
			expr->parent = uops;
			buf_append(&uops->uops.list, expr);
		}
		
		return uops;
	}
	
	return node;
}