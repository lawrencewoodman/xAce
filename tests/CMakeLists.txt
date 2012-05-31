include_directories(${xAce_SOURCE_DIR}/src)
add_executable(tape_test tape_test.c ${xAce_SOURCE_DIR}/src/tape.c)
target_link_libraries(tape_test)
add_test(tape_test tape_test)
