#include "ast_compiler.h"

void ac_add_class_declaration(AST *ast, CompilerBundle *cb)
{
    ASTClassDecl *a = (ASTClassDecl *)ast;
    LLVMStructCreateNamed(LLVMGetGlobalContext(), a->name);
}

char *ac_gen_method_name(char *class_name, char *method)
{
    char *name = malloc(strlen(class_name) + strlen(method) + 1);
    sprintf(name, "%s%s", class_name, method);
    return name;
}

char *ac_gen_vtable_name(char *class_name)
{
    char *name = malloc(strlen(class_name) + 12);
    sprintf(name, "__%s_vtable", class_name);
    return name;
}

void ac_generate_interface_methods_each(void *key, void *val, void *data)
{
    ASTClassDecl *cd = (ASTClassDecl *)data;
    int idx = (int)(uintptr_t)key;

    ASTFuncDecl *fd = val;
    EagleFunctionType *ety = (EagleFunctionType *)((EaglePointerType *)arr_get(&cd->types, idx - 1))->to;

    ty_add_interface_method(cd->name, fd->ident, (EagleTypeType *)ety);
}

void ac_generate_interface_definitions(AST *ast, CompilerBundle *cb)
{
    for(; ast; ast = ast->next)
    {
        if(ast->type != AIFCDECL)
            continue;

        ASTClassDecl *a = (ASTClassDecl *)ast;

        hst_for_each(&a->methods, ac_generate_interface_methods_each, a);
    }
}

void ac_compile_class_init(ASTClassDecl *cd, CompilerBundle *cb)
{
    ASTFuncDecl *fd = (ASTFuncDecl *)cd->initdecl;
    if(!fd)
        return;

    ASTVarDecl *vd = (ASTVarDecl *)fd->params;
    vd->atype = ast_make_counted(ast_make_pointer(ast_make_type(cd->name)));
    char *method_name = ac_gen_method_name(cd->name, (char *)"__init__");
    
    EagleFunctionType *ety = (EagleFunctionType *)cd->inittype;
    ety->params[0] = ett_pointer_type(ett_struct_type(cd->name));
    ((EaglePointerType *)ety->params[0])->counted = 1;
    LLVMTypeRef ft = ett_llvm_type((EagleTypeType *)ety);
    LLVMValueRef func = LLVMAddFunction(cb->module, method_name, ft);

    //ty_add_method(cd->name, fd->ident, (EagleTypeType *)ety);
    ty_add_init(cd->name, (EagleTypeType *)ety);

    free(method_name);

    ac_compile_function_ex((AST *)fd, cb, func, ety);

    ety->params[0] = ett_pointer_type(ett_base_type(ETAny));
}

void ac_compile_class_methods_each(void *key, void *val, void *data)
{
    ac_class_helper *h = (ac_class_helper *)data;
    ASTClassDecl *cd = (ASTClassDecl *)h->ast;

    int idx = (int)(uintptr_t)key;

    ASTFuncDecl *fd = val;
    ASTVarDecl *vd = (ASTVarDecl *)fd->params;
    vd->atype = ast_make_counted(ast_make_pointer(ast_make_type(cd->name)));

    char *method_name = ac_gen_method_name(cd->name, fd->ident);

    EagleFunctionType *ety = (EagleFunctionType *)((EaglePointerType *)arr_get(&cd->types, idx - 1))->to;
    ety->params[0] = ett_pointer_type(ett_struct_type(cd->name));
    ((EaglePointerType *)ety->params[0])->counted = 1;
    LLVMTypeRef ft = ett_llvm_type((EagleTypeType *)ety);
    LLVMValueRef func = LLVMAddFunction(h->cb->module, method_name, ft);

    ty_add_method(cd->name, fd->ident, (EagleTypeType *)ety);

    free(method_name);

    ac_compile_function_ex((AST *)fd, h->cb, func, ety);

    ety->params[0] = ett_pointer_type(ett_base_type(ETAny));

    hst_put(&cd->methods, (void *)(uintptr_t)idx, func, ahhd, ahed);
}

