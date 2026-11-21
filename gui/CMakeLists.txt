# GUI: runner_core + Qt6::Widgets. The view code is a static lib so the smoke
# test can link it; main.cpp is the composition root.
qt_add_library(gui_view STATIC
  views/MainView.h
  views/MainView.cpp
  views/AboutView.h
  views/AboutView.cpp
  views/OutputFormats.h
)
target_link_libraries(gui_view PUBLIC runner_core Qt6::Widgets)
target_include_directories(gui_view PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
pyrun_set_warnings(gui_view)

# WIN32 is explicit: without it the GUI exe opens a console window.
qt_add_executable(pyrun-gui WIN32 main.cpp)
target_link_libraries(pyrun-gui PRIVATE gui_view)
pyrun_set_warnings(pyrun-gui)
pyrun_add_version_info(pyrun-gui "Python Runner - run a Python script and watch its output")
set_target_properties(pyrun-gui PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")
pyrun_deploy_qt(pyrun-gui)

install(TARGETS pyrun-gui RUNTIME DESTINATION .)
qt_generate_deploy_app_script(TARGET pyrun-gui
  OUTPUT_SCRIPT pyrun_gui_deploy_script
  NO_UNSUPPORTED_PLATFORM_ERROR)
install(SCRIPT ${pyrun_gui_deploy_script})
