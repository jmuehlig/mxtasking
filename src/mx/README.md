# How to use `MxTasking`

## Build a simple _Hello World_ task
Every task inherits from `mx::tasking::TaskInterface` and implements the `execute` method, which is called when the task gets executed by the runtime.

    #include <mx/tasking/task.h>
    #include <iostream>
    class HelloWorldTask : public mx::tasking::TaskInterface
    {
    public:
        HelloWorldTask() = default;
        virtual ~HelloWorldTask() = default;
        
        virtual TaskInterface *execute(const std::uint16_t, const std::uint16_t)
        {
            std::cout << "Hello world from MxTasking!" << std::endl;
            return nullptr;
        }
    };
    
## Run the _Hello World_ task

    #include <mx/tasking/runtime.h>
    
    int main()
    {
        // Define which cores will be used (1 core here).
        auto cores = mx::util::core_set::build(1);
        
        // Create an instance of the task with the current core as first
        // parameter (we assume that we start at core 0).
        auto *task = mx::tasking::runtime::new_task<HelloWorldTask>(0);
    
        // Create a runtime for the given cores.
        mx::tasking::runtime_guard runtime { cores };
        
        // Schedule the task.
        mx::tasking::runtime::spawn(*task);
        
        // Will print: "Hello world from MxTasking!"
        return 0;
    }