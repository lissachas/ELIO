#include "error.hpp"

void Error::report(int line, std::string where, std::string message) {
    printf("At line %d : Error [%s] : %s \n", line, where.c_str(), message.c_str());
    had_error = true;
}

void Error::error(int line, std::string where, std::string message) {
    report(line, where, message);
}