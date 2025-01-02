#include <stdarg.h>
#include <stdio.h>
#include "9cc.h"

void error(char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    exit(1);
}

void error_at(char *loc, char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);

    int pos = loc - user_input;
    fprintf(stderr, "%s\n", user_input);
    fprintf(stderr, "%*s", pos, ""); // print pos spaces.
    fprintf(stderr, "^ ");
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    exit(1);
}

void init_log()
{
    FILE *file = fopen("log.txt", "w");
    if (file == NULL)
    {
        fprintf(stderr, "log.txtを開くことができませんでした\n");
        return;
    }
    fclose(file);
}

void log(const char *fmt, ...)
{
    FILE *file = fopen("log.txt", "a");
    if (file == NULL)
    {
        fprintf(stderr, "log.txtを開くことができませんでした\n");
        return;
    }

    va_list ap;
    va_start(ap, fmt);
    vfprintf(file, fmt, ap);
    va_end(ap);

    fprintf(file, "\n");
    fclose(file);
}

void log_tokens(Token *token)
{
    log("Tokens:");
    while (token)
    {
        log("  Token kind: %d, str: %.*s, len: %d", token->kind, token->len, token->str, token->len);
        token = token->next;
    }
}

void log_node(Node *node)
{
    if (!node)
        return;

    switch (node->kind)
    {
    case ND_ADD:
        log("  Node kind: ND_ADD");
        break;
    case ND_SUB:
        log("  Node kind: ND_SUB");
        break;
    case ND_MUL:
        log("  Node kind: ND_MUL");
        break;
    case ND_DIV:
        log("  Node kind: ND_DIV");
        break;
    case ND_NUM:
        log("  Node kind: ND_NUM, val: %d", node->val);
        break;
    case ND_EQ:
        log("  Node kind: ND_EQ");
        break;
    case ND_NE:
        log("  Node kind: ND_NE");
        break;
    case ND_LT:
        log("  Node kind: ND_LT");
        break;
    case ND_LE:
        log("  Node kind: ND_LE");
        break;
    case ND_ASSIGN:
        log("  Node kind: ND_ASSIGN");
        break;
    case ND_LVAR:
        log("  Node kind: ND_LVAR, offset: %d", node->offset);
        break;
    case ND_BLOCK:
        log("  Node kind: ND_BLOCK, body: %p", node->body);
        break;
    case ND_RETURN:
        log("  Node kind: ND_RETURN");
        break;
    case ND_IF:
        log("  Node kind: ND_IF");
        log("  Condition:");
        log_node(node->cond);
        log("  Then:");
        log_node(node->then);
        log("  Else:");
        log_node(node->els);
        break;
    case ND_FOR:
        log("  Node kind: ND_FOR");
        log("  Init:");
        if (node->init)
            log_node(node->init);
        log("  Condition:");
        if (node->cond)
            log_node(node->cond);
        log("  Increment:");
        if (node->inc)
            log_node(node->inc);
        break;
    case ND_FUNCALL:
        log("  Node kind: ND_FUNCALL, funcname: %s", node->funcname);
        break;
    default:
        log("  Node kind: unknown");
        break;
    }

    if (node->lhs)
    {
        log("  Left-hand side:");
        log_node(node->lhs);
    }

    if (node->rhs)
    {
        log("  Right-hand side:");
        log_node(node->rhs);
    }
}

void log_nodes(Node *nodes[])
{
    log("Nodes:");
    for (int i = 0; nodes[i] != NULL; i++)
    {
        log_node(nodes[i]);
    }
}

void log_function(Function *fn)
{
    log("Function:");
    log("  name: %s", fn->name);
    log("  stack_size: %d", fn->stack_size);
    log("  params:");
    for (LVar *var = fn->params; var; var = var->next)
    {
        log("    %s", var->name);
    }
    log("  locals:");
    for (LVar *var = fn->locals; var; var = var->next)
    {
        log("    %s", var->name);
    }
}