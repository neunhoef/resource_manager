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
  
  # Link with jemalloc if available
  if(JEMALLOC_FOUND)
    target_include_directories(${target} PRIVATE ${JEMALLOC_INCLUDE_DIRS})
    target_link_libraries(${target} PRIVATE ${JEMALLOC_LIBRARIES})
    target_compile_definitions(${target} PRIVATE USE_JEMALLOC=1)
  endif()
  
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