#include <stdio.h>
#include "compiler/ast_compiler.h"
#include "grammar/eagle.tab.h"

extern char *yytext;

extern FILE *yyin;
extern int yyparse();
extern int yylex();
extern int yylineno;

extern AST *ast_root;

hashtable global_args;

void first_pass()
{
    int token;
    int saveNext = 0;
    while((token = yylex()) != 0)
    {
        if(saveNext)
        {
            ty_add_name(yytext);
            saveNext = 0;
        }
        saveNext = token == TSTRUCT;
    }

    rewind(yyin);
    yylineno = 0;
}

int main(int argc, const char *argv[])
{
    etTargetData = LLVMCreateTargetData("");
    global_args = hst_create();

    if(argc < 2)
    {
        printf("Usage: %s <file> [args]\n", argv[0]);
        return 0;
    }

    if(argc > 2)
    {
        int i;
        for(i = 2; i < argc; i++)
            hst_put(&global_args, (char *)argv[i], (void *)1, NULL, NULL);
    }

    yyin = fopen(argv[1], "r");
    if(!yyin)
    {
        printf("Error: could not find file.\n");
        return 0;
    }

    ty_prepare();

    first_pass();

    yyparse();

    LLVMModuleRef module = ac_compile(ast_root);

    ty_teardown();

    LLVMDumpModule(module);

    LLVMInitializeNativeTarget();
    LLVMInitializeNativeAsmPrinter();

    LLVMDisposeModule(module);

    LLVMDisposeTargetData(etTargetData);
    
//    int token;
//    while ((token = yylex()) != 0)
//        printf("Token: %d (%s)\n", token, yytext);
//    return 0;
    
    return 0;
}
