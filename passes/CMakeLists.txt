add_subdirectory(Utils Utils)

# EDDI 
add_library(EDDI SHARED
    DuplicateGlobals.cpp
    EDDI.cpp
    FuncRetToRef.cpp
    Utils/Utils.cpp
)
target_compile_definitions(EDDI PRIVATE SELECTIVE_CHECKING=0 CHECK_AT_STORES CHECK_AT_CALLS CHECK_AT_BRANCH)
target_link_libraries(EDDI PUBLIC TypeDeductionAnalysis)

# FDSC
add_library(FDSC SHARED
    DuplicateGlobals.cpp
    EDDI.cpp
    FuncRetToRef.cpp
    Utils/Utils.cpp
)
target_compile_definitions(FDSC PRIVATE SELECTIVE_CHECKING=1 CHECK_AT_STORES CHECK_AT_CALLS CHECK_AT_BRANCH)
target_link_libraries(FDSC PUBLIC TypeDeductionAnalysis)

# sEDDI
add_library(SEDDI SHARED
    DuplicateGlobals.cpp
    EDDI.cpp
    FuncRetToRef.cpp
    Utils/Utils.cpp
)
target_compile_definitions(SEDDI PRIVATE SELECTIVE_CHECKING=0 CHECK_AT_CALLS CHECK_AT_BRANCH)
target_link_libraries(SEDDI PUBLIC TypeDeductionAnalysis)


# CFCSS
add_library(CFCSS SHARED
    CFCSS.cpp
    Utils/Utils.cpp
)
target_link_libraries(CFCSS PUBLIC TypeDeductionAnalysis)

# RASM
add_library(RASM SHARED
    RASM.cpp
    Utils/Utils.cpp
)
target_compile_definitions(RASM PRIVATE INTRA_FUNCTION_CFC=0)
target_link_libraries(RASM PUBLIC TypeDeductionAnalysis)


# inter-RASM
add_library(INTER_RASM SHARED
    RASM.cpp
    Utils/Utils.cpp
)
target_compile_definitions(INTER_RASM PRIVATE INTRA_FUNCTION_CFC=1)
target_link_libraries(INTER_RASM PUBLIC TypeDeductionAnalysis)

add_library(PROFILER SHARED
    Profiling/ASPISCheckProfiler.cpp
    Utils/Utils.cpp
)
target_link_libraries(PROFILER PUBLIC TypeDeductionAnalysis)
