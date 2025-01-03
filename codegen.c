#include "9cc.h"

static char *argreg[] = {"%rdi", "%rsi", "%rdx", "%rcx", "%r8", "%r9"};
static Function *current_fn;
static void gen(Node *node);

static int align_to(int n, int align)
{
    return (n + align - 1) / align * align;
}

static void assign_lvar_offsets(Function *prog)
{
    for (Function *fn = prog; fn; fn = fn->next)
    {
        int offset = 0;
        for (LVar *var = fn->locals; var; var = var->next)
        {
            offset += 8;
            var->offset = -offset;
        }
        fn->stack_size = align_to(offset, 16);
    }
}

static int count(void)
{
    static int i = 1;
    return i++;
}

void gen_lval(Node *node)
{
    switch (node->kind)
    {
    case ND_LVAR:
        printf("  lea rax, [rbp-%d]\n", node->offset);
        printf("  push rax\n");
        return;
    case ND_DEREF:
        gen(node->lhs);
        return;
    default:
        error("gen_lval: invalid node");
        return;
    }
}

void gen(Node *node)
{
    printf("# start gen node (type is %d)\n", node->kind);
    switch (node->kind)
    {
    case ND_NUM:
        printf("  push %d\n", node->val);
        return;
    case ND_LVAR:
        gen_lval(node);
        printf("  pop rax\n");
        printf("  mov rax, [rax]\n");
        printf("  push rax\n");
        return;
    case ND_ADDR:
        gen_lval(node->lhs);
        return;
    case ND_DEREF:
        gen(node->lhs);
        printf("  pop rax\n");
        printf("  mov rax, [rax]\n");
        printf("  push rax\n");
        return;
    case ND_ASSIGN:
        gen_lval(node->lhs);
        gen(node->rhs);

        printf("  pop rdi\n");
        printf("  pop rax\n");
        printf("  mov [rax], rdi\n");
        printf("  push rdi\n");
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
            printf("  pop %s\n", argreg[i]);
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
        printf("  add rax, rdi\n");
        break;
    case ND_SUB:
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

void codegen(Function *prog)
{
    log("Start codegen:");
    assign_lvar_offsets(prog);
    printf(".intel_syntax noprefix\n");
    for (Function *fn = prog; fn; fn = fn->next)
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
        for (LVar *var = fn->params; var; var = var->next)
        {
            printf("  mov [rbp%d], %s\n", var->offset, argreg[i++]);
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
