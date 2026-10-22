add_executable(dlsim-helper main.cpp HelperServer.h HelperServer.cpp)
target_link_libraries(dlsim-helper PRIVATE dlsim_core Qt6::Core)

configure_file(org.dlsim.helper.policy.in ${CMAKE_CURRENT_BINARY_DIR}/org.dlsim.helper.policy @ONLY)

get_filename_component(_helper_dir "${DLSIM_HELPER_INSTALL_PATH}" DIRECTORY)
install(TARGETS dlsim-helper RUNTIME DESTINATION "${_helper_dir}")
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/org.dlsim.helper.policy DESTINATION "${DLSIM_POLKIT_ACTIONS_DIR}")
