add_library(kernelcad_compiler_options INTERFACE)

if(MSVC)
  target_compile_options(kernelcad_compiler_options INTERFACE
    /W4
    /wd4068  # unknown pragma
    /wd4100  # unreferenced formal parameter (OCCT headers)
    /wd4127  # conditional expression is constant
  )
else()
  target_compile_options(kernelcad_compiler_options INTERFACE
    -Wall -Wextra -Wpedantic
    -Wno-unused-parameter   # OCCT headers trigger this
    -Wno-deprecated-copy    # OCCT internal
  )
endif()
