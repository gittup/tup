/* CS540 Assignment 3: Mike Shal */
%{
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
extern int yylineno;

/* These defines are used in the type part of the data structure. This can be
 * used to determine the expected type of an argument, or for an actual type
 * of a symbol in the symbol table. Functions in the symbol table can be
 * marked as IMPLEMENTED in addition to INT_FUNC or VOID_FUNC to indicate
 * that the function has been implemented. This is used to make sure a function
 * has only one implementation, even if it is declared more than once.
 */
#define INT_TYPE 1
#define VOID_TYPE 2
#define ARRAY 4
#define INT_FUNC 8
#define VOID_FUNC 16
#define FUNC (INT_FUNC | VOID_FUNC)
#define IMPLEMENTED 32
%}

/* The data used to keep track of symbols and expected types. It contains the
 * symbol text, as well as the type of the symbol, and the number of parameters
 * in function declarations or function calls.
 */
%union {
	struct {
		char *text;
		int type;
		int num_params;
	} data;
}

/* Symbol not needed here since it returns yytext[0] */
%token NUMBER ID STRING META_STATEMENT CONDITION_OP COMPARISON_OP TYPE IF ELSE WHILE RETURN READ WRITE PRINT CONTINUE BREAK

%%
/* <program> --> <meta statements><data decls> <func list>
 * <meta statements>-->meta_statement <meta statements> | epsilon
 * <data decls> --> <data decls> <type name> <id list> semicolon | epsilon
 * <id list> --> <id> | <id list> comma <id>
 * <id> --> ID | ID left_bracket <expression> right_bracket
 * <func list> --> <func> <func list>  | epsilon
 * <func> --> <func decl> semicolon | <func decl> left_brace <data decls> <statements> right_brace
 * <func decl> --> <type name> ID left_parenthesis <parameter list> right_parenthesis
 * <type name> --> int | void
 * <parameter list> --> epsilon | void | <non-empty list>
 * <non-empty list> --> <type name> ID | <non-empty list> comma <type name> ID
*/
program: meta_statements data_decls func_list
	;
meta_statements : META_STATEMENT meta_statements
	|
	;
data_decls : data_decls type_name id_list ';'
	|
	;
	/* Since id_list is only used in data_decls, we can add the symbols
	 * immediately.
	 */
id_list : id {add_symbol($1.data.text, $1.data.type, 0);}
	| id_list ',' id {add_symbol($3.data.text, $3.data.type, 0);}
	;
	/* List the ID's type as an integer or an array appropriately. Also
	 * save the name of the symbol.
	 */
id : ID {$$.data.text = $1.data.text; $$.data.type = INT_TYPE;}
	| ID '[' expression ']' {$$.data.text = $1.data.text; $$.data.type = ARRAY;}
	;
func_list : func func_list
	|
	;
	/* Once a function is implemented, mark it as such so if we try to
	 * re-define it, it will be an error. If it's just a function
	 * declaration, be sure to remove all parameters from the scope.
	 */
func : func_decl ';' {clear_func_params();}
	| func_decl '{' data_decls statements '}' {function_implemented($1.data.text);}
	;
func_decl : type_name ID '(' parameter_list ')' {add_func($2.data.text, $1.data.type, $4.data.num_params); $$.data.text = $2.data.text;}
	;
type_name : TYPE {if(strcmp($1.data.text, "int") == 0) {$$.data.type = INT_TYPE;} else{$$.data.type = VOID_TYPE;}}
	;
	/* Keep track of the number of parameters that are passed in. Since
	 * they are all ints we don't need to keep track of anything beyond
	 * that.
	 */
parameter_list : 
	| TYPE {if(strcmp($1.data.text, "void") != 0) {error("expecting void parameter list.\n");}}
	| non_empty_list {$$.data.num_params = $1.data.num_params;}
	;
non_empty_list : type_name ID {$$.data.num_params = 1; add_func_param($2.data.text, $1.data.type);}
	| non_empty_list ',' type_name ID {$$.data.num_params = $1.data.num_params + 1; add_func_param($4.data.text, $3.data.type);}
	;

