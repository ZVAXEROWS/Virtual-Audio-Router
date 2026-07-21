# ===========================================================================
# bindings/CMakeLists.txt — pybind11 Python Extension Module
# ===========================================================================
#
# Builds: var_engine.pyd  (Windows Python extension)
# Output goes to: gui/  so that `import var_engine` works without PYTHONPATH tricks.
#
# The extension module links against var_engine_lib (static).
# No audio data crosses the Python/C++ boundary — only DeviceInfo dicts,
# status strings, and log entries.

pybind11_add_module(var_engine
    src/var_bindings.cpp
)

target_link_libraries(var_engine
    PRIVATE
        var_engine_lib
)

target_include_directories(var_engine
    PRIVATE
        "${CMAKE_SOURCE_DIR}/engine/include"
        "${CMAKE_SOURCE_DIR}/core/include"
        "${CMAKE_BINARY_DIR}/include"
)

# Place the .pyd directly into the gui/ directory so Python finds it
set_target_properties(var_engine PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/gui"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/gui"
)

# Also copy to gui/ for multi-config generators (MSVC Debug/Release)
foreach(CONFIG_TYPE DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
    set_target_properties(var_engine PROPERTIES
        LIBRARY_OUTPUT_DIRECTORY_${CONFIG_TYPE} "${CMAKE_SOURCE_DIR}/gui"
        RUNTIME_OUTPUT_DIRECTORY_${CONFIG_TYPE} "${CMAKE_SOURCE_DIR}/gui"
    )
endforeach()
