add_definitions(-DSTATICLIB)

add_subdirectory(gtest)
include_directories(${gtest_SOURCE_DIR}/include ${gtest_SOURCE_DIR} ../version)

file(GLOB_RECURSE CORE_TESTS core_tests/*)
file(GLOB_RECURSE CRYPTO_TESTS crypto/*)
file(GLOB_RECURSE PERFORMANCE_TESTS performance_tests/*)
file(GLOB_RECURSE CORE_PROXY core_proxy/*)
file(GLOB_RECURSE TEST_GENERATOR TestGenerator/*)
file(GLOB_RECURSE UNIT_TESTS unit_tests/*)
file(GLOB_RECURSE INTEGRATION_TEST_LIB integration_test_lib/*)
file(GLOB_RECURSE INTEGRATION_TESTS integration_tests/*)
file(GLOB_RECURSE TRANSFERS_TESTS transfers_tests/*)



source_group(core_tests FILES ${CORE_TESTS})
source_group(crypto_tests FILES ${CRYPTO_TESTS})
source_group(performance_tests FILES ${PERFORMANCE_TESTS})
source_group(core_proxy FILES ${CORE_PROXY})
source_group(TestGenerator FILES ${TEST_GENERATOR})
source_group(unit_tests FILES ${UNIT_TESTS})
source_group(integration_test_lib FILES ${INTEGRATION_TEST_LIB})
source_group(integration_tests FILES ${INTEGRATION_TESTS})
source_group(transfers_tests FILES ${TRANSFERS_TESTS})


# add_subdirectory(daemon_tests)

add_library(TestGenerator ${TEST_GENERATOR})
add_library(integration_test_lib ${INTEGRATION_TEST_LIB})


add_executable(coretests ${CORE_TESTS})
add_executable(crypto-tests ${CRYPTO_TESTS})
add_executable(difficulty-tests difficulty/difficulty.cpp)
add_executable(hash-tests hash/main.cpp)
add_executable(hash-target-tests hash-target.cpp)
add_executable(performance_tests ${PERFORMANCE_TESTS})
add_executable(core_proxy ${CORE_PROXY} ../src/p2p/NetNodeConfig.cpp)
add_executable(unit_tests ${UNIT_TESTS})
add_executable(net_load_tests_clt net_load_tests/clt.cpp)
add_executable(net_load_tests_srv net_load_tests/srv.cpp)
add_executable(integration_tests ${INTEGRATION_TESTS} ../src/p2p/NetNodeConfig.cpp)
add_executable(transfers_tests ${TRANSFERS_TESTS} ../src/p2p/NetNodeConfig.cpp ../src/cryptonote_core/MinerConfig.cpp ../src/cryptonote_core/CoreConfig.cpp)


target_link_libraries(core_proxy epee cryptonote_core common crypto upnpc-static ${Boost_LIBRARIES})
target_link_libraries(coretests epee cryptonote_core common crypto TestGenerator ${Boost_LIBRARIES})
target_link_libraries(difficulty-tests epee cryptonote_core common crypto ${Boost_LIBRARIES})
target_link_libraries(hash-tests crypto)
target_link_libraries(hash-target-tests epee crypto cryptonote_core)
target_link_libraries(performance_tests epee cryptonote_core common crypto ${Boost_LIBRARIES})
target_link_libraries(unit_tests epee wallet TestGenerator cryptonote_core common crypto gtest_main transfers serialization inprocess_node ${Boost_LIBRARIES})
target_link_libraries(net_load_tests_clt epee cryptonote_core common crypto gtest_main ${Boost_LIBRARIES})
target_link_libraries(net_load_tests_srv epee cryptonote_core common crypto gtest_main ${Boost_LIBRARIES})
target_link_libraries(integration_tests integration_test_lib epee wallet node_rpc_proxy rpc transfers cryptonote_core crypto common upnpc-static serialization System inprocess_node ${Boost_LIBRARIES})
target_link_libraries(transfers_tests integration_test_lib epee node_rpc_proxy rpc upnpc-static transfers System gtest_main inprocess_node wallet serialization cryptonote_core crypto common ${Boost_LIBRARIES})



file(GLOB_RECURSE NODE_RPC_PROXY_TEST node_rpc_proxy_test/*)
source_group(node_rpc_proxy_test FILES ${NODE_RPC_PROXY_TEST})
add_executable(node_rpc_proxy_test ${NODE_RPC_PROXY_TEST})
target_link_libraries(node_rpc_proxy_test epee rpc node_rpc_proxy cryptonote_core common crypto serialization ${Boost_LIBRARIES})

if(NOT MSVC)
  set_property(TARGET gtest gtest_main unit_tests net_load_tests_clt net_load_tests_srv TestGenerator integration_test_lib integration_tests APPEND_STRING PROPERTY COMPILE_FLAGS " -Wno-undef -Wno-sign-compare")
endif()

add_custom_target(tests DEPENDS coretests difficulty hash performance_tests core_proxy unit_tests node_rpc_proxy_test integration_tests transfers_tests)
set_property(TARGET coretests crypto-tests difficulty-tests gtest gtest_main hash-tests hash-target-tests performance_tests core_proxy unit_tests tests net_load_tests_clt net_load_tests_srv node_rpc_proxy_test TestGenerator integration_test_lib integration_tests PROPERTY FOLDER "tests")
set_property(TARGET transfers_tests PROPERTY FOLDER "tests")

add_dependencies(core_proxy version)

add_test(coretests coretests --generate_and_play_test_data)
add_test(crypto crypto-tests ${CMAKE_CURRENT_SOURCE_DIR}/crypto/tests.txt)
add_test(difficulty difficulty-tests ${CMAKE_CURRENT_SOURCE_DIR}/difficulty/data.txt)
foreach(hash IN ITEMS fast slow tree extra-blake extra-groestl extra-jh extra-skein)
  add_test(hash-${hash} hash-tests ${hash} ${CMAKE_CURRENT_SOURCE_DIR}/hash/tests-${hash}.txt)
endforeach(hash)
add_test(hash-target hash-target-tests)
add_test(unit_tests unit_tests)
