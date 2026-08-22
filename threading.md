We don't need to control CTFSPackets, since they're buffered quickly.

There might be latency when forwarding ethernet frames to the sinks.  The sinks
should each be on a separate thread.

They work with Ethernet frame views which refer to the buffer in the traffic
decoder.  So a mutex will be needed to protect that data.

In practice, this means that compaction is delayed until the traffic decoder can
get the mutex.

Implementation can have a jthread in the sink class on which the send call runs.

The ethernet packet view should share a shared mutex for which the compaction
algorithms waits, while sending is in progress.

There is an issue of back pressure, where a send can prevent compaction for an
arbitrarily long amount of time, causing the buffer to grow indefinitely.  So
there should be some fixed buffer size limit after which we stop accepting new
packets, OR have a time limit on the send call after which we give up and
release the lock.

