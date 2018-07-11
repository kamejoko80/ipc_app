/*
 *  Copyright (C) : 2018
 *  File name     : ipc_app
 *  Author        : Dang Minh Phuong
 *  Email         : kamejoko80@yahoo.com
 *
 *  This program is free software, you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 */
 
#include <linux/netlink.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>

/*
 * Build command : gcc -o demo ipc_app.c -lpthread
 */

#define NETLINK_USER 31
#define NETLINK_GROUP 1

#define MAX_PAYLOAD 1024 /* maximum payload size*/
struct sockaddr_nl src_addr, dest_addr;
struct nlmsghdr *snd_nlh = NULL;
struct nlmsghdr *rcv_nlh = NULL;

int sock_fd;
struct iovec snd_iov;
struct iovec rcv_iov;
struct msghdr snd_msg;
struct msghdr rcv_msg;

volatile int stop = 0;

int pthread_kill(pthread_t thread, int sig);

void handler(int sig)
{
    stop = 1;
}

void msg_send(char *data, int len)
{
    memset(snd_nlh, 0, NLMSG_SPACE(MAX_PAYLOAD));
    snd_nlh->nlmsg_len = len + sizeof(struct nlmsghdr);
    snd_nlh->nlmsg_pid = getpid();
    snd_nlh->nlmsg_flags = 0;

    memcpy(NLMSG_DATA(snd_nlh), (void *)data, len);

    snd_iov.iov_base = (void *)snd_nlh;
    snd_iov.iov_len = snd_nlh->nlmsg_len;
    snd_msg.msg_name = (void *)&dest_addr;
    snd_msg.msg_namelen = sizeof(dest_addr);
    snd_msg.msg_iov = &snd_iov;
    snd_msg.msg_iovlen = 1;

    sendmsg(sock_fd, &snd_msg, 0);
}

void msg_receive(char *data, int *len)
{
    int rcv_len;

    memset(rcv_nlh, 0, NLMSG_SPACE(MAX_PAYLOAD));
    rcv_nlh->nlmsg_len = NLMSG_SPACE(MAX_PAYLOAD);
    rcv_nlh->nlmsg_pid = getpid();
    rcv_nlh->nlmsg_flags = 0;

    rcv_iov.iov_base = (void *)rcv_nlh;
    rcv_iov.iov_len = rcv_nlh->nlmsg_len;
    rcv_msg.msg_name = (void *)&dest_addr;
    rcv_msg.msg_namelen = sizeof(dest_addr);
    rcv_msg.msg_iov = &rcv_iov;
    rcv_msg.msg_iovlen = 1;

    /* read message from kernel */
    recvmsg(sock_fd, &rcv_msg, 0);

    /* get data */
    *len = rcv_nlh->nlmsg_len - sizeof(struct nlmsghdr);
    memcpy((void *)data, NLMSG_DATA(rcv_nlh), *len);
}

void *msg_receive_and_display(void* arg)
{
    char data[256];
    int len;

    while (!stop)
    {
        memset(data, 0, 256);
        msg_receive(data, &len);
        printf("%s\r\n", data);
    }

    return NULL;
}

int main()
{
    char data[256];
    int err;
    pthread_t tid;
    char c = 0;

    /* Register signal handler */
    signal(SIGUSR1, handler);

    sock_fd = socket(PF_NETLINK, SOCK_RAW, NETLINK_USER);
    if (sock_fd < 0)
        return -1;

    /* Initialize socket source address */
    memset(&src_addr, 0, sizeof(src_addr));
    src_addr.nl_family = AF_NETLINK;
    src_addr.nl_pid = getpid(); /* self pid */
    src_addr.nl_groups = NETLINK_GROUP; /* multicast */

    memset(&dest_addr, 0, sizeof(dest_addr));
    dest_addr.nl_family = AF_NETLINK;
    dest_addr.nl_pid = 0; /* For Linux Kernel */
    dest_addr.nl_groups = NETLINK_GROUP; /* multicast */

    /* allocate netlink header */
    snd_nlh = (struct nlmsghdr *)malloc(NLMSG_SPACE(MAX_PAYLOAD));
    rcv_nlh = (struct nlmsghdr *)malloc(NLMSG_SPACE(MAX_PAYLOAD));
    if((snd_nlh == NULL) || (rcv_nlh == NULL)){
        printf("could not allocate nl header\n");
        exit(1);
    }

    /* bind the socket */
    bind(sock_fd, (struct sockaddr *)&src_addr, sizeof(src_addr));

    err = pthread_create(&tid, NULL, msg_receive_and_display, NULL);
    if (err)
    {
        printf("pthread_create: %m\n");
        exit(1);
    }

    printf("IPC chat program\r\n");
    printf("Press Ctr+C to stop the program\r\n");

    while(!stop)
    {
        memset(data, 0, 256);
        fgets(data, 256, stdin);
        /* check esc character */
        msg_send(data, strlen(data));
    }

    pthread_kill(tid, SIGUSR1);
    pthread_join(tid, NULL);

    close(sock_fd);
}

