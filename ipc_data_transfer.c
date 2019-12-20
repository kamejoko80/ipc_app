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
#include <sys/stat.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>

/*
 * Message catalog:
 *
 * Name        : CMD_SEND_DATA_SIZE
 * Byte 0      : 0x01
 * Byte 1-4    : Data size
 * Emitter     : SoC
 * Description : SoC sends data size to MCU
 *
 * Name        : CMD_SEND_DATA
 * Byte 0      : 0x02
 * Byte 1      : Data len
 * Byte 2-n    : Data
 * Emitter     : SoC
 * Description : SoC sends data to MCU
 *
 * Name        : CMD_ACK
 * Byte 0      : 0x03
 * Emitter     : MCU, SoC
 * Description : Send ACK
 *
 * Name        : CMD_NAK
 * Byte 0      : 0x04
 * Byte 1      : Error code
 * Emitter     : MCU, SoC
 * Description : Send NAK with error code
 */

/* Message commands definition */
#define CMD_SEND_DATA_SIZE (0x01)
#define CMD_SEND_DATA      (0x02)
#define CMD_ACK            (0x03)
#define CMD_NAK            (0x04)

#define DELAY              (5000)
#define FILE_RD_CNT        (200) 

/*
 * Build command : gcc -o data_transfer ipc_data_transfer.c -lpthread
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

void send_sync_frame(void)
{
	char sync_frame[]={0xA5, 0xA5, 0xA5, 0xA5};
    msg_send(sync_frame, 4);
} 

void send_data_size(unsigned int size)
{
    char cmd[5];
    
    cmd[0] = CMD_SEND_DATA_SIZE;
    cmd[1] = (size & 0xFF);
    cmd[2] = (size >> 8) & 0xFF;
    cmd[3] = (size >> 16) & 0xFF;
    cmd[4] = (size >> 24) & 0xFF;

    msg_send(cmd, 5);
}

void send_data(char *data, char len)
{
   char buf[255];
   
   buf[0] = CMD_SEND_DATA;
   buf[1] = len;
   memcpy(&buf[2], data, len);
 
   msg_send(buf, len + 2);
}

void check_resp(void)
{
    char resp[256];
    int len;
    
    msg_receive(resp, &len);
    
    if(len)
    {
        switch (resp[0])
        {
            case CMD_ACK:
                printf("ACK received\n");
            break;
            case CMD_NAK:
                printf("NAK received\n");
            break;            
        }
    }
}

void *msg_receive_thread(void* arg)
{
    char data[256];
    int len;

    while (!stop)
    {
        check_resp();
    }

    return NULL;
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

void data_transfer(void)
{
    char data[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20};

    send_sync_frame();
    usleep(DELAY);
    send_data_size(20);
    usleep(DELAY);
    check_resp();
    usleep(DELAY);
    send_data(&data[0], 9);
    usleep(DELAY);
    check_resp();
    usleep(DELAY);
    send_data(&data[9], 11);
    usleep(DELAY);
    check_resp();    
}

void send_file_data(char *file_name)
{
    FILE *fptr;
    struct stat st;    
    unsigned int len;
    char buffer[FILE_RD_CNT];
    
    /* Get file size */    
    stat(file_name, &st);    
    printf("File size = %d\r\n", (unsigned int)st.st_size);

    /* Send sync frame and file size */
    send_sync_frame();
    usleep(DELAY);
    send_data_size((unsigned int)st.st_size);
    usleep(DELAY);
    
    /* Open file */    
    if ((fptr = fopen(file_name, "rb")) == NULL){
       printf("Error! opening file");
       exit(1);
    }
    
    /* Read and send data */
    while (1) {
        
        /* Read file */
        len = fread(buffer, 1, FILE_RD_CNT, fptr);
        
        /* Send to IPC */
        send_data(buffer, len);
        usleep(DELAY);
        check_resp();
        usleep(DELAY);        
        
        if(len != FILE_RD_CNT){
            break;
        }
    }
        
    printf("Close file %s\r\n", file_name);
    
    fclose(fptr);    
}

int main(int argc, char *argv[])
{
    int err;
    
    if(argc < 2){
       printf("error file input\r\n");
       printf("run ./data_transfer binary_file_name\r\n");
       exit(1);
    }
        
    sock_fd = socket(PF_NETLINK, SOCK_RAW, NETLINK_USER);
    if (sock_fd < 0)
        exit(1);

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

    printf("IPC data transfer program\r\n");

    //data_transfer();

    send_file_data(argv[1]);
    
    close(sock_fd);
}
