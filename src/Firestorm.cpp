/**
 * @file
 *
 * FIRE STORM entry point
 *
 * @author Oliver Dixon
 * @date 2026-08-20
 */

#include <iostream>
#include <ostream>

#include "TCPConnection.hpp"

int main()
{
    using namespace firestorm;

    TCPConnection incoming(5733);
    std::println(std::cout, "CONNECTED");

    return 0;
}
