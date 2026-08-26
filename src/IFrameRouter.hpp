/**
 * @file
 *
 * IFrameRouter specification
 *
 * @author Oliver Dixon
 * @date 2026-08-26
 */

#ifndef FIRESTORM_IFRAMEROUTER_HPP
#define FIRESTORM_IFRAMEROUTER_HPP

namespace firestorm
{

struct EthernetFrameView;

/**
 * @todo Document
 */
class IFrameRouter
{
public:
    virtual ~IFrameRouter() = default;

    virtual void route(const EthernetFrameView& view) = 0;

    virtual void drain_and_report() = 0;
};

} // namespace firestorm

#endif // FIRESTORM_IFRAMEROUTER_HPP
