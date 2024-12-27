#include <stdio.h>
#include "9cc.h"

char *user_input;
Token *token;

int main(int argc, char **argv)
{
    init_log();
    if (argc != 2)
    {
        fprintf(stderr, "引数の個数が正しくありません\n");
        return 1;
    }

    // トークナイズしてパースする
    user_input = argv[1];
    token = tokenize();
    log_tokens(token);
    Function *prog = parse();
    log_nodes(prog->body);
    codegen(prog);

    return 0;
}