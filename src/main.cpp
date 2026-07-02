#include <iostream>
#include <cstdlib>

#include "Application.h"

int main()
{
    try 
    {
        Application application;
        application.run();
    }
    catch (const std::exception& e)
    {
        std::cerr << e.what() << "\n";
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}