#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <cstdlib>  // for system()
#include <windows.h>
#include <filesystem>

#ifdef _WIN64
#define TARGET_MACHINE "\" /MACHINE:X64"
#else
#define TARGET_MACHINE "\" /MACHINE:X86"
#endif


bool fileExists(const std::string& path) {
    DWORD attrib = GetFileAttributesA(path.c_str());
    return (attrib != INVALID_FILE_ATTRIBUTES && !(attrib & FILE_ATTRIBUTE_DIRECTORY));
}

// Runs a command, waits for completion, returns exit code
int runCommand(const std::string& cmd) {
    std::cout << "Running: " << cmd << "\n";
    int result = system(cmd.c_str());
    if (result != 0) {
        std::cerr << "Command failed with code " << result << "\n";
    }
    return result;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: generate_import_lib YourDll.dll\n";
        return 1;
    }

	std::string inputDll = argv[1];
	std::string dllName = std::filesystem::path(argv[1]).replace_extension().string(); // Get just the file name

	std::string outDir = "output\\";
	std::filesystem::create_directories(outDir); // Ensure output directory exists
    std::string dumpbinOut = outDir + dllName + ".exports.txt";
    std::string defFile = outDir + dllName + ".def";
    std::string libFile = outDir + dllName + ".lib";

    // Check DLL exists
    if (!fileExists(inputDll)) {
        std::cerr << "DLL file not found: " << inputDll << "\n";
        return 1;
    }

    // Run dumpbin
    std::string dumpbinCmd = "dumpbin /EXPORTS \"" + inputDll + "\" > \"" + dumpbinOut + "\"";
    if (runCommand(dumpbinCmd) != 0) return 1;

    // Open dumpbin output and parse exports
    std::ifstream in(dumpbinOut);
    if (!in.is_open()) {
        std::cerr << "Failed to open dumpbin output file: " << dumpbinOut << "\n";
        return 1;
    }

    std::ofstream def(defFile);
    if (!def.is_open()) {
        std::cerr << "Failed to open .def file for writing: " << defFile << "\n";
        return 1;
    }

    def << "LIBRARY " << dllName << "\n";
    def << "EXPORTS\n";

    std::string line;
    bool startExtract = false;
    while (std::getline(in, line)) {
		std::cout << "Processing line: " << line << "\n";

        if (!startExtract) {
            if (line.find("ordinal") != std::string::npos &&
                line.find("hint") != std::string::npos &&
                line.find("RVA") != std::string::npos &&
                line.find("name") != std::string::npos) {
                startExtract = true;
            }
            continue;
        }

        if (line.empty()) continue;

        std::istringstream iss(line);
        std::vector<std::string> tokens{ std::istream_iterator<std::string>{iss}, std::istream_iterator<std::string>{} };

        if (tokens.size() >= 4) {
            std::string exportName = tokens[3];
            def << "    " << exportName << "\n";
            std::cout << "Exported: " << exportName << "\n";
        }
    }

    def.close();
    in.close();

    std::cout << "Created " << defFile << "\n";

    // Run lib.exe
    std::string libCmd = "lib /DEF:\"" + defFile + "\" /OUT:\"" + libFile + TARGET_MACHINE;
    if (runCommand(libCmd) != 0) return 1;

    std::cout << "Created import library " << libFile << "\n";

    return 0;
}
