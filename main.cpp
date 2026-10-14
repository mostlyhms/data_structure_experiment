#include <iostream>
#include <string>

#include "cli.h"

// Starts the menu on standard input/output.
// argv[1]: optional data file path; defaults to "students.txt" in the working directory. Returns 0.
int main(int argc, char** argv) {
    const std::string data_path = argc > 1 ? argv[1] : "students.txt";
    Cli cli(std::cin, std::cout, data_path);
    cli.run();
    return 0;
}
