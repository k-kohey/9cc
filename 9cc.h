#include <assert.h>
#include <stdbool.h>

#define _POSIX_C_SOURCE 200809L

typedef struct Token Token;
typedef struct Node Node;
typedef struct Type Type;
typedef struct Var Var;
typedef struct Function Function;

typedef enum
{
    TK_RESERVED, // 記号
    TK_IDENT,    // 識別子
    TK_NUM,      // 整数トークン
    TK_EOF,      // 入力の終わりを表すトークン
    TK_RETURN,   // return
} TokenKind;

struct Token
{
    TokenKind kind; // トークンの型
    Token *next;    // 次の入力トークン
    int val;        // kindがTK_NUMの場合、その数値
    char *str;      // トークン文字列
    int len;        // トークンの長さ
};

// ローカル変数の型
typedef struct Var Var;
struct Var
{
    char *name; // Variable name
    Type *ty;   // Type
    int offset; // Offset from RBP
    bool is_local;
};

typedef struct VarList VarList;
struct VarList
{
    VarList *next;
    Var *var;
};

struct Function
{
    Function *next;
    VarList *params;
    char *name;
    Node *body[100];
    VarList *locals;
    int stack_size;
};

typedef struct
{
    VarList *globals;
    Function *fns;
} Program;

Program *program();

Token *peek(char *s);
void expect(char *op);
extern char *user_input;
extern Token *token;

Token *tokenize();

typedef enum
{
    ND_ADD,    // +
    ND_SUB,    // -
    ND_MUL,    // *
    ND_DIV,    // /
    ND_NUM,    // 整数
    ND_EQ,     // ==
    ND_NE,     // !=
    ND_LT,     // <
    ND_LE,     // <=
    ND_ASSIGN, // =
    ND_ADDR,   // 単項&
    ND_DEREF,  // 単項*
    ND_LVAR,   // ローカル変数
    ND_RETURN,
    ND_BLOCK, // { ... }
    ND_IF,
    ND_FOR,
    ND_FUNCALL,
    ND_NULL,
    ND_EXPR_STMT,
    ND_SIZEOF, // "sizeof"
} NodeKind;

// 抽象構文木のノードの型
struct Node
{
    NodeKind kind; // ノードの型
    Node *lhs;     // 左辺
    Node *rhs;     // 右辺
    // TODO: Nodeを連結リストにする
    Node *body[100]; // kindがND_BLOCKの場合のみ使う
    Type *ty;        // Type, e.g. int or pointer to int

    // kindがND_IFかFORの場合のみ使う
    Node *cond;
    Node *then;
    Node *els;
    Node *init;
    Node *inc;

    char *funcname;
    Node *args[6];

    Var *var; // kindがND＿LVARの場合のみ使う
    int val;  // kindがND＿NUMの場合のみ使う
};

typedef enum
{
    TY_INT,
    TY_PTR,
    TY_ARRAY,
} TypeKind;

struct Type
{
    TypeKind kind;
    Type *base;
    int array_size;
};

extern Type *ty_int;

Type *int_type();
Type *pointer_to(Type *base);
void add_type(Program *prog);
Type *array_of(Type *base, int size);
int size_of(Type *ty);

Program *program();
void codegen(Program *prog);

// log
void init_log();
void log(const char *fmt, ...);
void log_tokens(Token *token);
void log_nodes(Node *nodes[]);
void log_node(Node *node);
void log_function(Function *fn);
void error_at(char *loc, char *fmt, ...);
void error(char *fmt, ...);
