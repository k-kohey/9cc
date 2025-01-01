#include "9cc.h"

static int align_to(int n, int align)
{
    return (n + align - 1) / align * align;
}

static void assign_lvar_offsets(Function *prog)
{
    int offset = 0;
    for (LVar *var = prog->locals; var; var = var->next)
    {
        offset += 8;
        var->offset = -offset;
    }
    prog->stack_size = align_to(offset, 16);
}

static int count(void)
{
    static int i = 1;
    return i++;
}

void gen_lval(Node *node)
{
    if (node->kind != ND_LVAR)
    {
        error("代入の左辺値が変数ではありません");
    }

    printf("  mov rax, rbp\n");
    printf("  sub rax, %d\n", node->offset);
    printf("  push rax\n");
}

void gen(Node *node)
{
    log("  gen node (type is %d)", node->kind);
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
        printf("  mov rsp, rbp\n");
        printf("  pop rbp\n");
        printf("  ret\n");
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
}

void codegen(Function *prog)
{
    log("Start codegen:");
    assign_lvar_offsets(prog);
    printf(".intel_syntax noprefix\n");
    printf(".global main\n");
    printf("main:\n");

    // プロローグ
    printf("  push rbp\n");
    printf("  mov rbp, rsp\n");
    printf("  sub rsp, %d\n", prog->stack_size);

    for (int i = 0; prog->body[i]; i++)
    {
        gen(prog->body[i]);
        printf("  pop rax\n");
    }

    // エピローグ
    // 最後の式の結果がRAXに残っているのでそれが返り値になる
    printf("  mov rsp, rbp\n");
    printf("  pop rbp\n");
    printf("  ret\n");
}