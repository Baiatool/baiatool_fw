/*******************************************************************
 *
 *  ████  ████ ███ ████ ███ ████ ████ █
 *  █  ██ █  █  █  █  █  █  █  █ █  █ █
 *  ████  ████  █  ████  █  █  █ █  █ █
 *  █  ██ █  █  █  █  █  █  █  █ █  █ █
 *  ████  █  █ ███ █  █  █  ████ ████ ███
 *
 ********************************************************************
 * @file confirm_schedule.h
 *
 * @brief HTTP endpoint for confirming the pending schedule via RFID tap.
 * @author José Félix de Oliveira Neto (josefelix.neto@edge.ufal.br)
 * @version 0.1
 * @date 30/06/2026
 *
 * @copyright Copyright (c) 2026
 *
 *******************************************************************/

#ifndef ENDPOINT_CONFIRM_SCHEDULE_H
#define ENDPOINT_CONFIRM_SCHEDULE_H

/**
 * @brief Notify the server that the user has confirmed the pending schedule
 *        by tapping their RFID/NFC tag.
 *
 * @param sock   Connected TCP socket.
 * @param host   Server hostname string.
 * @param port   Server port string.
 *
 * @retval 0   Success (HTTP 2xx).
 * @retval <0  HTTP error or unexpected status code.
 */
int confirm_schedule_endpoint_post(int sock, const char *host, const char *port);

#endif /* ENDPOINT_CONFIRM_SCHEDULE_H */
