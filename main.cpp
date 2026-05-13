#include <iostream>
#include <fstream>
#include <string>
#include "lexer.hpp"


void run_file(char const* filename) {
    std::string line;
    std::string set;

    std::ifstream in(filename);
    if (in.is_open()) {
        while (std::getline(in, line)) {
            set.append(line);
        }
    }
    in.close();

    Lexer lex;
    lex.start(set);
}

int main(int argc, char const *argv[])
{
    if (argc > 1) {
        printf("Usage: realize [script]");
        exit(1);
    } else if (argc == 1) {
        run_file(argv[0]);
    } else {
        printf("Usage: realize [script]");
        exit(1);
    }
    return 0;
}
