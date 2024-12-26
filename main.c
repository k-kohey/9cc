#include <stdio.h>
#include "9cc.h"

char *user_input;
Token *token;

int main(int argc, char **argv)
{
    if (argc != 2)
    {
        fprintf(stderr, "引数の個数が正しくありません\n");
        return 1;
    }

    // トークナイズしてパースする
    user_input = argv[1];
    token = tokenize();
    program();
    codegen();

    return 0;
}