#include <string>
#include <fstream>
#include <iostream>
#include <vector>

#include "FileSystem.h"
#include "util.h"


bool runCommand(std::vector<std::string> arguments) {
    // Separate out the command and the arguments
    std::string command = arguments[0];
    arguments.erase(arguments.begin());
    bool isValid = true;

    if (command.compare("M") == 0) {
        if (arguments.size() != 1) {
            isValid = false;
        }

    } else if (command.compare("C") == 0) {
        if (arguments.size() != 2) {
            isValid = false;
        }
    } else if (command.compare("D") == 0) {
        if (arguments.size() != 1) {
            isValid = false;
        }
        
    } else if (command.compare("R") == 0) {
        if (arguments.size() != 2) {
            isValid = false;
        }
        
    } else if (command.compare("W") == 0) {
        if (arguments.size() != 2) {
            isValid = false;
        }
        
    } else if (command.compare("B") == 0) {
        //??? THIS COULD BE MORE
        if (arguments.size() != 1) {
            isValid = false;
        }
        
    } else if (command.compare("L") == 0) {
        if (arguments.size() != 0) {
            isValid = false;
        }
        
    } else if (command.compare("E") == 0) {
        if (arguments.size() != 2) {
            isValid = false;
        }
        
    } else if (command.compare("O") == 0) {
        if (arguments.size() != 0) {
            isValid = false;
        }
        
    } else if (command.compare("Y") == 0) {
        if (arguments.size() != 1) {
            isValid = false;
        }
        
    } else {
        return false;
    }

    return isValid;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        std::cerr << "Please provide a command file.\n";
        return 0;
    } else if (argc > 2) {
        std::cerr << "Too many arguments. Please provide one command file.\n";
        return 0;
    }

    std::string command_file_name = argv[1];
    std::ifstream command_file (command_file_name);

    if (!command_file.is_open()){
        std::cerr << "Unable to open the command file.\n";
        return 0;
    }

    std::string command;
    int line_number = 0;
    while (getline(command_file, command)) {
        line_number++;
        std::cout<<command << std::endl;
        std::vector<std::string> arguments = tokenize(command, " ");
        if (runCommand(arguments) == false) {
            std::cerr << "Command Error: " << command_file_name << ", " << line_number << std::endl;
        }
    }

    command_file.close();

    return 0;
}