#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <endian.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <assert.h>
#include <inttypes.h>

int main(int argc, char *argv[]){
    FILE *file;
	int socket_fd = -1;
	char *buff;
	int byte_send, byte_got, cur_bytes;
	char *data_to_send;
	uint64_t N, net_N, C = -1;
	struct sockaddr_in server_address;
	if (argc != 4) {
        perror("should recieve 4 arguments!");
        exit(1);
    }

    file = fopen(argv[3],"rb");
	if (file == NULL) {
        perror("faild to open the file");
        exit(1);
    }

    fseek(file, 0, SEEK_END);
	N = ftell(file);
	// saving input file as a dynamic-allocated array of chars
	fseek(file, 0, SEEK_SET);
	data_to_send = malloc(N);
	if (data_to_send == NULL){
		perror("Error malloc");
        exit(1);
	}
	if (fread(data_to_send, 1, N, file) != N){
		perror("ERROR reading from file");
        exit(1);
	}
	fclose(file);

    
    //Create a TCP connection to the specified server port on the specified server IP
    if ((socket_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
		perror("Failed in creating a Soket");
        exit(1);
	}

	memset(&server_address, 0, sizeof(server_address));
	inet_pton(AF_INET, argv[1], &server_address.sin_addr); // sets IP adress
	server_address.sin_family = AF_INET;
	server_address.sin_port = htons(atoi(argv[2])); // sets port

	// connect socket to the target address
	if (connect(socket_fd,
               (struct sockaddr *)&server_address,
                sizeof(server_address)) < 0)
    {
		perror("Socket connection to server failed");
        exit(1);
	}


    net_N = (htobe64(N));
	buff = (char*)&net_N;
	byte_send = 0;
	while(byte_send < 8){
		cur_bytes = write(socket_fd, buff + byte_send, 8 - byte_send);
		if(cur_bytes < 0){ 
			perror("Failed sending N to the server");
        	exit(1);
		}
		byte_send += cur_bytes;
	}
	// write file data to server
	byte_send = 0;
	while(byte_send < N){
		cur_bytes = write(socket_fd, data_to_send + byte_send, N - byte_send);
		if(cur_bytes < 0){
			perror("Failed in sending byte to server");
        	exit(1);
		}
		byte_send += cur_bytes;
	}
	free(data_to_send);
	
    // geting C from the server
    byte_got = 0;
    buff =(char *)&C; 
	while (byte_got < 8){
		cur_bytes = read(socket_fd, buff + byte_got, 8 - byte_got);
		if(cur_bytes < 0){
			perror("Faild in geting C from server");
        	exit(1);
		}
		byte_got += cur_bytes;
	}
	close(socket_fd);

    C = be64toh(C); 
    printf("# of printable characters: %lu\n", C);
	exit(0);

}       