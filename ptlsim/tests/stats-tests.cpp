
#include <gtest/gtest.h>

// We disable Assert of Simulator
#define DISABLE_ASSERT
#include <ptlsim.h>
#include <statsBuilder.h>

#include <sstream>
#define reset_stream(os) { os.str(""); }
        using std::ostringstream;

namespace {

    class TestStat : public Statable {
        public:
            StatArray<W64, 10> arr1;
            StatObj<W64> ct1;
            StatObj<W64> ct2;
            StatObj<W64> ct3;
            StatString st1;
            StatString st2;
            StatEquation<W64, W64, StatObjFormulaAdd> sum;
            StatEquation<W64, double, StatObjFormulaDiv> div;
            StatArray<W64, 3> time_arr;

            TestStat() : Statable("test")
                         , arr1("arr1", this)
                         , ct1("ct1", this)
                         , ct2("ct2", this)
                         , ct3("ct3", this)
                         , st1("st1", this)
                         , st2("st2", this)
                         , sum("sum", this)
                         , div("div", this)
                         , time_arr("time_arr", this)
            {
                sum.add_elem(&ct1);
                sum.add_elem(&ct2);

                div.add_elem(&ct1);
                div.add_elem(&ct2);
            }
    };

    /* FIXME: for now, this test must be first because of
       the way StatBuilder keeps its state; the other
       tests add fields which wouldn't be there in a
       normal program so there are extra fields in the
       output
       */
    TEST(Stats, TimeStats) {
        StatsBuilder &builder = StatsBuilder::get();
		builder.delete_nodes();

        ostringstream os;
        TestStat st;
        builder.init_timer_stats();

        st.ct1.set_default_stats(kernel_stats);
        st.ct2.set_default_stats(user_stats);
        st.ct3.set_default_stats(user_stats);
        st.time_arr.set_default_stats(user_stats);
        st.ct3.enable_periodic_dump();
        st.sum.enable_periodic_dump();
        st.time_arr.enable_periodic_dump();

        ASSERT_TRUE(st.ct1.is_dump_periodic());
        ASSERT_TRUE(st.ct2.is_dump_periodic());
        ASSERT_TRUE(st.ct3.is_dump_periodic());
        ASSERT_TRUE(st.time_arr.is_dump_periodic());
        ASSERT_TRUE(st.is_dump_periodic());
        ASSERT_TRUE(builder.is_dump_periodic());
        ASSERT_TRUE(st.sum.is_dump_periodic());
        ASSERT_FALSE(st.div.is_dump_periodic());

        builder.dump_header(os);
        ASSERT_STREQ(os.str().c_str(), "sim_cycle,test.ct1,test.ct2,test.ct3,test.sum,test.time_arr.0,test.time_arr.1,test.time_arr.2\n");
        reset_stream(os);

        st.ct1++;
        builder.dump_periodic(os,0);
        ASSERT_STREQ(os.str().c_str(), "0,1,0,0,1,0,0,0\n");
        reset_stream(os);

        st.ct1.set_default_stats(user_stats);
        st.ct1 += 30;
        builder.dump_periodic(os,100);
        ASSERT_STREQ(os.str().c_str(), "100,30,0,0,30,0,0,0\n");
        reset_stream(os);

        st.ct2 += 19;
        st.ct2++;
        st.time_arr[1] += 5;
        builder.dump_periodic(os,200);
        ASSERT_STREQ(os.str().c_str(), "200,0,20,0,20,0,5,0\n");
        reset_stream(os);

        st.ct1 += 1;
        st.ct2++;
        st.time_arr[0] += 10;
        st.time_arr[1]++;
        builder.dump_periodic(os,300);
        ASSERT_STREQ(os.str().c_str(), "300,1,1,0,2,10,1,0\n");
        reset_stream(os);

    }

    TEST(Stats, StatArray) {

        TestStat st;

        st.arr1.set_default_stats(user_stats);

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

        st.arr1.set_default_stats(kernel_stats);

        foreach(i, 10) {
            ASSERT_EQ(st.arr1[i], 0);
        }

        st.arr1.set_default_stats(user_stats);

        foreach(i, 10) {
            if(i%2)
                ASSERT_EQ(st.arr1[i], 20);
            else
                ASSERT_EQ(st.arr1[i], 0);
        }

        // Test the operators
        W64* u_pt_i0 = &st.arr1[0];

        StatArray<W64, 10>::BaseArr& barr = st.arr1(user_stats);

        foreach(i, 10) {
            ASSERT_EQ(u_pt_i0, &barr[i]);
            ASSERT_EQ(u_pt_i0, &st.arr1[i]);
            ASSERT_EQ(*u_pt_i0, barr[i]);
            u_pt_i0++;
        }

        // Test ct1
        st.ct1.set_default_stats(user_stats);
        foreach(i, 20) {
            st.ct1++;
        }

        ASSERT_EQ(st.ct1(user_stats), 20);

        st.ct1 += 20;
        ASSERT_EQ(st.ct1(user_stats), 40);

        foreach(i, 19)
            st.ct1--;
        ASSERT_EQ(st.ct1(user_stats), 21);
    }

