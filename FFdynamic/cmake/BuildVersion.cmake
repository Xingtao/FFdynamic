function (FFdynamic_build_version)
    # get compile time
    execute_process(
        COMMAND date "+%Y.%m.%d %H:%M:%S"
        OUTPUT_VARIABLE compileTime
        OUTPUT_STRIP_TRAILING_WHITESPACE
        )

    if(EXISTS ${CMAKE_SOURCE_DIR}/../.git)
        # get git version
        execute_process(
            COMMAND git --git-dir=${CMAKE_SOURCE_DIR}/../.git --work-tree=${CMAKE_SOURCE_DIR}/../ rev-parse HEAD
            OUTPUT_VARIABLE FFdynamicBuildVersion
            OUTPUT_STRIP_TRAILING_WHITESPACE
            )
    else()
        message(WARNING "Could not get version info")
    endif()

    MESSAGE("Compile Time: " ${compileTime})
    MESSAGE("FFdynamic build version: " ${FFdynamicBuildVersion})

    # add macros
    ADD_DEFINITIONS(-DCOMPILE_TIME="${compileTime}")
    ADD_DEFINITIONS(-DCOMPILE_VERSION="${FFdynamicBuildVersion}")

endfunction(FFdynamic_build_version)
