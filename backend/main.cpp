#include <drogon/drogon.h>

#include "controllers/DbTestController.hpp"

int main()
{
    drogon::app().loadConfigFile("backend/config/config.json");
    registerDbTestRoutes();
    drogon::app().run();
    return 0;
}
