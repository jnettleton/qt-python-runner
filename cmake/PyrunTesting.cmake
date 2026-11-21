# Environment every CTest test runs with.
#  - QT_QPA_PLATFORM=offscreen: widget tests need no display (Windows, X11 or
#    a headless CI box alike).
#  - Windows: Qt's DLL directory is prepended to PATH for that test process only
#    (nothing is installed or edited system-wide). Not done elsewhere: CMake
#    bakes the Qt library directory into the build RPATH, and a lib dir at the
#    front of PATH would only skew interpreter discovery.
#  - Sanitizer builds on POSIX: abort on the first finding, with stack traces.
function(pyrun_set_test_environment test)
  set(_env "QT_QPA_PLATFORM=offscreen")
  if(PYRUN_ASAN AND NOT MSVC)
    list(APPEND _env
      "ASAN_OPTIONS=abort_on_error=0:detect_leaks=1:strict_string_checks=1"
      "UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1")
  endif()
  set_tests_properties(${test} PROPERTIES ENVIRONMENT "${_env}")
  if(WIN32)
    set_property(TEST ${test} APPEND PROPERTY ENVIRONMENT_MODIFICATION
      "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>")
  endif()
endfunction()
