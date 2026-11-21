# Toolchain sanity, run once at configure time.
#
# MSVC: the Qt kit is msvc2022_64 (built with VS 2022, toolset v143). MSVC
# keeps the C++ ABI binary compatible across v140..v145, so the same kit links
# with the VS 2026 toolset (v145, cl 14.5x) and with 2022. The one documented
# exception is whole-program optimisation: /GL objects and /LTCG libraries are
# NOT compatible across toolset versions, so they are refused outright.
if(MSVC)
  message(STATUS "MSVC ${CMAKE_CXX_COMPILER_VERSION} (_MSC_VER ${MSVC_VERSION}); "
                 "Qt kit: ${Qt6_DIR}")

  set(_pyrun_bad "")
  foreach(_cfg "" _DEBUG _RELEASE _RELWITHDEBINFO _MINSIZEREL)
    foreach(_var CMAKE_CXX_FLAGS${_cfg} CMAKE_EXE_LINKER_FLAGS${_cfg}
                 CMAKE_STATIC_LINKER_FLAGS${_cfg} CMAKE_SHARED_LINKER_FLAGS${_cfg})
      if("${${_var}}" MATCHES "(^| )[-/](GL|LTCG)( |:|$)")
        string(APPEND _pyrun_bad " ${_var}='${${_var}}'")
      endif()
    endforeach()
  endforeach()
  if(CMAKE_INTERPROCEDURAL_OPTIMIZATION)
    string(APPEND _pyrun_bad " CMAKE_INTERPROCEDURAL_OPTIMIZATION=ON")
  endif()
  if(_pyrun_bad)
    message(FATAL_ERROR
      "Whole-program optimisation (/GL, /LTCG, IPO) is not allowed: it is not "
      "binary compatible across MSVC toolset versions (VS 2022 vs 2026) and "
      "breaks linking against the msvc2022_64 Qt kit. Offending:${_pyrun_bad}")
  endif()
endif()
