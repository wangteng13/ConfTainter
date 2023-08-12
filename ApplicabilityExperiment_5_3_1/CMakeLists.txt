project(tainter)
cmake_minimum_required(VERSION 2.8)




find_package(LLVM 10.0.0 REQUIRED CONFIG)

message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

# Needed to use support library
# -fno-rtti -I/usr/local/boost 
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -std=c++14")

message(STATUS "Found LLVM_INCLUDE_DIRS ${LLVM_INCLUDE_DIRS}")
include_directories(${LLVM_INCLUDE_DIRS})
message(STATUS "Found LLVM_DEFINITIONS ${LLVM_DEFINITIONS}")




add_definitions(${LLVM_DEFINITIONS})


set(LLVM_LINK_COMPONENTS
    Core
    ExecutionEngine
    Interpreter
    MC
    Support
    nativecodegen
    mcjit
    X86
    asmparser
    objcarcopts
    scalaropts
    transformutils
    irreader
)


add_executable(TInfer_ValueRel
    TInfer_ValueRel.cpp
	tainter.cpp
)

add_executable(TInfer_ControlDeps
    TInfer_ControlDeps.cpp
	tainter.cpp
)

llvm_map_components_to_libnames(llvm_libs ${LLVM_LINK_COMPONENTS})


find_package(Boost 1.73.0 COMPONENTS serialization)
include_directories(${Boost_INCLUDE_DIRS})
link_directories(${Boost_LIBRARY_DIRS})
if(Boost_FOUND)
    set(Boost_INCLUDE_DIRS /usr/local/include)
    set(Boost_LIBRARY_DIRS /usr/local/lib)
    message(STATUS "Boost_INCLUDE_DIRS ${Boost_INCLUDE_DIRS}")
    message(STATUS "Boost_LIBRARY_DIRS ${Boost_LIBRARY_DIRS}")

    #link llvm component
    message(STATUS "Found llvm_libs ${llvm_libs}")
    target_link_libraries(TInfer_ValueRel ${llvm_libs} Boost::serialization)

    target_link_libraries(TInfer_ValueRel
        pthread
        dl
        ${llvm_libs}
    )
    target_link_libraries(TInfer_ControlDeps
        pthread
        dl
        ${llvm_libs}
    )

else()
    #link llvm component
    message(STATUS "Found llvm_libs ${llvm_libs}")
    target_link_libraries(TInfer_ValueRel ${llvm_libs})

    target_link_libraries(TInfer_ValueRel
        pthread
        dl
        ${llvm_libs}
    )
    target_link_libraries(TInfer_ControlDeps
        pthread
        dl
        ${llvm_libs}
    )

endif()



