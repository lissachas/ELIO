#pragma once

#include <string>

class Error {
    public: 
        void error(int line, std::string where, std::string message);

    private:

        void report(int line, std::string where, std::string message);
        
        bool had_error = false;
};