void ac_make_class_definitions(AST *ast, CompilerBundle *cb)
{
    AST *old = ast;
    for(; ast; ast = ast->next)
    {
        if(ast->type != ACLASSDECL)
            continue;

        ASTClassDecl *a = (ASTClassDecl *)ast;

        LLVMTypeRef *tys = malloc(sizeof(LLVMTypeRef) * a->types.count);
        int i;
        for(i = 0; i < a->types.count; i++)
            tys[i] = ett_llvm_type(arr_get(&a->types, i));

        LLVMTypeRef loaded = LLVMGetTypeByName(cb->module, a->name);
        LLVMStructSetBody(loaded, tys, a->types.count, 0);
        free(tys);

        ty_add_struct_def(a->name, &a->names, &a->types);
    }

    ast = old;
    for(; ast; ast = ast->next)
    {
        if(ast->type != ACLASSDECL)
            continue;

        ASTClassDecl *a = (ASTClassDecl *)ast;

        ac_class_helper h;
        h.ast = ast;
        h.cb = cb;

        LLVMValueRef constnames = NULL;
        LLVMValueRef offsets = NULL;
        if(a->interfaces.count)
        {
            int count = (int)a->interfaces.count;
            LLVMValueRef constnamesarr[a->interfaces.count];
            LLVMValueRef offsetsarr[a->interfaces.count];
            int curoffset = 0;
            int i;
            for(i = 0; i < (int)a->interfaces.count; i++)
            {
                char *interface = a->interfaces.items[i];
                constnamesarr[i] = ac_make_floating_string(cb, interface, "_ifc");

                // constnamesarr[i] = LLVMBuildGlobalStringPtr(cb->builder, a->interfaces.items[i], "iname");
                offsetsarr[i] = LLVMConstInt(LLVMInt64Type(), curoffset, 0);
                curoffset += ty_interface_count(interface);
            }

            LLVMValueRef initcn = LLVMConstArray(LLVMPointerType(LLVMInt8Type(), 0), constnamesarr, (int)a->interfaces.count);
            LLVMValueRef initof = LLVMConstArray(LLVMInt64Type(), offsetsarr, (int)a->interfaces.count);

            constnames = LLVMAddGlobal(cb->module, LLVMArrayType(LLVMPointerType(LLVMInt8Type(), 0), count), "_ifn");
            offsets = LLVMAddGlobal(cb->module, LLVMArrayType(LLVMInt64Type(), count), "_ifo");

            LLVMSetInitializer(constnames, initcn);
            LLVMSetInitializer(offsets, initof);
            h.interface_pointers = malloc(sizeof(LLVMValueRef) * a->interfaces.count);
        }

        hst_for_each(&a->methods, ac_compile_class_methods_each, &h);
        ac_make_class_constructor(ast, cb);
        ac_make_class_destructor(ast, cb);
        ac_compile_class_init(a, cb);

        if(a->interfaces.count)
        {
            LLVMValueRef z = LLVMConstInt(LLVMInt32Type(), 0, 0);
            LLVMValueRef indirvals[] = {
                LLVMConstInt(LLVMInt32Type(), (int)a->interfaces.count, 0),
                LLVMConstGEP(constnames, &z, 1),
                LLVMConstGEP(offsets, &z, 1),
                constnames
            };

            char *vtablename = ac_gen_vtable_name(a->name);
            LLVMTypeRef indirtype = ty_class_indirect();
            LLVMValueRef vtableinit = LLVMConstNamedStruct(indirtype, indirvals, 4);
            LLVMValueRef vtable = LLVMAddGlobal(cb->module, indirtype, vtablename);
            free(vtablename);

            LLVMSetInitializer(vtable, vtableinit);
        }
        // if(ty_needs_destructor(ett_struct_type(a->name))) // {
        //     ac_make_struct_destructor(ast, cb);
        //     ac_make_struct_constructor(ast, cb);
        //     ac_make_struct_copy_constructor(ast, cb);
        // }
    }
}

