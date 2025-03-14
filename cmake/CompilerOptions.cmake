# Function to set compiler options for a target
function(set_compiler_options target)
  # Common compiler flags
  target_compile_options(${target} PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -Werror
    -Wno-unused-parameter
    -pthread
  )
  
  # Link with pthread
  target_link_libraries(${target} PRIVATE pthread)
  
  # Debug build specific options
  target_compile_options(${target} PRIVATE $<$<CONFIG:Debug>:
    -g
    -O0
    -fno-omit-frame-pointer
  >)
  
  # Release build specific options
  target_compile_options(${target} PRIVATE $<$<CONFIG:Release>:
    -O3
    -march=native
    -DNDEBUG
  >)
  
  # RelWithDebInfo build specific options
  target_compile_options(${target} PRIVATE $<$<CONFIG:RelWithDebInfo>:
    -O2
    -g
    -DNDEBUG
  >)
  
  # MinSizeRel build specific options
  target_compile_options(${target} PRIVATE $<$<CONFIG:MinSizeRel>:
    -Os
    -DNDEBUG
  >)
endfunction() 