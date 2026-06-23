/*******************************************************************
 *
 *  ████  ████ ███ ████ ███ ████ ████ █
 *  █  ██ █  █  █  █  █  █  █  █ █  █ █
 *  ████  ████  █  ████  █  █  █ █  █ █
 *  █  ██ █  █  █  █  █  █  █  █ █  █ █
 *  ████  █  █ ███ █  █  █  ████ ████ ███
 *
 ********************************************************************
 * @file http_shell.c
 *
 * @brief Shell commands for sending HTTP requests manually.
 * @author José Félix de Oliveira Neto (josefelix.neto@edge.ufal.br)
 * @version 0.1
 * @date 23/06/2026
 *
 * @copyright Copyright (c) 2026
 *
 *******************************************************************/

#if CONFIG_SHELL

#include <errno.h>
#include <string.h>
#include <zephyr/kernel.h>
#include <zephyr/net/net_ip.h>
#include <zephyr/net/socket.h>
#include <zephyr/shell/shell.h>

#include "endpoints/http_request.h"

#define HTTP_SHELL_RECV_BUF_SIZE 512
#define HTTP_SHELL_TIMEOUT_MS    5000

static uint8_t recv_buf[HTTP_SHELL_RECV_BUF_SIZE];

static int response_cb(struct http_response *rsp, enum http_final_call final_data, void *user_data)
{
	const struct shell *sh = (const struct shell *)user_data;

	if (rsp->body_frag_len > 0U) {
		shell_fprintf(sh, SHELL_NORMAL, "%.*s", (int)rsp->body_frag_len,
			      (char *)rsp->body_frag_start);
	}

	if (final_data == HTTP_DATA_FINAL) {
		shell_print(sh, "\nHTTP status: %d", rsp->http_status_code);
	}

	return 0;
}

static int connect_to_host(const struct shell *sh, const char *host, const char *port, bool use_tls)
{
	struct zsock_addrinfo hints = {
		.ai_family = AF_INET,
		.ai_socktype = SOCK_STREAM,
	};
	struct zsock_addrinfo *res;
	int proto;
	int sock;
	int ret;

	ret = zsock_getaddrinfo(host, port, &hints, &res);
	if (ret != 0) {
		shell_error(sh, "DNS lookup failed for %s: %d", host, ret);
		return -ENOENT;
	}

	proto = use_tls ? NET_IPPROTO_TLS_1_2 : res->ai_protocol;

	sock = zsock_socket(res->ai_family, res->ai_socktype, proto);
	if (sock < 0) {
		ret = -errno;
		shell_error(sh, "Socket creation failed: %d", ret);
		zsock_freeaddrinfo(res);
		return ret;
	}

	if (use_tls) {
		/* SNI: tell the server which hostname we're connecting to */
		ret = zsock_setsockopt(sock, SOL_TLS, TLS_HOSTNAME, host, strlen(host));
		if (ret < 0) {
			ret = -errno;
			shell_error(sh, "TLS_HOSTNAME failed: %d", ret);
			(void)zsock_close(sock);
			zsock_freeaddrinfo(res);
			return ret;
		}

		/* Skip certificate verification for now (Step 3 adds the CA cert) */
		int verify = TLS_PEER_VERIFY_NONE;

		ret = zsock_setsockopt(sock, SOL_TLS, TLS_PEER_VERIFY, &verify, sizeof(verify));
		if (ret < 0) {
			ret = -errno;
			shell_error(sh, "TLS_PEER_VERIFY failed: %d", ret);
			(void)zsock_close(sock);
			zsock_freeaddrinfo(res);
			return ret;
		}
	}

	ret = zsock_connect(sock, res->ai_addr, res->ai_addrlen);
	zsock_freeaddrinfo(res);

	if (ret < 0) {
		ret = -errno;
		shell_error(sh, "Connect to %s:%s failed: %d", host, port, ret);
		(void)zsock_close(sock);
		return ret;
	}

	return sock;
}

