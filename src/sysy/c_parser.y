%{
#include <cstdio>
#include <cstdlib>
#include "c_ast.hpp"
extern int yylineno;
extern CBlkPtr c_ast_root;
int yyparse();
int yylex();
void yyerror(const char *msg, int lineno = yylineno);
%}

%union {
    const char* id;
    int decimal_const;
    CNode* node;
    CBlock* block;
    CDeclList* decl_list;
    CVarDecl* var_decl;
    std::vector<CVarDecl*>* real_decl_list;
    CFuncDef* func_def;
    CStmt* stmt;
    CExpr* expr;
    CCondExpr* cond;
    std::vector<CExpr*>* expr_list;
    DataType dat;
};

%token tEQ tNEQ tLEQ tGEQ tLAND tLOR tINT tVOID tCONST tIF tELSE tWHILE
       tBREAK tCONT tRET
%token<id> tID
%token<decimal_const> tLITERAL

%type<node> BlockItem
%type<block> Root CompUnit Block BlockItems
%type<decl_list> Decl ConstDecl VarDecl ConstDefs VarDefs
%type<var_decl> ConstDef VarDef FuncFParam
%type<real_decl_list> FuncFParams
%type<func_def> FuncDef
%type<stmt> Stmt
%type<expr> ConstWidths ConstInitVal ConstInitVals InitVal InitVals Exp
            LVal Widths PrimaryExp Number UnaryExp MulExp AddExp
            RelExp EqExp ConstExp
%type<cond> Cond LAndExp LOrExp
%type<expr_list> FuncRParams
%type<dat> FuncType

%%

Root:
      CompUnit                  { c_ast_root = $1; }
    ;
CompUnit:
      CompUnit Decl             { $1->add_item($2); $$ = $1; }
    | CompUnit FuncDef          { $1->add_item($2); $$ = $1; }
    |                           { $$ = NEW(CBlock)(); }
    ;

Decl: ConstDecl                 { $$ = $1; }
    | VarDecl                   { $$ = $1; }
    ;
ConstDecl:
      tCONST FuncType ConstDefs ';'
                                { $$ = $3; }
    ;
ConstDefs:
      ConstDef                  { $$ = NEW(CDeclList)(true); $$->add_decl($1); }
    | ConstDefs ',' ConstDef    { $$->add_decl($3); }
    ;
ConstDef:
      tID ConstWidths '=' ConstInitVal
                                {
                                  $$ = NEW(CVarDecl)($1, $2, $4, true);
                                }
    ;
ConstWidths:
      '[' ConstExp ']' ConstWidths
                                {
                                  $2->set_next($4); $$ = $2;
                                }
    |                           { $$ = nullptr; }
    ;
ConstInitVal:
      ConstExp                  { $$ = $1; }
    | '{' ConstInitVals '}'     { $$ = NEW(CExpr)(eINIT); $$->set_child($2); }
    | '{' '}'                   { $$ = NEW(CExpr)(eINIT); }
    ;
ConstInitVals:
      ConstInitVal              { $$ = $1;}
    | ConstInitVal ',' ConstInitVals
                                {
                                  $1->set_next($3); $$ = $1;
                                }
    ;
VarDecl:
      FuncType VarDefs ';'      { $$ = $2; }
    ;
VarDefs:
      VarDef                    { $$ = NEW(CDeclList)(false); $$->add_decl($1); }
    | VarDefs ',' VarDef        { $$->add_decl($3); }
    ;
VarDef:
      tID ConstWidths           { $$ = NEW(CVarDecl)($1, $2); }
    | tID ConstWidths '=' InitVal
                                {
                                  $$ = NEW(CVarDecl)($1, $2, $4);
                                }
    ;
InitVal:
      Exp                       { $$ = $1; }
    | '{' InitVals '}'          { $$ = NEW(CExpr)(eINIT); $$->set_child($2); }
    | '{' '}'                   { $$ = NEW(CExpr)(eINIT); }
    ;
InitVals:
      InitVal                   { $$ = $1; }
    | InitVal ',' InitVals      { $1->set_next($3); $$ = $1; }
    ;

FuncDef:
      FuncType tID '(' ')' Block
                                {
                                  $5->set_func_body();
                                  $$ = NEW(CFuncDef)($1, $2, $5);
                                }
    | FuncType tID '(' FuncFParams ')' Block
                                {
                                  $6->set_func_body();
                                  $$ = NEW(CFuncDef)($1, $2, $6, *$4);
                                }
    ;
FuncType:
      tVOID                     { $$ = datVOID; }
    | tINT                      { $$ = datINT; }
    ;
FuncFParams:
      FuncFParam                { $$ = NEW(CVarDPtrList)({$1}); }
    | FuncFParams ',' FuncFParam{ $1->push_back($3); $$ = $1; }
    ;
FuncFParam:
      FuncType tID              { $$ = NEW(CVarDecl)($2, nullptr, nullptr, false, true); }
    | FuncType tID '[' ']' ConstWidths
                                {
                                  $$ = NEW(CVarDecl)($2, $5, nullptr, false, true, true);
                                }
    ;

