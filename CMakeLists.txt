cmake_minimum_required(VERSION 3.25)
project(rammer C)

set(CMAKE_C_STANDARD 23)

add_executable(rammer main.c)

# Install
string(ASCII 27 Esc)
set(Orange     "${Esc}[0;33m")
set(ColorReset "${Esc}[m")

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/rammer DESTINATION bin)
install(CODE "execute_process(COMMAND chown root:root ${CMAKE_INSTALL_PREFIX}/bin/rammer)")
install(CODE "execute_process(COMMAND chmod 4745 ${CMAKE_INSTALL_PREFIX}/bin/rammer)")
install(CODE "message(\"-- Installer: ${Orange}Rammer binary is ${CMAKE_INSTALL_PREFIX}/bin/rammer${ColorReset}\")")