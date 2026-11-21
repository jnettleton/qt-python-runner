# Adds a generated .rc with a VERSIONINFO resource to an executable target.
# Every value comes from the CMake project VERSION and the PYRUN_* names in the
# top-level CMakeLists, so the version still has exactly one source.
# No-op off Windows. No icon is embedded (by decision).
function(pyrun_add_version_info target description)
  if(NOT WIN32)
    return()
  endif()
  set(PYRUN_RC_DESCRIPTION "${description}")
  # Configure time: the executable is named after the target.
  set(PYRUN_RC_FILENAME "${target}.exe")
  set(rc "${CMAKE_CURRENT_BINARY_DIR}/${target}_version.rc")
  configure_file("${PROJECT_SOURCE_DIR}/cmake/version.rc.in" "${rc}" @ONLY)
  target_sources(${target} PRIVATE "${rc}")
endfunction()
