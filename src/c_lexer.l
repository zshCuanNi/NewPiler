%option yylineno
%option noyywrap
%option nounput
%{
#include <cstdlib>
#include "c_ast.hpp"
#include "c_parser.tab.hpp"

int lineno = 1;

// dummy function
void check();
%}

%x ML_COMMENT

ignore              ([ \t\a\r]+)
unary_op            ([,;=+-/*%<>!\(\)\[\]{}])
identifier          ([_a-zA-Z][_a-zA-Z0-9]*)
decimal             ([1-9][0-9]*)
octal               (0[0-7]*)
hexadecimal         (0[xX][0-9a-fA-F]+)

%%

"//".*              { /* eat up a comment line */ }

"/*"                { /* begin of eating  */ BEGIN(ML_COMMENT); }
<ML_COMMENT>"*/"    { /* eat up comment lines */ BEGIN(INITIAL); }
<ML_COMMENT>[^*\n]+
<ML_COMMENT>"*"
<ML_COMMENT>"\n"    { lineno += 1; }

\n                  { lineno += 1; }
{ignore}            { /* ignore */ }

{unary_op}          { return yytext[0]; }
"=="                { return tEQ; }
"!="                { return tNEQ; }
"<="                { return tLEQ; }
">="                { return tGEQ; }
"&&"                { return tLAND; }
"||"                { return tLOR; }
"int"               { return tINT; }
"void"              { return tVOID; }
"const"             { return tCONST; }
"if"                { return tIF; }
"else"              { return tELSE; }
"while"             { return tWHILE; }
"break"             { return tBREAK; }
"continue"          { return tCONT; }
"return"            { return tRET; }

{identifier}        {
                      char* id = strdup(yytext);
                      yylval.id = id;
                      return tID;
                    }
{decimal}           {
                      yylval.decimal_const = strtol(yytext, NULL, 0);
                      return tLITERAL;
                    }
{octal}             {
                      yylval.decimal_const = strtol(yytext, NULL, 0);
                      return tLITERAL;
                    }
{hexadecimal}       {
                      yylval.decimal_const = strtol(yytext, NULL, 0);
                      return tLITERAL;
                    }
.                   { /* unexpected character */ }

%%
void check() {
    printf("%s, line %d\n", yytext, lineno);
}
