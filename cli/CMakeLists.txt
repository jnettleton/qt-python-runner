# Console app: Qt6::Core only (via runner_core). No Widgets/Gui link, by design.
qt_add_executable(pyrun
  main.cpp
  ConsoleEncodingGuard.h
  ConsoleEncodingGuard.cpp
  ConsoleColor.h
  ConsoleColor.cpp
  views/ConsoleView.h
  views/ConsoleView.cpp
)
target_link_libraries(pyrun PRIVATE runner_core)
target_include_directories(pyrun PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
set_target_properties(pyrun PROPERTIES WIN32_EXECUTABLE FALSE)
pyrun_set_warnings(pyrun)
pyrun_add_version_info(pyrun "pyrun - run Python scripts from the console")
set_target_properties(pyrun PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")
pyrun_deploy_qt(pyrun)

# CLI smoke tests that need neither Python nor a GUI.
add_test(NAME cli_version COMMAND pyrun --version)
set_tests_properties(cli_version PROPERTIES
  PASS_REGULAR_EXPRESSION "^pyrun ${PROJECT_VERSION}")
pyrun_set_test_environment(cli_version)

add_test(NAME cli_no_args_is_usage_error COMMAND pyrun)
set_tests_properties(cli_no_args_is_usage_error PROPERTIES
  PASS_REGULAR_EXPRESSION "no scripts given")
pyrun_set_test_environment(cli_no_args_is_usage_error)

add_test(NAME cli_missing_script_fails COMMAND pyrun "${CMAKE_CURRENT_SOURCE_DIR}/does-not-exist.py")
set_tests_properties(cli_missing_script_fails PROPERTIES
  # Either no interpreter (exit 2) or a missing script (exit 1): never success.
  WILL_FAIL TRUE)
pyrun_set_test_environment(cli_missing_script_fails)

install(TARGETS pyrun RUNTIME DESTINATION ${PYRUN_INSTALL_BINDIR})
if(WIN32)  # windeployqt; see PyrunDeploy.cmake for why Linux has no deploy script
  qt_generate_deploy_app_script(TARGET pyrun
    OUTPUT_SCRIPT pyrun_deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR)
  install(SCRIPT ${pyrun_deploy_script})
endif()
