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
 * Interface for routing decoded Ethernet frames.
 *
 * Implementations determine the destinations applicable to each frame and arrange its delivery according to an
 * implementation-defined routing policy. Delivery may occur asynchronously.
 *
 * After submitting the final frame, callers should invoke @ref drain_and_report to await outstanding delivery work,
 * report statistics, and observe propagated failures.
 */
class IFrameRouter
{
public:
    /**
     * Destroys the router.
     */
    virtual ~IFrameRouter() = default;

    /**
     * Routes a decoded Ethernet frame.
     *
     * The implementation determines the applicable endpoint destinations. If it retains the frame after the function
     * returns, it must first copy the referenced bytes into owned storage.
     *
     * @param view Non-owning view of the decoded Ethernet frame.
     *
     * @note Storage referenced by @p view need only remain valid for the duration of this call.
     */
    virtual void route(const EthernetFrameView& view) = 0;

    /**
     * Waits for outstanding deliveries and reports routing statistics.
     *
     * Implementations wait for all frames accepted by preceding calls to @ref route and report the resulting routing
     * and delivery statistics to the command line.
     */
    virtual void drain_and_report() = 0;
};

} // namespace firestorm

#endif // FIRESTORM_IFRAMEROUTER_HPP
