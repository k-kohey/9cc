#define _POSIX_C_SOURCE 200809L

typedef struct Token Token;
typedef struct Node Node;
typedef struct LVar LVar;
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
struct LVar
{
    LVar *next; // 次の変数かNULL
    char *name; // 変数の名前
    int len;    // 名前の長さ
    int offset; // RBPからのオフセット
};

struct Function
{
    Function *next;
    LVar *params;
    char *name;
    Node *body[100];
    LVar *locals;
    int stack_size;
};

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
    ND_LVAR,   // ローカル変数
    ND_RETURN,
    ND_BLOCK, // { ... }
    ND_IF,
    ND_FOR,
    ND_FUNCALL,
} NodeKind;

// 抽象構文木のノードの型
struct Node
{
    NodeKind kind;   // ノードの型
    Node *lhs;       // 左辺
    Node *rhs;       // 右辺
    int val;         // kindがND＿NUMの場合のみ使う
    int offset;      // kindがND_LVARの場合のみ使う
    Node *body[100]; // kindがND_BLOCKの場合のみ使う

    // kindがND_IFかFORの場合のみ使う
    Node *cond;
    Node *then;
    Node *els;
    Node *init;
    Node *inc;

    char *funcname;
    Node *args[6];
};

Function *parse();
void codegen(Function *prog);

// log
void init_log();
void log(const char *fmt, ...);
void log_tokens(Token *token);
void log_nodes(Node *nodes[]);
void log_function(Function *fn);
void error_at(char *loc, char *fmt, ...);
