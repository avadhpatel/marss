
#include <gtest/gtest.h>

#define DISABLE_ASSERT
#include <ptlsim.h>
#include <logic.h>

namespace {

    /* Test FullyAssociativeTags16bit */
    TEST(Logic, AssocTags16bit)
    {
        const int size = 64;
        FullyAssociativeTags16bit<size, size> tags;

        int chunks = tags.chunkcount;
        int pads = tags.padchunkcount;
        ASSERT_EQ(chunks, 8);
        ASSERT_EQ(pads, 8);

        /* Check default value is -1 */
        foreach(i, size) {
            W16 val = tags[i];
            ASSERT_EQ(val, (W16(-1)));
            ASSERT_FALSE(tags.isvalid(i));
        }

        /* Test insert */
        foreach(i, 8) {
            tags.insert(i);
        }
        foreach(i, 8) {
            W16 val = tags[i];
            ASSERT_EQ(val, (W16(i)));
        }

        /* now we remove 3rd entry */
        tags.collapse(2);
        for(int i=2; i < 7; i++) {
            W16 val = tags[i];
            ASSERT_EQ((W16(i+1)), val) << "Tags: " << tags;
        }
    }

    /* Test simulation freq related functions */
    TEST(Sim, SimFreq)
    {
#define TEST_NS_TO_CYCLE(freq, ns, expected) \
        config.core_freq_hz = freq; \
        ASSERT_EQ(expected, ns_to_simcycles(ns));

        TEST_NS_TO_CYCLE(1e9, 10, 10);
        TEST_NS_TO_CYCLE(2.8e9, 20, 56);
        TEST_NS_TO_CYCLE(3e9, 1, 3);
        TEST_NS_TO_CYCLE(3e9, 50, 150);
#undef TEST_NS_TO_CYCLE
    }

    TEST(Sim, InvalidTag)
    {
        W64 invalid = InvalidTag<W64>::INVALID;
        ASSERT_EQ(-1, invalid);
    }
};
