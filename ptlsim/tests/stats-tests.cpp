
#include <gtest/gtest.h>

// We disable Assert of Simulator
#define DISABLE_ASSERT
#include <ptlsim.h>
#include <statsBuilder.h>

namespace {

    class TestStat : public Statable {
        public:
            StatArray<W64, 10> arr1;
            StatObj<W64> ct1;

            TestStat() : Statable("test")
                         , arr1("arr1", this)
                         , ct1("ct1", this)
        {}
    };

    TEST(Stats, StatArray) {

        TestStat st;

        st.arr1.set_default_stats(n_user_stats);

        foreach(i, 10) {
            ASSERT_EQ(st.arr1[i], 0);
        }

        foreach(i, 20) {
            foreach(i, 10) {
                if(i % 2) {
                    st.arr1[i]++;
                }
            }
        }

        st.arr1.set_default_stats(n_kernel_stats);

        foreach(i, 10) {
            ASSERT_EQ(st.arr1[i], 0);
        }

        st.arr1.set_default_stats(n_user_stats);

        foreach(i, 10) {
            if(i%2)
                ASSERT_EQ(st.arr1[i], 20);
            else
                ASSERT_EQ(st.arr1[i], 0);
        }

        // Test the operators
        W64* u_pt_i0 = &st.arr1[0];

        StatArray<W64, 10>::BaseArr& barr = st.arr1(n_user_stats);

        foreach(i, 10) {
            ASSERT_EQ(u_pt_i0, &barr[i]);
            ASSERT_EQ(u_pt_i0, &st.arr1[i]);
            ASSERT_EQ(*u_pt_i0, barr[i]);
            u_pt_i0++;
        }

        // Test ct1
        st.ct1.set_default_stats(n_user_stats);
        foreach(i, 20) {
            st.ct1++;
        }

        ASSERT_EQ(st.ct1(n_user_stats), 20);

        st.ct1 += 20;
        ASSERT_EQ(st.ct1(n_user_stats), 40);

        foreach(i, 19)
            st.ct1--;
        ASSERT_EQ(st.ct1(n_user_stats), 21);
    }

};