/* <statements> --> <statements> <statement> | epsilon
 * <statement> --> <assignment> | <func call> | <if statement> | <while statement> | <return statement> | <break statement> | <continue statement> | read left_parenthesis  ID right_parenthesis semicolon | write left_parenthesis <expression> right_parenthesis semicolon | print left_parenthesis  STRING right_parenthesis semicolon
 * <assignment> --> <id> equal_sign <expression> semicolon
 * <func call> --> ID left_parenthesis <expr list> right_parenthesis semicolon
 * <expr list> --> epsilon | <non-empty expr list>
 * <non-empty expr list> --> <expression> | <non-empty expr list> comma <expression>
 */
statements : statements statement
	|
	;
statement : assignment
	| func_call
	| if_statement
	| while_statement
	| return_statement
	| break_statement
	| continue_statement
	| READ '(' ID ')' ';' {check_id($3.data.text, INT_TYPE, 0);}
	| WRITE '(' expression ')' ';'
	| PRINT '(' STRING ')' ';'
	;
	/* Verify that only integer variables can be set. Also verify that
	 * only functions can be called.
	 */
assignment : id '=' expression ';' {check_id($1.data.text, $1.data.type, 0);}
	;
	/* Keep track of the number of parameters used in function calls.
	 * Verify that this number matches the number of parameters from the
	 * declaration.
	 */
func_call : ID '(' expr_list ')' ';' {check_id($1.data.text, FUNC, $3.data.num_params);} /* Check 2 */
	;
expr_list :
	| non_empty_expr_list {$$.data.num_params = $1.data.num_params;}
	;
non_empty_expr_list : expression {$$.data.num_params = 1;}
	| non_empty_expr_list ',' expression {$$.data.num_params = $1.data.num_params + 1;}
	;

/* <if statement> --> if left_parenthesis <condition expression> right_parenthesis <block statements> <else statement>
 * <else statement> --> else <block statements> | epsilon
 * <block statements> --> left_brace <data decls> <statements> right_brace
 * <condition expression> -->  <condition> | <condition> <condition op> <condition>
 * <condition op> --> double_and_sign | double_or_sign
 * <condition> --> <expression> <comparison op> <expression>
 * <comparison op> --> == | != | > | >= | < | <=
 */
if_statement : IF '(' condition_expression ')' block_statements else_statement
	;
else_statement : ELSE block_statements
	|
	;
block_statements : '{' data_decls statements '}'
	;
condition_expression : condition | condition condition_op condition
	;
condition_op : CONDITION_OP
	;
condition : expression comparison_op expression
	;
comparison_op : COMPARISON_OP
	;

/* <while statement> --> while left_parenthesis <condition expression> right_parenthesis <block statements>
 * <return statement> --> return <expression> semicolon | return semicolon
 * <break statement> ---> break semicolon
 * <continue statement> ---> continue semicolon
 */
while_statement : WHILE '(' condition_expression ')' block_statements {in_while--;}
	;
	/* Verify that a return statement corresponds to the return type of
	 * the function. Also save the fact that we returned an int if we
	 * did so, so we can say if a function did not return a value when it
	 * should have.
	 */
return_statement : RETURN expression ';' {check_return_type(INT_FUNC); return_type = INT_TYPE;} /* Check 6 */
	| RETURN ';' {check_return_type(VOID_FUNC);} /* Check 7 */
	;
	/* Verify that break and continue statements occur within while loops.
	 */
break_statement : BREAK ';' {check_break();}
	;
continue_statement : CONTINUE ';' {check_continue();}
	;

/* <expression> --> <term> | <expression> <addop> <term>
 * <addop> --> plus_sign | minus_sign
 * <term> --> <factor> | <term> <mulop> <factor>
 * <mulop> --> star_sign | forward_slash
 * <factor> --> ID | ID left_bracket <expression> right_bracket | ID left_parenthesis <expr list> right_parenthesis | left_parenthesis <expression> right_parenthesis |  NUMBER |  minus_sign NUMBER
 */
expression : term
	| expression addop term
	;
addop : '+'
	| '-'
	;
term : factor
	| term mulop factor
	;
mulop : '*'
	| '/'
	;
	/* Make sure when an ID shows up in factor that it is of the correct
	 * type. Eg: we expect plain IDs to be ints, ID[] to be arrays, and
	 * ID() to be functions.
	 */
