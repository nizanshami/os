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
    
    if(argc != 4){
        printf("error should run program: program_name path(message_slot) channel_id message");
        exit(1);
    }
    

    fd = open(argv[1], O_WRONLY);
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
    res = write(fd, argv[3], strlen(argv[3]));
    if(res != strlen(argv[3])){
        perror("ERROR: write failed");
        exit(1);
    }
    close(fd);
    return 0;
}