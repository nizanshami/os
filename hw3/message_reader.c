#include "message_slot.h"
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <string.h>
#include <unistd.h>     /* exit */


int main(int argc, char *argv[]){
    int fd, res;
    unsigned long channel_id;
    char message[BUF_LEN+1];
    
    if(argc != 3){
        printf("error should run program: a.out path(message_slot) channel_id message");
        exit(1);
    }
    
    channel_id = 0;
    fd = open(argv[1], O_RDONLY);
    if(fd < 0){
        perror("ERROR: cannt open file");
        exit(1);
    }
    channel_id = atoi(argv[2]);
    res = ioctl(fd, MSG_SLOT_CHANNEL, channel_id);
    if(res != 0){
        perror("ERROR: ioctl faild");
        exit(1);
    }
    res = read(fd, message, BUF_LEN);
    if(res < 0){
        perror("ERROR: read failed");
        exit(1);
    }
    close(fd);
    message[res] = '\0';
    printf("%s\n", message);
    return 0;

}