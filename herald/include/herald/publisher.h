#pragma once

/// \addtogroup API
/// @{

#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

  struct publisher_t;

  /// Return code for publisher functions.
  enum publisher_error {
    /// Operation was successful.
    PUB_OK = 0,

    /// Could not create and bind to the socket.
    PUB_NOSOCKET,

    /// Message exceeds buffer size and cannot be published.
    PUB_TOOLARGE,

    /// Attempted to publish message on unititialized publisher.
    PUB_NOTRUNNING
  };

  /// Create a publisher. Does not initialize server until \ref publisher_init is called.
  ///
  /// NOTE: should not be freed, use \ref publisher_destroy to shutdown and cleanup the
  /// publisher.
  ///
  /// \param port the port to bind the server which accepts subscriber connections.
  /// \param buffer_size the maximum allowed size of messages to be published.
  /// \return an uninitialized publisher handle.
  publisher_t *publisher_create(const int port, const size_t buffer_size);

  /// Destroy a publisher. If it was initialized, it will close server connection and disconnect
  /// all connected clients as well as destroying all shared memory regions.
  ///
  /// \param publisher the publisher handle to destroy.
  void publisher_destroy(publisher_t *publisher);

  /// Initialize a publisher. This will bind to the configured tcp port and start accepting
  /// connections from subscribers.
  ///
  /// \param publisher the publisher to initialize.
  /// \return PUB_OK if initialization was successful or an errcode if not.
  publisher_error publisher_init(publisher_t *publisher);

  /// Publish a message to all subscribers.
  ///
  /// NOTE: This function will queue up the message to be published, and will return immediately.
  /// If an error occurs while the message was being published to subscriber(s) then there will
  /// be no way to detect it. Consider this a best effort.
  ///
  /// \param publisher the publisher that will publish this message to is subscribers.
  /// \param data the payload to publish.
  /// \param length the length of the message payload.
  /// \return PUB_OK if the message was valid and received by the publisher.
  publisher_error publisher_publish(publisher_t *publisher, const void* data, const size_t length);

#ifdef __cplusplus
} //end extern "C"
#endif

/// @}
