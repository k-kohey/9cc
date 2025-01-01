#include <stdio.h>
#include <stdbool.h>
#include "9cc.h"

extern LVar *locals;

// 次のトークンが期待している記号のときには、トークンを1つ読み進めて
// 真を返す。それ以外の場合には偽を返す。
bool consume(char *op)
{
    if (token->kind != TK_RESERVED ||
        strlen(op) != token->len ||
        memcmp(token->str, op, token->len))
        return false;
    token = token->next;
    return true;
}

bool consume_return()
{
    if (token->kind != TK_RETURN)
        return false;
    token = token->next;
    return true;
}

Token *consume_ident()
{
    if (token->kind != TK_IDENT)
        return NULL;
    Token *t = token;
    token = token->next;
    return t;
}

Node *new_node(NodeKind kind, Node *lhs, Node *rhs)
{
    Node *node = calloc(1, sizeof(Node));
    node->kind = kind;
    node->lhs = lhs;
    node->rhs = rhs;
    return node;
}

Node *new_node_num(int val)
{
    Node *node = calloc(1, sizeof(Node));
    node->kind = ND_NUM;
    node->val = val;
    return node;
}

Node *expr();
Node *equality();
Node *relational();
Node *add();
Node *mul();
Node *primary();
Node *unary();
Node *assign();
Node *stmt();
Node *compound_stmt();

// equality = relational ("==" relational | "!=" relational)*
Node *equality()
{
    Node *node = relational();

    for (;;)
    {
        if (consume("=="))
            node = new_node(ND_EQ, node, relational());
        else if (consume("!="))
            node = new_node(ND_NE, node, relational());
        else
            return node;
    }
}

// relational = add ("<" add | "<=" add | ">" add | ">=" add)*
Node *relational()
{
    Node *node = add();

    for (;;)
    {
        if (consume("<"))
            node = new_node(ND_LT, node, add());
        else if (consume("<="))
            node = new_node(ND_LE, node, add());
        else if (consume(">"))
            node = new_node(ND_LT, add(), node);
        else if (consume(">="))
            node = new_node(ND_LE, add(), node);
        else
            return node;
    }
}

// add = mul ("+" mul | "-" mul)*
Node *add()
{
    Node *node = mul();

    for (;;)
    {
        if (consume("+"))
            node = new_node(ND_ADD, node, mul());
        else if (consume("-"))
            node = new_node(ND_SUB, node, mul());
        else
            return node;
    }
}

// mul     = unary ("*" unary | "/" unary)*
Node *mul()
{
    Node *node = unary();

    for (;;)
    {
        if (consume("*"))
            node = new_node(ND_MUL, node, unary());
        else if (consume("/"))
            node = new_node(ND_DIV, node, unary());
        else
            return node;
    }
}

LVar *locals;

LVar *find_lvar(Token *tok)
{
    for (LVar *var = locals; var; var = var->next)
        if (var->len == tok->len && !memcmp(tok->str, var->name, var->len))
        {
            return var;
        }
    return NULL;
}

// primary = num | ident | "(" expr ")"
Node *primary()
{
    // 次のトークンが"("なら、"(" expr ")"のはず
    if (consume("("))
    {
        Node *node = expr();
        expect(")");
        return node;
    }
    Token *tok = consume_ident();
    if (tok)
    {
        Node *node = calloc(1, sizeof(Node));
        node->kind = ND_LVAR;

        LVar *lvar = find_lvar(tok);
        if (lvar)
        {
            node->offset = lvar->offset;
        }
        else
        {
            lvar = calloc(1, sizeof(LVar));
            lvar->next = locals;
            lvar->name = tok->str;
            lvar->len = tok->len;
            lvar->offset = (locals ? locals->offset : 0) + 8;
            node->offset = lvar->offset;
            locals = lvar;
        }
        return node;
    }

    // そうでなければ数値のはず
    return new_node_num(expect_number());
}

// unary   = ("+" | "-")? primary
Node *unary()
{
    if (consume("+"))
        return unary();
    if (consume("-"))
        return new_node(ND_SUB, new_node_num(0), unary());
    return primary();
}

Node *code[100];

// assign  = equality ("=" assign)?
Node *assign()
{
    Node *node = equality();
    if (consume("="))
        node = new_node(ND_ASSIGN, node, assign());
    return node;
}

// expr = assign
Node *expr()
{
    return assign();
}

// compound-stmt = stmt* "}"
Node *compound_stmt()
{
    Node *node = new_node(ND_BLOCK, NULL, NULL);
    int i = 0;
    while (!consume("}"))
    {
        node->body[i++] = stmt();
    }
    node->body[i] = NULL;
    return node;
}

// stmt = expr ";"
// | "return" expr ";"
// | "{" compound-stmt |
// | "if" "(" expr ")" stmt ("else" stmt)?
// | "for" "(" expr-stmt expr? ";" expr? ")" stmt
Node *stmt()
{
    Node *node;

    if (consume_return())
    {
        Node *n = expr();
        node = new_node(ND_RETURN, n, NULL);
    }
    else if (consume("if"))
    {
        Node *node = new_node(ND_IF, NULL, NULL);
        expect("(");
        node->cond = expr();
        expect(")");
        node->then = stmt();
        if (consume("else"))
            node->els = stmt();
        return node;
    }
    else if (consume("for"))
    {
        Node *node = new_node(ND_FOR, NULL, NULL);
        expect("(");
        if (!consume(";"))
        {
            node->init = expr();
            expect(";");
        }

        if (!consume(";"))
        {
            node->cond = expr();
            expect(";");
        }

        if (!consume(")"))
        {
            node->inc = expr();
            expect(")");
        }

        node->then = stmt();

        return node;
    }
    else if (consume("{"))
    {
        return compound_stmt();
    }
    else
    {
        node = expr();
    }

    if (!consume(";"))
        error_at(token->str, "';'ではないトークンです");
    return node;
}

// program = stmt*
Function *parse()
{
    consume("{");
    Function *prog = calloc(1, sizeof(Function));
    Node *stmts = compound_stmt();
    for (int i = 0; stmts->body[i]; i++)
    {
        prog->body[i] = stmts->body[i];
    }
    prog->locals = locals;
    return prog;
}
