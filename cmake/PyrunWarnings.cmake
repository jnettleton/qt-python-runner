# Per-target warning policy. Applied to our targets only, never globally.
option(PYRUN_WARNINGS_AS_ERRORS "Treat warnings as errors" ON)
option(PYRUN_ASAN "Build with sanitizers (MSVC: /fsanitize=address; GCC/Clang: address + undefined)" OFF)

function(pyrun_set_warnings target)
  if(MSVC)
    target_compile_options(${target} PRIVATE
      /W4 /permissive- /utf-8 /Zc:__cplusplus /external:W0)
    if(PYRUN_WARNINGS_AS_ERRORS)
      target_compile_options(${target} PRIVATE /WX)
    endif()
    if(PYRUN_ASAN)
      # Needs debug info and no incremental linking. Incompatible with /RTC1,
      # so the asan preset uses RelWithDebInfo.
      target_compile_options(${target} PRIVATE /fsanitize=address /Zi)
      target_link_options(${target} PRIVATE /INCREMENTAL:NO)
    endif()
  else()
    target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
    if(PYRUN_WARNINGS_AS_ERRORS)
      target_compile_options(${target} PRIVATE -Werror)
    endif()
    if(PYRUN_ASAN)
      # UBSan is available here (not on MSVC). A finding aborts the test.
      target_compile_options(${target} PRIVATE
        -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer)
      target_link_options(${target} PRIVATE -fsanitize=address,undefined)
    endif()
  endif()
endfunction()
