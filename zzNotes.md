To build OTel Cpp library [Mac]
==============================
clone git@github.com:lukeina2z/opentelemetry-cpp.git
use branch 'lk01'

brew install grpc
brew --prefix grpc    =>   /opt/homebrew/opt/grpc
brew install protobuf
/opt/homebrew/opt/protobuf    =>     /opt/homebrew/opt/protobuf

cmake -B build -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=OFF \
    -DCMAKE_CXX_STANDARD=17 -DWITH_OTLP_GRPC=ON  -DWITH_OTLP_HTTP=ON
    
    -DCMAKE_PREFIX_PATH="/opt/homebrew/opt/grpc;/opt/homebrew/opt/protobuf"

cd build
sudo cmake --build .

cmake --install . --prefix ../../otel-cpp-pkg --config Debug


Build Otel on Windows
====================
Install dependencies:
git clone https://github.com/microsoft/vcpkg.git
cd vcpkg
.\bootstrap-vcpkg.bat
.\vcpkg install grpc protobuf curl nlohmann-json --triplet=x64-windows

cmake -B build -G "Visual Studio 17 2022" ^
    -A x64 -DCMAKE_BUILD_TYPE=Debug ^
    -DBUILD_TESTING=OFF ^
    -DWITH_OTLP_GRPC=ON  -DWITH_OTLP_HTTP=ON ^
    -DCMAKE_TOOLCHAIN_FILE="D:/vcpkg-git/scripts/buildsystems/vcpkg.cmake"

Run this with admin permission
cmake --build . --config Debug

cmake --install . --prefix ../../otel-cpp-pkg --config Debug