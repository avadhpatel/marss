
#include <gtest/gtest.h>

#define DISABLE_ASSERT
#include <ptlsim.h>
#include <ptl-qemu.h>
#include <superstl.h>

void read_simpoint_file();
int get_simpoint(int id);
int get_simpoint_label(int id);
void add_simpoint(int point, int label);
void set_next_simpoint(CPUX86State* ctx);
void clear_simpoints(void);

namespace {

    TEST(Simpoint, ReadingFile)
    {
        int point;
        ofstream of("/tmp/test_simpoint");

        foreach (i, 10) {
            of << i * 10 << " " << (i + 1) % 10 << endl;
        }

        of.close();

        config.simpoint_file = "/tmp/test_simpoint";

        clear_simpoints();
        read_simpoint_file();

        foreach (i, 10) {
            point = get_simpoint(i);
            ASSERT_EQ(i * 10, point);
        }
    }

    TEST(Simpoint, SimpointInterval)
    {
        /* Test default interval size to 10mil */
        ASSERT_EQ(10000000, config.simpoint_interval);
    }

    TEST(Simpoint, SetNextSimpoint)
    {
        Context& ctx = contextof(0);

        clear_simpoints();
        ofstream of("/tmp/test_simpoint");
        of << "320 0\n";
        of << "20 1\n";
        of << "10 2\n";
        of << "3220 3\n";

        of.close();

        config.simpoint_file = "/tmp/test_simpoint";
        read_simpoint_file();

        ASSERT_EQ(10, get_simpoint(0));
        ASSERT_EQ(20, get_simpoint(1));
        ASSERT_EQ(320, get_simpoint(2));
        ASSERT_EQ(3220, get_simpoint(3));

        set_next_simpoint(&ctx);
        ASSERT_EQ(10 * config.simpoint_interval, ctx.simpoint_decr);

        set_next_simpoint(&ctx);
        ASSERT_EQ(10 * config.simpoint_interval, ctx.simpoint_decr);

        set_next_simpoint(&ctx);
        ASSERT_EQ(300 * config.simpoint_interval, ctx.simpoint_decr);

        set_next_simpoint(&ctx);
        ASSERT_EQ(2900 * config.simpoint_interval, ctx.simpoint_decr);
    }

    TEST(Simpoint, ChkName)
    {
        stringbuf* name;
        Context& ctx = contextof(0);

        clear_simpoints();
        add_simpoint(10, 0);
        add_simpoint(20, 1);

        set_next_simpoint(&ctx);
        name = get_simpoint_chk_name();
        EXPECT_STREQ("simpoint_sp_0", name->buf);
        delete name;

        set_next_simpoint(&ctx);
        name = get_simpoint_chk_name();
        EXPECT_STREQ("simpoint_sp_1", name->buf);
        delete name;

        clear_simpoints();
        add_simpoint(10, 1);
        add_simpoint(20, 0);
        config.simpoint_chk_name = "test";

        set_next_simpoint(&ctx);
        name = get_simpoint_chk_name();
        EXPECT_STREQ("test_sp_1", name->buf);
        delete name;

        set_next_simpoint(&ctx);
        name = get_simpoint_chk_name();
        EXPECT_STREQ("test_sp_0", name->buf);
        delete name;
    }
};
