#include <stdio.h>
#include <stdbool.h>
#include "9cc.h"

VarList *locals;
VarList *globals;

Var *push_var(char *name, Type *ty, bool is_local)
{
    Var *var = calloc(1, sizeof(Var));
    var->name = name;
    var->ty = ty;
    var->is_local = is_local;

    VarList *vl = calloc(1, sizeof(VarList));
    vl->var = var;

    if (is_local)
    {
        vl->next = locals;
        locals = vl;
    }
    else
    {
        vl->next = globals;
        globals = vl;
    }
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

Token *consume_sizeof()
{
    if (token->kind != TK_RESERVED || token->len != 6 || memcmp(token->str, "sizeof", 6))
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

Node *new_var(Var *var)
{
    Node *node = new_node(ND_LVAR, NULL, NULL);
    node->var = var;
    return node;
}

Function *function();
Type *basetype();
void global_var();
Node *declaration();
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
Node *postfix();

bool is_function()
{
    Token *tok = token;
    basetype();
    bool isFunc = consume_ident() && consume("(");
    token = tok;
    return isFunc;
}

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

    for (VarList *vl = globals; vl; vl = vl->next)
    {
        Var *var = vl->var;
        if (strlen(var->name) == tok->len && !memcmp(tok->str, var->name, tok->len))
            return var;
    }

    return NULL;
}

// basetype = ("char" | "int") "*"*
Type *basetype()
{
    Type *ty;
    if (consume("char"))
    {
        ty = char_type();
    }
    else
    {
        expect("int");
        ty = int_type();
    }

    while (consume("*"))
        ty = pointer_to(ty);
    return ty;
}

Type *read_type_suffix(Type *base)
{
    if (!consume("["))
        return base;
    int sz = expect_number();
    expect("]");
    base = read_type_suffix(base);
    return array_of(base, sz);
}

VarList *read_func_param()
{
    Type *ty = basetype();
    char *name = expect_ident();
    ty = read_type_suffix(ty);
    VarList *vl = calloc(1, sizeof(VarList));
    vl->var = push_var(name, ty, true);
    return vl;
}

VarList *read_func_params()
{
    if (consume(")"))
        return NULL;

    VarList *head = read_func_param();
    VarList *cur = head;

    while (!consume(")"))
    {
        expect(",");
        cur->next = read_func_param();
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

// primary = "(" expr ")" | "sizeof" unary | ident func-args? | num
Node *primary()
{
    Token *tok;
    // 次のトークンが"("なら、"(" expr ")"のはず
    if (consume("("))
    {
        Node *node = expr();
        expect(")");
        return node;
    }
    if (tok = consume_sizeof())
        return new_node(ND_SIZEOF, unary(), NULL);
    if (tok = consume_ident())
    {
        if (consume("("))
        {
            return funcall(tok);
        }
        Node *node = calloc(1, sizeof(Node));
        node->kind = ND_LVAR;

        Var *var = find_lvar(tok);
        if (!var)
            error_at(tok->str, "変数が見つかりません");
        node->var = var;
        return node;
    }

    // そうでなければ数値のはず
    return new_node_num(expect_number());
}

// postfix = primary ("[" expr "]")*
Node *postfix()
{
    Node *node = primary();

    while (consume("["))
    {
        // x[y] is short for *(x+y)
        Node *exp = new_node(ND_ADD, node, expr());
        expect("]");
        node = new_node(ND_DEREF, exp, NULL);
    }
    return node;
}

// unary   = ("+" | "-" | "*" | "&")? unary | postfix
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
    return postfix();
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

bool is_typename()
{
    return peek("int") || peek("char");
}

// stmt = expr ";"
// | "return" expr ";"
// | "{" compound-stmt |
// | "if" "(" expr ")" stmt ("else" stmt)?
// | "for" "(" expr-stmt expr? ";" expr? ")" stmt
// | "while" "(" expr ")" stmt
// | declaration
Node *stmt()
{
    Node *node;

    if (consume_return())
    {
        Node *n = expr();
        node = new_node(ND_RETURN, n, NULL);
        expect(";");
        return node;
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
    else if (is_typename())
    {
        return declaration();
    }
    else
    {
        node = expr();
        expect(";");
        return node;
    }
}

// function = basetype ident "(" params? ")" "{" stmt* "}"
// params   = param ("," param)*
// param    = basetype ident
Function *function()
{
    locals = NULL;
    Function *fn = calloc(1, sizeof(Function));
    basetype();
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

// global-var = basetype ident ("[" num "]")* ";"
void global_var()
{
    Type *ty = basetype();
    char *name = expect_ident();
    ty = read_type_suffix(ty);
    expect(";");
    push_var(name, ty, false);
}

// declaration = basetype ident ("[" num "]")* ("=" expr) ";"
Node *declaration()
{
    Type *ty = basetype();
    char *name = expect_ident();
    ty = read_type_suffix(ty);
    Var *var = push_var(name, ty, true);
    if (consume(";"))
        return new_node(ND_NULL, NULL, NULL);

    expect("=");
    Node *lhs = new_var(var);
    Node *rhs = expr();
    expect(";");
    Node *node = new_node(ND_ASSIGN, lhs, rhs);
    return new_node(ND_EXPR_STMT, node, NULL);
}

// program = (global-var | function)*
Program *program()
{
    // consume("{");
    Function head = {};
    Function *cur = &head;
    globals = NULL;
    while (!at_eof())
    {
        if (is_function())
        {
            cur->next = function();
            cur = cur->next;
        }
        else
        {
            global_var();
        }
    }
    Program *prog = calloc(1, sizeof(Program));
    prog->globals = globals;
    prog->fns = head.next;
    return prog;
}
