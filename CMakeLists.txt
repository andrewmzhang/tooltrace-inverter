cmake_minimum_required(VERSION 4.2.1)
project(gridfinity_inverter CXX)

include(ExternalProject)
include(ProcessorCount)

if (NOT EMSCRIPTEN)
    message(FATAL_ERROR "This project must be built with Emscripten (emcmake).")
endif ()

# Use C++20
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Define main target
add_executable(index main.cpp)
set(CMAKE_EXECUTABLE_SUFFIX ".html") # <- you override/change .js to .html
set_target_properties(index PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
target_link_options(index PRIVATE
        --shell-file ${PROJECT_SOURCE_DIR}/shell.html
        -sEXPORTED_FUNCTIONS=_malloc,_free
        -sEXPORTED_RUNTIME_METHODS=stringToUTF8,UTF8ToString,lengthBytesUTF8
        -sALLOW_MEMORY_GROWTH
        -sNO_DISABLE_EXCEPTION_CATCHING
        -sMODULARIZE=1
        -sWASM=1
)

# Setup OCCT library as a dependency
set(OCCT_LIBS
        TKernel
        TKMath
        TKG2d
        TKG3d
        TKGeomBase
        TKBRep
        TKGeomAlgo
        TKTopAlgo
        TKPrim
        TKBool
        TKShHealing
        TKXSBase
        TKDESTL
        TKBO
        TKDESTEP
        TKMesh
)
set(OCCT_LIB_DIR "${PROJECT_BINARY_DIR}/occt-prefix/src/occt-build/lin32/clang/lib")
set(OCCT_INCLUDE_DIR "${PROJECT_BINARY_DIR}/occt-prefix/src/occt-build/include/opencascade")
set(OCCT_TARGETS "")
foreach (lib IN LISTS OCCT_LIBS)
    list(APPEND OCCT_TARGETS "${lib}")
endforeach ()
# ExternalProject to build OCCT with Emscripten and static flags
# Cannot use FetchContent, since OCCT uses CMAKE_SOURCE_DIR
ExternalProject_Add(occt
        GIT_REPOSITORY https://github.com/Open-Cascade-SAS/OCCT.git
        GIT_TAG V8_0_0_rc3
        CMAKE_COMMAND emcmake ${CMAKE_COMMAND} -DBUILD_LIBRARY_TYPE=static -DBUILD_MODULE_Draw=OFF -DUSE_FREETYPE=OFF
        BUILD_COMMAND ${CMAKE_COMMAND} --build . -- -j 4 ${OCCT_TARGETS}
        INSTALL_COMMAND ""
        UPDATE_COMMAND ""
)
add_dependencies(index occt)
# Add OCCT library as dependency to index target
target_include_directories(index PRIVATE ${OCCT_INCLUDE_DIR})
target_link_directories(index PRIVATE ${OCCT_LIB_DIR})
target_link_libraries(index PRIVATE ${OCCT_LIBS})
