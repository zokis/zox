#include <stdlib.h>
#include <string.h>
#include <setjmp.h>
#include "ast.h"
#include "lexer.h"
#include "parser.h"
#include "malloc_safe.h"
#include "global.h"

extern ExecutionContext global_context;

Parser *create_parser(Token *tokens, long long int token_count) {
    Parser *parser = (Parser *)malloc_safe(sizeof(Parser), "Failed to allocate memory for Parser");
    parser->tokens = tokens;
    parser->token_count = token_count;
    parser->current = 0;
    return parser;
}

short int not_eof(Parser *parser) {
    // printf("token: %s\n", parser->tokens[parser->current].value);
    return parser->current < parser->token_count && parser->tokens[parser->current].type != EOFTk;
}

Token at(Parser *parser) {
    return parser->tokens[parser->current];
}

Token eat(Parser *parser) {
    return parser->tokens[parser->current++];
}

Token expect(Parser *parser, TokenType type, const char *err) {
    Token token = eat(parser);
    if (token.type != type) {
        parser_error(err, &token, type);
    }
    return token;
}

Stmt *parse_stmt(Parser *parser) {
    if (at(parser).type == ImportTk) {
        return parse_import_stmt(parser);
    }
    if (at(parser).type == LetTk) {
        return (Stmt *)parse_var_declaration(parser);
    }
    return (Stmt *)parse_expr(parser);
}

Program *produce_ast(Parser *parser, const char *source_code) {
    Program *program = create_program(NULL, 0);
    program->body = NULL;
    program->body_count = 0;
    if (strlen(source_code) == 0 || (strlen(source_code) == 1 && source_code[0] == ';')) {
        return program;
    }
    while (not_eof(parser)) {
        program->body = realloc_safe(
            program->body, sizeof(Stmt *) * (program->body_count + 1),
            "produce_ast"
        );
        Stmt *stmt = parse_stmt(parser);
        if (stmt != NULL) {
            program->body[program->body_count++] = stmt;
        }
    }
    return program;
}

Expr *parse_expr(Parser *parser) {
    return parse_relational_expr(parser);
}

Expr *parse_var_declaration(Parser *parser) {
    eat(parser);
    Token varname = expect(
        parser, IdentifierTk,
        "Expected identifier name following let const keywords."
    );
    if(at(parser).type == SemiColonTk) {
        eat(parser);
        return (Expr *)create_var_expr(varname.value, (Expr *)create_nil_literal());
    }
    Token eq = expect(
        parser, EqualsTk,
        "Variable declaration statment must end with semicolon."
    );
    Expr* value = (Expr *)parse_expr(parser);
    if (at(parser).type == OpenParenTk) {
        value = parse_call_expr(parser, value);
    }
    expect(
        parser, SemiColonTk,
        "Missing ;"
    );
    return (Expr *)create_var_expr(varname.value, value);
}

Expr *parse_relational_expr(Parser *parser) {
    return parse_logical_or(parser);
}

Expr *parse_logical_or(Parser *parser) {
    Expr *left = parse_logical_and(parser);
    if (!left) return NULL;
    while (1) {
        Token token = at(parser);
        if (strcmp(token.value, "||") != 0) {
            break;
        }
        char *operator = eat(parser).value;
        Expr *right = parse_logical_and(parser);
        if (!right) {
            free_expr(left);
            return NULL;
        }
        printf("operator or: %s\n", operator);
        left = (Expr *)create_binary_expr(left, right, operator);
    }
    return left;
}

Expr *parse_logical_and(Parser *parser) {
    Expr *left = parse_equality(parser);
    if (!left) return NULL;
    while (1) {
        Token token = at(parser);
        if (strcmp(token.value, "&&") != 0) {
            break;
        }
        char *operator = eat(parser).value;
        Expr *right = parse_equality(parser);
        if (!right) {
            free_expr(left);
            return NULL;
        }
        printf("operator and: %s\n", operator);
        left = (Expr *)create_binary_expr(left, right, operator);
    }
    return left;
}

Expr *parse_equality(Parser *parser) {
    Expr *left = parse_comparison(parser);
    if (!left) return NULL;
    while (1) {
        Token token = at(parser);
        if (strcmp(token.value, "==") != 0 && strcmp(token.value, "!=") != 0) {
            break;
        }
        char *operator = eat(parser).value;
        Expr *right = parse_comparison(parser);
        if (!right) {
            free_expr(left);
            return NULL;
        }
        printf("operator eq: %s\n", operator);
        left = (Expr *)create_binary_expr(left, right, operator);
    }
    return left;
}

