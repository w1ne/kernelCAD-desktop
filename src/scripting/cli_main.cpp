#include "ScriptEngine.h"
#include <iostream>
#include <string>
#include <fstream>

int main(int argc, char* argv[])
{
    scripting::ScriptEngine engine;

    // Send log messages to stderr so they don't pollute the JSON output.
    engine.setLogCallback([](const std::string& msg) {
        std::cerr << "[log] " << msg << std::endl;
    });

    // Batch mode: kernelcad-cli script.json
    if (argc > 1) {
        std::ifstream f(argv[1]);
        if (!f.is_open()) {
            std::cerr << "Error: cannot open " << argv[1] << std::endl;
            return 1;
        }
        std::string content((std::istreambuf_iterator<char>(f)),
                             std::istreambuf_iterator<char>());
        std::cout << engine.executeBatch(content) << std::endl;
        return 0;
    }

    // Interactive mode: read one JSON command per line from stdin (JSONL)
    std::string line;
    while (std::getline(std::cin, line)) {
        // Skip empty lines and comments
        if (line.empty() || line[0] == '#') continue;
        std::string result = engine.execute(line);
        std::cout << result << std::endl;
        std::cout.flush();
    }
    return 0;
}
