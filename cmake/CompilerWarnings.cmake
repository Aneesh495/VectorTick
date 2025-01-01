# Compiler warnings configuration

function(set_warning_flags target)
    target_compile_options(${target} PRIVATE
        $<$<CXX_COMPILER_ID:GNU>:
            -Wall -Wextra -Wpedantic -Werror
            -Wno-unknown-pragmas
            -Wno-missing-field-initializers
        >
        $<$<CXX_COMPILER_ID:Clang>:
            -Wall -Wextra -Wpedantic -Werror
            -Wno-unknown-pragmas
            -Wno-missing-field-initializers
            -Wno-unused-lambda-capture
        >
        $<$<CXX_COMPILER_ID:AppleClang>:
            -Wall -Wextra -Wpedantic -Werror
            -Wno-unknown-pragmas
            -Wno-missing-field-initializers
        >
    )
endfunction()
