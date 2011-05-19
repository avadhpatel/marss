
#include <gtest/gtest.h>

// We disable Assert of Simulator
#define DISABLE_ASSERT
#include <basecore.h>

#include <machine.h>

namespace {

    using namespace Core;

    // The Fixture for testing BaseMachine
    class BaseCoreMachineTest : public ::testing::Test {
        public:
            BaseMachine* base_machine;

            BaseCoreMachineTest()
            {
                base_machine = (BaseMachine*)(PTLsimMachine::getmachine("base"));
            }

            virtual void SetUp()
            {}

            virtual void TearDown()
            {
                base_machine->reset();
            }
    };

    TEST_F(BaseCoreMachineTest, BaseMachineCreated)
    {
        ASSERT_TRUE(base_machine);
        ASSERT_EQ(base_machine->initialized, 0);
    }

    TEST_F(BaseCoreMachineTest, Reset)
    {
        base_machine->reset();

        ASSERT_TRUE(base_machine->context_used.iszero());
        ASSERT_EQ(base_machine->context_counter, 0);
        ASSERT_EQ(base_machine->coreid_counter, 0);

        ASSERT_EQ(base_machine->cores.count(), 0);
    }

    TEST_F(BaseCoreMachineTest, GetNextContext)
    {
        base_machine->reset();

        foreach(i, NUM_SIM_CORES) {
            Context& ctx = base_machine->get_next_context();
            ASSERT_TRUE(&ctx) << "No context " << i << " found";
            ASSERT_EQ(ctx.cpu_index, i);
        }
    }

}; // namespace
