SET(CYCAMORE_SOURCE_DIR ${CMAKE_SOURCE_DIR})
# Tell CMake where the modules are
LIST(APPEND CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_INSTALL_PREFIX}/share/cyclus/cmake" ${CYCAMORE_SOURCE_DIR}/cmake)
MESSAGE("--CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH}")

INCLUDE(CyclusBuildSetup)

cyclus_minimum_cmake_version(MINIMUM_CMAKE_VERSION)
CMAKE_MINIMUM_REQUIRED(VERSION ${MINIMUM_CMAKE_VERSION})

cyclus_require_out_of_source_build()

# This project name is cycamore.
PROJECT(CYCAMORE VERSION 1.6.0)

cyclus_require_cxx_support()

# quiets fortify_source warnings when not compiling with optimizations
# in linux distros where compilers were compiled with fortify_source enabled by
# default (e.g. Arch linux).
MESSAGE("-- CMake Build Type: ${CMAKE_BUILD_TYPE}")
STRING(TOLOWER "${CMAKE_BUILD_TYPE}" BUILD_TYPE)
IF(NOT ${BUILD_TYPE} STREQUAL "release")
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0")
ENDIF()
# no overflow warnings because of silly coin-ness
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-overflow")

IF(NOT CYCLUS_DOC_ONLY)
    # Direct any binary installation paths to this directory
    SET(CYCAMORE_BINARY_DIR ${CMAKE_BINARY_DIR})

    # This makes all the libraries build as SHARED
    SET(BUILD_SHARED_LIBS true)

    # Setup build locations.
    cyclus_setup_build_locations(${CYCAMORE_BINARY_DIR})

    SET(CYCAMORE_EXECUTABLE_DIR ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})

    cyclus_set_rpath()

    # get dependency hints
    if (NOT DEPS_ROOT_DIR)
        get_filename_component(compdir ${CMAKE_CXX_COMPILER} DIRECTORY)
        get_filename_component(DEPS_ROOT_DIR ${compdir} DIRECTORY)
    endif (NOT DEPS_ROOT_DIR)
    SET(DEPS_HINTS HINTS "${DEPS_ROOT_DIR}")
    SET(DEPS_BIN_HINTS ${DEPS_HINTS} "${DEPS_ROOT_DIR}/bin")
    SET(DEPS_LIB_HINTS ${DEPS_HINTS} "${DEPS_ROOT_DIR}/lib")
    SET(DEPS_INCLUDE_HINTS HINTS "${DEPS_ROOT_DIR}/include")
    MESSAGE("-- Dependency Root Dir (DEPS_ROOT_DIR): ${DEPS_ROOT_DIR}")
    MESSAGE("-- Dependency Hints (DEPS_HINTS): ${DEPS_HINTS}")
    MESSAGE("-- Dependency Binary Hints (DEPS_BIN_HINTS): ${DEPS_BIN_HINTS}")
    MESSAGE("-- Dependency Library Hints (DEPS_LIB_HINTS): ${DEPS_LIB_HINTS}")
    MESSAGE("-- Dependency Include Hints (DEPS_INCLUDE_HINTS): ${DEPS_INCLUDE_HINTS}")

    # Search pkg-config utility first
    FIND_PACKAGE(PkgConfig REQUIRED)

    # Find cyclus
    FIND_PACKAGE(Cyclus REQUIRED)
    SET(
      CYCAMORE_INCLUDE_DIRS
      ${CYCAMORE_INCLUDE_DIRS}
      ${CYCLUS_CORE_INCLUDE_DIR} # INSTALL_PATH/include/cyclus
      )
    SET(LIBS ${LIBS} ${CYCLUS_CORE_LIBRARIES})

    # Include macros installed with cyclus core
    INCLUDE(UseCyclus)

    # Include the boost header files and the program_options library
    # Please be sure to use Boost rather than BOOST.
    # Capitalization matters on some platforms
    SET(Boost_USE_STATIC_LIBS OFF)
    SET(Boost_USE_STATIC_RUNTIME OFF)
    # SET(Boost_USE_MULTITHREADED    OFF)
    if (DEPS_ROOT_DIR)
      SET(BOOST_ROOT "${DEPS_ROOT_DIR}")
      SET(BOOST_INCLUDEDIR "${DEPS_INCLUDE_HINTS}")
    endif (DEPS_ROOT_DIR)
    FIND_PACKAGE(Boost COMPONENTS program_options filesystem system serialization REQUIRED)
    MESSAGE("--    Boost Root: ${Boost_ROOT}")
    MESSAGE("--    Boost Include directory: ${Boost_INCLUDE_DIR}")
    SET(CYCAMORE_INCLUDE_DIRS ${CYCAMORE_INCLUDE_DIRS} ${Boost_INCLUDE_DIRS})
    MESSAGE("--    Boost Library directories: ${Boost_LIBRARY_DIRS}")
    SET(LIBS ${LIBS} ${Boost_PROGRAM_OPTIONS_LIBRARY})
    MESSAGE("--    Boost Program Options location: ${Boost_PROGRAM_OPTIONS_LIBRARY}")
    SET(LIBS ${LIBS} ${Boost_SYSTEM_LIBRARY})
    MESSAGE("--    Boost System location: ${Boost_SYSTEM_LIBRARY}")
    SET(LIBS ${LIBS} ${Boost_FILESYSTEM_LIBRARY})
    MESSAGE("--    Boost Filesystem location: ${Boost_FILESYSTEM_LIBRARY}")
    SET(LIBS ${LIBS} ${Boost_SERIALIZATION_LIBRARY})
    MESSAGE("--    Boost Serialization location: ${Boost_SERIALIZATION_LIBRARY}")
    ADD_DEFINITIONS(-DBOOST_VERSION_MINOR=${Boost_VERSION_MINOR})

    # find lapack and link to it
    # note there is no include directory variable:
    # http://www.cmake.org/cmake/help/v3.0/module/FindLAPACK.html
    FIND_PACKAGE(LAPACK REQUIRED)
    set(LIBS ${LIBS} ${LAPACK_LIBRARIES})
    MESSAGE("\tFound LAPACK Libraries: ${LAPACK_LIBRARIES}")

    # Find HDF5
    FIND_PACKAGE(HDF5 REQUIRED COMPONENTS HL)
    ADD_DEFINITIONS(${HDF5_DEFINITIONS})
    if(NOT HDF5_LIBRARY_DIRS STREQUAL "")
      set(LIBS ${LIBS} ${HDF5_LIBRARIES})
      link_directories(${HDF5_LIBRARY_DIRS})
    endif()
    set(LIBS ${LIBS} ${HDF5_C_LIBRARIES} ${HDF5_C_HL_LIBRARIES})
    MESSAGE("--    HDF5 Root: ${HDF5_ROOT}")
    MESSAGE("--    HDF5 Include directory: ${HDF5_INCLUDE_DIR}")
    SET(CYCAMORE_INCLUDE_DIRS ${CYCAMORE_INCLUDE_DIRS} ${HDF5_INCLUDE_DIR})
    MESSAGE("--    HDF5 Library directories: ${HDF5_LIBRARY_DIRS}")
    MESSAGE("--    HDF5 Libraries: ${HDF5_C_LIBRARIES}")
    MESSAGE("--    HDF5 High Level Libraries: ${HDF5_C_HL_LIBRARIES}")

    # find coin and link to it
    if(DEFAULT_ALLOW_MILPS)
        FIND_PACKAGE(COIN REQUIRED)
        if(COIN_FOUND)
        SET(CYCAMORE_INCLUDE_DIRS ${CYCAMORE_INCLUDE_DIRS} ${COIN_INCLUDE_DIRS})
        SET(LIBS ${LIBS} ${COIN_LIBRARIES})
        endif()
        MESSAGE("--    COIN Root: ${COIN_ROOT}")
        MESSAGE("--    COIN Include directories: ${COIN_INCLUDE_DIRS}")
        MESSAGE("--    COIN Libraries: ${COIN_LIBRARIES}")
    endif()

    # find SQLite
    FIND_PACKAGE( SQLite3 REQUIRED )
    SET(CYCAMORE_INCLUDE_DIRS ${CYCAMORE_INCLUDE_DIRS} ${SQLite3_INCLUDE_DIR})
    SET(LIBS ${LIBS} ${SQLite3_LIBRARIES})
    MESSAGE("--    SQLite3 Include directories: ${SQLite3_INCLUDE_DIR}")
    MESSAGE("--    SQLite3 Libraries: ${SQLite_LIBRARIES}")

    #
    # Some optional libraries to link in, as availble. Required for conda.
    #
    # pcre
    FIND_LIBRARY(PCRE_LIBRARIES pcre ${DEPS_LIB_HINTS})
    MESSAGE("-- Found PCRE Libraries (optional): ${PCRE_LIBRARIES}")
    IF(PCRE_LIBRARIES)
        set(LIBS ${LIBS} ${PCRE_LIBRARIES})
    ENDIF(PCRE_LIBRARIES)

    # gfortran
    FIND_LIBRARY(GFORTRAN_LIBRARIES gfortran ${DEPS_LIB_HINTS})
    MESSAGE("-- Found GFORTRAN Libraries (optional): ${GFORTRAN_LIBRARIES}")
    IF(GFORTRAN_LIBRARIES)
        set(LIBS ${LIBS} ${GFORTRAN_LIBRARIES})
    ENDIF(GFORTRAN_LIBRARIES)

    # openblas
    FIND_LIBRARY(OPENBLAS_LIBRARIES openblas ${DEPS_LIB_HINTS})
    MESSAGE("-- Found OPENBLAS Libraries (optional): ${OPENBLAS_LIBRARIES}")
    IF(OPENBLAS_LIBRARIES)
        set(LIBS ${LIBS} ${OPENBLAS_LIBRARIES})
    ENDIF(OPENBLAS_LIBRARIES)

    # ClpSolver
    FIND_LIBRARY(CLPSOLVER_LIBRARIES ClpSolver ${DEPS_LIB_HINTS})
    MESSAGE("-- Found CLPSOLVER Libraries (optional): ${CLPSOLVER_LIBRARIES}")
    IF(CLPSOLVER_LIBRARIES)
        set(LIBS ${LIBS} ${CLPSOLVER_LIBRARIES})
    ENDIF(CLPSOLVER_LIBRARIES)

    # iconv
    FIND_LIBRARY(ICONV_LIBRARIES iconv ${DEPS_LIB_HINTS})
    MESSAGE("-- Found ICONV Libraries (optional): ${ICONV_LIBRARIES}")
    IF(ICONV_LIBRARIES)
        set(LIBS ${LIBS} ${ICONV_LIBRARIES})
    ENDIF(ICONV_LIBRARIES)

    # icudata
    FIND_LIBRARY(ICUDATA_LIBRARIES icudata ${DEPS_LIB_HINTS})
    MESSAGE("-- Found ICUDATA Libraries (optional): ${ICUDATA_LIBRARIES}")
    IF(ICUDATA_LIBRARIES)
        set(LIBS ${LIBS} ${ICUDATA_LIBRARIES})
    ENDIF(ICUDATA_LIBRARIES)

    # icui18n
    FIND_LIBRARY(ICUI18N_LIBRARIES icui18n ${DEPS_LIB_HINTS})
    MESSAGE("-- Found ICUI18N Libraries (optional): ${ICUI18N_LIBRARIES}")
    IF(ICUI18N_LIBRARIES)
        set(LIBS ${LIBS} ${ICUI18N_LIBRARIES})
    ENDIF(ICUI18N_LIBRARIES)

    # icuuc
    FIND_LIBRARY(ICUUC_LIBRARIES icuuc ${DEPS_LIB_HINTS})
    MESSAGE("-- Found ICUUC Libraries (optional): ${ICUUC_LIBRARIES}")
    IF(ICUUC_LIBRARIES)
        set(LIBS ${LIBS} ${ICUUC_LIBRARIES})
    ENDIF(ICUUC_LIBRARIES)

    # Python
    find_package(Python3 COMPONENTS Interpreter Development)
    message("-- PYTHON_EXECUTABLE: ${Python3_EXECUTABLE}")
    MESSAGE("-- Found Python Libraries (optional): ${Python3_LIBRARIES}")
    IF(Python3_FOUND)
        set(LIBS ${LIBS} ${Python3_LIBRARIES})
        message("libs set with ${Python3_LIBRARIES}")
    ENDIF(Python3_FOUND)

    # include all the directories we just found
    INCLUDE_DIRECTORIES(${CYCAMORE_INCLUDE_DIRS})

    # ------------------------- Add the Agents -----------------------------------
    ADD_SUBDIRECTORY(src)

    # ------------------------- Google Test -----------------------------------

    # Be sure to clear these each time
    SET(GENERATED_TEST_LIST "" CACHE INTERNAL "")
    SET(GENERATED_FILTER_LIST "" CACHE INTERNAL "")
    SET(GENERATED_TEST_SOURCE_LIST "" CACHE INTERNAL "")

    # Create an executable for all the gtests
    # Options for testing
    OPTION(USE_TESTING "Build testing" ON)
    IF(USE_TESTING)
        enable_testing()
        INCLUDE(CTest)
    ENDIF()

    ADD_SUBDIRECTORY(tests)

    SET(TestSource
        ${TestSource}
        ${CYCAMORE_TEST_CORE}
        )

    FIND_PACKAGE(Threads)
    IF(CMAKE_USE_PTHREADS_INIT)  # The pthreads library is available.
        SET(cxx_base_flags "${cxx_base_flags} -DGTEST_HAS_PTHREAD=1")
    ENDIF()

    # Build cycamore_unit_tests
    ADD_EXECUTABLE(cycamore_unit_tests
        tests/cycamore_unit_test_driver.cc
        ${TestSource}
        )

    TARGET_LINK_LIBRARIES(cycamore_unit_tests
        dl
        ${LIBS}
        cycamore
        ${CYCLUS_TEST_LIBRARIES}
        )

    INSTALL(TARGETS cycamore_unit_tests
        RUNTIME DESTINATION bin
        COMPONENT testing
        )

    ##############################################################################################
    ################################## begin uninstall target ####################################
    ##############################################################################################

    CONFIGURE_FILE(
        "${CMAKE_CURRENT_SOURCE_DIR}/config/cmake_uninstall.cmake.in"
        "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake"
        IMMEDIATE @ONLY
        )

    ADD_CUSTOM_TARGET(uninstall
        COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake)

    ##############################################################################################
    ################################### end uninstall target #####################################
    ##############################################################################################

    ##############################################################################################
    ####################################### begin cpack ##########################################
    ##############################################################################################


    # Here's where we package it with CPack
    SET(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Cycamore : The Cyclus Additional Module Repository.")

    # Here we set some components for installation with cpack
    SET(CPACK_COMPONENTS_ALL cycamore testing libraries data core)
    SET(CPACK_GENERATOR "DEB")

    # Organisation
    SET(CPACK_DEBIAN_PACKAGE_MAINTAINER "CNERG, UW-Madison") #required
    SET(CPACK_PACKAGE_VENDOR "CNERG, UW-Madison")
    SET(CPACK_PACKAGE_DESCRIPTION_FILE "${CMAKE_CURRENT_SOURCE_DIR}/README.rst")
    SET(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE.rst")

    # Version
    SET(CPACK_PACKAGE_VERSION_MAJOR "${CMAKE_PROJECT_VERSION_MAJOR}")
    SET(CPACK_PACKAGE_VERSION_MINOR "${CMAKE_PROJECT_VERSION_MINOR}")
    SET(CPACK_PACKAGE_VERSION_PATCH "${CMAKE_PROJECT_VERSION_PATCH}")

    # Dependencies
    SET(CPACK_DEBIAN_PACKAGE_DEPENDS "cyclus (>= 1.5.5)")
    MESSAGE("CPACK_DEBIAN_PACKAGE_DEPENDS ${CPACK_DEBIAN_PACKAGE_DEPENDS}")

    # Names
    SET(CPACK_PACKAGE_NAME "cycamore")
    SET(CPACK_PACKAGE_INSTALL_DIRECTORY "cycamore_${CPACK_PACKAGE_VERSION_MAJOR}")
    SET(CPACK_PACKAGE_INSTALL_DIRECTORY "${CPACK_PACKAGE_INSTALL_DIRECTORY}.${CPACK_PACKAGE_VERSION_MINOR}")
    SET(CPACK_PACKAGE_INSTALL_DIRECTORY "${CPACK_PACKAGE_INSTALL_DIRECTORY}.${CPACK_PACKAGE_VERSION_PATCH}")

    # Configuration
    SET(CPACK_INCLUDE_TOPLEVEL_DIRECTORY "1")
    SET(CPACK_DEBIAN_PACKAGE_SHLIBDEPS OFF)
    CONFIGURE_FILE(
      "${CYCAMORE_SOURCE_DIR}/cmake/CycamoreCPackOptions.cmake.in"
      "${CYCAMORE_BINARY_DIR}/cmake/CycamoreCPackOptions.cmake" @ONLY
      )
    SET(CPACK_PROJECT_CONFIG_FILE "${CYCAMORE_BINARY_DIR}/cmake/CycamoreCPackOptions.cmake")
    SET(CPACK_PACKAGE_EXECUTABLES "cycamore" "cycamore_unit_tests")

    INCLUDE(CPack)

    SET(ROOT_DIR ${CYCLUS_ROOT_DIR})
    SET(PROJ_DIR ${PROJECT_BINARY_DIR})
    SET(CORE_SHARE_DIR ${CYCLUS_CORE_SHARE_DIR})

    ##############################################################################################
    ######################################## end cpack ###########################################
    ##############################################################################################

ENDIF(NOT CYCLUS_DOC_ONLY)

# This is the directory that holds the doxygen doxyfile template (doxy.conf.in)
SET(DOC_INPUT_DIR ${CYCAMORE_SOURCE_DIR}/doc)

# If doxygen exists, use the doc/CMakeLists.txt file for further instructions.
FIND_PACKAGE(Doxygen)
IF(DOXYGEN_FOUND)
    ADD_SUBDIRECTORY(doc)
    SET(DOC_OUTPUT_DIR ${CMAKE_BINARY_DIR}/doc)
ELSE(DOXYGEN_FOUND)
    MESSAGE(STATUS "WARNING: Doxygen not found - doc won't be created")
ENDIF(DOXYGEN_FOUND)
