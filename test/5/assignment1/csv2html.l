/* CS540 Assignment 1: Mike Shal */
%{
	/* This is used to keep track of whether or not a quoted string was
	 * just displayed. This is used to display the " character when two
	 * quotes are in a row inside a quoted string.
	 */
	int inQuote = 0;

	/* Prints a quoted string. This handles removing the quotes from the
	 * beginning and the end of the string. It also converts all embedded
	 * newlines to <br> tags. Finally, it sets the inQuote flag to 1. If
	 * the flag is set when entering this function (meaning there are
	 * back-to-back quoted strings), then a " character is displayed. This
	 * handles the "" inside a quoted string.
	 */
	void print_qstr(char *text, int length)
	{
		int x;
		int y;
		text[length-1] = 0;
		if(inQuote) {
			printf("\"");
		}
		y = 1;
		for(x=1; x < length-1; x++) {
			if(text[x] == '\n') {
				text[x] = 0;
				printf("%s<br>\n", text+y);
				y = x+1;
			}
		}
		printf("%s", text+y);
		inQuote=1;
	}
%}
 /* A string with quotes around it */
qstr (\"[^"]*\")
 /* A string without quotes around it */
str ([^",]*)
%%
 /* Match a quoted string at the begining of a line. In this case, start a
  * row and print the quoted string.
  */
^{qstr} {printf("<tr><td>"); print_qstr(yytext, yyleng);}

 /* Match a non-quoted string at the beginning of a line. Start a row and set
  * inQuote to 0.
  */
^{str} {printf("<tr><td>%s", yytext); inQuote=0;}

 /* Match a comma at the beginning of a line. This starts a row and makes an
  * empty field, then starts the next field.
  */
^, {printf("<tr><td></td>\n<td>"); inQuote=0;}

 /* Match a comma. This marks the end of one field and the beginning of the
  * next. */
, {printf("</td>\n<td>"); inQuote=0;}

 /* Match a newline. This marks the end of a field and a row. */
\n {printf("</td></tr>\n\n"); inQuote=0;}

 /* Match a quoted string. Display it using the print_qstr function. */
{qstr} {print_qstr(yytext, yyleng);}
%%
int yywrap(void)
{
	return 1;
}

int main(void)
{
	printf("<html><body>\n");
	printf("<table border=3>\n");
	yylex();
	printf("</table>\n");
	printf("</body></html>\n");
	return 0;
}
