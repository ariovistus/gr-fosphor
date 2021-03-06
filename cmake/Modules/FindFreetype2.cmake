if(NOT FREETYPE2_FOUND)
  INCLUDE(FindPkgConfig)
  pkg_check_modules (FREETYPE2_PKG freetype2)

  # find ft2build.h, which was added in 2.1.6; this could be in the
  # top level include directory (for API release 16 or earlier) or in
  # freetype2 (for API release 17 or later).

  find_path(FREETYPE2_INCLUDE_DIR_ft2build
    NAMES
    ft2build.h
    HINTS
    ${FREETYPE2_PKG_INCLUDE_DIRS}
    ENV FREETYPE2_DIR
    PATHS
    /usr
    /usr/local
    /opt/local
    ${FREETYPE2_DIR}
    PATH_SUFFIXES
    include/freetype2
    include
  )

  if(FREETYPE2_INCLUDE_DIR_ft2build)

    # Freetype changed header locations between API release 16 and 17;
    # look for config/ftheader.h depending on the API release number.

    if(NOT FREETYPE2_PKG_VERSION)

      # PKGCONFIG failed to find a package, but ft2build.h was
      # located.  Guess API release version based on trailing
      # directory of ft2build.h location.

      STRING(REGEX MATCH "[^/]*$" FREETYPE2_ft2build_DIR ${FREETYPE2_INCLUDE_DIR_ft2build})
      message("FREETYPE2_ft2build_DIR is '${FREETYPE2_ft2build_DIR}'")
      STRING(COMPARE NOTEQUAL ${FREETYPE2_ft2build_DIR} "freetype2" FREETYPE_IS_2_4_OR_EARLIER)

    else(NOT FREETYPE2_PKG_VERSION)

      # PKGCONFIG provides version information; use that

      STRING(REGEX MATCH "[^.]*" FREETYPE2_RELEASE ${FREETYPE2_PKG_VERSION})
      STRING(COMPARE LESS ${FREETYPE2_RELEASE} 17 FREETYPE_IS_2_4_OR_EARLIER)

    endif(NOT FREETYPE2_PKG_VERSION)

    if(FREETYPE_IS_2_4_OR_EARLIER)
      # freetype 2.4 or earlier
      set(FTHEADER_NAME freetype/config/ftheader.h)
    else(FREETYPE_IS_2_4_OR_EARLIER)
      # freetype 2.5 or later
      set(FTHEADER_NAME config/ftheader.h)
    endif(FREETYPE_IS_2_4_OR_EARLIER)

    find_path(FREETYPE2_INCLUDE_DIR_ftheader
      NAMES
      ${FTHEADER_NAME}
      HINTS
      ${FREETYPE2_PKG_INCLUDE_DIRS}
      ${FREETYPE2_INCLUDE_DIR_ft2build}
      ENV FREETYPE2_DIR
      PATH_SUFFIXES
      freetype2
      include/freetype2
      include
      NO_DEFAULT_PATH
    )

    if(FREETYPE2_INCLUDE_DIR_ftheader)
      string(COMPARE EQUAL ${FREETYPE2_INCLUDE_DIR_ft2build} ${FREETYPE2_INCLUDE_DIR_ftheader} FREETYPE2_DIRS_SAME)
      if(FREETYPE2_DIRS_SAME)
        set(FREETYPE2_INCLUDE_DIRS ${FREETYPE2_INCLUDE_DIR_ft2build})
      else()
        set(FREETYPE2_INCLUDE_DIRS ${FREETYPE2_INCLUDE_DIR_ft2build};${FREETYPE2_INCLUDE_DIR_ftheader})
      endif()

      find_library(FREETYPE2_LIBRARIES
        NAMES
        freetype
        HINTS
        ${FREETYPE2_PKG_LIBRARY_DIRS}
        ENV FREETYPE_DIR
        PATHS
        /usr
        /usr/local
        /opt/local
    	${FREETYPE2_DIR}
        PATH_SUFFIXES
        lib
	lib64
      )

      if(FREETYPE2_LIBRARIES)
        set(FREETYPE2_FOUND TRUE CACHE INTERNAL "freetype2 found")
        message(STATUS "Found freetype2: ${FREETYPE2_INCLUDE_DIRS}, ${FREETYPE2_LIBRARIES}")
      else(FREETYPE2_INCLUDE_DIRS AND FREETYPE2_LIBRARIES)
        set(FREETYPE2_FOUND FALSE CACHE INTERNAL "freetype2 found")
        message(STATUS "freetype2 not found.")
      endif(FREETYPE2_LIBRARIES)
    endif(FREETYPE2_INCLUDE_DIR_ftheader)
  endif(FREETYPE2_INCLUDE_DIR_ft2build)

  mark_as_advanced(FREETYPE2_INCLUDE_DIRS FREETYPE2_LIBRARIES)

endif(NOT FREETYPE2_FOUND)
