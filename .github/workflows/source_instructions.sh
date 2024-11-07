# Compile
apt-get install cmake python3 qt5-default qtbase5-dev, qtchooser, qt5-qmake, qtbase5-dev-tools, libqt5svg5-dev
pip3 install conan
git clone https://github.com/JohnDTill/Forscape
cd ./Forscape
conan install --generator cmake_find_package --install-folder build/conan-dependencies app
cmake -DCMAKE_BUILD_TYPE=Release -B build -S app
cd build
make

# Run
#./Forscape