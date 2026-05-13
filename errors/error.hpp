#pragma once

#include <string>

class Error {
    public: 
        void error(int line, std::string where, std::string message);
        bool _had_error() const { return had_error; }

    private:

        void report(int line, std::string where, std::string message);
        
        bool had_error = false;
};

struct ParseError : public std::exception {
    const char* what() const noexcept override { return "parse error"; }
};