static int cmd_http_get(const struct shell *sh, size_t argc, char **argv)
{
	const char *host = argv[1];
	const char *port = argv[2];
	const char *url = argv[3];
	int sock;
	int ret;

	sock = connect_to_host(sh, host, port, false);
	if (sock < 0) {
		return sock;
	}

	ret = HTTP_REQ_GET(sock, host, port, url, NULL, NULL, 0U, response_cb, recv_buf,
			   sizeof(recv_buf), HTTP_SHELL_TIMEOUT_MS, (void *)sh);
	if (ret < 0) {
		shell_error(sh, "HTTP GET failed: %d", ret);
	}

	(void)zsock_close(sock);
	return ret < 0 ? ret : 0;
}

static int cmd_http_post(const struct shell *sh, size_t argc, char **argv)
{
	const char *host = argv[1];
	const char *port = argv[2];
	const char *url = argv[3];
	const char *content_type = argv[4];
	const char *payload = argv[5];
	size_t payload_len = strlen(payload);
	int sock;
	int ret;

	sock = connect_to_host(sh, host, port, false);
	if (sock < 0) {
		return sock;
	}

	ret = HTTP_REQ_POST(sock, host, port, url, content_type, payload, payload_len, response_cb,
			    recv_buf, sizeof(recv_buf), HTTP_SHELL_TIMEOUT_MS, (void *)sh);
	if (ret < 0) {
		shell_error(sh, "HTTP POST failed: %d", ret);
	}

	(void)zsock_close(sock);
	return ret < 0 ? ret : 0;
}

static int cmd_https_get(const struct shell *sh, size_t argc, char **argv)
{
	const char *host = argv[1];
	const char *port = argv[2];
	const char *url = argv[3];
	int sock;
	int ret;

	sock = connect_to_host(sh, host, port, true);
	if (sock < 0) {
		return sock;
	}

	ret = HTTP_REQ_GET(sock, host, port, url, NULL, NULL, 0U, response_cb, recv_buf,
			   sizeof(recv_buf), HTTP_SHELL_TIMEOUT_MS, (void *)sh);
	if (ret < 0) {
		shell_error(sh, "HTTPS GET failed: %d", ret);
	}

	(void)zsock_close(sock);
	return ret < 0 ? ret : 0;
}

static int cmd_https_post(const struct shell *sh, size_t argc, char **argv)
{
	const char *host = argv[1];
	const char *port = argv[2];
	const char *url = argv[3];
	const char *content_type = argv[4];
	const char *payload = argv[5];
	size_t payload_len = strlen(payload);
	int sock;
	int ret;

	sock = connect_to_host(sh, host, port, true);
	if (sock < 0) {
		return sock;
	}

	ret = HTTP_REQ_POST(sock, host, port, url, content_type, payload, payload_len, response_cb,
			    recv_buf, sizeof(recv_buf), HTTP_SHELL_TIMEOUT_MS, (void *)sh);
	if (ret < 0) {
		shell_error(sh, "HTTPS POST failed: %d", ret);
	}

	(void)zsock_close(sock);
	return ret < 0 ? ret : 0;
}

SHELL_STATIC_SUBCMD_SET_CREATE(http_cmds,
			       SHELL_CMD_ARG(get, NULL, "<host> <port> <url>", cmd_http_get, 4, 0),
			       SHELL_CMD_ARG(post, NULL,
					     "<host> <port> <url> <content-type> <payload>",
					     cmd_http_post, 6, 0),
			       SHELL_SUBCMD_SET_END);

SHELL_STATIC_SUBCMD_SET_CREATE(https_cmds,
			       SHELL_CMD_ARG(get, NULL, "<host> <port> <url>", cmd_https_get, 4, 0),
			       SHELL_CMD_ARG(post, NULL,
					     "<host> <port> <url> <content-type> <payload>",
					     cmd_https_post, 6, 0),
			       SHELL_SUBCMD_SET_END);

SHELL_CMD_REGISTER(http, &http_cmds, "HTTP client commands", NULL);
SHELL_CMD_REGISTER(https, &https_cmds, "HTTPS client commands", NULL);

#endif /* CONFIG_SHELL */
