#include <iostream>
#include <string>

const std::string VERSION = "0.1.0";

int main(int argc, char* argv[]) {
    std::cout << "agent-probe v" << VERSION << "\n";
    std::cout << "Static analysis tool for identifying agent integration points\n";
    return 0;
}
