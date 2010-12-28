
#include <gtest/gtest.h>

// We disable Assert of Simulator
#define DISABLE_ASSERT
#include <basecore.h>

#define INSIDE_DEFCORE
#include <defcore.h>

namespace {

    using namespace Core;

    // The Fixture for testing BaseCoreMachine
    class BaseCoreMachineTest : public ::testing::Test {
        public:
            BaseCoreMachine* base_machine;

            BaseCoreMachineTest()
            {
                base_machine = (BaseCoreMachine*)(PTLsimMachine::getmachine("base"));
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

    TEST_F(BaseCoreMachineTest, DefaultInitialized)
    {
        // We setup the config for default core and create a new machine
        config.core_config = "default";

        base_machine->reset();

        base_machine->init(config);

        ASSERT_TRUE(base_machine->context_used.allset());
        ASSERT_EQ(base_machine->context_counter, MAX_CONTEXTS);
        ASSERT_EQ(base_machine->context_counter, NUM_SIM_CORES);

        ASSERT_EQ(base_machine->cores.count(), NUM_SIM_CORES);

        foreach(i, NUM_SIM_CORES) {
            DefaultCoreModel::DefaultCore* core =
                (DefaultCoreModel::DefaultCore*)base_machine->cores[i];

            ASSERT_EQ(core->threadcount, 1);
        }

        ASSERT_TRUE(base_machine->memoryHierarchyPtr) <<
            "MemoryHierarchy not created";
    }

    TEST_F(BaseCoreMachineTest, HT_SMTInitialized)
    {
        if(NUM_SIM_CORES % 4 != 0) {
            EXPECT_EQ(NUM_SIM_CORES % 4, 0) <<
                "To test ht-smt config, NUM_SIM_CORES must be in " \
                "multiplication of 4";
            return;
        }

        // We setup the config for default core and create a new machine
        config.core_config = "ht-smt";

        base_machine->reset();

        base_machine->init(config);

        ASSERT_TRUE(base_machine->context_used.allset());
        ASSERT_EQ(base_machine->context_counter, MAX_CONTEXTS);

        ASSERT_EQ(base_machine->cores.count(), (NUM_SIM_CORES / 4) * 3);

        foreach(i, base_machine->cores.count()) {
            DefaultCoreModel::DefaultCore* core =
                (DefaultCoreModel::DefaultCore*)base_machine->cores[i];

            if(i % 3 == 0) {
                // First core must have two threads
                ASSERT_EQ(core->threadcount, 2);
            } else {
                ASSERT_EQ(core->threadcount, 1);
            }
        }

        ASSERT_TRUE(base_machine->memoryHierarchyPtr) <<
            "MemoryHierarchy not created";
    }

}; // namespace
