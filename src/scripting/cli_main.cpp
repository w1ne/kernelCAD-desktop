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

    // Schema mode: kernelcad-cli --schema
    if (argc > 1 && std::string(argv[1]) == "--schema") {
        std::cout <<
            "{\n"
            "  \"name\": \"kernelcad-cli\",\n"
            "  \"version\": \"0.1.0\",\n"
            "  \"protocol\": \"JSONL: one JSON command per line on stdin, one JSON response per line on stdout\",\n"
            "  \"commands\": {\n"
            "    \"document\": [\"newDocument\",\"save\",\"load\",\"importStep\",\"exportStep\",\"exportStl\",\"undo\",\"redo\",\"recompute\"],\n"
            "    \"primitives\": [\"createBox\",\"createCylinder\",\"createSphere\",\"createTorus\",\"createPipe\"],\n"
            "    \"sketch\": [\"createSketch\",\"sketchAddPoint\",\"sketchAddLine\",\"sketchAddRectangle\",\"sketchAddCircle\",\"sketchAddArc\",\"sketchAddConstraint\",\"sketchSolve\",\"sketchDetectProfiles\"],\n"
            "    \"features\": [\"extrude\",\"fillet\",\"chamfer\",\"shell\",\"mirror\",\"circularPattern\",\"rectangularPattern\",\"hole\",\"combine\"],\n"
            "    \"queries\": [\"listBodies\",\"listFeatures\",\"getProperties\",\"faceCount\",\"edgeCount\"],\n"
            "    \"timeline\": [\"setMarker\",\"suppress\",\"deleteFeature\"]\n"
            "  },\n"
            "  \"workflow\": \"newDocument -> createSketch(plane) -> sketchAddRectangle -> sketchSolve -> extrude(sketchId,distance) -> fillet(bodyId,radius) -> exportStep(path)\",\n"
            "  \"idFormat\": \"Auto-incremented: sketch_1, body_1, pt_1, ln_1, extrude_1, fillet_2\",\n"
            "  \"units\": \"mm for distances, degrees for angles\",\n"
            "  \"commandFormat\": {\"cmd\":\"name\",\"id\":1,\"param\":\"value\"},\n"
            "  \"responseOk\": {\"id\":1,\"ok\":true,\"result\":{}},\n"
            "  \"responseError\": {\"id\":1,\"ok\":false,\"error\":\"description\"}\n"
            "}\n" << std::flush;
        return 0;
    }

    // Help mode
    if (argc > 1 && (std::string(argv[1]) == "--help" || std::string(argv[1]) == "-h")) {
        std::cerr << "Usage: kernelcad-cli [options] [script.json]\n"
                  << "\n"
                  << "Options:\n"
                  << "  --schema    Print API schema as JSON and exit\n"
                  << "  --help      Show this help\n"
                  << "\n"
                  << "Modes:\n"
                  << "  (no args)   Interactive JSONL mode (stdin → stdout)\n"
                  << "  script.json Batch mode (execute commands from file)\n"
                  << "\n"
                  << "Protocol: one JSON command per line, one JSON response per line.\n"
                  << "Example:  echo '{\"cmd\":\"createBox\",\"dx\":50,\"dy\":30,\"dz\":20}' | kernelcad-cli\n";
        return 0;
    }

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
