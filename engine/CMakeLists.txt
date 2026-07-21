# ===========================================================================
# engine/CMakeLists.txt — Virtual Audio Router Engine Library
# ===========================================================================

# Static library: var_engine_lib
# Linked by: bindings (var_engine Python extension) and tests

add_library(var_engine_lib STATIC
    # Implementations
    src/Logger.cpp
    src/EventDispatcher.cpp
    src/ThreadPool.cpp
    src/DeviceManager.cpp
    src/AudioEngine.cpp
    src/OutputDevice.cpp
    src/AudioRouter.cpp
    src/BufferManager.cpp
    src/Resampler.cpp
    src/LatencyManager.cpp
    src/SynchronizationManager.cpp
)

target_include_directories(var_engine_lib
    PUBLIC
        # Engine's own headers
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
        # Shared core types (Result.h, Types.h, Constants.h)
        "${CMAKE_SOURCE_DIR}/core/include"
        # Generated version header
        "${CMAKE_BINARY_DIR}/include"
)

# Windows-specific system libraries
if(WIN32)
    target_link_libraries(var_engine_lib
        PUBLIC
            ${VAR_WINDOWS_LIBS}
            propsys     # IPropertyStore, PropVariantInit/Clear
            ksuser      # KSDATAFORMAT_SUBTYPE_IEEE_FLOAT GUIDs
    )
endif()

# Compile features (redundant with global setting but explicit per-target is best practice)
target_compile_features(var_engine_lib PUBLIC cxx_std_20)

# Preprocessor definitions
target_compile_definitions(var_engine_lib
    PRIVATE
        VAR_ENGINE_VERSION_STRING="${PROJECT_VERSION}"
)
