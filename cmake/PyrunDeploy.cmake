# Make a built exe runnable straight from the build tree (Explorer
# double-click, Visual Studio F5, plain cmd) with no Qt on PATH.
#
# POST_BUILD runs windeployqt against the target file, in debug or release
# mode to match the configuration, so Qt6*d.dll and platforms/qwindows(d).dll
# land beside the exe. It re-runs only when the exe is relinked. Under
# PYRUN_ASAN the MSVC AddressSanitizer runtime DLL is copied too (it is
# otherwise found only through the vcvars PATH).
#
# Not the install()-time deploy script: that serves `cmake --install`; this
# serves the build tree. Both are kept.
#
# Linux needs no build-tree step: CMake bakes a build RPATH to the Qt it
# found, so the exes run from the build tree as they are.
#
# Linux install tree: bin/pyrun and bin/pyrun-gui, nothing bundled. Qt's
# qt_generate_deploy_app_script was tried against a distro Qt 6.10: it copies
# ~200 transitive libraries (GTK, glycin, ...) and plugins into lib/ and then
# fails because patchelf is not installed. It is built for an official Qt
# (qt.io / aqt) layout, not for distro packages, so it is not wired up. Instead
# the installed exes carry an RPATH to the Qt they were linked against when that
# Qt is outside the system library path (below); with a distro Qt the system
# loader finds the libraries and the target needs the matching runtime
# packages (see README).
if(UNIX)
  set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)
endif()
if(WIN32 AND NOT TARGET Qt6::windeployqt)
  message(WARNING "Qt6::windeployqt not found; build-tree exes will need Qt on PATH")
endif()

function(pyrun_deploy_qt target)
  if(NOT WIN32 OR NOT TARGET Qt6::windeployqt)
    return()
  endif()
  add_custom_command(TARGET ${target} POST_BUILD
    COMMAND Qt6::windeployqt
            $<IF:$<CONFIG:Debug>,--debug,--release>
            --no-translations --no-system-d3d-compiler --no-system-dxc-compiler
            --no-opengl-sw --no-compiler-runtime
            --skip-plugin-types generic,iconengines,imageformats,networkinformation,tls
            --dir "$<TARGET_FILE_DIR:${target}>"
            "$<TARGET_FILE:${target}>"
    COMMENT "windeployqt ${target}"
    VERBATIM)

  if(PYRUN_ASAN AND MSVC)
    # The runtime DLLs sit next to cl.exe in both the VS 2022 and VS 2026
    # toolsets (same file names, x64); the directory comes from whichever cl
    # this build configured with, so the copy follows the toolset in use.
    get_filename_component(_cl_dir "${CMAKE_CXX_COMPILER}" DIRECTORY)
    set(_asan_dlls "")
    foreach(_name clang_rt.asan_dynamic-x86_64.dll clang_rt.asan_dbg_dynamic-x86_64.dll)
      if(EXISTS "${_cl_dir}/${_name}")
        list(APPEND _asan_dlls "${_cl_dir}/${_name}")
      else()
        message(WARNING "ASan runtime ${_name} not found next to cl.exe (${_cl_dir})")
      endif()
    endforeach()
    if(_asan_dlls)
      add_custom_command(TARGET ${target} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${_asan_dlls} "$<TARGET_FILE_DIR:${target}>"
        COMMENT "copy ASan runtime for ${target}"
        VERBATIM)
    endif()
  endif()
endfunction()
