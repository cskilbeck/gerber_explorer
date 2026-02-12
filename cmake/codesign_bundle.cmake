# Ad-hoc codesign all dylibs in the Frameworks directory, then the app bundle.
# Usage: cmake -DBUNDLE_DIR=/path/to/Foo.app -P codesign_bundle.cmake

if(NOT BUNDLE_DIR)
    message(FATAL_ERROR "BUNDLE_DIR not set")
endif()

file(GLOB _dylibs "${BUNDLE_DIR}/Contents/Frameworks/*.dylib")
foreach(_lib ${_dylibs})
    message(STATUS "Codesigning: ${_lib}")
    execute_process(
        COMMAND codesign --force --sign - "${_lib}"
        RESULT_VARIABLE _result
    )
    if(_result)
        message(WARNING "codesign failed for ${_lib}")
    endif()
endforeach()

message(STATUS "Codesigning: ${BUNDLE_DIR}")
execute_process(
    COMMAND codesign --force --sign - "${BUNDLE_DIR}"
    RESULT_VARIABLE _result
)
if(_result)
    message(WARNING "codesign failed for ${BUNDLE_DIR}")
endif()
