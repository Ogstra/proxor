# Release
if (DEFINED APP_VERSION_OVERRIDE AND NOT "${APP_VERSION_OVERRIDE}" STREQUAL "")
    set(NKR_VERSION "${APP_VERSION_OVERRIDE}")
else()
    file(STRINGS VERSION.txt NKR_VERSION)
endif()
add_compile_definitions(NKR_VERSION=\"${NKR_VERSION}\")

# Debug
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -DNKR_CPP_DEBUG")

# Func
function(nkr_add_compile_definitions arg)
    message("[add_compile_definitions] ${ARGV}")
    add_compile_definitions(${ARGV})
endfunction()
