#include "common.h"


//
// Parser state and utility stuff
//

struct parser_s {
	module_p module;
	size_t   pos;
	
	list_t(token_type_t) tried_token_types;
	FILE* error_stream;
};


static token_p next_filtered_token(parser_p parser, bool ignore_line_breaks) {
	size_t pos = parser->pos;
	while (pos < parser->module->tokens.len) {
		token_p token = &parser->module->tokens.ptr[pos];
		pos++;
		
		// Skip whitespace and comment tokens
		if ( token->type == T_WS || token->type == T_COMMENT )
			continue;
		// Also skip whitespaces with newlines if we're told to do so
		if ( token->type == T_WSNL && ignore_line_breaks )
			continue;
		
		// Return the first token we didn't skip
		return token;
	}
	
	// We're either beyond the last token or found no filtered token beyond pos
	return NULL;
}

static void parser_error(parser_p parser, const char* message) {
	token_p token = &parser->module->tokens.ptr[parser->pos];
	fprintf(parser->error_stream, "%s:%d:%d: ", parser->module->filename,
		token_line(parser->module, token),
		token_col(parser->module, token)
	);
	
	if (message) {
		fputs(message, parser->error_stream);
		fputs("\n", parser->error_stream);
	}
	
	fputs("expected", parser->error_stream);
	for(size_t i = 0; i < parser->tried_token_types.len; i++) {
		char* desc = token_desc(parser->tried_token_types.ptr[i]);
		if (!desc)
			desc = token_type_name(parser->tried_token_types.ptr[i]);
		
		fprintf(parser->error_stream, " %s", desc);
		if (i != parser->tried_token_types.len - 1)
			fputs(",", parser->error_stream);
	}
	
	fputs(" after ", parser->error_stream);
	token_print(parser->error_stream, token, TP_INLINE_DUMP);
	fputs("\n", parser->error_stream);
	
	size_t token_index = token - parser->module->tokens.ptr;
	token_print_range(parser->error_stream, parser->module, token_index, 1);
}

static token_p try(parser_p parser, token_type_t type) {
	bool already_tried = false;
	for(size_t i = 0; i < parser->tried_token_types.len; i++) {
		if (parser->tried_token_types.ptr[i] == type) {
			already_tried = true;
			break;
		}
	}
	
	if (!already_tried)
		list_append(&parser->tried_token_types, type);
	
	token_p token = next_filtered_token(parser, (type == T_WSNL) ? false : true);
	if (token && token->type == type)
		return token;
	return NULL;
}

static token_p consume(parser_p parser, token_p token) {
	if (token == NULL) {
		fprintf(stderr, "consume(): Tried to consume NULL!\n");
		abort();
	}
	
	// Since index is unsigned negative indices will wrap around to very large
	// indices that are out of bounds, too. So the next if catches them as well.
	size_t index = token - parser->module->tokens.ptr;
	if ( index >= parser->module->tokens.len ) {
		fprintf(stderr, "consume(): Token not part of the currently parsed module!\n");
		abort();
	}
	
	// Advance parser position and clear tried token types
	parser->pos = index + 1;
	list_destroy(&parser->tried_token_types);
	
	return token;
}

static token_p try_consume(parser_p parser, token_type_t type) {
	token_p token = try(parser, type);
	if (token)
		consume(parser, token);
	return token;
}

static token_p consume_type(parser_p parser, token_type_t type) {
	token_p token = try(parser, type);
	if (!token) {
		parser_error(parser, NULL);
		abort();
	}
	return consume(parser, token);
}



//
// Public parser interface to parse a rule
//

node_p parse(module_p module, parser_rule_func_t rule, FILE* error_stream) {
	parser_t parser = (parser_t){
		.module       = module,
		.error_stream = error_stream,
		.pos          = 0
	};
	
	node_p node = rule(&parser);
	consume_type(&parser, T_EOF);
	
	list_destroy(&parser.tried_token_types);
	parser.error_stream = NULL;
	return node;
}