factor : ID {check_id($1.data.text, INT_TYPE, 0);} /* Check 3 */
	| ID '[' expression ']' {check_id($1.data.text, ARRAY, 0);} /* Check 1 */
	| ID '(' expr_list ')' {check_id($1.data.text, INT_FUNC, $3.data.num_params);} /* Check 2, Check 4 */
	| '(' expression ')'
	| NUMBER
	| '-' NUMBER
	;

%%

/* A structure used to describe a symbol. This holds the name of the symbol,
 * the type (one of the defines at the top of the file), the number of
 * parameters (or 0 if it's not a function), the integer scope (0..n), and
 * a pointer to the next symbol in the list.
 */
struct symbol {
	char *text;
	int type;
	int num_params;
	int scope;
	struct symbol *next;
};

/* A structure used to contain a symbol table. Each symbol table exists at a
 * scope. The next symbol table in the list is at one less scope. Each symbol
 * table has a list of (possibly empty) symbols in it.
 */
struct symtab {
	int scope;
	struct symbol *syms;
	struct symtab *next;
};

/* Define the global symbol table, as well as some global information used to
 * keep track of functions (for checking return types and saving parameters).
 * Also keep track of whether or not we're in a while loop
 */
static struct symtab *symbol_table;
static struct symbol global_sym = {0, 0, 0, 0, 0};
static struct symbol *func_params = 0;
static int return_type = 0;
static int err_count = 0;
static int in_while = 0;

int main()
{
	/* Initialize the symbol table to be empty, and then parse. Display
	 * "Pass" if no errors were encountered.
	 */
	symbol_table = malloc(sizeof(*symbol_table));
	symbol_table->scope = 0;
	symbol_table->syms = 0;
	symbol_table->next = 0;
	yyparse();
	if(err_count) {
		printf("Detected %i errors.\n", err_count);
		return -1;
	} else {
		printf("Pass\n");
	}
	return 0;
}

extern char *yytext;

yyerror()
{
	printf("Syntax error line %i: '%s'\n", yylineno, yytext);
	err_count++;
}

/* Helper function to display errors. */
void error(const char *s, ...)
{
	va_list ap;
	printf("Syntax error line %i: ", yylineno);
	va_start(ap, s);
	vprintf(s, ap);
	va_end(ap);
	err_count++;
}

int yywrap(void)
{
	return 1;
}

/* Helper function to convert a type to a string. */
const char *get_type(int type)
{
	if(type == INT_TYPE) {
		return "int";
	} else if(type == VOID_TYPE) {
		return "void";
	} else if(type == ARRAY) {
		return "array";
	} else if(type & INT_FUNC) {
		return "function returning int";
	} else if(type & VOID_FUNC) {
		return "void function";
	} else if(type == FUNC) {
		return "function";
	} else {
		return "unknown";
	}
}

/* Finds a symbol in the symbol table. Each symbol table is successfully
 * searched until the symbol is found. Since the symtabs are in decreasing
 * scope order, the first symbol found is the closest scoped symbol.
 */
struct symbol *find_sym(const char *text)
{
	struct symtab *tab;
	tab = symbol_table;
	while(tab) {
		struct symbol *sym;
		sym = tab->syms;
		while(sym) {
			if(strcmp(sym->text, text) == 0) {
				return sym;
			}
			sym = sym->next;
		}
		tab = tab->next;
	}
	return 0;
}

/* Verifies that a symbol is of the correct type and has the correct number of
 * arguments. If the symbol is not found, it is reported as undefined. If
 * it is found but has the wrong type or the wrong number of arguments, this
 * is displayed as well.
 */
void check_id(const char *text, int type, int num_params)
{
	struct symbol *sym;
	int found = 0;
	sym = find_sym(text);
	if(sym) {
		if(!(sym->type & type)) {
			error("expecting %s type, but symbol '%s' is %s type in this scope.\n", get_type(type), text, get_type(sym->type));
		}
		if(sym->num_params != num_params) {
			error("function '%s' expecting %i parameters, but %i parameters were given.\n", text, sym->num_params, num_params);
		}
	} else {
		error("undefined symbol: %s\n", text);
	}
}

/* Adds a new symbol table to the front of the list and initializes it with
 * the list of function parameters (if any).
 */
void scope_add()
{
	struct symtab *newtab = malloc(sizeof(*newtab));
	newtab->scope = symbol_table->scope + 1;
	newtab->syms = func_params;
	func_params = 0;
	newtab->next = symbol_table;
	symbol_table = newtab;
}

