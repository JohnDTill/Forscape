#include <cassert>
#include <fstream>
#include <forscape_serial.h>
#include <string>
#include "report.h"

inline bool testSerial(){
    std::ifstream good_examples("serial_valid.txt");
    if(!good_examples.is_open()){
        printf("Failed to open \"serial_valid.txt\"\n");
        return false;
    }

    std::ifstream bad_examples("serial_malformed.txt");
    if(!bad_examples.is_open()){
        printf("Failed to open \"serial_malformed.txt\"\n");
        return false;
    }

    bool passing = true;

    std::string line;
    while(std::getline(good_examples, line)){
        if(!Forscape::isValidSerial(line)){
            printf("Valid input rejected: %s\n", line.c_str());
            passing = false;
        }
    }

    while(std::getline(bad_examples, line)){
        if(Forscape::isValidSerial(line)){
            printf("Invalid input accepted: %s\n", line.c_str());
            passing = false;
        }
    }

    report("Serial spec", passing);
    return passing;
}
