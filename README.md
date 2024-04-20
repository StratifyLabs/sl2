# sl


`sl` is a command line tool to be used with StratifyOS.

# Bootstrapping sl

For building on a new OS and architecture, try these commands:

```
git clone git@github.com:StratifyLabs/sl
cd sl
cmake -P bootstrap.cmake
source profile.sh
mkdir -p cmake_link
cd cmake_link
cmake .. -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -GNinja -DCMSDK_LOCAL_PATH=$PWD/../SDK/local
ninja
ninja install
```
