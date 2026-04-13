matlab_add_mex(
    NAME bindings
    SRC bindings.cpp
    LINK_TO ${Matlab_MEX_LIBRARY} ${Matlab_MX_LIBRARY}
)

target_link_libraries(bindings mexforge)
target_compile_features(bindings PRIVATE cxx_std_17)

# Platform-specific flags
if(MSVC)
    target_compile_options(bindings PRIVATE /bigobj)
endif()
