/*!
    Test file
*/

#include "osapi.h"
#include "user_interface.h"
#include "espconn.h"
#include "mem.h"
#include "limits.h"
#include "vscpclient.h"


// Debug output.
#if 0
#define PRINTF(...) os_printf(__VA_ARGS__)
#else
#define PRINTF(...)
#endif



// Internal state.
typedef struct {
    char *hostname;
	int port;
	char *buffer;
	int buffer_size;
    char *user;
    char *password;
    char *cmd;
    char *cmd_data;
	bool secure;
    vscp_state state;
    vscp_sub_state sub_state;
	vscp_callback user_callback;
} request_args;



///////////////////////////////////////////////////////////////////////////////
// receive_callback
//

static void ICACHE_FLASH_ATTR receive_callback(void * arg, char * buf, unsigned short len)
{
	os_printf("-----receive_callback----\r\n");

	struct espconn * conn = (struct espconn *)arg;
	request_args * req = (request_args *)conn->reverse;

	if (req->buffer == NULL) {
		return;
	}

	// Let's do the equivalent of a realloc().
	const int new_size = req->buffer_size + len;
	char * new_buffer;
	if (new_size > BUFFER_SIZE_MAX || NULL == (new_buffer = (char *)os_malloc(new_size))) {
		os_printf("Response too long (%d)\n", new_size);
		req->buffer[0] = '\0'; // Discard the buffer to avoid using an incomplete response.
		if (req->secure)
			espconn_secure_disconnect(conn);
		else
			espconn_disconnect(conn);
		return; // The disconnect callback will be called.
	}

	os_memcpy(new_buffer, req->buffer, req->buffer_size);
	os_memcpy(new_buffer + req->buffer_size - 1 /*overwrite the null character*/, buf, len); // Append new data.
	new_buffer[new_size - 1] = '\0'; // Make sure there is an end of string.

	os_free(req->buffer);
	req->buffer = new_buffer;
	req->buffer_size = new_size;
}

///////////////////////////////////////////////////////////////////////////////
// sent_callback
//

static void ICACHE_FLASH_ATTR sent_callback(void * arg)
{
	os_printf("-----sent_callback----\r\n");

	struct espconn * conn = (struct espconn *)arg;
	request_args * req = (request_args *)conn->reverse;

	if (req->cmd_data == NULL) {
		PRINTF("All sent\n");
	}
	else {
		// The headers were sent, now send the contents.
		PRINTF("Sending request body\n");
		if (req->secure)
			espconn_secure_sent(conn, (uint8_t *)req->cmd_data, strlen(req->cmd_data));
		else
			espconn_sent(conn, (uint8_t *)req->cmd_data, strlen(req->cmd_data));
		os_free(req->cmd_data);
		req->cmd_data = NULL;
	}
}

///////////////////////////////////////////////////////////////////////////////
// connect_callback
//

static void ICACHE_FLASH_ATTR connect_callback(void * arg)
{
	os_printf("-----connect_callback----\r\n");

	PRINTF("Connected\n");
	struct espconn * conn = (struct espconn *)arg;
	request_args * req = (request_args *)conn->reverse;

	espconn_regist_recvcb(conn, receive_callback);
	espconn_regist_sentcb(conn, sent_callback);

	const char * method = "GET";
	char post_headers[32] = "";

	// if (req->post_data != NULL) { // If there is data this is a POST request.
	// 	method = "POST";
	// 	os_sprintf(post_headers, "Content-Length: %d\r\n", strlen(req->post_data));
	// }

    char buf[69]; // AKHE TODO
    int len = 10;
	// char buf[69 + strlen(method) + strlen(req->path) + strlen(req->hostname) +
	// 		 strlen(req->headers) + strlen(post_headers)];
	// int len = os_sprintf(buf,
	// 					 "%s %s HTTP/1.1\r\n"
	// 					 "Host: %s:%d\r\n"
	// 					 "Connection: close\r\n"
	// 					 "User-Agent: ESP8266\r\n"
	// 					 "%s"
	// 					 "%s"
	// 					 "\r\n",
	// 					 method, req->path, req->hostname, req->port, req->headers, post_headers);

	if (req->secure)
		espconn_secure_sent(conn, (uint8_t *)buf, len);
	else{
		espconn_sent(conn, (uint8_t *)buf, len);
		//os_printf("send http data %d : \n%s \r\n",len,buf);
	}
	//os_free(req->headers); // AKHE TODO
	//req->headers = NULL;
	PRINTF("Sending request header\n");
}

