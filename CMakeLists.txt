if(WIN32 OR CYGWIN)
  set(LLVM_LINK_COMPONENTS Core Support)
endif()

add_llvm_library( SIHFT MODULE BUILDTREE_ONLY
        FuncRetToRef.cpp
        DuplicateGlobals.cpp
        CFCSS.cpp
        EDDI.cpp
        RASM.cpp
  DEPENDS
  intrinsics_gen
  PLUGIN_TOOL
  opt
  )
