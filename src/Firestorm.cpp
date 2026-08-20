/**
 * @file
 *
 * FIRE STORM entry point
 *
 * @author Oliver Dixon
 * @date 2026-08-20
 */

#include "Driver.hpp"

int main()
{
    using namespace firestorm;

    Driver driver;
    driver.run({ "./file.pdf" });


    return 0;
}
