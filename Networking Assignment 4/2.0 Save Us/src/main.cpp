#include "engine/window.hpp"
#include "engine/opengl.hpp"
#include "game/game.hpp"
#include <iostream>

int main()
{
    char serviceID = 's';

    std::cout << "Enter 's' to play as server or 'c' to play as client"<< std::endl;
    std::cin >> serviceID;

    bool server = true;

    if (serviceID == 'c')
        server = false;

    game::instance().create(server);
    bool exit = false;
    do {
        exit = !game::instance().update();
    } while (!exit);
    game::instance().destroy();
    return 0;
}
