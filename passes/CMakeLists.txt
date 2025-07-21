add_subdirectory(Utils Utils)

# DataProtection
add_library(DataProtection SHARED
    EDDI.cpp
    Utils/Utils.cpp
)

# CFCSS
add_library(CFCSS SHARED
    CFCSS.cpp
    Utils/Utils.cpp
)

# RASM
add_library(RASM SHARED
    RASM.cpp
    Utils/Utils.cpp
)
target_compile_definitions(RASM PRIVATE INTRA_FUNCTION_CFC=0)


# inter-RASM
add_library(INTER_RASM SHARED
    RASM.cpp
    Utils/Utils.cpp
)
target_compile_definitions(INTER_RASM PRIVATE INTRA_FUNCTION_CFC=1)

add_library(PROFILER SHARED
    Profiling/ASPISCheckProfiler.cpp
    Utils/Utils.cpp
)