    TEST(Stats, StatString) {
        TestStat st;

        st.set_default_stats(user_stats);

        st.st1 = "String Test Assignment";
        st.st2 = "string,test,assignment";
        st.st2.set_split(",");

        ASSERT_STREQ(st.st1(user_stats), "String Test Assignment");
        ASSERT_STREQ(st.st2(user_stats), "string,test,assignment");

        {
            YAML::Emitter out;
            out << YAML::BeginMap;
            out = st.st1.dump(out, user_stats);
            out << YAML::EndMap;

            ASSERT_TRUE(out.good());
            ASSERT_STREQ(out.c_str(), "---\nst1: String Test Assignment");
        }

        {
            YAML::Emitter out;
            out << YAML::BeginMap;
            out = st.st2.dump(out, user_stats);
            out << YAML::EndMap;

            ASSERT_TRUE(out.good());
            ASSERT_STREQ(out.c_str(), "---\nst2: [string, test, assignment]");
        }

        {
            st.st2 = "string test assignment";
            st.st2.set_split(" ");

            YAML::Emitter out;
            out << YAML::BeginMap;
            out = st.st2.dump(out, user_stats);
            out << YAML::EndMap;

            ASSERT_TRUE(out.good());
            ASSERT_STREQ(out.c_str(), "---\nst2: [string, test, assignment]");
        }
    }

    TEST(Stats, ObjFormulaAdd) {

        TestStat st;

        st.ct1.set_default_stats(user_stats);
        st.ct2.set_default_stats(user_stats);
        st.ct3.set_default_stats(user_stats);

        /* Test Add */
        foreach(i, 100) {
            st.ct1++;
            st.ct2++;
        }

        YAML::Emitter out;
        out << YAML::BeginMap;
        out = st.sum.dump(out, user_stats);
        out << YAML::EndMap;

        W64 res = st.sum(user_stats);
        ASSERT_EQ(res, 200);

        ASSERT_TRUE(out.good());
        ASSERT_STREQ(out.c_str(), "---\nsum: 200");
    }

    TEST(Stats, ObjFormulaDiv) {

        TestStat st;

        st.ct1.set_default_stats(user_stats);
        st.ct2.set_default_stats(user_stats);

        /* Test Add */
        foreach(i, 100) {
            st.ct1++;
        }

        foreach(i, 3) {
            st.ct2++;
        }

        YAML::Emitter out;
        out << YAML::BeginMap;
        out = st.div.dump(out, user_stats);
        out << YAML::EndMap;

        double res = st.div(user_stats);
        ASSERT_EQ(res, double(100)/double(3));

        ASSERT_TRUE(out.good());
        ASSERT_STREQ(out.c_str(), "---\ndiv: 33.3333");
    }

    TEST(Stats, Summary) {
        StatsBuilder &builder = StatsBuilder::get();

        ostringstream os;
        TestStat st;

        st.ct1.set_default_stats(kernel_stats);
        st.ct2.set_default_stats(user_stats);
        st.ct1.enable_summary();
        st.ct2.enable_summary();

        ASSERT_TRUE(st.ct1.is_summarize_enabled());
        ASSERT_TRUE(st.ct2.is_summarize_enabled());

        foreach (i, 10) {
            st.ct1++;
            st.ct2++;
        }

        builder.dump_summary(os);
        stringbuf test_str;
        test_str << "user.test.ct1 = 0\n";
        test_str << "user.test.ct2 = 10\n";
        test_str << "kernel.test.ct1 = 10\n";
        test_str << "kernel.test.ct2 = 0\n";
        test_str << "total.test.ct1 = 0\n";
        test_str << "total.test.ct2 = 0\n";

        ASSERT_STREQ(os.str().c_str(), test_str.buf);
    }

	TEST(Stats, GetStat) {
        StatsBuilder &builder = StatsBuilder::get();
		builder.delete_nodes();
		user_stats->reset();
		kernel_stats->reset();

        TestStat st;
		st.ct1.set_default_stats(user_stats);

        foreach (i, 10) {
            st.ct1++;
        }

		StatObj<W64>* m_ct1 = (StatObj<W64>*)builder.get_stat_obj("test:ct1");
		ASSERT_EQ(m_ct1, &st.ct1);

		W64 ct1_val = (W64&)(*m_ct1)(user_stats);

		ASSERT_EQ(ct1_val, 10);
	}
};