///////////////////////////////////////////////////////////////////////////////
// disconnect_callback
//

static void ICACHE_FLASH_ATTR disconnect_callback(void * arg)
{
	PRINTF("Disconnected\n");
	struct espconn *conn = (struct espconn *)arg;

	if(conn == NULL) {
		return;
	}

	if(conn->reverse != NULL) {
		request_args * req = (request_args *)conn->reverse;
		int http_status = -1;
		char * body = "";
		if (req->buffer == NULL) {
			os_printf("Buffer shouldn't be NULL\n");
		}
		else if (req->buffer[0] != '\0') {
			// FIXME: make sure this is not a partial response, using the Content-Length header.

			const char * version = "HTTP/1.1 ";
			if (os_strncmp(req->buffer, version, strlen(version)) != 0) {
				os_printf("Invalid version in %s\n", req->buffer);
			}
			else {
				http_status = atoi(req->buffer + strlen(version));
				body = (char *)os_strstr(req->buffer, "\r\n\r\n") + 4;
				if(os_strstr(req->buffer, "Transfer-Encoding: chunked"))
				{
					int body_size = req->buffer_size - (body - req->buffer);
					char chunked_decode_buffer[body_size];
					os_memset(chunked_decode_buffer, 0, body_size);
					// Chunked data
					chunked_decode(body, chunked_decode_buffer);
					os_memcpy(body, chunked_decode_buffer, body_size);
				}
			}
		}

		if (req->user_callback != NULL) { // Callback is optional.
			req->user_callback(body, http_status, req->buffer);
		}

		os_free(req->buffer);
		os_free(req->hostname);
		os_free(req->cmd_data);
		os_free(req);
	}
	espconn_delete(conn);
	if(conn->proto.tcp != NULL) {
		os_free(conn->proto.tcp);
	}
	os_free(conn);
}

///////////////////////////////////////////////////////////////////////////////
// error_callback
//

static void ICACHE_FLASH_ATTR error_callback(void *arg, sint8 errType)
{
	PRINTF("Disconnected with error\n");
	disconnect_callback(arg);
}

///////////////////////////////////////////////////////////////////////////////
// dns_callback
//

static void ICACHE_FLASH_ATTR dns_callback(const char * hostname, ip_addr_t * addr, void * arg)
{
	request_args * req = (request_args *)arg;

	if (addr == NULL) {
		os_printf("DNS failed for %s\n", hostname);
		if (req->user_callback != NULL) {
			req->user_callback("", -1, "");
		}
		os_free(req);
	}
	else {
		PRINTF("DNS found %s " IPSTR "\n", hostname, IP2STR(addr));

		struct espconn * conn = (struct espconn *)os_malloc(sizeof(struct espconn));
		conn->type = ESPCONN_TCP;
		conn->state = ESPCONN_NONE;
		conn->proto.tcp = (esp_tcp *)os_malloc(sizeof(esp_tcp));
		conn->proto.tcp->local_port = espconn_port();
		conn->proto.tcp->remote_port = req->port;
		conn->reverse = req;

		os_memcpy(conn->proto.tcp->remote_ip, addr, 4);

		espconn_regist_connectcb(conn, connect_callback);
		espconn_regist_disconcb(conn, disconnect_callback);
		espconn_regist_reconcb(conn, error_callback);

		if (req->secure) {
			espconn_secure_set_size(ESPCONN_CLIENT,5120); // set SSL buffer size
			espconn_secure_connect(conn);
		} else {
			espconn_connect(conn);
		}
	}
}

///////////////////////////////////////////////////////////////////////////////
// vscp_connect
//

