#include <string>
#include <fstream>
#include <iostream>

#include "FileSystem.h"

int main(int argc, char **argv) {
    if (argc < 2) {
        std::cout << "Please provide a command file.\n";
        return 0;
    } else if (argc > 2) {
        std::cout << "Too many arguments. Please provide one command file.\n";
        return 0;
    }

    std::ifstream command_file (argv[1]);

    if (!command_file.is_open()){
        std::cout << "Unable to open the command file.\n";
        return 0;
    }

    return 0;
}