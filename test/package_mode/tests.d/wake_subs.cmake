# Post-wake subscription retry backoff (Qt Core only).
add_executable(wake_subscription_retry_test
    ${CMAKE_CURRENT_LIST_DIR}/../wake_subscription_retry_test.cpp
    ${PROXOR_SRC}/platform/WakeSubscriptionRetry.cpp)
target_include_directories(wake_subscription_retry_test PRIVATE ${PROXOR_SRC})
target_link_libraries(wake_subscription_retry_test PRIVATE Qt6::Core Qt6::Test)
add_test(NAME wake_subscription_retry_test COMMAND wake_subscription_retry_test)
