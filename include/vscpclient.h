#ifndef VSCPCLIENT_H
#define VSCPCLIENT_H

#define VSCP_STATUS_GENERIC_ERROR  -1   // In case of TCP or DNS error the callback is called with this status.
#define BUFFER_SIZE_MAX            5000 // Size of http responses that will cause an error.

// The VSCP client is designed as a state machine

typedef enum vscp_state {
    vscp_state_virgin,              // no wifi
    vscp_state_wifi,                // Connected to wifi router
    vscp_state_not_connected,       // virgin state
    vscp_state_waiting_resolve,     // resolving hostname
    vscp_state_waiting_connect,     // Waiting for host to connect
    vscp_state_connected,           // connected to host
    // vscp_state_credentials_username,// user command sent - waiting for OK
    // vscp_state_credentials_password,// pass command sent - waiting for OK
    // vscp_state_credentials_ok
} vscp_state;

typedef enum vscp_sub_state {
    vscp_sub_state_none,
    vscp_sub_state_command_sent,    // Command has been sent, waiting for response
    vscp_sub_state_ok,              // Command success
    vscp_sub_state_error,           // command error
    vscp_sub_state_timeout          // timeout waiting for command respons
} vscp_sub_state;


/*
 * "full_response" is a string containing all response headers and the response body.
 * "response_body and "http_status" are extracted from "full_response" for convenience.
 *
 * A successful request corresponds to an HTTP status code of 200 (OK).
 * More info at http://en.wikipedia.org/wiki/List_of_HTTP_status_codes
 */
typedef void (* vscp_callback)(char *response, 
                                int status, 
                                char *full_response);

/*
 * Download a web page from its URL.
 * Try:
 * http_get("http://wtfismyip.com/text", http_callback_example);
 */
void ICACHE_FLASH_ATTR http_get(const char *url, 
                                    const char *headers, 
                                    vscp_callback user_callback);

/*
 * Post data to a web form.
 * The data should be encoded as application/x-www-form-urlencoded.
 * Try:
 * http_post("http://httpbin.org/post", "first_word=hello&second_word=world", http_callback_example);
 */

/*
 * Call this function to skip URL parsing if the arguments are already in separate variables.
 */
void ICACHE_FLASH_ATTR vscp_command(const char *hostname, 
                                        int port, 
                                        bool secure, 
                                        const char *path, 
                                        const char *post_data, 
                                        const char *headers, 
                                        vscp_callback user_callback );

/*
 * Output on the UART.
 */
void vscp_callback_example(char *response, 
                            int status, 
                            char *full_response);

#endif