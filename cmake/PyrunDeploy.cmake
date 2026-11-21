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
if(NOT TARGET Qt6::windeployqt)
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
    get_filename_component(_cl_dir "${CMAKE_CXX_COMPILER}" DIRECTORY)
    set(_asan_dll "${_cl_dir}/clang_rt.asan_dynamic-x86_64.dll")
    set(_asan_dbg "${_cl_dir}/clang_rt.asan_dbg_dynamic-x86_64.dll")
    if(EXISTS "${_asan_dll}")
      add_custom_command(TARGET ${target} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${_asan_dll}" "${_asan_dbg}" "$<TARGET_FILE_DIR:${target}>"
        COMMENT "copy ASan runtime for ${target}"
        VERBATIM)
    else()
      message(WARNING "ASan runtime DLL not found next to cl.exe (${_cl_dir})")
    endif()
  endif()
endfunction()
