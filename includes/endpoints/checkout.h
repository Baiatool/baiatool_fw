/*******************************************************************
 *
 *  ████  ████ ███ ████ ███ ████ ████ █
 *  █  ██ █  █  █  █  █  █  █  █ █  █ █
 *  ████  ████  █  ████  █  █  █ █  █ █
 *  █  ██ █  █  █  █  █  █  █  █ █  █ █
 *  ████  █  █ ███ █  █  █  ████ ████ ███
 *
 ********************************************************************
 * @file checkout.h
 *
 * @brief HTTP endpoint for ending the active schedule (user checkout).
 * @author José Félix de Oliveira Neto (josefelix.neto@edge.ufal.br)
 * @version 0.1
 * @date 01/07/2026
 *
 * @copyright Copyright (c) 2026
 *
 *******************************************************************/

#ifndef ENDPOINT_CHECKOUT_H
#define ENDPOINT_CHECKOUT_H

/**
 * @brief Notify the server that the user has ended their workstation session
 *        by tapping their RFID/NFC tag.
 *
 * @param sock   Connected TCP socket.
 * @param host   Server hostname string.
 * @param port   Server port string.
 *
 * @retval 0   Success (HTTP 2xx).
 * @retval <0  HTTP error or unexpected status code.
 */
int checkout_endpoint_post(int sock, const char *host, const char *port);

#endif /* ENDPOINT_CHECKOUT_H */
