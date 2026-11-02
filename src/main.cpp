#include "Application.h"
#include "LifecycleTests.h"
#include <string>

int main(int argc, char* argv[])
{
    if (argc == 2 && std::string(argv[1]) == "--lifecycle-tests")
        return RunLifecycleTests();

    Application application;
    application.Run();
    return 0;
}
