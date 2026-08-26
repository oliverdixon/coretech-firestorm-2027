/**
 * @file
 *
 * TestDecoder specification
 *
 * @author Oliver Dixon
 * @date 2026-08-26
 */

#include "../TestHarness.hpp"

using namespace firestorm::test;

namespace
{

void tmp(TestHarness& harness)
{
}

} // namespace

int main()
{
    return TestHarness::run({{.name = "tmp", .function = tmp}});
}
