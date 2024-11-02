#include "report.h"

#include <fstream>
#include <typeset_controller.h>
#include <typeset_model.h>

using namespace Forscape;

inline bool testConvertToUnicode(){
    bool ran_test_case = false;
    bool passing = true;

    const std::string file_name = BASE_TEST_DIR "/export_to_unicode_regression_cases.π";
    std::ifstream infile(std::filesystem::u8path(file_name));
    assert(infile.is_open());

    std::string line;
    size_t line_num = 0;
    while(std::getline(infile, line)){
        line_num++;
        if(line.empty() || line.front() != '>') continue;

        const auto test_start = line.find(':');
        if(test_start == std::string::npos){
            std::cout << __FILE__ << ": did not find test delimiter on line " << line_num << std::endl;
            std::cout << "    " << line << std::endl;
            passing = false;
            continue;
        }

        const std::string input = line.substr(1+test_start);
        if(!std::getline(infile, line)){
            std::cout << __FILE__ << ": file ended before test expectation on line " << line_num << std::endl;
            passing = false;
            break;
        }
        line_num++;

        static constexpr std::string_view exp_str = "Expected:";
        if(line.substr(0, exp_str.size()) != exp_str){
            std::cout << __FILE__ << ": test not followed by \"" << exp_str << "\" on line " << line_num << std::endl;
            std::cout << "    " << line << std::endl;
            passing = false;
            continue;
        }

        const std::string expected = line.substr(exp_str.size());
        Typeset::Model* model = Typeset::Model::fromSerial(input);
        Typeset::Controller controller(model);
        controller.selectAll();

        std::string output;
        controller.selection().convertToUnicode(output);
        delete model;

        if(output != expected){
            std::cout << file_name << " " << line_num-1 << ": expected \"" << expected << "\", but output was \""
                      << output << "\"" << std::endl;
            passing = false;
        }

        ran_test_case = true;
    }

    if(!ran_test_case){
        std::cout << __FILE__ << ": the test logic did not find any test cases" << std::endl;
        passing = false;
    }

    report("Convert to Unicode", passing);

    return passing;
}
