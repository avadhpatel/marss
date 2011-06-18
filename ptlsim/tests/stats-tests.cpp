
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
            StatObj<W64> ct2;
            StatObj<W64> ct3;
            StatString st1;
            StatString st2;

            TestStat() : Statable("test")
                         , arr1("arr1", this)
                         , ct1("ct1", this)
                         , ct2("ct2", this)
                         , ct3("ct3", this)
                         , st1("st1", this)
                         , st2("st2", this)
        {}
    };

    /* FIXME: for now, this test must be first because of
       the way StatBuilder keeps its state; the other
       tests add fields which wouldn't be there in a
       normal program so there are extra fields in the
       output
       */
    TEST(Stats, TimeStats) {
#include <sstream>
#define reset_stream(os) { os.str(""); }
        using std::ostringstream;

        StatsBuilder &builder = StatsBuilder::get();

        ostringstream os;
        TestStat st;
        Stats *time_stats = builder.get_new_stats();
        st.ct1.set_default_stats(n_kernel_stats);
        st.ct2.set_default_stats(n_global_stats);
        st.ct3.set_default_stats(n_global_stats);
        st.ct1.set_time_stats(time_stats);
        st.ct2.set_time_stats(time_stats);
        st.ct3.set_time_stats(time_stats);

        builder.dump_header(os);
        ASSERT_STREQ(os.str().c_str(), "sim_cycle,test.ct1,test.ct2,test.ct3\n");
        reset_stream(os);

        st.ct1++;
        builder.dump_periodic(os,0);
        ASSERT_STREQ(os.str().c_str(), "0,1,0,0\n");
        reset_stream(os);

        st.ct1.set_default_stats(n_user_stats);
        st.ct1 += 30;
        builder.dump_periodic(os,100);
        ASSERT_STREQ(os.str().c_str(), "100,30,0,0\n");
        reset_stream(os);

        st.ct2 += 19;
        st.ct2++;
        builder.dump_periodic(os,200);
        ASSERT_STREQ(os.str().c_str(), "200,0,20,0\n");
        reset_stream(os);

        st.ct1 += 1;
        st.ct2++;
        builder.dump_periodic(os,300);
        ASSERT_STREQ(os.str().c_str(), "300,1,1,0\n");
        reset_stream(os);

    }

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
