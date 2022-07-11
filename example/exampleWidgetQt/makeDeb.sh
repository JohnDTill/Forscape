conan install --generator cmake_find_package --install-folder ../build/conan-dependencies .
cmake -S . -B ../build -D CMAKE_BUILD_TYPE=Release -DMAKE_DEB=YES
cd ../build
make
cpack -G DEB
cd ../exampleWidgetQt