//
// Try functions for different rules
//

static token_p try_cexpr(parser_p parser);
static token_p try_eos(parser_p parser);
static token_p consume_eos(parser_p parser);

node_p parse_cexpr(parser_p parser);


/*

//
// Parser rules
//

void parse_stmts(parser_p parser, node_p parent, node_list_p list);


node_p parse_module(parser_p parser) {
	node_p node = node_alloc(NT_MODULE);
	node->tokens = parser->module->tokens;
	
	while ( ! try_consume(parser, T_EOF) ) {
		node_p def = NULL;
		
		if ( try(parser, T_FUNC) ) {
			def = parse_func_def(parser);
		} else {
			parser_error(parser, NULL);
			return node;
		}
		
		node_append(node, &node->module.defs, def);
	}
	
	return node;
}


//
// Definitions
//

node_p parse_func_def(parser_p parser) {
	node_p node = node_alloc(NT_FUNC);
	
	consume_type(parser, T_FUNC);
	node->func.name = consume_type(parser, T_ID)->source;
	
	token_p t = NULL;
	if ( (t = try_consume(parser, T_IN)) || (t = try_consume(parser, T_OUT)) ) {
		node_list_p arg_list = NULL;
		if ( t->type == T_IN )
			arg_list = &node->func.in;
		else if ( t->type == T_OUT )
			arg_list = &node->func.out;
		else
			abort();
		
		consume_type(parser, T_RBO);
		while ( !try(parser, T_RBC) ) {
			node_p arg = node_alloc(NT_ARG);
			
			t = consume_type(parser, T_ID);
			node_p type_expr = node_alloc_set(NT_ID, arg, &arg->arg.type_expr);
			type_expr->id.name = t->src;
			
			// Set the arg name if we got an ID after the type. Otherwise leave
			// the arg unnamed (nulled out)
			if ( try(parser, T_ID) )
				arg->arg.name = consume_type(parser, T_ID)->src;
			
			node_append(node, arg_list, arg);
			
			if ( !try_consume(parser, T_COMMA) )
				break;
		}
		consume_type(parser, T_RBC);
		
	} else if ( try_consume(parser, T_CBO) ) {
		parse_stmts(parser, node, &node->func.body);
		consume_type(parser, T_CBC);
	} else if ( try_consume(parser, T_DO) ) {
		parse_stmts(parser, node, &node->func.body);
		consume_type(parser, T_END);
	}
	
	return node;
}

void parse_stmts(parser_p parser, node_p parent, node_list_p list) {
	while ( try_stmt_start(parser) ) {
		node_p stmt = parse_stmt(parser);
		node_append(parent, list, stmt);
	}
}

*/

//
// Statements
//

static token_p try_stmt(parser_p parser) {
	token_p t = NULL;
	if      ( (t = try(parser, T_CBO))   ) return t;
	else if ( (t = try(parser, T_DO))    ) return t;
	else if ( (t = try(parser, T_WHILE)) ) return t;
	else if ( (t = try(parser, T_IF))    ) return t;
	else if ( (t = try_cexpr(parser))    ) return t;
	
	return NULL;
}

