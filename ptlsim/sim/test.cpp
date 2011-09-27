
#include <test.h>

#ifdef ENABLE_TESTS

#include <gtest/gtest.h>

void run_tests()
{
    int argc = 1;
    char *argv[1];
    char name[] = "none";
    bool tests_passed = 0;

    argv[0] = name;

    ::testing::InitGoogleTest(&argc, argv);
    tests_passed = RUN_ALL_TESTS();

    exit(0);
}

#else

void run_tests()
{
    return;
}

#endif