Expr *parse_comparison(Parser *parser) {
    Expr *left = parse_additive_expr(parser);
    if (!left) return NULL;
    while (1) {
        Token token = at(parser);
        if (token.value[0] != '<' && token.value[0] != '>' &&
            strcmp(token.value, "<=") != 0 && strcmp(token.value, ">=") != 0) {
            break;
        }
        char *operator = eat(parser).value;
        Expr *right = parse_additive_expr(parser);
        if (!right) {
            free_expr(left);
            return NULL;
        }
        printf("operator cmp: %s\n", operator);
        left = (Expr *)create_binary_expr(left, right, operator);
    }
    return left;
}

Expr *parse_additive_expr(Parser *parser) {
    Expr *left = (Expr *)parse_bitwise_expr(parser);
    if (!left) return NULL;
    while (1) {
        Token token = at(parser);
        if (token.type == ArrowTk || (token.value[0] != '+' && token.value[0] != '-')) {
            break;
        }
        char *operator = eat(parser).value;
        Expr *right = (Expr *)parse_multiplicative_expr(parser);
        if (!right) {
            free_expr(left);
            return NULL;
        }
        printf("operator add: %s\n", operator);
        left = (Expr *)create_binary_expr(left, right, operator);
    }
    return left;
}

Expr *parse_bitwise_expr(Parser *parser) {
    Expr *left = parse_multiplicative_expr(parser);
    while (1) {
        Token token = at(parser);
        if (
            token.value[0] != '^' &&
            token.value[0] != '&' &&
            token.value[0] != '|' &&
            strcmp(token.value, "<<") != 0 &&
            strcmp(token.value, ">>") != 0
        ) {
            break;
        }
        char *operator = eat(parser).value;
        Expr *right = parse_multiplicative_expr(parser);
        left = (Expr *)create_binary_expr(left, right, operator);
    }
    return left;
}

Expr *parse_multiplicative_expr(Parser *parser) {
    Expr *left = parse_primary_expr(parser);
    if (!left) return NULL;
    while (1) {
        Token token = at(parser);
        if (token.type == ArrowTk || (
            token.value[0] != '/' &&
            token.value[0] != '*' &&
            token.value[0] != '%'
        )) {
            break;
        }
        char *operator = eat(parser).value;
        Expr *right = (Expr *)parse_primary_expr(parser);
        if (!right) {
            free_expr(left);
            return NULL;
        }
        printf("operator mul: %s\n", operator);
        left = (Expr *)create_binary_expr(left, right, operator);
    }
    return left;
}

Expr *parse_list_literal(Parser *parser) {
    eat(parser);
    Expr **elements = NULL;
    size_t element_count = 0;
    while (at(parser).type != CloseBraceTk) {
        if (element_count > 0) {
            expect(parser, CommaTk, "Expected ',' between list elements.");
        }
        elements = realloc_safe(elements, sizeof(Expr *) * (element_count + 1), "parse_list_literal elements");
        elements[element_count++] = (Expr *)parse_expr(parser);
    }
    expect(parser, CloseBraceTk, "Expected '}' after list elements.");
    return (Expr *)create_list_literal(elements, element_count);
}

Expr *parse_table_literal(Parser *parser) {
    expect(parser, OpenTableTk, "Expected '|>' at the beginning of table literal.");
    char **columns = NULL;
    size_t column_count = 0;
    while (at(parser).type != CloseTableTk) {
        if (column_count > 0) {
            expect(parser, SemiColonTk, "Expected ';' between table columns.");
        }
        Token column = expect(parser, IdentifierTk, "Expected column name in table literal.");
        columns = realloc_safe(columns, sizeof(char *) * (column_count + 1), "parse_table_literal columns");
        columns[column_count++] = strdup(column.value);
    }
    expect(parser, CloseTableTk, "Expected '<|' at the end of table literal.");
    return (Expr *)create_table_literal(columns, column_count);
}