node_p parse_stmt(parser_p parser) {
	token_p t = NULL;
	node_p node = NULL;
	
	if ( (t = try_consume(parser, T_CBO)) || (t = try_consume(parser, T_DO)) ) {
		// stmt = "{"  [ stmt ] "}"
		//        "do" [ stmt ] "end"
		node = node_alloc(NT_SCOPE);
		
		while ( try_stmt(parser) )
			node_append(node, &node->scope.stmts, parse_stmt(parser) );
		
		consume_type(parser, (t->type == T_CBO) ? T_CBC : T_END);
	} else if ( try_consume(parser, T_WHILE) ) {
		// stmt = "while" expr "do" [ stmt ] "end"
		//                     "{"  [ stmt ] "}"
		//                     WSNL [ stmt ] "end"  // check as last alternative, see note 1
		node = node_alloc(NT_WHILE_STMT);
		node_p cond = parse_expr(parser);
		node_set(node, &node->while_stmt.cond, cond);
		
		t = try_consume(parser, T_DO);
		if (!t) t = try_consume(parser, T_CBO);
		if (!t) t = try_consume(parser, T_WSNL);
		if (!t) {
			parser_error(parser, "while needs a block as body!");
			abort();
		}
		
		while ( try_stmt(parser) )
			node_append(node, &node->while_stmt.body, parse_stmt(parser) );
		
		consume_type(parser, (t->type == T_DO || t->type == T_WSNL) ? T_END : T_CBC );
	} else if ( try_consume(parser, T_IF) ) {
		// "if" expr "do" [ stmt ]     ( "else"     [ stmt ] )? "end"
		//           "{"  [ stmt ] "}" ( "else" "{" [ stmt ] "}" )?
		//           WSNL [ stmt ]     ( "else"     [ stmt ] )? "end"  // check as last alternative, see note 1
		node = node_alloc(NT_IF_STMT);
		node_p cond = parse_expr(parser);
		node_set(node, &node->if_stmt.cond, cond);
		
		t = try_consume(parser, T_DO);
		if (!t) t = try_consume(parser, T_CBO);
		if (!t) t = try_consume(parser, T_WSNL);
		if (!t) {
			parser_error(parser, "if needs a block as body!");
			abort();
		}
		
		while ( try_stmt(parser) )
			node_append(node, &node->if_stmt.true_case, parse_stmt(parser) );
		
		if (t->type == T_CBO)
			consume_type(parser, T_CBC);
		
		if ( try_consume(parser, T_ELSE) ) {
			if (t->type == T_CBO)
				consume_type(parser, T_CBO);
			
			while ( try_stmt(parser) )
				node_append(node, &node->if_stmt.false_case, parse_stmt(parser) );
			
			if (t->type == T_CBO)
				consume_type(parser, T_CBC);
		}
		
		if (t->type == T_DO || t->type == T_WSNL)
			consume_type(parser, T_END);
	} else if ( try_cexpr(parser) ) {
		// stmt = cexpr ...
		// TODO: Implement var definition and binary ops (from expr)
		node = parse_cexpr(parser);
	} else {
		parser_error(parser, NULL);
		abort();
	}
	
	return node;
}

static token_p try_eos(parser_p parser) {
	token_p t = NULL;
	if      ( (t = try(parser, T_EOF))  ) return t;
	else if ( (t = try(parser, T_SEMI)) ) return t;
	else if ( (t = try(parser, T_CBC))  ) return t;
	else if ( (t = try(parser, T_END))  ) return t;
	else if ( (t = try(parser, T_WSNL)) ) return t;
	return NULL;
}

static token_p consume_eos(parser_p parser) {
	token_p t = NULL;
	if      ( (t = try_consume(parser, T_EOF))  ) return t;
	else if ( (t = try_consume(parser, T_SEMI)) ) return t;
	else if ( (t = try(parser, T_CBC))          ) return t;
	else if ( (t = try(parser, T_END))          ) return t;
	else if ( (t = try_consume(parser, T_WSNL)) ) return t;
	return NULL;
}



//
// Expressions
//

static token_p try_cexpr(parser_p parser) {
	token_p t = NULL;
	if      ( (t = try(parser, T_ID)) ) return t;
	else if ( (t = try(parser, T_INT)) ) return t;
	else if ( (t = try(parser, T_STR)) ) return t;
	else if ( (t = try(parser, T_RBO)) ) return t;
	#define UNARY_OP(token, id, name)  \
		else if ( (t = try(parser, token)) ) return t;
	#include "op_spec.h"
	
	return NULL;
}

