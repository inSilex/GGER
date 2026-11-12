# Extend the path in `base_path` with the path in `current_segment`, returning
# the result in `joined_path`. If `current_segment` is an absolute path then
# just return it, in effect overriding `base_path`, and issue a warning.
#
# Note that the code returns a relative path (avoiding introducing leading
# slashes) if `base_path` is empty.
#
# Vendored from LLVM 20.1.8 cmake/Modules/ExtendPath.cmake.  The Ubuntu
# llvm-20-dev package does not install this module, but a standalone
# compiler-rt configure includes it.
function(extend_path joined_path base_path current_segment)
  if("${current_segment}" STREQUAL "")
    set(temp_path "${base_path}")
  elseif("${base_path}" STREQUAL "")
    set(temp_path "${current_segment}")
  elseif(IS_ABSOLUTE "${current_segment}")
    message(WARNING "Since \"${current_segment}\" is absolute, it overrides base path: \"${base_path}\".")
    set(temp_path "${current_segment}")
  else()
    set(temp_path "${base_path}/${current_segment}")
  endif()
  set(${joined_path} "${temp_path}" PARENT_SCOPE)
endfunction()
