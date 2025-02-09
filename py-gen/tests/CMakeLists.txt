project(tests)

include(CTest)
enable_testing()

include(get_cpm)

set(CMAKE_WARN_DEPRECATED
    OFF
    CACHE BOOL "" FORCE)
cpmaddpackage("gh:onqtam/doctest@2.4.11")
set(CMAKE_WARN_DEPRECATED
    ON
    CACHE BOOL "" FORCE)

add_executable(tests main.cpp)
target_link_libraries(tests PRIVATE doctest)