Expr *parse_dict_literal(Parser *parser) {
    expect(parser, OpenBracketTk, "Expected '[' at the beginning of dictionary literal.");
    Expr **keys = NULL;
    Expr **values = NULL;
    size_t element_count = 0;

    while (at(parser).type != CloseBracketTk) {
        if (element_count > 0) {
            expect(parser, SemiColonTk, "Expected ';' between dictionary elements.");
        }

        keys = realloc_safe(keys, sizeof(Expr *) * (element_count + 1), "parse_dict_literal keys");
        values = realloc_safe(values, sizeof(Expr *) * (element_count + 1), "parse_dict_literal values");

        Expr *key = (Expr *)parse_expr(parser);
        if (!key) {
            // Handle error
            break;
        }

        keys[element_count] = key;

        expect(parser, ArrowTk, "Expected '->' between key and value.");
        Expr *value = (Expr *)parse_expr(parser);
        if (!value) {
            // Handle error
            break;
        }

        values[element_count] = value;
        element_count++;
    }
    expect(parser, CloseBracketTk, "Expected ']' at the end of dictionary literal.");

    return (Expr *)create_dict_literal(keys, values, element_count);
}

char *parse_string(const char *raw_value) {
    char *parsed_value = malloc_safe(strlen(raw_value) + 1, "Failed to allocate memory for parsed string");
    int i = 0, j = 0;
    while (raw_value[i]) {
        if (raw_value[i] == '\\' && raw_value[i+1]) {
            switch (raw_value[i+1]) {
                case 'n': parsed_value[j++] = '\n'; break;
                case 't': parsed_value[j++] = '\t'; break;
                case 'r': parsed_value[j++] = '\r'; break;
                case 'b': parsed_value[j++] = '\b'; break;
                case 'f': parsed_value[j++] = '\f'; break;
                case '"': parsed_value[j++] = '"'; break;
                case '\'': parsed_value[j++] = '\''; break;
                case '\\': parsed_value[j++] = '\\'; break;
                default: parsed_value[j++] = raw_value[i];
            }
            i += 2;
        } else {
            parsed_value[j++] = raw_value[i++];
        }
    }
    parsed_value[j] = '\0';
    return parsed_value;
}


Expr *parse_identifier_expr(Parser *parser) {
    const char *varname = eat(parser).value;
    Expr *identifier = (Expr *)create_identifier(varname);

    while (1) {
        if (at(parser).type == EqualsTk) {
            eat(parser);
            return (Expr *)assign_var_expr(varname, (Expr *)parse_expr(parser));
        } else if (at(parser).type == OpenParenTk) {
            identifier = (Expr *)parse_call_expr(parser, identifier);
        } else if (at(parser).type == OpenBracketTk) {
            eat(parser);
            Expr *start = NULL;
            Expr *end = NULL;
            short int is_slice = 0;
            if(at(parser).type == ElseTk) {
                eat(parser);
                start = (Expr *)create_numeric_literal(0);
                if (at(parser).type != CloseBracketTk) {
                    end = (Expr *)parse_expr(parser);
                }
                is_slice = 1;
            } else {
                start = (Expr *)parse_expr(parser);
                if (at(parser).type == ElseTk) {
                    eat(parser);
                    if (at(parser).type != CloseBracketTk) {
                        end = (Expr *)parse_expr(parser);
                    }
                    is_slice = 1;
                }
            }
            expect(parser, CloseBracketTk, "Expected ']' after list index.");
            if (at(parser).type == EqualsTk) {
                eat(parser);
                return (Expr *)assign_list_expr(varname, start, (Expr *)parse_expr(parser));
            }
            identifier = (Expr *)create_list_index(identifier, start, end, is_slice);
        } else if (at(parser).type == OpenBraceTk) {
            eat(parser);
            Expr *key = (Expr *)parse_expr(parser);
            expect(parser, CloseBraceTk, "Expected '}' after dict key.");
            if (at(parser).type == EqualsTk) {
                eat(parser);
                return (Expr *)assign_dict_expr(varname, key, (Expr *)parse_expr(parser));
            }
            identifier = (Expr *)create_dict_key(identifier, key);
        } else {
            break;
        }
    }
    return identifier;
}

