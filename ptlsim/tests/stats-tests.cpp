
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
            StatString st1;
            StatString st2;

            TestStat() : Statable("test")
                         , arr1("arr1", this)
                         , ct1("ct1", this)
                         , st1("st1", this)
                         , st2("st2", this)
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

    TEST(Stats, StatString) {
        TestStat st;

        st.set_default_stats(n_user_stats);

        st.st1 = "String Test Assignment";
        st.st2 = "string,test,assignment";
        st.st2.set_split(",");

        ASSERT_STREQ(st.st1(n_user_stats), "String Test Assignment");
        ASSERT_STREQ(st.st2(n_user_stats), "string,test,assignment");

        {
            YAML::Emitter out;
            out << YAML::BeginMap;
            out = st.st1.dump(out, n_user_stats);
            out << YAML::EndMap;

            ASSERT_TRUE(out.good());
            ASSERT_STREQ(out.c_str(), "---\nst1: String Test Assignment");
        }

        {
            YAML::Emitter out;
            out << YAML::BeginMap;
            out = st.st2.dump(out, n_user_stats);
            out << YAML::EndMap;

            ASSERT_TRUE(out.good());
            ASSERT_STREQ(out.c_str(), "---\nst2: [string, test, assignment]");
        }

        {
            st.st2 = "string test assignment";
            st.st2.set_split(" ");

            YAML::Emitter out;
            out << YAML::BeginMap;
            out = st.st2.dump(out, n_user_stats);
            out << YAML::EndMap;

            ASSERT_TRUE(out.good());
            ASSERT_STREQ(out.c_str(), "---\nst2: [string, test, assignment]");
        }
    }

};
