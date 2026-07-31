cmake_minimum_required(VERSION 3.20)
project(SystemApp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Static-link the runtime for a portable single-binary drop onto a device,
# where you generally can't rely on a matching libstdc++/libc++ being
# present. This flag only applies to compilers that understand it (GCC);
# NDK/Clang builds use ANDROID_STL from the toolchain file instead.
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static-libgcc -static-libstdc++")
endif()

add_compile_options(-Wall -Wextra -Wpedantic)

file(GLOB_RECURSE SYSTEMAPP_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_SOURCE_DIR}/src/*.cpp"
)

add_executable(systemapp ${SYSTEMAPP_SOURCES})

target_include_directories(systemapp PRIVATE
    "${CMAKE_SOURCE_DIR}/include"
)

# When cross-compiling for Android via the NDK toolchain file, link against
# the log library so future logcat-wrapper / native crash reporting can hook
# in without a separate find_library dance. Harmless no-op on the host.
find_library(ANDROID_LOG_LIB log)
if(ANDROID_LOG_LIB)
    target_link_libraries(systemapp PRIVATE ${ANDROID_LOG_LIB})
endif()

install(TARGETS systemapp RUNTIME DESTINATION bin)

# --- Notes for cross-compiling with the Android NDK -------------------------
# cmake -B build-android -G Ninja \
#   -DCMAKE_TOOLCHAIN_FILE=$ANDROID_NDK_HOME/build/cmake/android.toolchain.cmake \
#   -DANDROID_ABI=arm64-v8a \
#   -DANDROID_PLATFORM=android-29 \
#   -DANDROID_STL=c++_static
# cmake --build build-android
# -----------------------------------------------------------------------------
