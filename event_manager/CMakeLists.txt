project (optee_event_manager C)

add_executable (
	${PROJECT_NAME}
	host/command_handlers.c
	host/connection.c
	host/enclave_utils.c
	host/event_manager.c
	host/main.c
	host/module_api.c
	host/module_list.c
	host/networking.c
    host/utils.c
)


target_include_directories(
	${PROJECT_NAME}
	PRIVATE host
	PRIVATE include
)

target_link_libraries (${PROJECT_NAME} PRIVATE teec)

install (TARGETS ${PROJECT_NAME} DESTINATION ${CMAKE_INSTALL_BINDIR})