void ac_make_class_constructor(AST *ast, CompilerBundle *cb)
{
    ASTClassDecl *a = (ASTClassDecl *)ast;
    LLVMValueRef func = ac_gen_struct_constructor_func(a->name, cb, 0); // This just generates a name

    EagleTypeType *ett = ett_pointer_type(ett_struct_type(a->name));

    LLVMBasicBlockRef entry = LLVMAppendBasicBlock(func, "entry");
    LLVMPositionBuilderAtEnd(cb->builder, entry);
    LLVMValueRef in = LLVMGetParam(func, 0);
    LLVMValueRef strct = LLVMBuildBitCast(cb->builder, in, ett_llvm_type(ett), "");

    arraylist *types = &a->types;
    int i;
    for(i = 0; i < types->count; i++)
    {
        EagleTypeType *t = arr_get(types, i);
        if(ET_IS_COUNTED(t) || ET_IS_WEAK(t))
        {
            LLVMValueRef gep = LLVMBuildStructGEP(cb->builder, strct, i, "");
            LLVMBuildStore(cb->builder, LLVMConstPointerNull(ett_llvm_type(t)), gep);
        }
        else if(t->type == ETStruct && ty_needs_destructor(t))
        {
            LLVMValueRef fc = ac_gen_struct_constructor_func(((EagleStructType *)t)->name, cb, 0);
            LLVMValueRef param = LLVMBuildStructGEP(cb->builder, strct, i, "");
            param = LLVMBuildBitCast(cb->builder, param, LLVMPointerType(LLVMInt8Type(), 0), "");
            LLVMBuildCall(cb->builder, fc, &param, 1, "");
        }

        LLVMValueRef meth = hst_get(&a->methods, (void *)(uintptr_t)(i + 1), ahhd, ahed);
        if(meth)
        {
            meth = LLVMBuildBitCast(cb->builder, meth, ett_llvm_type(t), "");
            LLVMValueRef gep = LLVMBuildStructGEP(cb->builder, strct, i, "");
            LLVMBuildStore(cb->builder, meth, gep);
        }
    }

    LLVMBuildRetVoid(cb->builder);
}

void ac_make_class_destructor(AST *ast, CompilerBundle *cb)
{
    ASTStructDecl *a = (ASTStructDecl *)ast;
    LLVMValueRef func = ac_gen_struct_destructor_func(a->name, cb);

    LLVMBasicBlockRef entry = LLVMAppendBasicBlock(func, "entry");
    LLVMPositionBuilderAtEnd(cb->builder, entry);

    EaglePointerType *ett = (EaglePointerType *)ett_pointer_type(ett_struct_type(a->name));
    LLVMValueRef pos = LLVMBuildAlloca(cb->builder, ett_llvm_type((EagleTypeType *)ett), "");
    LLVMValueRef cmp = LLVMBuildICmp(cb->builder, LLVMIntEQ, LLVMGetParam(func, 1), LLVMConstInt(LLVMInt1Type(), 1, 0), "");
    LLVMBasicBlockRef ifBB = LLVMAppendBasicBlock(func, "if");
    LLVMBasicBlockRef elseBB = LLVMAppendBasicBlock(func, "else");
    LLVMBasicBlockRef mergeBB = LLVMAppendBasicBlock(func, "merge");
    LLVMBuildCondBr(cb->builder, cmp, ifBB, elseBB);
    LLVMPositionBuilderAtEnd(cb->builder, ifBB);

    ett->counted = 1;
    LLVMValueRef cast = LLVMBuildBitCast(cb->builder, LLVMGetParam(func, 0), ett_llvm_type((EagleTypeType *)ett), "");
    LLVMBuildStore(cb->builder, LLVMBuildStructGEP(cb->builder, cast, 5, ""), pos);
    LLVMBuildBr(cb->builder, mergeBB);

    ett->counted = 0;
    LLVMPositionBuilderAtEnd(cb->builder, elseBB);
    cast = LLVMBuildBitCast(cb->builder, LLVMGetParam(func, 0), ett_llvm_type((EagleTypeType *)ett), "");
    LLVMBuildStore(cb->builder, cast, pos);
    LLVMBuildBr(cb->builder, mergeBB);

    LLVMPositionBuilderAtEnd(cb->builder, mergeBB);
    pos = LLVMBuildLoad(cb->builder, pos, "");

    arraylist *types = &a->types;
    int i;
    for(i = 0; i < types->count; i++)
    {
        EagleTypeType *t = arr_get(types, i);
        if(ET_IS_COUNTED(t))
        {
            LLVMValueRef gep = LLVMBuildStructGEP(cb->builder, pos, i, "");
            ac_decr_pointer(cb, &gep, t);
        }
        else if(ET_IS_WEAK(t))
        {
            LLVMValueRef gep = LLVMBuildStructGEP(cb->builder, pos, i, "");
            ac_remove_weak_pointer(cb, gep, t);
        }
        else if(t->type == ETStruct && ty_needs_destructor(t))
        {
            LLVMValueRef fc = ac_gen_struct_destructor_func(((EagleStructType *)t)->name, cb);
            LLVMValueRef params[2];
            params[0] = LLVMBuildBitCast(cb->builder, LLVMBuildStructGEP(cb->builder, pos, i, ""), 
                    LLVMPointerType(LLVMInt8Type(), 0), "");
            params[1] = LLVMConstInt(LLVMInt1Type(), 0, 0);
            LLVMBuildCall(cb->builder, fc, params, 2, "");
        }
    }

    LLVMBuildRetVoid(cb->builder);
}
