link_directories(${CMAKE_BINARY_DIR}/lib)

add_executable(Test_Validator Test_Validator.cpp)
target_include_directories(Test_Validator PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(Test_Validator PUBLIC Validator AccountData Crypto Trie Utils Mediator)
add_test(NAME Test_Validator COMMAND Test_Validator)
