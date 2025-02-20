#include "9cc.h"

char *argreg1[] = {"dil", "sil", "dl", "cl", "r8b", "r9b"};
char *argreg8[] = {"rdi", "rsi", "rdx", "rcx", "r8", "r9"};
static Function *current_fn;
static void gen(Node *node);

int align_to(int n, int align)
{
    return (n + align - 1) & ~(align - 1);
}

static void assign_lvar_offsets(Program *prog)
{
    for (Function *fn = prog->fns; fn; fn = fn->next)
    {
        int offset = 0;
        for (VarList *vl = fn->locals; vl; vl = vl->next)
        {
            Var *var = vl->var;
            offset += size_of(var->ty);
            var->offset = offset;
        }
        fn->stack_size = align_to(offset, 8);
    }
}

void load(Type *ty)
{
    printf("  pop rax\n");
    if (size_of(ty) == 1)
        printf("  movsx rax, byte ptr [rax]\n");
    else
        printf("  mov rax, [rax]\n");
    printf("  push rax\n");
}

void store(Type *ty)
{
    printf("  pop rdi\n");
    printf("  pop rax\n");
    if (size_of(ty) == 1)
        printf("  mov [rax], dil\n");
    else
        printf("  mov [rax], rdi\n");
    printf("  push rdi\n");
}

static int count(void)
{
    static int i = 1;
    return i++;
}

void gen_addr(Node *node)
{
    switch (node->kind)
    {
    case ND_LVAR:
    {
        Var *var = node->var;
        if (var->is_local)
        {
            printf("  lea rax, [rbp-%d]\n", node->var->offset);
            printf("  push rax\n");
        }
        else
        {
            printf("  lea rax, [rip+%s]\n", var->name);
            printf("  push rax\n");
        }
        return;
    }
    case ND_DEREF:
        gen(node->lhs);
        return;
    default:
        error("gen_addr: invalid node");
        return;
    }
}

void gen_lval(Node *node)
{
    if (node->ty->kind == TY_ARRAY)
        error("not an lvalue");
    gen_addr(node);
}

void gen(Node *node)
{
    printf("# start gen node (type is %d)\n", node->kind);
    switch (node->kind)
    {
    case ND_NULL:
        return;
    case ND_NUM:
        printf("  push %d\n", node->val);
        return;
    case ND_EXPR_STMT:
        gen(node->lhs);
        printf("  add rsp, 8\n");
        return;
    case ND_LVAR:
        gen_addr(node);
        if (node->ty->kind != TY_ARRAY)
        {
            load(node->ty);
        }
        return;
    case ND_ADDR:
        gen_addr(node->lhs);
        return;
    case ND_DEREF:
        gen(node->lhs);
        if (node->ty->kind != TY_ARRAY)
        {
            load(node->ty);
        }
        return;
    case ND_ASSIGN:
        gen_lval(node->lhs);
        gen(node->rhs);
        store(node->ty);
        return;
    case ND_RETURN:
        gen(node->lhs);
        printf("  pop rax\n");
        printf("  jmp .L.return.%s\n", current_fn->name);
        return;
    case ND_BLOCK:
        for (int i = 0; node->body[i]; i++)
        {
            gen(node->body[i]);
        }
        return;
    case ND_IF:
    {
        int c = count();
        gen(node->cond);
        printf("  pop rax\n");
        printf("  cmp rax, 0\n");
        printf("  je  .L.else.%d\n", c);
        gen(node->then);
        printf("  jmp .L.end.%d\n", c);
        printf(".L.else.%d:\n", c);
        if (node->els)
            gen(node->els);
        printf(".L.end.%d:\n", c);
        return;
    }
    case ND_FOR:
    {
        int c = count();
        if (node->init)
            gen(node->init);
        printf(".L.begin.%d:\n", c);
        if (node->cond)
        {
            gen(node->cond);
            printf("  pop rax\n");
            printf("  cmp rax, 0\n");
            printf("  je  .L.end.%d\n", c);
        }
        gen(node->then);
        if (node->inc)
            gen(node->inc);
        printf("  jmp .L.begin.%d\n", c);
        printf(".L.end.%d:\n", c);
        return;
    }
    case ND_FUNCALL:
    {
        int nargs = 0;
        while (node->args[nargs])
        {
            gen(node->args[nargs++]);
        }

        for (int i = nargs - 1; i >= 0; i--)
        {
            printf("  pop %s\n", argreg8[i]);
        }

        printf("  mov rax, 0\n");
        printf("  call %s\n", node->funcname);
        printf("  push rax\n");
        return;
    }
    }

    gen(node->lhs);
    gen(node->rhs);

    printf("  pop rdi\n");
    printf("  pop rax\n");

    switch (node->kind)
    {
    case ND_ADD:
        if (node->ty->base)
            printf("  imul rdi, %d\n", size_of(node->ty->base));
        printf("  add rax, rdi\n");
        break;
    case ND_SUB:
        if (node->ty->base)
            printf("  imul rdi, %d\n", size_of(node->ty->base));
        printf("  sub rax, rdi\n");
        break;
    case ND_MUL:
        printf("  imul rax, rdi\n");
        break;
    case ND_DIV:
        printf("  cqo\n");
        printf("  idiv rdi\n");
        break;
    case ND_EQ:
        printf("  cmp rax, rdi\n");
        printf("  sete al\n");
        printf("  movzb rax, al\n");
        break;
    case ND_NE:
        printf("  cmp rax, rdi\n");
        printf("  setne al\n");
        printf("  movzb rax, al\n");
        break;
    case ND_LT:
        printf("  cmp rax, rdi\n");
        printf("  setl al\n");
        printf("  movzb rax, al\n");
        break;
    case ND_LE:
        printf("  cmp rax, rdi\n");
        printf("  setle al\n");
        printf("  movzb rax, al\n");
        break;
    }

    printf("  push rax\n");
    printf("# end gen node (type is %d)\n", node->kind);
}

void emit_data(Program *prog)
{
    printf(".data\n");

    for (VarList *vl = prog->globals; vl; vl = vl->next)
    {
        Var *var = vl->var;
        printf("%s:\n", var->name);
        printf("  .zero %d\n", size_of(var->ty));
    }
}

void load_arg(Var *var, int idx)
{
    int sz = size_of(var->ty);
    if (sz == 1)
    {
        printf("  mov [rbp-%d], %s\n", var->offset, argreg1[idx]);
    }
    else
    {
        assert(sz == 8);
        printf("  mov [rbp-%d], %s\n", var->offset, argreg8[idx]);
    }
}

void emit_text(Program *prog)
{
    printf(".text\n");

    for (Function *fn = prog->fns; fn; fn = fn->next)
    {
        printf(".global %s\n", fn->name);
        printf("%s:\n", fn->name);
        current_fn = fn;
        log_function(fn);

        // プロローグ
        printf("  push rbp\n");
        printf("  mov rbp, rsp\n");
        printf("  sub rsp, %d\n", fn->stack_size);

        int i = 0;
        for (VarList *vl = fn->params; vl; vl = vl->next)
        {
            load_arg(vl->var, i++);
        }

        for (int i = 0; fn->body[i]; i++)
        {
            gen(fn->body[i]);
        }

        // エピローグ
        printf(".L.return.%s:\n", fn->name);
        printf("  mov rsp, rbp\n");
        printf("  pop rbp\n");
        printf("  ret\n");
    }
}

void codegen(Program *prog)
{
    log("Start codegen:");
    assign_lvar_offsets(prog);
    printf(".intel_syntax noprefix\n");
    emit_data(prog);
    emit_text(prog);
}
