/*******************************************************************
 *
 *  ████  ████ ███ ████ ███ ████ ████ █
 *  █  ██ █  █  █  █  █  █  █  █ █  █ █
 *  ████  ████  █  ████  █  █  █ █  █ █
 *  █  ██ █  █  █  █  █  █  █  █ █  █ █
 *  ████  █  █ ███ █  █  █  ████ ████ ███
 *
 ********************************************************************
 * @file schedule.h
 *
 * @brief HTTP endpoint for fetching the pending schedule from the server.
 * @author José Félix de Oliveira Neto (josefelix.neto@edge.ufal.br)
 * @version 0.1
 * @date 26/06/2026
 *
 * @copyright Copyright (c) 2026
 *
 *******************************************************************/

#ifndef ENDPOINT_SCHEDULE_H
#define ENDPOINT_SCHEDULE_H

/**
 * @brief Fetch the pending schedule from the server and load it into the
 *        schedule service via SCHEDULE_CMD_LOAD.
 *
 * @param sock   Connected TCP socket.
 * @param host   Server hostname string.
 * @param port   Server port string.
 *
 * @retval 0      Success (including 404 — no schedule pending).
 * @retval -EINVAL JSON parse error or missing/invalid fields.
 * @retval -ENODATA 200 response with empty body.
 * @retval <0    HTTP or zbus error.
 */
int schedule_endpoint_fetch(int sock, const char *host, const char *port);

#endif /* ENDPOINT_SCHEDULE_H */
