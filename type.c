#include "9cc.h"
#include <stddef.h>

Type *new_type(TypeKind kind)
{
    Type *ty = calloc(1, sizeof(Type));
    ty->kind = kind;
    return ty;
}

Type *char_type()
{
    return new_type(TY_CHAR);
}

Type *int_type()
{
    return new_type(TY_INT);
}

Type *pointer_to(Type *base)
{
    Type *ty = new_type(TY_PTR);
    ty->base = base;
    return ty;
}

Type *array_of(Type *base, int size)
{
    Type *ty = new_type(TY_ARRAY);
    ty->base = base;
    ty->array_size = size;
    return ty;
}

int size_of(Type *ty)
{
    switch (ty->kind)
    {
    case TY_CHAR:
        return 1;
    case TY_INT:
    case TY_PTR:
        return 8;
    default:
        log("ty->kind: %d", ty->kind);
        assert(ty->kind == TY_ARRAY);
        return size_of(ty->base) * ty->array_size;
    }
}

void visit(Node *node)
{
    if (!node)
        return;

    visit(node->lhs);
    visit(node->rhs);
    visit(node->cond);
    visit(node->then);
    visit(node->els);
    visit(node->init);
    visit(node->inc);

    for (int i = 0; node->body[i]; i++)
        visit(node->body[i]);
    for (int i = 0; node->args[i]; i++)
        visit(node->args[i]);

    switch (node->kind)
    {
    case ND_MUL:
    case ND_DIV:
    case ND_EQ:
    case ND_NE:
    case ND_LT:
    case ND_LE:
    case ND_FUNCALL:
    case ND_NUM:
        node->ty = int_type();
        return;
    case ND_LVAR:
        node->ty = node->var->ty;
        return;
    case ND_ADD:
        if (node->rhs->ty->base)
        {
            Node *tmp = node->lhs;
            node->lhs = node->rhs;
            node->rhs = tmp;
        }
        if (node->rhs->ty->base)
            error("invalid pointer arithmetic operands");
        node->ty = node->lhs->ty;
        return;
    case ND_SUB:
        if (node->rhs->ty->base)
            error("invalid pointer arithmetic operands");
        node->ty = node->lhs->ty;
        return;
    case ND_ASSIGN:
        node->ty = node->lhs->ty;
        return;
    case ND_ADDR:
        if (node->lhs->ty->kind == TY_ARRAY)
            node->ty = pointer_to(node->lhs->ty->base);
        else
            node->ty = pointer_to(node->lhs->ty);
        return;
    case ND_DEREF:
        if (!node->lhs->ty->base)
            error("invalid pointer dereference");
        node->ty = node->lhs->ty->base;
        return;
    case ND_SIZEOF:
        node->kind = ND_NUM;
        node->ty = int_type();
        node->val = size_of(node->lhs->ty);
        node->lhs = NULL;
        return;
    }
}

void add_type(Program *prog)
{
    for (Function *fn = prog->fns; fn; fn = fn->next)
        for (int i = 0; fn->body[i]; i++)
            visit(fn->body[i]);
}