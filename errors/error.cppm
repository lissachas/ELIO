module;

#include <string>
export module error;

export class Error {
    public: 
        void error(int line, std::string where, std::string message);
        bool _had_error() const { return had_error; }

    private:

        void report(int line, std::string where, std::string message);
        
        bool had_error = false;
};

export struct ParseError : public std::exception {
    const char* what() const noexcept override { return "parse error"; }
};

void Error::report(int line, std::string where, std::string message) {
    printf("At line %d : Error [%s] : %s \n", line, where.c_str(), message.c_str());
    had_error = true;
}

void Error::error(int line, std::string where, std::string message) {
    report(line, where, message);
}