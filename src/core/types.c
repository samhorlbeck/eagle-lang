//
//  types.c
//  Eagle
//
//  Created by Sam Olsen on 7/22/15.
//  Copyright (c) 2015 Sam Olsen. All rights reserved.
//

#include <stdlib.h>
#include <string.h>
#include "types.h"

#define TTEST(t, targ, out) if(!strcmp(t, targ)) return ett_base_type(out)
#define ETEST(t, a, b) if(a == b) return t

LLVMTargetDataRef etTargetData = NULL;

EagleTypeType *et_parse_string(char *text)
{
    TTEST(text, "any", ETAny);
    TTEST(text, "bool", ETInt1);
    TTEST(text, "byte", ETInt8);
    TTEST(text, "int", ETInt32);
    TTEST(text, "long", ETInt64);
    TTEST(text, "double", ETDouble);
    TTEST(text, "void", ETVoid);
    
    return NULL;
}

EagleType et_promotion(EagleType left, EagleType right)
{
    if(left == ETNone || left == ETVoid || right == ETNone || right == ETVoid)
        return ETNone;

    return left > right ? left : right;
}

/*
LLVMTypeRef et_llvm_type(EagleType type)
{
    switch(type)
    {
        case ETVoid:
            return LLVMVoidType();
        case ETDouble:
            return LLVMDoubleType();
        case ETInt32:
            return LLVMInt32Type();
        case ETInt64:
            return LLVMInt64Type();
        default:
            return NULL;
    }
}
*/

LLVMTypeRef ett_llvm_type(EagleTypeType *type)
{
    switch(type->type)
    {
        case ETVoid:
            return LLVMVoidType();
        case ETDouble:
            return LLVMDoubleType();
        case ETInt1:
            return LLVMInt1Type();
        case ETAny:
        case ETInt8:
            return LLVMInt8Type();
        case ETInt32:
            return LLVMInt32Type();
        case ETInt64:
            return LLVMInt64Type();
        case ETPointer:
            return LLVMPointerType(ett_llvm_type(((EaglePointerType *)type)->to), 0);
        default:
            return NULL;
    }
}

EagleType et_eagle_type(LLVMTypeRef ty)
{
    ETEST(ETDouble, ty, LLVMDoubleType());
    ETEST(ETInt32, ty, LLVMInt32Type());
    ETEST(ETInt64, ty, LLVMInt64Type());
    ETEST(ETVoid, ty, LLVMVoidType());

    return ETNone;
}

EagleTypeType *ett_base_type(EagleType type)
{
    EagleTypeType *ett = malloc(sizeof(EagleTypeType));
    ett->type = type;

    return ett;
}

EagleTypeType *ett_pointer_type(EagleTypeType *to)
{
    EaglePointerType *ett = malloc(sizeof(EaglePointerType));
    ett->type = ETPointer;
    ett->to = to;

    return (EagleTypeType *)ett;
}

EagleTypeType *ett_function_type(EagleTypeType *retVal, EagleTypeType **params, int pct)
{
    EagleFunctionType *ett = malloc(sizeof(EagleFunctionType));
    ett->type = ETFunction;
    ett->retType = retVal;
    ett->params = malloc(sizeof(EagleTypeType *) * pct);
    memcpy(ett->params, params, sizeof(EagleTypeType *) * pct);
    ett->pct = pct;

    return (EagleTypeType *)ett;
}

EagleType ett_get_base_type(EagleTypeType *type)
{
    if(type->type == ETPointer)
        return ett_get_base_type(((EaglePointerType *)type)->to);

    return type->type;
}

int ett_are_same(EagleTypeType *left, EagleTypeType *right)
{
    if(left->type != right->type)
        return 0;

    EagleType theType = left->type;
    if(theType == ETPointer)
    {
        EaglePointerType *pl = (EaglePointerType *)left;
        EaglePointerType *pr = (EaglePointerType *)right;

        return ett_are_same(pl->to, pr->to);
    }

    return 1;
}

int ett_pointer_depth(EagleTypeType *t)
{
    EaglePointerType *pt = (EaglePointerType *)t;
    int i;
    for(i = 0; pt->type == ETPointer; pt = (EaglePointerType *)pt->to, i++);
    return i;
}

int ett_is_numeric(EagleTypeType *t)
{
    EagleType type = t->type;
    switch(type)
    {
        case ETInt1:
        case ETInt8:
        case ETInt32:
        case ETInt64:
        case ETDouble:
            return 1;
        default:
            return 0;
    }
}

int ett_size_of_type(EagleTypeType *t)
{
    return LLVMStoreSizeOfType(etTargetData, ett_llvm_type(t));
}

void ett_debug_print(EagleTypeType *t)
{
    switch(t->type)
    {
        case ETInt1:
            printf("Bool\n");
            return;
        case ETInt8:
            printf("Byte\n");
            return;
        case ETInt32:
            printf("Int\n");
            return;
        case ETInt64:
            printf("Long\n");
            return;
        case ETDouble:
            printf("Double\n");
            return;
        case ETVoid:
            printf("Void\n");
            return;
        case ETAny:
            printf("Any\n");
            return;
        case ETPointer:
            printf("Pointer to ");
            ett_debug_print(((EaglePointerType *)t)->to);
            return;
        default:
            return;
    }

    return;
}

