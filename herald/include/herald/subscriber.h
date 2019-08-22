#pragma once

/// \addtogroup API
/// @{

#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

  /// Return code for subscriber functions.
  enum subscriber_error {
    /// Operation was successful.
    SUB_OK = 0,

    /// Could not create and connect socket to publisher.
    SUB_NOSOCKET,

    /// Bad response from publisher, aborted.
    SUB_BADRESP,

    /// Could not initialize shared memory region.
    SUB_NOSHAREDMEM
  };

  struct subscriber_t;

  /// Function to be called when new data arrives from publisher.
  ///
  /// NOTE: The pointer to \p data is only valid for the lifetime of this function
  /// call, if it needs to last longer a copy should be made.
  ///
  /// \param data the buffer of data received.
  /// \param len the length of the buffer.
  typedef void (*callback_t)(const void *data, size_t len);

  /// Create an opaque subscriber handle. Does not initialize connection to publisher.
  ///
  /// NOTE: should not be freed, use \ref subscriber_destroy to shutdown and cleanup the
  /// subscriber.
  ///
  /// \param port the tcp port the publisher is running on.
  /// \param callback a callback function to be called when a new message is received.
  /// \return an unititialized subscriber handle.
  subscriber_t *subscriber_create(const int port, const callback_t callback);

  /// Destroy a subscriber. If it was initialized, it will close the remote connection
  /// to the publisher and cleanup the shared memory region.
  ///
  /// \param subscriber the subscriber handle to destroy.
  void subscriber_destroy(subscriber_t *subscriber);

  /// Initialize a subscriber. This will connect to the remote publisher and initialize
  /// the shared memory region.
  ///
  /// \param subscriber the subscriber handle to initialize.
  /// \return SUB_OK if initialization was successful or an errorcode if not.
  subscriber_error subscriber_init(subscriber_t *subscriber);

#ifdef __cplusplus
} //end extern "C"
#endif

/// @}
