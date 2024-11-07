# Compile
apt-get install cmake python3 qtbase5-dev libqt5svg5-dev
pip3 install -v "conan==1.65.0"
git clone https://github.com/JohnDTill/Forscape
cd ./Forscape
conan install --generator cmake_find_package --install-folder ./build/conan-dependencies ./app
cmake -DCMAKE_BUILD_TYPE=Release -B build -S app
cd build
make

# Run
# ./Forscape

# Test launch with timed close
timeout 10 ./Forscape -platform offscreen
if [ $? == 124 ]; then
    exit 0
fi