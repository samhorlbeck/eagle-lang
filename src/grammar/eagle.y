%{
    #include <stdlib.h>
    #include "compiler/ast.h"

    extern int yylex();
    extern int yyerror(char *);
    
    AST *ast_root = NULL;
%}

%union {
    int token;
    char *string;
    AST *node;
}

%token <string> TIDENTIFIER TINT TDOUBLE TTYPE
%token <token> TPLUS TMINUS TEQUALS
%token <token> TLPAREN TRPAREN TLBRACE TRBRACE
%token <token> TFUNC TRETURN TPUTS TEXTERN
%token <token> TCOLON TSEMI TNEWLINE TCOMMA
%type <node> program declarations declaration statements statement block funcdecl expression
%type <node> variabledecl vardecllist funccall calllist funcident funcsident externdecl typelist type

%right TEQUALS;
%left TPLUS;
%nonassoc TLPAREN;

%start program

%%

program             : declarations { ast_root = $1; };

declarations        : declaration { $$ = $1; }
                    | declaration declarations { $1->next = $2; $$ = $1; };

declaration         : funcdecl { $$ = $1; }
                    | externdecl { $$ = $1; };

type                : TIDENTIFIER { $$ = ast_make_type($1); }
                    | TTYPE { $$ = ast_make_type($1); };

funcdecl            : funcident block { ((ASTFuncDecl *)$1)->body = $2; $$ = $1; };

externdecl          : TEXTERN funcident TNEWLINE { $$ = $2; }
                    | TEXTERN funcsident TNEWLINE { $$ = $2; };

funcident           : TFUNC TIDENTIFIER TLPAREN TRPAREN TCOLON type { $$ = ast_make_func_decl($6, $2, NULL, NULL); }
                    | TFUNC TIDENTIFIER TLPAREN vardecllist TRPAREN TCOLON type { $$ = ast_make_func_decl($7, $2, NULL, $4); };

funcsident          : TFUNC TIDENTIFIER TLPAREN typelist TRPAREN TCOLON type { $$ = ast_make_func_decl($7, $2, NULL, $4); };

typelist            : type { $$ = ast_make_var_decl($1, NULL); }
                    | type TCOMMA typelist { AST *a = ast_make_var_decl($1, NULL); a->next = $3; $$ = a; };

vardecllist         : variabledecl { $$ = $1; }
                    | variabledecl TCOMMA vardecllist { $1->next = $3; $$ = $1; };

block               : TLBRACE statements TRBRACE { $$ = $2; };

statements          : statement { if($1) $$ = $1; else $$ = ast_make(); }
                    | statement statements { if($1) $1->next = $2; $$ = $1 ? $1 : $2; };

statement           : expression TSEMI { $$ = $1; }
                    | expression TNEWLINE { $$ = $1; }
                    | TRETURN expression TSEMI { $$ = ast_make_unary($2, 'r'); }
                    | TRETURN expression TNEWLINE { $$ = ast_make_unary($2, 'r'); }
                    | TPUTS expression TSEMI { $$ = ast_make_unary($2, 'p'); }
                    | TPUTS expression TNEWLINE { $$ = ast_make_unary($2, 'p'); }
                    | TNEWLINE { $$ = NULL; };

variabledecl        : type TIDENTIFIER { $$ = ast_make_var_decl($1, $2); };

funccall            : expression TLPAREN TRPAREN { $$ = ast_make_func_call($1, NULL); }
                    | expression TLPAREN calllist TRPAREN { $$ = ast_make_func_call($1, $3); };

calllist            : expression { $$ = $1; }
                    | expression TCOMMA calllist { $1->next = $3; $$ = $1; };

expression          : TINT { $$ = ast_make_int32($1); }
                    | TDOUBLE { $$ = ast_make_double($1); }
                    | expression TPLUS expression { $$ = ast_make_binary($1, $3, '+'); }
                    | variabledecl { $$ = $1; }
                    | variabledecl TEQUALS expression { $$ = ast_make_binary($1, $3, '='); }
                    | TIDENTIFIER TEQUALS expression { $$ = ast_make_binary(ast_make_identifier($1), $3, '='); }
                    | TIDENTIFIER { $$ = ast_make_identifier($1); }
                    | funccall { $$ = $1; };

%%
