#include <iostream>
#include <mx/tasking/runtime.h>

class HelloWorldTask : public mx::tasking::TaskInterface
{
public:
    constexpr HelloWorldTask() = default;
    ~HelloWorldTask() override = default;

    mx::tasking::TaskResult execute(const std::uint16_t /*core_id*/, const std::uint16_t /*channel_id*/) override
    {
        std::cout << "Hello World" << std::endl;

        // Stop MxTasking runtime after this task.
        return mx::tasking::TaskResult::make_stop();
    }
};

int main()
{
    // Define which cores will be used (1 core here).
    const auto cores = mx::util::core_set::build(1);

    { // Scope for the MxTasking runtime.

        // Create a runtime for the given cores.
        mx::tasking::runtime_guard _{cores};

        // Create an instance of the HelloWorldTask with the current core as first
        // parameter. The core is required for memory allocation.
        auto *hello_world_task = mx::tasking::runtime::new_task<HelloWorldTask>(cores.front());

        // Annotate the task to run on the first core.
        hello_world_task->annotate(cores.front());

        // Schedule the task.
        mx::tasking::runtime::spawn(*hello_world_task);
    }

    return 0;
}