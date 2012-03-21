
#include <test.h>

#ifdef ENABLE_TESTS

#include <gtest/gtest.h>
#include <iostream>

using namespace std;

void run_tests()
{
    int argc = 1;
    char *argv[1];
    char name[] = "none";
    bool tests_failed = 0;

    argv[0] = name;

    ::testing::InitGoogleTest(&argc, argv);
    tests_failed = RUN_ALL_TESTS();
    cout << "Testing " << (tests_failed ? "failed\n" : "passed\n");

    exit(0);
}

#else

void run_tests()
{
    return;
}

#endif
