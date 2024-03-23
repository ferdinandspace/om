cmake_minimum_required(VERSION 3.1)

project(om)

set(SRC_DIR src)
set(OM_SRC 
    #    ${SRC_DIR}/main.c
    ${SRC_DIR}/om_server.c
    ${SRC_DIR}/om_connection.c
)

add_library(om SHARED ${OM_SRC})

#add_executable(main ${OM_SRC})

