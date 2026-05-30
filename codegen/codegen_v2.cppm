module;
#include <vector>
#include <memory>
#include <unordered_map>
#include <string>
#include <string_view>
#include <cstddef>

export module codegen_v2;

import expr;
import typecheck;
import tokens;

export class Codogen {
    public:
    void generate(Node* program);

    private:
};