void ICACHE_FLASH_ATTR vscp_connect(const char * hostname, 
                                            int port, 
                                            bool secure, 
                                            const char *user, 
                                            const char *password, 
                                            vscp_callback user_callback)
{
	PRINTF("DNS request\n");

	request_args *req = (request_args *)os_malloc(sizeof(request_args));
    req->state = vscp_state_not_connected;
    req->sub_state = vscp_sub_state_none;
	req->hostname = esp_strdup(hostname);
	req->port = port;
	req->secure = secure;
	req->user = esp_strdup(user);
    req->password = esp_strdup(password);
	req->cmd = NULL;
    req->cmd_data = NULL;
	req->buffer_size = 1;
	req->buffer = (char *)os_malloc(1);
	req->buffer[0] = '\0'; // Empty string.
	req->user_callback = user_callback;

	ip_addr_t addr;
	err_t error = espconn_gethostbyname((struct espconn *)req, // It seems we don't need a real espconn pointer here.
										    hostname, 
                                            &addr, 
                                            dns_callback);

	if (error == ESPCONN_INPROGRESS) {
        req->state = vscp_state_waiting_resolve;
		PRINTF("DNS pending\n");
	}
	else if (error == ESPCONN_OK) {
		// Already in the local names table (or hostname was an IP address), execute the callback ourselves.
		req->state = vscp_state_waiting_resolve;
        dns_callback(hostname, &addr, req);
	}
	else {
		if (error == ESPCONN_ARG) {
			os_printf("DNS arg error %s\n", hostname);
		}
		else {
			os_printf("DNS error code %d\n", error);
		}
		dns_callback(hostname, NULL, req); // Handle all DNS errors the same way.
	}
}

///////////////////////////////////////////////////////////////////////////////
// vscp_cmd
//

void ICACHE_FLASH_ATTR vscp_cmd(const char *cmd, 
                                    const char *cmd_data, 
                                    vscp_callback user_callback)
{
	char hostname[128] = "";
	int port = 80;
	bool secure = false;

	// bool is_http  = os_strncmp(url, "http://",  strlen("http://"))  == 0;
	// bool is_https = os_strncmp(url, "https://", strlen("https://")) == 0;

	// if (is_http)
	// {
	// 	url += strlen("http://"); // Get rid of the protocol.
	// }
		
	// else if (is_https) {
	// 	port = 443;
	// 	secure = true;
	// 	url += strlen("https://"); // Get rid of the protocol.
	// } else {
	// 	os_printf("URL is not HTTP or HTTPS %s\n", url);
	// 	return;
	// }

	// char * path = os_strchr(url, '/');
	// if (path == NULL) {
	// 	path = os_strchr(url, '\0'); // Pointer to end of string.
	// }

	// char * colon = os_strchr(url, ':');
	// if (colon > path) {
	// 	colon = NULL; // Limit the search to characters before the path.
	// }

	// if (colon == NULL) { // The port is not present.
	// 	os_memcpy(hostname, url, path - url);
	// 	hostname[path - url] = '\0';
	// }
	// else {
	// 	port = atoi(colon + 1);
	// 	if (port == 0) {
	// 		os_printf("Port error %s\n", url);
	// 		return;
	// 	}

	// 	os_memcpy(hostname, url, colon - url);
	// 	hostname[colon - url] = '\0';
	// }


	// if (path[0] == '\0') { // Empty path is not allowed.
	// 	path = "/";
	// }
	os_printf("hostname=%s\n", hostname);
	os_printf("port=%d\n", port);
	os_printf("cmd=%s\n", cmd);
	PRINTF("hostname=%s\n", hostname);
	PRINTF("port=%d\n", port);
	PRINTF("cmd=%s\n", cmd);
	http_raw_request(hostname, 
                        port, 
                        secure, 
                        cmd, 
                        cmd_data, 
                        user_callback);
}


///////////////////////////////////////////////////////////////////////////////
// vscp_callback_example
//

void ICACHE_FLASH_ATTR vscp_callback_example(char *response, 
                                                int status, 
                                                char *full_response)
{
	os_printf("http_status=%d\n", status);	
	if (status != VSCP_STATUS_GENERIC_ERROR) {
		os_printf("strlen(full_response)=%d\n", strlen(full_response));
		os_printf("response=%s<EOF>\n", full_response);
	}
}