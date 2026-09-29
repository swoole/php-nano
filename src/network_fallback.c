#include "php.h"
#include "main/php_network.h"

#include <string.h>

/*
 * The plain file wrapper uses php_socket_strerror() for errno formatting even
 * when socket transports are not selected. main/network.c supplies the
 * upstream implementation when the network component is active; this small
 * host adapter preserves that PHPAPI boundary for file-only builds.
 */
#ifndef PHP_NANO_STREAM_TRANSPORT
#ifdef PHP_WIN32
char *php_socket_strerror_s(long error, char *buffer, size_t buffer_size)
{
    if (buffer == NULL) {
        char temporary[256];
        if (strerror_s(temporary, sizeof(temporary), (errno_t) error) != 0) {
            return estrdup("Unknown error");
        }
        return estrdup(temporary);
    }
    if (buffer_size != 0 && strerror_s(buffer, buffer_size, (errno_t) error) != 0) {
        strncpy_s(buffer, buffer_size, "Unknown error", _TRUNCATE);
    }
    return buffer;
}
#endif

PHPAPI char *php_socket_strerror(long error, char *buffer, size_t buffer_size)
{
	const char *message = strerror((int) error);
	if (message == NULL) {
		message = "Unknown error";
	}
	if (buffer == NULL) {
		return estrdup(message);
	}
	if (buffer_size != 0) {
		strncpy(buffer, message, buffer_size);
		buffer[buffer_size - 1] = '\0';
	}
	return buffer;
}
#endif
