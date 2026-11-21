# runner_core: UI-agnostic. Links Qt6::Core ONLY. If a Widgets/Gui header is
# needed here, the design is wrong; the link line is what enforces it.
configure_file(version.h.in "${CMAKE_CURRENT_BINARY_DIR}/generated/pyrun/version.h" @ONLY)

qt_add_library(runner_core STATIC
  models/OutputChannel.h
  models/RunResult.h
  models/StatusSeverity.h
  models/RunResult.cpp
  services/IProcessRunner.h
  views/IRunView.h
  services/OutputDecoder.h
  services/OutputDecoder.cpp
  models/FileList.h
  models/FileList.cpp
  services/PythonLocator.h
  services/PythonLocator.cpp
  services/ProcessRunner.h
  services/ProcessRunner.cpp
  presenters/RunPresenter.h
  presenters/RunPresenter.cpp
)

target_include_directories(runner_core PUBLIC
  "${CMAKE_CURRENT_SOURCE_DIR}"
  "${CMAKE_CURRENT_BINARY_DIR}/generated"
)
target_link_libraries(runner_core PUBLIC Qt6::Core)
target_compile_features(runner_core PUBLIC cxx_std_20)
pyrun_set_warnings(runner_core)