Block:
      '{' BlockItems '}'        { $$ = $2; }
    ;
BlockItems:
      BlockItems BlockItem      { $1->add_item($2);  $$ = $1; }
    |                           { $$ = NEW(CBlock)(); }
    ;
BlockItem:
      Decl                      { $$ = $1; }
    | Stmt                      { $$ = $1; }
    ;
Stmt: LVal '=' Exp ';'          { $$ = NEW(CAssignStmt)($1, $3); }
    | ';'                       { $$ = NEW(CBlockStmt)(NEW(CBlock)()); }
    | Exp ';'                   { $$ = NEW(CExprStmt)($1); }
    | Block                     { $$ = NEW(CBlockStmt)($1); }
    | tIF '(' Cond ')' Stmt     { $$ = NEW(CIfElseStmt)($3, $5); }
    | tIF '(' Cond ')' Stmt tELSE Stmt
                                { $$ = NEW(CIfElseStmt)($3, $5, $7); }
    | tWHILE '(' Cond ')' Stmt  { $$ = NEW(CWhileStmt)($3, $5); }
    | tBREAK ';'                { $$ = NEW(CBreakStmt)(); }
    | tCONT ';'                 { $$ = NEW(CContStmt)(); }
    | tRET ';'                  { $$ = NEW(CRetStmt)(); }
    | tRET Exp ';'              { $$ = NEW(CRetStmt)($2); }
    ;

Exp:  AddExp                    { $$ = $1; }
    ;
Cond: LOrExp                    { $$ = $1; }
    ;
LVal: tID Widths                { $$ = NEW(CLValExpr)($1, $2); }
    ;
Widths:
      '[' Exp ']' Widths        { $2->set_next($4); $$ = $2; }
    |                           { $$ = nullptr; }
    ;
PrimaryExp:
      '(' Exp ')'               { $$ = $2; }
    | LVal                      { $$ = $1; }
    | Number                    { $$ = $1; }
    ;
Number:
      tLITERAL                  { $$ = NEW(CExpr)(eNUM, $1); }
    ;
UnaryExp:
      PrimaryExp                { $$ = $1; }
    | tID '(' ')'               { $$ = NEW(CCallExpr)($1); }
    | tID '(' FuncRParams ')'   { $$ = NEW(CCallExpr)($1, *$3); }
    | '+' UnaryExp              {
                                  $$ = $2;
                                  /* $$ = NEW(ArithExpr)(opPOS, $2); */
                                }
    | '-' UnaryExp              { $$ = NEW(CArithExpr)(opNEG, $2); }
    | '!' UnaryExp              { $$ = NEW(CArithExpr)(opNOT, $2); }
    ;
FuncRParams:
      Exp                       { $$ = NEW(CExprPtrList)({$1}); }
    | FuncRParams ',' Exp       { $1->push_back($3); $$ = $1; }
    ;
MulExp:
      UnaryExp                  { $$ = $1; }
    | MulExp '*' UnaryExp       { $$ = NEW(CArithExpr)(opMUL, $1, $3); }
    | MulExp '/' UnaryExp       { $$ = NEW(CArithExpr)(opDIV, $1, $3); }
    | MulExp '%' UnaryExp       { $$ = NEW(CArithExpr)(opMOD, $1, $3); }
    ;
AddExp:
      MulExp                    { $$ = $1; }
    | AddExp '+' MulExp         { $$ = NEW(CArithExpr)(opADD, $1, $3); }
    | AddExp '-' MulExp         { $$ = NEW(CArithExpr)(opSUB, $1, $3); }
    ;
RelExp:
      AddExp                    { $$ = $1; }
    | RelExp '<' AddExp         { $$ = NEW(CArithExpr)(opLE, $1, $3); }
    | RelExp '>' AddExp         { $$ = NEW(CArithExpr)(opGE, $1, $3); }
    | RelExp tLEQ AddExp        { $$ = NEW(CArithExpr)(opLEQ, $1, $3); }
    | RelExp tGEQ AddExp        { $$ = NEW(CArithExpr)(opGEQ, $1, $3); }
    ;
EqExp:
      RelExp                    { $$ = $1; }
    | EqExp tEQ RelExp          { $$ = NEW(CArithExpr)(opEQ, $1, $3); }
    | EqExp tNEQ RelExp         { $$ = NEW(CArithExpr)(opNEQ, $1, $3); }
    ;
LAndExp:
      EqExp                     { $$ = NEW(CCondExpr)(opNONE, $1); }
    | LAndExp tLAND EqExp       { $$ = NEW(CCondExpr)(opLAND, $1, NEW(CCondExpr)(opNONE, $3)); }
    ;
LOrExp:
      LAndExp                   { $$ = $1; }
    | LOrExp tLOR LAndExp       { $$ = NEW(CCondExpr)(opLOR, $1, $3); }
    ;
ConstExp:
      AddExp                    { $$ = $1; }
    ;

%%

void yyerror(const char *msg, int lineno) {
    fprintf(stderr, "Syntax Error at line %d\n", lineno);
}