Stmt *parse_import_stmt(Parser *parser) {
    expect(parser, ImportTk, "Expected '~>' for import statement");
    
    char *module_name;
    if (at(parser).type == IdentifierImportTk || at(parser).type == IdentifierTk) {
        module_name = eat(parser).value;
    } else {
        Token tk = at(parser);
        parser_error("Expected module name in import statement", &tk, IdentifierImportTk);
        return NULL;
    }
    
    ImportStmt *import_stmt = malloc_safe(sizeof(ImportStmt), "ImportStmt allocation");
    import_stmt->base.kind = ImportAst;
    import_stmt->module_name = strdup(module_name);
    import_stmt->imports = NULL;
    import_stmt->import_count = 0;

    if (at(parser).type == OpenBraceTk) {
        do {
            eat(parser);
            char *name = expect(parser, IdentifierTk, "Expected identifier in import list").value;
            char *alias = NULL;
            if (at(parser).type == AsTk) {
                eat(parser);
                alias = expect(parser, IdentifierTk, "Expected alias after 'as' in import statement").value;
            }
            ImportItem *item = malloc_safe(sizeof(ImportItem), "ImportItem allocation");
            item->name = strdup(name);
            item->alias = alias ? strdup(alias) : NULL;
            import_stmt->imports = realloc_safe(import_stmt->imports, sizeof(ImportItem *) * (import_stmt->import_count + 1), "ImportStmt imports realloc");
            import_stmt->imports[import_stmt->import_count++] = item;
        } while (at(parser).type == CommaTk);

        expect(parser, CloseBraceTk, "Expected '}' after import list");
    } else {
        error("Expected '{' after module name in import statement");
    }
    
    expect(parser, SemiColonTk, "Expected ';' after import statement");
    
    return (Stmt *)import_stmt;
}

Expr *parse_primary_expr(Parser *parser) {
    TokenType tk = at(parser).type;

    switch (tk) {
        case ImportTk: {
            return (Expr *)parse_import_stmt(parser);
        }
        case FunctionTk: {
            eat(parser);
            return (Expr *)parse_func_def(parser);
        }
        case IdentifierTk: {
            return (Expr *)parse_identifier_expr(parser);
        }
        case NumberTk:
            return (Expr *)create_numeric_literal(atof(eat(parser).value));
        case StringTk: {
            const char *raw_value = eat(parser).value;
            char *parsed_value = parse_string(raw_value);
            Expr *result = (Expr *)create_string_literal(parsed_value);
            free_safe(parsed_value);
            return result;
        }
        case BooleanLiteralTk: {
            return (Expr *)create_boolean_literal(strcmp(eat(parser).value, "true") == 0 ? 1 : 0);
        }
        case OpenTableTk: {
            return parse_table_literal(parser);
        }
        case OpenBracketTk: {
            return parse_dict_literal(parser);
        }
        case OpenBraceTk: {
            return parse_list_literal(parser);
        }
        case NilTk: {
            eat(parser);
            return (Expr *)create_nil_literal();
        }
        case LetTk: {
            return (Expr *)parse_var_declaration(parser);
        }
        case SemiColonTk: {
            eat(parser);
            return parse_expr(parser);
        }
        case OpenParenTk: {
            eat(parser);
            Expr *value = (Expr *)parse_expr(parser);
            expect(
                parser, CloseParenTk,
                "Unexpected token found inside parenthesised expression. Expected closing parenthesis."
            );
            return value;
        }
        case IfTk: {
            eat(parser);
            return (Expr *)parse_if_expr(parser);
        }
        case WhileTk: {
            eat(parser);
            return (Expr *)parse_while_expr(parser);
        }
        case ForTk: {
            eat(parser);
            return (Expr *)parse_for_expr(parser);
        }
        case EOFTk: {
            return (Expr *)create_nil_literal();
        }
        default: {
            Token tk = at(parser);
            char error_message[100];
            snprintf(error_message, sizeof(error_message), "Unexpected token found during parsing! Token: %s\nLine %i; Column %i\n",
                tk.value, tk.line, tk.column);
            error(error_message);
        }
    }
}

Expr *parse_if_expr(Parser *parser) {
    expect(parser, OpenParenTk, "Expected '(' after '?' keyword.");
    Expr *cond = parse_expr(parser);
    expect(parser, CloseParenTk, "Expected ')' after '?' condition.");
    expect(parser, OpenBraceTk, "Expected '{' to start '?' body.");
    Stmt **body = NULL;
    size_t body_count = 0;
    while (at(parser).type != CloseBraceTk) {
        body = realloc_safe(body, sizeof(Stmt *) * (body_count + 1), "parse_if_expr body");
        body[body_count++] = parse_stmt(parser);
    }
    expect(parser, CloseBraceTk, "Expected '}' to close '?' body.");
    Stmt **else_body = NULL;
    size_t else_body_count = 0;
    IfExpr *else_if = NULL;
    while (at(parser).type == ElseTk) {
        eat(parser);
        if (at(parser).type == IfTk) {
            eat(parser);
            else_if = (IfExpr *)parse_if_expr(parser);
        } else {
            expect(parser, OpenBraceTk, "Expected '{' to start ':' body.");
            while (at(parser).type != CloseBraceTk) {
                else_body = realloc_safe(else_body, sizeof(Stmt *) * (else_body_count + 1), "parse_if_expr else_body");
                else_body[else_body_count++] = parse_stmt(parser);
            }
            expect(parser, CloseBraceTk, "Expected '}' to close ':' body.");
            break;
        }
    }
    return (Expr *)create_if(cond, body, body_count, else_if, else_body, else_body_count);
}

