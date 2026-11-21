set(PYRUN_FIXTURE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/fixtures")

# One Qt Test executable per file, registered with CTest. Environment per test
# comes from pyrun_set_test_environment (cmake/PyrunTesting.cmake).
function(pyrun_add_test name)
  qt_add_executable(${name} ${ARGN})
  target_link_libraries(${name} PRIVATE runner_core Qt6::Test)
  target_compile_definitions(${name} PRIVATE
    PYRUN_FIXTURE_DIR="${PYRUN_FIXTURE_DIR}"
    PYRUN_PROJECT_VERSION="${PROJECT_VERSION}")
  set_target_properties(${name} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")
  pyrun_set_warnings(${name})
  add_test(NAME ${name} COMMAND ${name})
  pyrun_set_test_environment(${name})
endfunction()

pyrun_add_test(tst_smoke tst_smoke.cpp)
pyrun_add_test(tst_outputdecoder tst_outputdecoder.cpp)
pyrun_add_test(tst_filelist tst_filelist.cpp)
pyrun_add_test(tst_pythonlocator tst_pythonlocator.cpp)
pyrun_add_test(tst_runpresenter tst_runpresenter.cpp Fakes.h)
pyrun_add_test(tst_processrunner tst_processrunner.cpp)

# Widget smoke test: the only test that needs Widgets (runs offscreen).
pyrun_add_test(tst_mainview tst_mainview.cpp)
target_link_libraries(tst_mainview PRIVATE gui_view)

# CLI view and colour policy. The CLI exe's sources are compiled in directly
# (Qt Core only): the test needs the view, not the exe.
pyrun_add_test(tst_consoleview tst_consoleview.cpp
  ../cli/views/ConsoleView.cpp
  ../cli/ConsoleColor.cpp)
target_include_directories(tst_consoleview PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../cli")
