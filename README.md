# Forscape
## A language and editor for scientific computation

![alt text](doc/readme/limerick.png?raw=true "Forscape")

### Focus on the problem, not the implementation details

**Forscape** aims to solve engineering problems with low-cost abstraction, safety checks, and an intuitive user experience. The editor supports math rendering so that objects such as matrices, fractions, and symbols can be programmed in parity with scientific papers and notes.

![alt text](doc/readme/EditorInteraction.gif?raw=true "Editor interaction")

Forscape has particular emphasis on matrices. Standard matrix operations such as multiplication, norm, cross product, transpose, etcetera are supported with mathematical syntax.

![alt text](doc/readme/root_finding.png?raw=true "Forscape is designed around matrices")

The editor code-model interaction has various features for matrices. The syntax highlighting will bold matrix identifiers. Where possible, dimensions are checked at compile time with violations highlighted in real time while typing. Hovering over an identifier will show the compile-time dimensions in the tooltip.

![alt text](doc/readme/EditorMatrixFeatures.png?raw=true "The editor code-model interaction has various matrix features")

## Installation

*But first, a warning*. Forscape is in early alpha. Feedback and bug reports are welcome, but backward compatibility will be broken. The language syntax will be broken. The file encoding will be broken. Crashes are likely.

#### Windows

Download and run [ForscapeInstaller_Win64.exe](https://github.com/JohnDTill/Forscape/releases/download/pre-alpha-0.0.2/ForscapeInstaller_Win64.exe).
Alternately, install the development prerequisites listed below and compile from source.

#### Linux
Use the [.deb file](https://github.com/JohnDTill/Forscape/releases/download/pre-alpha-0.0.2/forscape_0.0.1_amd64.deb) or compile from source using:
```
apt-get install cmake python3 qt5-default libqt5svg5-dev
pip3 install conan
git clone https://github.com/JohnDTill/Forscape
cd ./Forscape/app
conan install --generator cmake_find_package .
cmake CMakeLists.txt
make
./Forscape #Run
```

## Development Prerequisites

* Qt5/6 (https://www.qt.io/download)
  * QtSvg module
* Python3 on the system path
* Conan (after installing, pick up the Qt plugin for easy use)

#### Optional
* [GraphViz](https://graphviz.org/) on system path for debugging AST

#### External libraries / assets (no setup required)

* [Eigen](http://eigen.tuxfamily.org/index.php?title=Main_Page) (Header-only linear algebra)
* [parallel-hashmap](https://github.com/greg7mdp/parallel-hashmap) (Header-only, very fast and memory-friendly hashmaps and btrees)
* [readerwriterqueue](https://github.com/cameron314/readerwriterqueue) (Lock-free queue for single-producer/single-consumer messages)
* [spdlog](https://github.com/gabime/spdlog) (Very fast, header-only/compiled, C++ logging library.)
* Fonts with extensive unicode support:
  * [Computer Modern](https://www.fontsquirrel.com/fonts/computer-modern)
  * [JuliaMono](https://github.com/cormullion/juliamono)
  * [Quivira](http://quivira-font.com/)

## License

This project aims to be financially maintainable while ultimately resulting in an entirely open source codebase. The core is licensed under the MIT License - see the [LICENSE](LICENSE) file for details. Additional modules will be sold commercially for a fixed period starting at release, after which they will be added to the open source core.

## Contributing

Creating solid design foundations is a current challenge, but this work is interesting and anyone who is willing to tolerate a bit of ambiguity between prototype and production code may enjoy contributing. Forscape development is about learning, having fun, and embracing the Neumann architecture.

Progress and feature ideas are tracked in a [Jira board](https://forscape.atlassian.net/jira/software/c/projects/FOR/boards/1) and backlog. Tests are run automatically by GitHub runners and required to pass before merging to main. There is no official communication outlet yet -- feel free to reach out to me directly with questions or interest.

There is no documentation yet, either user or design docs. [Crafting Interpreters](http://www.craftinginterpreters.com/) is an excellent programming-languages primer and instant classic for anyone wanting to experiment with languages.

[![C++ Integration Tests (ubuntu-latest gcc)](https://github.com/JohnDTill/Forscape/actions/workflows/cpp_integration_tests.yml/badge.svg)](https://github.com/JohnDTill/Forscape/actions/workflows/cpp_integration_tests.yml)

[![C++ Integration Tests (ubuntu-latest gcc 32-bit)](https://github.com/JohnDTill/Forscape/actions/workflows/cpp_integration_tests_32bit.yml/badge.svg)](https://github.com/JohnDTill/Forscape/actions/workflows/cpp_integration_tests_32bit.yml)

[![C++ Integration Tests (ubuntu-latest clang)](https://github.com/JohnDTill/Forscape/actions/workflows/cpp_integration_tests_clang.yml/badge.svg)](https://github.com/JohnDTill/Forscape/actions/workflows/cpp_integration_tests_clang.yml)

[![C++ Integration Tests (windows-latest msbuild)](https://github.com/JohnDTill/Forscape/actions/workflows/cpp_integration_tests_win.yml/badge.svg)](https://github.com/JohnDTill/Forscape/actions/workflows/cpp_integration_tests_win.yml)

[![C++ Integration Tests (macos-latest)](https://github.com/JohnDTill/Forscape/actions/workflows/cpp_integration_tests_mac.yml/badge.svg)](https://github.com/JohnDTill/Forscape/actions/workflows/cpp_integration_tests_mac.yml)

[![Qt Integration Tests (ubuntu-latest gcc)](https://github.com/JohnDTill/Forscape/actions/workflows/qt_integration_tests.yml/badge.svg)](https://github.com/JohnDTill/Forscape/actions/workflows/qt_integration_tests.yml)


[![Qt Integration Tests (macos-latest)](https://github.com/JohnDTill/Forscape/actions/workflows/qt_integration_tests_mac.yml/badge.svg)](https://github.com/JohnDTill/Forscape/actions/workflows/qt_integration_tests_mac.yml)

[![Forscape App Compiles (ubuntu-latest gcc)](https://github.com/JohnDTill/Forscape/actions/workflows/app_compiles.yml/badge.svg)](https://github.com/JohnDTill/Forscape/actions/workflows/app_compiles.yml)
