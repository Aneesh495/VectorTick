// VectorTick Unified Test Suite Runner

#include "test_framework.hpp"
#include <iostream>
#include <string>

using namespace vectortick;
using namespace vectortick::test;

int main(int argc, char* argv[]) {
    std::string suite_filter;
    
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg.rfind("--suite=", 0) == 0) {
            suite_filter = arg.substr(8);
        } else if (arg == "-s" && i + 1 < argc) {
            suite_filter = argv[++i];
        } else if (arg == "--list") {
            std::cout << "Available test suites:\n";
            for (const auto& tc : TestRegistry::instance().tests()) {
                std::cout << "  " << tc.suite << "::" << tc.name << "\n";
            }
            return 0;
        } else if (arg[0] != '-') {
            suite_filter = arg;
        }
    }
    
    std::cout << "VectorTick Test Suite\n";
    std::cout << "=====================\n\n";
    
    int result = TestRegistry::instance().run(suite_filter);
    
    return result;
}
