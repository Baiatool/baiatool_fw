/*******************************************************************
 *
 *  ████  ████ ███ ████ ███ ████ ████ █
 *  █  ██ █  █  █  █  █  █  █  █ █  █ █
 *  ████  ████  █  ████  █  █  █ █  █ █
 *  █  ██ █  █  █  █  █  █  █  █ █  █ █
 *  ████  █  █ ███ █  █  █  ████ ████ ███
 *
 ********************************************************************
 * @file endpoint_shell.c
 *
 * @brief Shell commands for manually triggering HTTP endpoint functions.
 * @author José Félix de Oliveira Neto (josefelix.neto@edge.ufal.br)
 * @version 0.1
 * @date 30/06/2026
 *
 * @copyright Copyright (c) 2026
 *
 *******************************************************************/

#if CONFIG_SHELL_ENDPOINT

#include "endpoints/confirm_schedule.h"
#include "endpoints/schedule.h"

#include <errno.h>
#include <zephyr/net/net_ip.h>
#include <zephyr/net/socket.h>
#include <zephyr/shell/shell.h>

static int connect_to_host(const struct shell *sh, const char *host, const char *port)
{
	struct zsock_addrinfo hints = {
		.ai_family = AF_INET,
		.ai_socktype = SOCK_STREAM,
	};
	struct zsock_addrinfo *res;
	int sock;
	int ret;

	ret = zsock_getaddrinfo(host, port, &hints, &res);
	if (ret != 0) {
		shell_error(sh, "DNS lookup failed for %s: %d", host, ret);
		return -ENOENT;
	}

	sock = zsock_socket(res->ai_family, res->ai_socktype, res->ai_protocol);
	if (sock < 0) {
		ret = -errno;
		shell_error(sh, "Socket creation failed: %d", ret);
		zsock_freeaddrinfo(res);
		return ret;
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

static int cmd_fetch_schedule(const struct shell *sh, size_t argc, char **argv)
{
	const char *host = argv[1];
	const char *port = argv[2];
	int sock;
	int ret;

	sock = connect_to_host(sh, host, port);
	if (sock < 0) {
		return sock;
	}

	ret = schedule_endpoint_fetch(sock, host, port);

	(void)zsock_close(sock);

	if (ret < 0) {
		shell_error(sh, "fetch_schedule failed: %d", ret);
		return ret;
	}

	shell_print(sh, "fetch_schedule OK");
	return 0;
}

static int cmd_confirm_schedule(const struct shell *sh, size_t argc, char **argv)
{
	const char *host = argv[1];
	const char *port = argv[2];
	int sock;
	int ret;

	sock = connect_to_host(sh, host, port);
	if (sock < 0) {
		return sock;
	}

	ret = confirm_schedule_endpoint_post(sock, host, port);

	(void)zsock_close(sock);

	if (ret < 0) {
		shell_error(sh, "confirm_schedule failed: %d", ret);
		return ret;
	}

	shell_print(sh, "confirm_schedule OK");
	return 0;
}

SHELL_STATIC_SUBCMD_SET_CREATE(endpoint_cmds,
			       SHELL_CMD_ARG(fetch_schedule, NULL, "<host> <port>",
					     cmd_fetch_schedule, 3, 0),
			       SHELL_CMD_ARG(confirm_schedule, NULL, "<host> <port>",
					     cmd_confirm_schedule, 3, 0),
			       SHELL_SUBCMD_SET_END);

SHELL_CMD_REGISTER(endpoint, &endpoint_cmds, "Endpoint commands", NULL);

#endif /* CONFIG_SHELL_ENDPOINT */
