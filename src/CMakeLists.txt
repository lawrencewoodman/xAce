add_definitions(-DSCALE=2 -DWHITE_ON_BLACK -DXACE_VERSION=\"0.4.1\")
add_executable(xace xmain.c z80.c tape.c keyboard.c spooler.c)
target_link_libraries(xace X11 Xext)
install(TARGETS xace DESTINATION bin)
