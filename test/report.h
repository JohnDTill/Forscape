#ifndef REPORT_H
#define REPORT_H

#include <iostream>
#include <string>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <unordered_map>

constexpr size_t name_width = 20;

inline void report(const std::string& test_name, bool pass){
    std::cout << "-- " << test_name << ":";
    for(size_t i = test_name.size(); i < name_width; i++)
        std::cout << ' ';
    std::cout << (pass ? "pass" : "FAIL") << std::endl;
}

std::chrono::steady_clock::time_point start_time;

inline void startClock(){
    start_time = std::chrono::steady_clock::now();
}

std::vector<std::pair<std::string, double>> tests;
std::unordered_map<std::string, double> old_results;

void loadBaseline(){
    #ifdef NDEBUG
    std::ifstream in("../test/baseline_release.csv");
    #else
    std::ifstream in("../test/baseline_debug.csv");
    #endif
    assert(in.is_open());

    std::string line;
    std::getline(in, line); //Skip headers
    while(std::getline(in, line)){
        auto split = line.find(',');
        assert(split != std::string::npos);
        std::string name = line.substr(0, split);
        double old_result = std::stod(line.substr(split+1));
        old_results[name] = old_result;
    }

    std::cout << "Benchmark" << std::endl;
}

inline void report(const std::string& test_name, size_t N){
    auto duration = std::chrono::steady_clock::now() - start_time;
    auto avg_runtime_ns = duration.count() / static_cast<double>(N);
    tests.push_back(std::make_pair(test_name, avg_runtime_ns));

    if(old_results.empty()) loadBaseline();

    std::cout << "-- " << test_name << ":";
    for(size_t i = test_name.size(); i < name_width; i++)
        std::cout << ' ';

    size_t chars;
    if(avg_runtime_ns < 1e3){
        std::string str = std::to_string(avg_runtime_ns);
        std::cout << str << " ns";
        chars = str.size() + 3;
    }else if(avg_runtime_ns < 1e6){
        std::string str = std::to_string(avg_runtime_ns/1e3);
        std::cout << str << " us";
        chars = str.size() + 3;
    }else if(avg_runtime_ns < 1e9){
        std::string str = std::to_string(avg_runtime_ns/1e6);
        std::cout << str << " ms";
        chars = str.size() + 3;
    }else{
        std::string str = std::to_string(avg_runtime_ns/1e9);
        std::cout << str << " s";
        chars = str.size() + 2;
    }

    for(size_t i = chars; i < name_width; i++)
        std::cout << ' ';

    auto lookup = old_results.find(test_name);
    if(lookup == old_results.end()){
        std::cout << "no baseline";
    }else{
        double percent = 100*avg_runtime_ns / lookup->second;
        std::cout << percent << "% runtime";
        if(percent > 110) std::cout << "   !!!!";
    }

    std::cout << std::endl;
}

inline void recordResults(){
    std::filesystem::create_directory("../test/out");

    #ifdef NDEBUG
    std::ofstream out("../test/out/benchmark_release.csv");
    #else
    std::ofstream out("../test/out/benchmark_debug.csv");
    #endif

    out << "test,avg runtime (ns)\n";

    for(const auto& entry : tests)
        out << entry.first << "," << std::to_string(entry.second) << '\n';
}

#endif // REPORT_H
