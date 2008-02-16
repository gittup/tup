/* CS540 Assignment 3: Mike Shal */
letter [a-zA-Z]
digit [0-9]
number {digit}+
ID {letter}({letter}|{digit})*
type int|void
symbol (\(|\)|\{|\}|\[|\]|\,|\;|\+|\-|\*|\/|=)
condition_op (&&|\|\|)
comparison_op (==|!=|>|>=|<|<=)
string ["]([^"\n(\\\")]|\\\"|\\\n)*["]
meta_statement ^(#|\/\/).*$
%{
#include <stdio.h>
#include "y.tab.h"
void scope_add();
void scope_sub();
void add_while();
%}
%%
{number} {return NUMBER;}
{type} {yylval.data.text=strdup(yytext); return TYPE;}
if {return IF;}
else {return ELSE;}
 /* Determine whether or not we're inside a while loop. Once we find the 'while'
  * token we are entering a while loop. The parser will decrement the counter
  * when leaving the scope of the while loop. This is needed to determine if
  * continue and break statements are happening within a while loop.
  */
while {add_while(); return WHILE;}
return {return RETURN;}
read {return READ;}
write {return WRITE;}
print {return PRINT;}
continue {return CONTINUE;}
break {return BREAK;}
{string} {return STRING;}
{ID} {yylval.data.text=strdup(yytext); return ID;}
{condition_op} {return CONDITION_OP;}
{comparison_op} {return COMPARISON_OP;}
 /* Want to keep track of the scope as soon as an opening or closing brace
  * is found.
  */
{symbol} {if(yytext[0] == '{') {scope_add();} else if(yytext[0] == '}') {scope_sub();} return yytext[0];}
{meta_statement} {return META_STATEMENT;}
[\n] {yylineno++;}
. ;
%%
