/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zephyr/posix/arpa/inet.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/net/openthread.h>
#include <openthread/thread.h>

#if defined(CONFIG_BT)
#include "ble.h"
#endif

#if defined(CONFIG_CLI_SAMPLE_LOW_POWER)
#include "low_power.h"
#endif
#define PORT 4501

#define UDP_SERVER_WORKQ_STACK_SIZE 512
#define UDP_SERVER_WORKQ_PRIORITY 5

LOG_MODULE_REGISTER(cli_sample, CONFIG_OT_COMMAND_LINE_INTERFACE_LOG_LEVEL);

int sockfd;
struct sockaddr_in6 servaddr, cliaddr;
char buffer[1024];
socklen_t len = sizeof(cliaddr);
static struct k_work_q udp_server_workq;

static struct k_work receive_from;

K_THREAD_STACK_DEFINE(udp_server_workq_stack_area, UDP_SERVER_WORKQ_STACK_SIZE);

static void do_work(struct k_work *item)
{
    ARG_UNUSED(item);

    while (1)
    {
        int n = recvfrom(sockfd, (char *)buffer, sizeof(buffer), MSG_WAITALL, (struct sockaddr *)&cliaddr, &len);
        buffer[n] = '\0';
        printk("Received from client: %s\n", buffer);

        if (strcmp(buffer, "on\n") == 0)
        {
            printk("on command received\n");
            sendto(sockfd, (const char *)"ok\n", strlen("ok\n"), 0, (const struct sockaddr *)&cliaddr, len);
        }
        else if (strcmp(buffer, "off\n") == 0)
        {
            printk("off command received\n");
            sendto(sockfd, (const char *)"ok\n", strlen("ok\n"), 0, (const struct sockaddr *)&cliaddr, len);
        }
        else if (strcmp(buffer, "name\n") == 0)
        {
            printk("name command received\n");
            sendto(sockfd, (const char *)"Group 10\n", strlen("Group 10\n"), 0, (const struct sockaddr *)&cliaddr, len);
        }
        else
        {
            sendto(sockfd, (const char *)"Failed Command\n", strlen("Failed Command\n"), 0, (const struct sockaddr *)&cliaddr, len);
        }
    }
}

int main(void)
{
    if ((sockfd = socket(AF_INET6, SOCK_DGRAM, 0)) < 0)
    {
        perror("socket creation failed");
        exit(EXIT_FAILURE);
    }
    memset(&servaddr, 0, sizeof(servaddr));
    memset(&cliaddr, 0, sizeof(cliaddr));

    servaddr.sin6_family = AF_INET6;
    servaddr.sin6_addr = in6addr_any;
    servaddr.sin6_port = htons(PORT);
    if (bind(sockfd, (const struct sockaddr *)&servaddr, sizeof(servaddr)) < 0)
    {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }
    k_work_queue_init(&udp_server_workq);
    k_work_queue_start(&udp_server_workq, udp_server_workq_stack_area,
                       K_THREAD_STACK_SIZEOF(udp_server_workq_stack_area),
                       UDP_SERVER_WORKQ_PRIORITY, NULL);
    k_work_init(&receive_from, do_work);

    openthread_start(openthread_get_default_context());
}
