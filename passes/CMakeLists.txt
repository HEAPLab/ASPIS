add_subdirectory(Utils Utils)

# EDDI
add_library(EDDI SHARED
				DuplicateGlobals.cpp
				EDDI.cpp
				FuncRetToRef.cpp
				Utils/Utils.cpp
)
target_link_libraries(EDDI PUBLIC TypeDeductionAnalysis)
target_compile_definitions(EDDI PRIVATE DUPLICATE_ALL SELECTIVE_CHECKING=0 CHECK_AT_STORES CHECK_AT_CALLS CHECK_AT_BRANCH)

# REDDI 
add_library(REDDI SHARED
    DuplicateGlobals.cpp
    EDDI.cpp
    FuncRetToRef.cpp
    Utils/Utils.cpp
)
target_link_libraries(REDDI PUBLIC TypeDeductionAnalysis)
target_compile_definitions(REDDI PRIVATE SELECTIVE_CHECKING=0 CHECK_AT_STORES CHECK_AT_CALLS CHECK_AT_BRANCH)


# FDSC
add_library(FDSC SHARED
				DuplicateGlobals.cpp
				EDDI.cpp
				FuncRetToRef.cpp
				Utils/Utils.cpp
)
target_link_libraries(FDSC PUBLIC TypeDeductionAnalysis)
target_compile_definitions(FDSC PRIVATE DUPLICATE_ALL SELECTIVE_CHECKING=1 CHECK_AT_STORES CHECK_AT_CALLS CHECK_AT_BRANCH)

# sEDDI
add_library(SEDDI SHARED
				DuplicateGlobals.cpp
				EDDI.cpp
				FuncRetToRef.cpp
				Utils/Utils.cpp
)
target_link_libraries(SEDDI PUBLIC TypeDeductionAnalysis)
target_compile_definitions(SEDDI PRIVATE DUPLICATE_ALL SELECTIVE_CHECKING=0 CHECK_AT_CALLS CHECK_AT_BRANCH)


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
target_link_libraries(RASM PUBLIC TypeDeductionAnalysis)
target_compile_definitions(RASM PRIVATE INTER_FUNCTION_CFC=0)


# inter-RASM
add_library(INTER_RASM SHARED
				RASM.cpp
				Utils/Utils.cpp
)
target_link_libraries(INTER_RASM PUBLIC TypeDeductionAnalysis)
target_compile_definitions(INTER_RASM PRIVATE INTER_FUNCTION_CFC=1)

# RACFED
add_library(RACFED SHARED
								RACFED.cpp
								Utils/Utils.cpp
)
target_link_libraries(RACFED PUBLIC TypeDeductionAnalysis)

add_library(PROFILER SHARED
				Profiling/ASPISCheckProfiler.cpp
				Utils/Utils.cpp
)
target_link_libraries(PROFILER PUBLIC TypeDeductionAnalysis)
