#include <stdio.h>
#include <stdbool.h>
#include "9cc.h"

VarList *locals;

Var *push_var(char *name)
{
    Var *var = calloc(1, sizeof(Var));
    var->name = name;

    VarList *vl = calloc(1, sizeof(VarList));
    vl->var = var;
    vl->next = locals;
    locals = vl;
    return var;
}

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

char *expect_ident()
{
    Token *tk = consume_ident();
    if (!tk)
        error_at(token->str, "識別子ではありません");

    char *s = calloc(tk->len + 1, sizeof(char));
    strncpy(s, tk->str, tk->len);
    s[tk->len] = '\0';
    return s;
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

Var *find_lvar(Token *tok)
{
    for (VarList *vl = locals; vl; vl = vl->next)
    {
        Var *var = vl->var;
        if (strlen(var->name) == tok->len && !memcmp(tok->str, var->name, tok->len))
            return var;
    }
    return NULL;
}

VarList *read_func_params()
{
    if (consume(")"))
        return NULL;

    VarList *head = calloc(1, sizeof(VarList));
    head->var = push_var(expect_ident());
    VarList *cur = head;

    while (!consume(")"))
    {
        expect(",");
        cur->next = calloc(1, sizeof(VarList));
        cur->next->var = push_var(expect_ident());
        cur = cur->next;
    }

    return head;
}

// funcall = ident "(" (assign ("," assign)*)? ")"
Node *funcall(Token *tok)
{
    Node *node = new_node(ND_FUNCALL, NULL, NULL);
    node->funcname = calloc(tok->len + 1, sizeof(char));
    strncpy(node->funcname, tok->str, tok->len);
    node->funcname[tok->len] = '\0';

    int i = 0;
    while (!consume(")"))
    {
        node->args[i++] = assign();
        consume(",");
    }
    node->args[i] = NULL;
    return node;
}

// primary = "(" expr ")" | funcall | num
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
        if (consume("("))
        {
            return funcall(tok);
        }
        Node *node = calloc(1, sizeof(Node));
        node->kind = ND_LVAR;

        Var *var = find_lvar(tok);
        if (!var)
        {
            char *s = calloc(tok->len + 1, sizeof(char));
            strncpy(s, tok->str, tok->len);
            var = push_var(s);
        }
        node->var = var;
        return node;
    }

    // そうでなければ数値のはず
    return new_node_num(expect_number());
}

// unary   = ("+" | "-" | "*" | "&")? primary
Node *unary()
{
    if (consume("+"))
        return unary();
    if (consume("-"))
        return new_node(ND_SUB, new_node_num(0), unary());
    if (consume("&"))
        return new_node(ND_ADDR, unary(), NULL);
    if (consume("*"))
        return new_node(ND_DEREF, unary(), NULL);
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
        node->body[i] = stmt();
        i++;
    }
    node->body[i] = NULL;
    return node;
}

// stmt = expr ";"
// | "return" expr ";"
// | "{" compound-stmt |
// | "if" "(" expr ")" stmt ("else" stmt)?
// | "for" "(" expr-stmt expr? ";" expr? ")" stmt
// | "while" "(" expr ")" stmt
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
    else if (consume("while"))
    {
        Node *node = new_node(ND_FOR, NULL, NULL);
        expect("(");
        node->cond = expr();
        expect(")");
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

// function = ident "(" params? ")" "{" stmt* "}"
// params   = ident ("," ident)*
Function *function()
{
    locals = NULL;
    Function *fn = calloc(1, sizeof(Function));
    fn->name = expect_ident();
    expect("(");
    fn->params = read_func_params();
    expect("{");
    Node *stmts = compound_stmt();
    for (int i = 0; stmts->body[i]; i++)
    {
        fn->body[i] = stmts->body[i];
    }
    fn->locals = locals;
}

// program = function*
Function *parse()
{
    consume("{");
    Function head = {};
    Function *prog = &head;
    while (!at_eof())
    {
        prog = prog->next = function();
    }
    return head.next;
}
