add_definitions(-DSTATICLIB)

include_directories(${gtest_SOURCE_DIR}/include ${gtest_SOURCE_DIR} ../version)

file(GLOB_RECURSE CoreTests core_tests/*)
file(GLOB_RECURSE CryptoTests crypto/*)
file(GLOB_RECURSE FunctionalTests functional_tests/*)
file(GLOB_RECURSE IntegrationTestLibrary integration_test_lib/*)
file(GLOB_RECURSE IntegrationTests integration_tests/*)
file(GLOB_RECURSE NodeRpcProxyTests node_rpc_proxy_test/*)
file(GLOB_RECURSE PerformanceTests performance_tests/*)
file(GLOB_RECURSE SystemTests System/*)
file(GLOB_RECURSE TestGenerator TestGenerator/*)
file(GLOB_RECURSE TransfersTests transfers_tests/*)
file(GLOB_RECURSE UnitTests unit_tests/*)

source_group("" FILES ${CoreTests} ${CryptoTests} ${FunctionalTests} ${IntegrationTestLibrary} ${IntegrationTests} ${NodeRpcProxyTests} ${PerformanceTests} ${SystemTests} ${TestGenerator} ${TransfersTests} ${UnitTests})
source_group("" FILES ${CryptoNoteProtocol} ${P2p})

add_library(IntegrationTestLibrary ${IntegrationTestLibrary})
add_library(TestGenerator ${TestGenerator})

add_executable(CoreTests ${CoreTests})
add_executable(CryptoTests ${CryptoTests})
add_executable(IntegrationTests ${IntegrationTests})
add_executable(NodeRpcProxyTests ${NodeRpcProxyTests})
add_executable(PerformanceTests ${PerformanceTests})
add_executable(SystemTests ${SystemTests})
add_executable(TransfersTests ${TransfersTests})
add_executable(UnitTests ${UnitTests})

add_executable(DifficultyTests difficulty/difficulty.cpp)
add_executable(HashTargetTests hash-target.cpp)
add_executable(HashTests hash/main.cpp)

target_link_libraries(CoreTests epee TestGenerator CryptoNoteCore Logging Common Crypto ${Boost_LIBRARIES})
target_link_libraries(IntegrationTests epee IntegrationTestLibrary Wallet NodeRpcProxy InProcessNode P2P Rpc Http Transfers Serialization System CryptoNoteCore Logging Common Crypto gtest upnpc-static ${Boost_LIBRARIES})
target_link_libraries(NodeRpcProxyTests epee NodeRpcProxy CryptoNoteCore Rpc Http Serialization System Logging Common Crypto ${Boost_LIBRARIES})
target_link_libraries(PerformanceTests epee CryptoNoteCore Logging Common Crypto ${Boost_LIBRARIES})
target_link_libraries(SystemTests System gtest_main)
if (MSVC)
  target_link_libraries(SystemTests ws2_32)
endif ()

target_link_libraries(TransfersTests IntegrationTestLibrary Wallet epee gtest_main CryptoNoteCore InProcessNode NodeRpcProxy P2P Rpc Http Serialization System Transfers Logging Common Crypto upnpc-static ${Boost_LIBRARIES})
target_link_libraries(UnitTests epee gtest_main Wallet TestGenerator CryptoNoteCore InProcessNode Transfers Serialization System Logging Common Crypto ${Boost_LIBRARIES})

target_link_libraries(DifficultyTests epee CryptoNoteCore Crypto Logging Common ${Boost_LIBRARIES})
target_link_libraries(HashTargetTests epee CryptoNoteCore Crypto)
target_link_libraries(HashTests Crypto)

if(NOT MSVC)
  set_property(TARGET gtest gtest_main IntegrationTestLibrary IntegrationTests TestGenerator UnitTests APPEND_STRING PROPERTY COMPILE_FLAGS " -Wno-undef -Wno-sign-compare")
endif()

add_custom_target(tests DEPENDS CoreTests IntegrationTests NodeRpcProxyTests PerformanceTests SystemTests TransfersTests UnitTests DifficultyTests HashTargetTests)

set_property(TARGET
  tests

  IntegrationTestLibrary
  TestGenerator

  CoreTests
  CryptoTests
  IntegrationTests
  NodeRpcProxyTests
  PerformanceTests
  SystemTests
  TransfersTests
  UnitTests

  DifficultyTests
  HashTargetTests
  HashTests
PROPERTY FOLDER "tests")

add_dependencies(IntegrationTestLibrary version)

set_property(TARGET CoreTests PROPERTY OUTPUT_NAME "core_tests")
set_property(TARGET CryptoTests PROPERTY OUTPUT_NAME "crypto_tests")
set_property(TARGET IntegrationTests PROPERTY OUTPUT_NAME "integration_tests")
set_property(TARGET NodeRpcProxyTests PROPERTY OUTPUT_NAME "node_rpc_proxy_tests")
set_property(TARGET PerformanceTests PROPERTY OUTPUT_NAME "performance_tests")
set_property(TARGET SystemTests PROPERTY OUTPUT_NAME "system_tests")
set_property(TARGET TransfersTests PROPERTY OUTPUT_NAME "transfers_tests")
set_property(TARGET UnitTests PROPERTY OUTPUT_NAME "unit_tests")
set_property(TARGET DifficultyTests PROPERTY OUTPUT_NAME "difficulty_tests")
set_property(TARGET HashTargetTests PROPERTY OUTPUT_NAME "hash_target_tests")
set_property(TARGET HashTests PROPERTY OUTPUT_NAME "hash_tests")

add_test(CoreTests core_tests --generate_and_play_test_data)
add_test(CryptoTests crypto_tests ${CMAKE_CURRENT_SOURCE_DIR}/crypto/tests.txt)
add_test(DifficultyTests difficulty_tests ${CMAKE_CURRENT_SOURCE_DIR}/difficulty/data.txt)
foreach(hash IN ITEMS fast slow tree extra-blake extra-groestl extra-jh extra-skein)
  add_test(hash-${hash} hash_tests ${hash} ${CMAKE_CURRENT_SOURCE_DIR}/hash/tests-${hash}.txt)
endforeach(hash)
add_test(HashTargetTests hash_target_tests)
add_test(SystemTests system_tests)
add_test(UnitTests unit_tests)
