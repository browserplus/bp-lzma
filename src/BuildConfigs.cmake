# require cmake 2.6.2 or higher
CMAKE_MINIMUM_REQUIRED(VERSION 2.6.2 FATAL_ERROR)

IF (POLICY CMP0011) 
  cmake_policy(SET CMP0011 OLD)
ENDIF (POLICY CMP0011) 
cmake_policy(SET CMP0003 NEW)
cmake_policy(SET CMP0009 NEW)

SET (CMAKE_CONFIGURATION_TYPES Debug Release 
     CACHE STRING "bp-ruby build configs" FORCE)

# reduce redundancy in the cmake language
SET(CMAKE_ALLOW_LOOSE_LOOP_CONSTRUCTS 1)

IF (NOT CMAKE_BUILD_TYPE)
  SET(CMAKE_BUILD_TYPE "Debug")
ENDIF ()

# now set up the build configurations
IF(WIN32)
    SET(win32Defs "/DWINDOWS /D_WINDOWS /DWIN32 /D_WIN32 /DXP_WIN32 /DUNICODE /D_UNICODE /DWIN32_LEAN_AND_MEAN /DNOSOUND /DNOCOMM /DNOMCX /DNOSERVICE /DNOIME /DNORPC")
    SET(disabledWarnings "/wd4100 /wd4127 /wd4201 /wd4250 /wd4251 /wd4275 /wd4800")
    SET(CMAKE_CXX_FLAGS
        "${win32Defs} /EHsc /Gy /MT /W4 ${disabledWarnings} /Zi"
        CACHE STRING "BrowserPlus CXX flags" FORCE)
    SET(CMAKE_CXX_FLAGS_DEBUG "/MTd /DDEBUG /D_DEBUG /Od /RTC1 /RTCc"
        CACHE STRING "BrowserPlus debug CXX flags" FORCE)
    SET(CMAKE_CXX_FLAGS_RELEASE "/MT /DNDEBUG /O1"
        CACHE STRING "BrowserPlus release CXX flags" FORCE)
  
    # libs to ignore, from http://msdn.microsoft.com/en-us/library/aa267384.aspx
    #
    SET(noDefaultLibFlagsDebug "/NODEFAULTLIB:libc.lib /NODEFAULTLIB:libcmt.lib /NODEFAULTLIB:msvcrt.lib /NODEFAULTLIB:libcd.lib /NODEFAULTLIB:msvcrtd.lib")
    SET(noDefaultLibFlagsRelease "/NODEFAULTLIB:libc.lib /NODEFAULTLIB:msvcrt.lib /NODEFAULTLIB:libcd.lib /NODEFAULTLIB:libcmtd.lib /NODEFAULTLIB:msvcrtd.lib")

    SET(linkFlags "/DEBUG /MANIFEST:NO")
    SET(linkFlagsDebug " ${noDefaultLibFlagsDebug}")
    SET(linkFlagsRelease " /INCREMENTAL:NO /OPT:REF /OPT:ICF ${noDefaultLibFlagsRelease}")
  
    SET(CMAKE_EXE_LINKER_FLAGS "${linkFlags}"
        CACHE STRING "BrowserPlus linker flags" FORCE)
    SET(CMAKE_EXE_LINKER_FLAGS_DEBUG "${linkFlagsDebug}"
        CACHE STRING "BrowserPlus debug linker flags" FORCE)
    SET(CMAKE_EXE_LINKER_FLAGS_RELEASE "${linkFlagsRelease}"
        CACHE STRING "BrowserPlus release linker flags" FORCE)
    SET(CMAKE_SHARED_LINKER_FLAGS "${linkFlags}"
        CACHE STRING "BrowserPlus shared linker flags" FORCE)
    SET(CMAKE_SHARED_LINKER_FLAGS_DEBUG "${linkFlagsDebug}"
        CACHE STRING "BrowserPlus shared debug linker flags" FORCE)
    SET(CMAKE_SHARED_LINKER_FLAGS_RELEASE "${linkFlagsRelease}"
        CACHE STRING "BrowserPlus shared release linker flags" FORCE)

    SET(CMAKE_MODULE_LINKER_FLAGS "${linkFlags}"
        CACHE STRING "BrowserPlus module linker flags" FORCE)
    SET(CMAKE_MODULE_LINKER_FLAGS_DEBUG "${linkFlagsDebug}"
        CACHE STRING "BrowserPlus module debug linker flags" FORCE)
    SET(CMAKE_MODULE_LINKER_FLAGS_RELEASE "${linkFlagsRelease}"
        CACHE STRING "BrowserPlus module release linker flags" FORCE)
ELSE ()
    SET(isysrootFlag)
    IF (APPLE)
      SET (CMAKE_OSX_SYSROOT "/Developer/SDKs/MacOSX10.4u.sdk"
	       CACHE STRING "Compile for tiger backwards compat" FORCE)
      SET(isysrootFlag "-isysroot ${CMAKE_OSX_SYSROOT}")
      SET(minVersionFlag "-mmacosx-version-min=10.4")
      SET(CMAKE_FRAMEWORK_PATH "${CMAKE_OSX_SYSROOT}/System/Library/Frameworks"
	      CACHE STRING "use 10.4 frameworks" FORCE)

      SET(CMAKE_MODULE_LINKER_FLAGS "${minVersionFlag} ${isysrootFlag}")
      SET(CMAKE_EXE_LINKER_FLAGS "-dead_strip -dead_strip_dylibs ${minVersionFlag} ${isysrootFlag}")
      SET(CMAKE_SHARED_LINKER_FLAGS "${minVersionFlag} ${isysrootFlag}  -Wl,-single_module")
      ADD_DEFINITIONS(-DMACOSX -D_MACOSX -DMAC -D_MAC -DXP_MACOSX)
      SET(CMAKE_C_COMPILER gcc-4.0)
      SET(CMAKE_CXX_COMPILER g++-4.0)
    ELSE()
      ADD_DEFINITIONS(-DLINUX -D_LINUX -DXP_LINUX)
    ENDIF()

    SET(CMAKE_CXX_FLAGS "-Wall ${isysrootFlag} ${minVersionFlag}")
    SET(CMAKE_CXX_FLAGS_DEBUG "-DDEBUG -g")
    SET(CMAKE_CXX_FLAGS_RELEASE "-DNDEBUG -Os")
    SET(CMAKE_MODULE_LINKER_FLAGS_RELEASE "-Wl,-x")
    SET(CMAKE_EXE_LINKER_FLAGS_RELEASE "-Wl,-x")
    SET(CMAKE_SHARED_LINKER_FLAGS_RELEASE "-Wl,-x")
ENDIF ()