Expr *parse_while_expr(Parser *parser) {
    expect(parser, OpenParenTk, "Expected '(' after '#' keyword.");
    Expr *cond = parse_expr(parser);
    expect(parser, CloseParenTk, "Expected ')' after '#' condition.");
    expect(parser, OpenBraceTk, "Expected '{' to start '#' body.");
    Stmt **body = NULL;
    size_t body_count = 0;
    while (at(parser).type != CloseBraceTk) {
        body = realloc_safe(body, sizeof(Stmt *) * (body_count + 1), "parse_while_expr body");
        body[body_count++] = parse_stmt(parser);
    }
    expect(parser, CloseBraceTk, "Expected '}' to close '#' body.");
    return (Expr *)create_while(cond, body, body_count);
}

Expr *parse_for_expr(Parser *parser) {
    expect(parser, OpenParenTk, "Expected '(' after '@' keyword.");
    Expr *initialization = (Expr *)parse_stmt(parser);  
    Expr *condition = (Expr *)parse_expr(parser);
    expect(parser, SemiColonTk, "Expected ';' after for condition.");
    Expr *increment = (Expr *)parse_expr(parser);
    expect(parser, CloseParenTk, "Expected ')' after for increment.");
    expect(parser, OpenBraceTk, "Expected '{' to start '@' body.");
    Stmt **body = NULL;
    size_t body_count = 0;
    while (at(parser).type != CloseBraceTk) {
        body = realloc_safe(body, sizeof(Stmt *) * (body_count + 1), "parse_for_expr body");
        body[body_count++] = parse_stmt(parser);
    }
    expect(parser, CloseBraceTk, "Expected '}' to close '@' body.");
    return (Expr *)create_for_expr(initialization, condition, increment, body, body_count);
}

Expr *parse_func_def(Parser *parser) {
     Token name_token = expect(parser, IdentifierTk, "Expected function name after '$'.");
    char *name = strdup(name_token.value);
    expect(parser, OpenParenTk, "Expected '(' after function name.");
    char **params = NULL;
    size_t param_count = 0;
    while (at(parser).type != CloseParenTk) {
        if (param_count > 0) {
            expect(parser, CommaTk, "Expected ',' between function parameters.");
        }
        Token param = expect(parser, IdentifierTk, "Expected parameter name.");
        params = realloc_safe(params, sizeof(char *) * (param_count + 1), "parse_func_def params");
        params[param_count++] = strdup(param.value);
    }
    expect(parser, CloseParenTk, "Expected ')' after function parameters.");
    expect(parser, OpenBraceTk, "Expected '{' to start function body.");
    Stmt **body = NULL;
    size_t body_count = 0;
    while (at(parser).type != CloseBraceTk) {
        body = realloc_safe(body, sizeof(Stmt *) * (body_count + 1), "parse_func_def body");
        body[body_count++] = parse_stmt(parser);
    }
    expect(parser, CloseBraceTk, "Expected '}' to end function body.");
    return (Expr *)create_func_def(name, params, param_count, body, body_count);
}

Expr *parse_call_expr(Parser *parser, Expr *callee) {
    expect(parser, OpenParenTk, "Expected '(' after function name.");
    Expr **args = NULL;
    size_t arg_count = 0;
    while (at(parser).type != CloseParenTk) {
        if (arg_count > 0) {
            expect(parser, CommaTk, "Expected ',' between arguments.");
        }
        args = realloc_safe(args, sizeof(Expr *) * (arg_count + 1), "parse_call_expr args");
        args[arg_count++] = parse_expr(parser);
    }
    expect(parser, CloseParenTk, "Expected ')' after arguments.");
    return (Expr *)create_call_expr(callee, args, arg_count);
}
