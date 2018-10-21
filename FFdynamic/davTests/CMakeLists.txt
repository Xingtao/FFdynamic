add_executable(filterTest ffmpegFilterTest.cpp testCommon.cpp)
add_executable(avMixerTest avMixTest.cpp testCommon.cpp)
add_executable(streamletMixerTest streamletMixTest.cpp testCommon.cpp)
add_executable(simpleTranscode simpleTranscode.cpp)
# add_executable(parallelTranscode parallelTranscode.cpp)

set(bins filterTest avMixerTest streamletMixerTest simpleTranscode)
foreach(bin ${bins})
  target_link_libraries(${bin}
    PUBLIC $<$<CXX_COMPILER_ID:GNU>:>
    PUBLIC $<$<CXX_COMPILER_ID:Clang>:>
    PUBLIC ffdynamic::ffdynamic)
endforeach()

# enable_testing()
# add_test(Tester ${bin} COMMAND)
