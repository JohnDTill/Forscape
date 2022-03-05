# Forscape
## A language and editor for scientific computation

![alt text](limerick.png?raw=true "Forscape")

### Focus on the problem, not the implementation details

**Forscape** solves engineering problems with an unprecedented level of abstraction so you get reliable results quickly. This high-level approach starts with intuitive syntax. Program with the same notation you use to write equations thanks to our innovative math rendering with semantic formatting. Matrices, fractions, symbols– write code using the same notation you use to think through problems on the whiteboard.

![alt text](root_finding.png?raw=true "Forscape")

## Installation

*But first, a warning*. Forscape is in early alpha. Feedback and bug reports are welcome, but backward compatibility will be broken. The language syntax will be broken. The file encoding will be broken. Crashes are likely.

#### Windows

DO THIS: make an installer

Alternately, install the development prerequisites listed below and compile from source.

#### Linux
Compile from source using:
```
apt-get install cmake python3 qt5-default libqt5svg5-dev
git clone https://github.com/JohnDTill/Forscape
cd ./Forscape/example/exampleWidgetQt
cmake CMakeLists.txt
make
./Forscape #Run
```

## Development Prerequisites

* Qt5/6 (https://www.qt.io/download)
  * QtSvg module
* Python3 on the system path

#### Optional
* [GraphViz](https://graphviz.org/) on system path for debugging AST

#### Bundled software

* [Eigen](http://eigen.tuxfamily.org/index.php?title=Main_Page) (Header-only linear algebra)
* [readerwriterqueue](https://github.com/cameron314/readerwriterqueue) (Lock-free queue for single-producer/single-consumer messages)
* Fonts with extensive unicode support:
  * [Computer Modern](https://www.fontsquirrel.com/fonts/computer-modern)
  * [Quivira](http://quivira-font.com/)

## License

This project aims to be financially maintainable while ultimately resulting in an entirely open source codebase. The core is licensed under the MIT License - see the [LICENSE](LICENSE) file for details. Additional modules will be sold commercially for a six-year period starting at release, after which they will be added to the open source core.

## Contributing

Feel free to report any bugs in the issue tracker and message me about feature requests or general design discussion. Tests are run automatically and required to pass before merging to main.

[![C++ Integration Tests](https://github.com/JohnDTill/Forscape/actions/workflows/cpp_integration_tests.yml/badge.svg?event=push)](https://github.com/JohnDTill/Forscape/actions/workflows/cpp_integration_tests.yml)

[![Qt Integration Tests](https://github.com/JohnDTill/Forscape/actions/workflows/qt_integration_tests.yml/badge.svg?event=push)](https://github.com/JohnDTill/Forscape/actions/workflows/qt_integration_tests.yml)
