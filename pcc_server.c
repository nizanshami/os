#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <endian.h>
#include <string.h>
#include <sys/types.h>
#include <assert.h>
#include <signal.h>
#include <stdbool.h>

uint64_t pcc_total[95] = {0};
int conn_fd = -1;
bool sigint = false;

void close_connection(){
    int i;
    for(i = 0;i < 95;i++){
        printf("char '%c' : %lu times\n", (i + 32), pcc_total[i]);
    }
    exit(0);

}

int get_data(char *buff, int len){
    int byte_got = 0;
    int bytes = 1;

    while(bytes > 0){
        bytes = read(conn_fd, buff + byte_got, len - byte_got);
        byte_got += bytes;
        
    }

    if(bytes == 0 && byte_got == len){//read done
        return 0;
    }
    if(bytes == 0){//dont read len byte
        printf("\n Error : reading server keep accepting connections. %s \n", strerror(errno));
        return -1;
    }
    if (!(errno == ETIMEDOUT || errno == ECONNRESET || errno == EPIPE))
    {
        printf("\n reading failed, Error: %s\n", strerror(errno));
        exit(1);
    }
    printf("\n ERROR: %s\n", strerror(errno));
    return -1;
}

int send_data(char *buff, int len){
    int bytes = 1;
    int bytes_send = 0;

    while(bytes > 0){
        bytes = write(conn_fd, buff + bytes_send, len - bytes_send);
        bytes_send += bytes;
        
    }
    if(bytes == 0 && bytes_send == len){//write done
        return 0;
    }
    if(bytes == 0){//dont write len byte
        printf("\n Error : write server keep accepting connections. %s \n", strerror(errno));
        return -1;
    }
    if (!(errno == ETIMEDOUT || errno == ECONNRESET || errno == EPIPE))
    {
        printf("\n writing failed, Error: %s\n", strerror(errno));
        exit(1);
    }
    printf("\n ERROR: %s\n", strerror(errno));
    return -1;
}

void handle_connection(){
    char *int_buff = NULL;
    char *rcv_buff = NULL;
    uint64_t N, net_N, net_C = -1, C;
    uint64_t pcc_connection[95] = {0};

    // Read N from the client 
    int_buff = (char *)&net_N;
    if (get_data(int_buff, 8) == -1)
    {
        return;
    }
    N = be64toh(net_N);
    
    
    // get data from client
    rcv_buff = malloc(N);
    if (get_data(rcv_buff, N) == -1)
    {
        free(rcv_buff);
        return;
    }

    C = 0;
    for (int i = 0; i < N; i++)
    {
        if (rcv_buff[i] > 31 && rcv_buff[i] < 127)
        {
            C++;
            pcc_connection[(int)(rcv_buff[i] - 32)]++;
        }
    }
    free(rcv_buff);

    //send C to client.
    int_buff =(char *)&net_C;
    net_C = htobe64(C);
    if (send_data(int_buff, 8) == -1)
    {
        return;
    }
    // Update pcc_total.
    for (int i = 0; i < 95; i++)
    {
        pcc_total[i] += pcc_connection[i];
    }
    close(conn_fd);
    conn_fd = -1;
}



void signal_handler(){
    if(conn_fd < 0){// there is a connction
        close_connection();
    }
    sigint = true;
}

int main(int argc,  char *argv[]){
    struct sockaddr_in serv_addr;
    socklen_t addrsize = sizeof(serv_addr);
    int listen_fd = -1;
    int rt = 1;
    struct sigaction sa;
    
    // check number of arguments
    if (argc != 2)
    {
        fprintf(stderr, "should get 2 arguments, Error: %s\n", strerror(EINVAL));
        exit(1);
    }
    //init signal handler
    sa.sa_handler = &signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if(sigaction(SIGINT, &sa, 0) != 0){
        printf("\n Error : sa faild. %s \n", strerror(errno));
        return 1;
    }


    //crate socket 
    listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if(listen_fd == -1){
        printf("\n Error : faild to create socket %s \n", strerror(errno));
        return 1;
    }
    if(setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &rt, sizeof(int)) == -1){
        printf("\n Error : setsocket faild %s \n", strerror(errno));
        return 1;
    }

    memset(&serv_addr, 0, addrsize);

    serv_addr.sin_family = AF_INET;
    // INADDR_ANY = any local machine address
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(atoi(argv[1]));

    //bind
    if(bind(listen_fd, (struct sockaddr *)&serv_addr, addrsize) == -1){
    printf("\n Error : Bind Failed. %s \n", strerror(errno));
    return 1;
  }

  if( 0 != listen(listen_fd, 10 )){
    printf("\n Error : Listen Failed. %s \n", strerror(errno));
    return 1;
  }

    while(1){
        if(sigint){
            close_connection();
        }

        conn_fd = accept(listen_fd, NULL, NULL);
        if(conn_fd < 0){
            printf("\n Error : accept Failed. %s \n", strerror(errno));
            return 1;
        }
        
        handle_connection();
    }
    close(conn_fd);
    conn_fd = -1;

}