::Usage: createInstaller.bat <qtVersionNumber>
set qtver=%1
rmdir /s /q build
mkdir build
call qtenv2.bat
cd %~dp0
conan install --generator cmake_find_package --install-folder ../../build/conan-dependencies ..
cmake.exe -S .. -B ../../build -D CMAKE_BUILD_TYPE=Release
cmake.exe --build ../../build --target ALL_BUILD --config Release
windeployqt.exe ../../build/Release/Forscape.exe
mkdir "packages\com.automath.forscape\data"
robocopy ..\..\build\Release packages\com.automath.forscape\data Forscape.exe *.dll /S
copy ..\lambda.ico config
copy "..\..\LICENSE" "packages\com.automath.forscape\meta"
binarycreator.exe --offline-only -c config/config.xml -p packages ForscapeInstaller_Win64.exe
