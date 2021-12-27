# Forscape
## A language and editor for scientific computation

![alt text](limerick.png?raw=true "Forscape")

### Focus on the problem, not the implementation details

**Forscape** solves engineering problems with an unprecedented level of abstraction so you get reliable results quickly. This high-level approach starts with intuitive syntax. Program with the same notation you use to write equations thanks to our innovative math rendering with semantic formatting. Matrices, fractions, symbols– write code using the same notation you use to think through problems on the whiteboard.

![alt text](root_finding.png?raw=true "Forscape")

## Prerequisites

* Qt (https://www.qt.io/download)

## Bundled software

* [Eigen](http://eigen.tuxfamily.org/index.php?title=Main_Page) (Header-only linear algebra)
* [readerwriterqueue](https://github.com/cameron314/readerwriterqueue) (Lock-free queue for single-producer/single-consumer messages)

## License

This project aims to be financially maintainable while ultimately resulting in an entirely open source codebase. The core is licensed under the MIT License - see the [LICENSE](LICENSE) file for details. Additional modules will be sold commercially for a six-year period starting at release, after which they will be added to the open source core.

## Contributing

Feel free to report any bugs in the issue tracker and message me about feature requests or general design discussion. Pure C++ tests are run automatically and required to pass before merging to main, but please run Qt tests if GUI code is modified (eventually this will be automated too).

[![C++ Integration Tests](https://github.com/JohnDTill/Forscape/actions/workflows/run_tests.yml/badge.svg?event=push)](https://github.com/JohnDTill/Forscape/actions/workflows/run_tests.yml)
