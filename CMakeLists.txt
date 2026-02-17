cmake_minimum_required(VERSION 4.1)
project(EChat LANGUAGES C)

# LIBRARY FOR MENU INTERFACE 
add_library(Menu
    "src/menu.c"
    "include/menu.h"
)
target_include_directories(Menu
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# LIBRARY FOR TERMINAL INTERFACE CONFIGURATION
add_library(Terminal
    "src/terminal.c"
    "include/terminal.h"
)
target_include_directories(Terminal
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# SERVER
add_executable(server
    "src/server.c"
)

target_include_directories(server
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(server PRIVATE EChatCore)

# CLIENT
add_executable(client
    "src/CLI_client.c"
)

target_include_directories(client
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/OpenSSL/include
)

target_link_libraries(client PRIVATE Menu)
target_link_libraries(client PRIVATE Terminal)

# OpenSSL
target_link_directories(server PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/OpenSSL/lib/VC/x64/MD)
target_link_directories(client PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/OpenSSL/lib/VC/x64/MD)

if(WIN32)
target_link_libraries(server PRIVATE ws2_32 libssl.lib libcrypto.lib)
target_link_libraries(client PRIVATE ws2_32 libcrypto.lib)
endif()


# TESTS
function(add_echat_test name)
    add_executable(${name} ${ARGN})

    target_include_directories(${name}
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/include
            ${CMAKE_CURRENT_SOURCE_DIR}/OpenSSL/include
    )

    target_link_directories(${name}
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/OpenSSL/lib/VC/x64/MD
    )

    target_link_libraries(${name} PRIVATE Menu)
    target_link_libraries(${name} PRIVATE Terminal)

    if (WIN32)
    target_link_libraries(${name}
        PRIVATE
            ws2_32
            libcrypto.lib
            libssl.lib
    )
    endif()
endfunction()

option(ENABLE_TESTS "Build test executables" ON)

if(ENABLE_TESTS)
    add_echat_test(test_server
        tests/test_server.c
    )

    add_echat_test(test_client
        tests/test_client.c
    )

    add_echat_test(test_menu
        tests/test_menu.c
    )
endif() # ENABLE_TESTS