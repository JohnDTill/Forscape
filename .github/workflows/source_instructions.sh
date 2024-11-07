# Compile
apt-get install cmake python3 qtbase5-dev libqt5svg5-dev
pip3 install -v "conan==1.65.0"
git clone https://github.com/JohnDTill/Forscape
cd ./Forscape
conan install --generator cmake_find_package --install-folder ./build/conan-dependencies ./app
cmake -B build -S app --config Release
cd build
make

# Run
#./Forscape