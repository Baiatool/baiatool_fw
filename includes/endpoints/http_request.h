/*******************************************************************
 *
 *  ████  ████ ███ ████ ███ ████ ████ █
 *  █  ██ █  █  █  █  █  █  █  █ █  █ █
 *  ████  ████  █  ████  █  █  █ █  █ █
 *  █  ██ █  █  █  █  █  █  █  █ █  █ █
 *  ████  █  █ ███ █  █  █  ████ ████ ███
 *
 ********************************************************************
 * @file http_request.h
 *
 * @brief Definition of HTTP request macros, responsible for sending HTTP requests.
 * @author José Félix de Oliveira Neto (josefelix.neto@edge.ufal.br)
 * @version 0.1
 * @date 23/06/2026
 *
 * @copyright Copyright (c) 2026
 *
 *******************************************************************/
#ifndef ENDPOINTS_HTTP_REQUEST_H_
#define ENDPOINTS_HTTP_REQUEST_H_

#include <zephyr/net/http/client.h>

static inline int http_do_request(int sock, enum http_method method, const char *host,
				  const char *port, const char *url, const char *content_type,
				  const char *payload, size_t payload_len,
				  http_response_cb_t response_cb, uint8_t *recv_buf,
				  size_t recv_buf_len, int32_t timeout_ms, void *user_data)
{
	struct http_request req = {
		.method = method,
		.protocol = "HTTP/1.1",
		.host = host,
		.port = port,
		.url = url,
		.content_type_value = content_type,
		.payload = payload,
		.payload_len = payload_len,
		.response = response_cb,
		.recv_buf = recv_buf,
		.recv_buf_len = recv_buf_len,
	};

	return http_client_req(sock, &req, timeout_ms, user_data);
}

/* GET — pass NULL/0U for _ct/_payload/_payload_len when no body is needed */
#define HTTP_REQ_GET(_sock, _host, _port, _url, _ct, _payload, _payload_len, _cb, _buf, _buf_len,  \
		     _timeout_ms, _user_data)                                                      \
	http_do_request((_sock), HTTP_GET, (_host), (_port), (_url), (_ct), (_payload),            \
			(_payload_len), (_cb), (_buf), (_buf_len), (_timeout_ms), (_user_data))

/* POST — same signature as GET, method pre-filled */
#define HTTP_REQ_POST(_sock, _host, _port, _url, _ct, _payload, _payload_len, _cb, _buf, _buf_len, \
		      _timeout_ms, _user_data)                                                     \
	http_do_request((_sock), HTTP_POST, (_host), (_port), (_url), (_ct), (_payload),           \
			(_payload_len), (_cb), (_buf), (_buf_len), (_timeout_ms), (_user_data))

#endif /* ENDPOINTS_HTTP_REQUEST_H_ */
