# ===========================================================
# tests/ — Unit tests
# ===========================================================

add_executable(ProtocolTests
    test_protocol.cpp
)

target_link_libraries(ProtocolTests
    PRIVATE
        CommonLib
)

# Register with CTest
add_test(NAME ProtocolTests COMMAND ProtocolTests)