node_p parse_cexpr(parser_p parser) {
	token_p t = NULL;
	node_p node = NULL;
	
	if ( (t = try_consume(parser, T_ID)) ) {
		node = node_alloc(NT_ID);
		node->id.name = t->source;
	} else if ( (t = try_consume(parser, T_INT)) ) {
		node = node_alloc(NT_INTL);
		node->intl.value = t->int_val;
	} else if ( (t = try_consume(parser, T_STR)) ) {
		node = node_alloc(NT_STRL);
		node->strl.value = t->str_val;
	} else if ( (t = try_consume(parser, T_RBO)) ) {
		node = parse_expr(parser);
		consume_type(parser, T_RBC);
	
	// cexpr = unary_op cexpr
	#define UNARY_OP(token, id, name)                     \
		} else if ( (t = try_consume(parser, token)) ) {  \
			node = node_alloc(NT_UNARY_OP);               \
			node->unary_op.index = id;                    \
			node_p arg = parse_cexpr(parser);             \
			node_set(node, &node->unary_op.arg, arg);
	#include "op_spec.h"
	
	} else {
		parser_error(parser, NULL);
		abort();
	}
	
	// One complete cexpr parsed, now process the trailing stuff.
	// Since we can chain together any number of cexpr with that trailing stuff
	// (we're left recursive) we have to do this in a loop here.
	while (true) {
		if ( (t = try_consume(parser, T_RBO)) ) {
			// cexpr = cexpr "(" ( expr [ "," expr ] )? ")"
			node_p target_expr = node;
			node = node_alloc(NT_CALL);
			node_set(node, &node->call.target_expr, target_expr);
			
			if ( !try(parser, T_RBC) ) {
				node_p expr = parse_expr(parser);
				node_append(node, &node->call.args, expr);
				
				while ( try_consume(parser, T_COMMA) ) {
					expr = parse_expr(parser);
					node_append(node, &node->call.args, expr);
				}
			}
			
			consume_type(parser, T_RBC);
		} else if ( (t = try_consume(parser, T_SBO)) ) {
			// cexpr "[" ( expr [ "," expr ] )? "]"
			node_p target_expr = node;
			node = node_alloc(NT_INDEX);
			node_set(node, &node->index.target_expr, target_expr);
			
			if ( !try(parser, T_SBC) ) {
				node_p expr = parse_expr(parser);
				node_append(node, &node->index.args, expr);
				
				while ( try_consume(parser, T_COMMA) ) {
					expr = parse_expr(parser);
					node_append(node, &node->index.args, expr);
				}
			}
			
			consume_type(parser, T_SBC);
		} else if ( (t = try_consume(parser, T_PERIOD)) ) {
			// cexpr "." ID
			node_p aggregate = node;
			node = node_alloc(NT_MEMBER);
			node_set(node, &node->member.aggregate, aggregate);
			node->member.member = consume_type(parser, T_ID)->source;
		} else {
			break;
		}
	}
	
	return node;
}

static token_p try_binary_op(parser_p parser) {
	token_p t = NULL;
	if ( (t = try(parser, T_ID)) ) return t;
	#define BINARY_OP(token, id, name)  \
		else if ( (t = try(parser, token)) ) return t;
	#include "op_spec.h"
	
	return NULL;
}

node_p parse_expr(parser_p parser) {
	// cexpr [ binary_op cexpr ]
	node_p node = parse_cexpr(parser);
	
	token_p t = NULL;
	if ( try_binary_op(parser) && !try_eos(parser) ) {
		// Got an operator, wrap everything into an uops node and collect the
		// remaining operators and expressions.
		node_p cexpr = node;
		node = node_alloc(NT_UOPS);
		node_append(node, &node->uops.list, cexpr);
		
		while ( (t = try_binary_op(parser)) && !try_eos(parser) ) {
			consume(parser, t);
			
			// TODO: Store the token in the node, the resolve uops pass can then
			// look at the token to figure out which operator it was.
			node_p op_node = node_alloc_append(NT_ID, node, &node->uops.list);
			op_node->id.name = t->source;
			
			cexpr = parse_cexpr(parser);
			node_append(node, &node->uops.list, cexpr);
		}
	}
	
	return node;
}