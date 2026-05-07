# CMake generated Testfile for 
# Source directory: /home/tz/Data/Code/C++/project
# Build directory: /home/tz/Data/Code/C++/project/build-linux
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test(sentinel.pipeline_smoke "/home/tz/Data/Code/C++/project/build-linux/sentinel_tests" "/home/tz/Data/Code/C++/project/config")
set_tests_properties(sentinel.pipeline_smoke PROPERTIES  _BACKTRACE_TRIPLES "/home/tz/Data/Code/C++/project/CMakeLists.txt;56;add_test;/home/tz/Data/Code/C++/project/CMakeLists.txt;0;")
add_test(sentinel.signal_fd "/home/tz/Data/Code/C++/project/build-linux/sentinel_signal_tests")
set_tests_properties(sentinel.signal_fd PROPERTIES  _BACKTRACE_TRIPLES "/home/tz/Data/Code/C++/project/CMakeLists.txt;70;add_test;/home/tz/Data/Code/C++/project/CMakeLists.txt;0;")