/* Deletes the most recent symbol table from the scope. These symbols will no
 * longer be found in symbol resolution. Since we're leaving the scope, also
 * verify that if we're returning to global scope to make sure the function
 * returns an int if it has an int return type (ie: we should have found at
 * least one "return <expression>" statement.
 */
void scope_sub()
{
	struct symbol *sym;
	struct symtab *tab;

	sym = symbol_table->syms;
	while(sym) {
		struct symbol *tmp;
		tmp = sym->next;
		free(sym);
		sym = tmp;
	}
	tab = symbol_table->next;
	free(symbol_table);
	symbol_table = tab;

	if(symbol_table->scope == 0) {
		if(global_sym.type == INT_FUNC && return_type != INT_TYPE) {
			error("int function '%s' doesn't return an int.\n", global_sym.text);
		}
		return_type = 0;
	}
}

/* Adds a symbol to the symbol table. If the symbol is already defined, or if
 * it was previously declared as a function but the parameters don't match,
 * then this is an error. If the function is being implemented, we mark the
 * function type with the IMPLEMENTED flag. That way if it is implemented again
 * later we can flag this as an error.
 */
void add_symbol(const char *text, int type, int num_params)
{
	struct symbol *sym;

	sym = find_sym(text);
	if(sym && sym->scope == symbol_table->scope) {
		if(type & FUNC) {
			if(!(sym->type & IMPLEMENTED)) {
				if(!(type & sym->type)) {
					error("function '%s' redefined with different return type.\n", text);
				}
				if(num_params != sym->num_params) {
					error("function '%s' redefined with different parameters.\n", text);
				}
			} else {
				error("function '%s' is already defined.\n", text);
			}
		} else {
			error("symbol '%s' is redefined in the same scope.\n", text);
		}
	} else {
		sym = malloc(sizeof(*sym));
		sym->text = strdup(text);
		sym->type = type;
		sym->num_params = num_params;
		sym->scope = symbol_table->scope;
		sym->next = symbol_table->syms;
		symbol_table->syms = sym;
	}
	if(symbol_table->scope == 0) {
		/* Keep track of what function we're in. */
		global_sym.text = sym->text;
		global_sym.type = type;
	}
}

/* Add a function with the given return type and number of parameters. */
void add_func(const char *text, int type, int num_params)
{
	if(type == INT_TYPE) {
		add_symbol(text, INT_FUNC, num_params);
	} else {
		add_symbol(text, VOID_FUNC, num_params);
	}
}

/* Keep track of the function parameters in a separate list so they can be
 * moved to the appropriate symbol table when it is created.
 */
void add_func_param(const char *text, int type)
{
	struct symbol *sym;
	sym = malloc(sizeof(*sym));
	sym->text = strdup(text);
	sym->type = type;
	sym->num_params = 0;
	sym->scope = symbol_table->scope + 1;
	sym->next = func_params;
	func_params = sym;
}

/* Clear out function parameters. They may not be needed if it's a function
 * declaration.
 */
void clear_func_params()
{
	while(func_params) {
		struct symbol *tmp = func_params->next;
		free(func_params);
		func_params = tmp;
	}
}

/* Verifies the return type of the function. This is needed to make sure that
 * return statements are being used properly.
 */
void check_return_type(int type)
{
	if(type != global_sym.type) {
		if(type == INT_FUNC) {
			error("can't return value in void function '%s'\n", global_sym.text);
		} else {
			error("must return value for int function '%s'\n", global_sym.text);
		}
	}
}

/* Sets the function named 'text' to be IMPLEMENTED. This occurs after a
 * function definition has been processed.
 */
void function_implemented(const char *text)
{
	struct symbol *sym;
	sym = find_sym(text);
	sym->type |= IMPLEMENTED;
}

/* Verify a break statement is within a while loop. */
void check_break(void)
{
	if(!in_while) {
		error("'break' statement found outside while loop\n");
	}
}

/* Verify a continue statement is within a while loop. */
void check_continue(void)
{
	if(!in_while) {
		error("'continue' statement found outside while loop\n");
	}
}

/* Called from the scanner - this lets us know that we're in a while loop.
 * Use ++ since we could be in nested while loops.
 */
void add_while(void)
{
	in_while++;
}
