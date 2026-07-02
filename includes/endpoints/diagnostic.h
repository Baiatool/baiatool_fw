/*******************************************************************
 *
 *  ████  ████ ███ ████ ███ ████ ████ █
 *  █  ██ █  █  █  █  █  █  █  █ █  █ █
 *  ████  ████  █  ████  █  █  █ █  █ █
 *  █  ██ █  █  █  █  █  █  █  █ █  █ █
 *  ████  █  █ ███ █  █  █  ████ ████ ███
 *
 ********************************************************************
 * @file diagnostic.h
 *
 * @brief HTTP endpoint for sending device diagnostic data to the server.
 * @author José Félix de Oliveira Neto (josefelix.neto@edge.ufal.br)
 * @version 0.1
 * @date 01/07/2026
 *
 * @copyright Copyright (c) 2026
 *
 *******************************************************************/

#ifndef ENDPOINT_DIAGNOSTIC_H
#define ENDPOINT_DIAGNOSTIC_H

/**
 * @brief Send device diagnostic data (settings snapshot) to the server.
 *
 * Reads Wi-Fi SSID and schedule state from NVS and POSTs them to
 * /api/diagnostic. The PSK is never sent.
 *
 * @param sock   Connected TCP socket.
 * @param host   Server hostname string.
 * @param port   Server port string.
 *
 * @retval 0   Success (HTTP 2xx).
 * @retval <0  Settings read error, HTTP error, or unexpected status code.
 */
int diagnostic_endpoint_post(int sock, const char *host, const char *port);

#endif /* ENDPOINT_DIAGNOSTIC_H */
