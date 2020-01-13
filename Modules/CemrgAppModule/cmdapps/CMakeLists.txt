option(BUILD_CEMRG_FIXSHELL "Build fixing shell values command line app. " ON)
option(BUILD_CEMRG_CLIPPINGTOOL "Build clipping valve command line app. " OFF)

if(BUILD_CEMRG_FIXSHELL)
  mitkFunctionCreateCommandLineApp(
    NAME CemrgFixShell
    DEPENDS MitkCemrgAppModule
    CPP_FILES CemrgFixShellApp.cpp
  )
endif()

if(BUILD_CEMRG_CLIPPINGTOOL)
  mitkFunctionCreateCommandLineApp(
    NAME CemrgClippingTool
    DEPENDS MitkCemrgAppModule
    CPP_FILES CemrgClippingTool.cpp
  )
endif()
