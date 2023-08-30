#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>
#include <arpa/inet.h>
#include <time.h>
#include <vector>
#include <string>
#include "utils.h"
#include "common.h"
#include <pthread.h>
#include <map>
#include <iostream>

#define DATA_PORT 20009 
#define PSIZE 512   
#define BSIZE 65536
#define CTRL_PORT 30009
#define FSIZE 131072
#define NAME "Nienazwany Nadajnik"
#define BROADCAST "255.255.255.255"
#define RTIME 250
#define BUFFER_SIZE 20


char *host;
uint64_t begin_data;
uint16_t port = DATA_PORT;
uint16_t psize = PSIZE;
uint64_t fsize = FSIZE;
uint64_t max_size;
uint16_t bsize = (uint16_t) BSIZE;
const char* name = NAME;
char * queue;
char* mcast_addr;
uint64_t ctrl_port = CTRL_PORT;
std::vector < uint64_t > retransmission_buffer;
uint64_t rtime = RTIME; 
pthread_mutex_t  fifo;
std::map<uint64_t, uint64_t> first_byte_num_positions;
time_t start;


void checkAddress(){
    if(mcast_addr == NULL ){
        exit(1);
    }
}

void checkCorrectAddress(char *address){
    for( int i = 0; i < 4; i++){
        uint16_t number = readNumberAddress(address);
        if( number > 255 ){
            exit(1);
        }
        if ( i == 0 ) {
            if( number < 224 || number > 239) {
                exit(1);
            }
        }
        if ( i != 3) {
            while(*address != '.'){
                address++;
            }
        }
    }
}

void setValue(int argc, char *argv[]){
    for( int i = 1 ; i < argc; i+=2){
        if(!checkIfExist(argc, i+1)){
            exit(1);
        }
        if(!strcmp(argv[i],"-a")){
            checkCorrectAddress(argv[i+1]);
            mcast_addr = argv[i +1];
        }
        else if(!strcmp(argv[i],"-P")){
            if(checkPositiveNumber(argv[i + 1])) {
                port = read_port(argv[i + 1]);
            }
            else {
                exit(1);
            }
        }
        else if(!strcmp(argv[i],"-p")){
            if(checkPositiveNumber(argv[i + 1])){
                psize = strtoull(argv[i + 1], NULL, 10);
            } else {
                exit(1);
            }
        }
        else if(!strcmp(argv[i],"-n")){
            if (checkName(argv[i+1])) {
                name = argv[i + 1];
            } else {
                exit(1);
            }
        }
        else if(!strcmp(argv[i],"-C")){
            if(checkPositiveNumber(argv[i + 1])) {
                ctrl_port = read_port(argv[i + 1]);
            }
            else {
                exit(1);
            }
        }
        else if (!strcmp(argv[i],"-f")){
            if(checkPositiveNumber(argv[i + 1])) {
                fsize = strtoull(argv[i + 1], NULL, 10);
            }
            else {
                exit(1);
            }
        }
        else if(!strcmp(argv[i], "-R")){
            if(checkPositiveNumber(argv[i + 1])) {
                rtime = strtoull(argv[i + 1], NULL, 10);
            }
            else {
                exit(1);
            }
        }
        else {
            exit(1);
        }
    }
    checkAddress();
}

void send_message_udp(int socket_fd, const struct sockaddr_in *send_address, const char *message) {
    size_t message_length = psize +  2 * sizeof(uint64_t);
    if (message_length == BUFFER_SIZE) {
        fatal("parameters must be less than %d characters long", BUFFER_SIZE);
    }
    int send_flags = 0;
    socklen_t address_length = (socklen_t) sizeof(*send_address);
    errno = 0;
    ssize_t sent_length = sendto(socket_fd, message, message_length, send_flags,
                                 (struct sockaddr *) send_address, address_length);
    if (sent_length < 0) {
        PRINT_ERRNO();
    }
    ENSURE(sent_length == (ssize_t) message_length);
}

void send_audio(int socket_fd, const struct sockaddr_in *send_address, const char *message) {
    size_t audio_length = psize +  2 * sizeof(uint64_t);
    int send_flags = 0;
    socklen_t address_length = (socklen_t) sizeof(*send_address);
    errno = 0;
    ssize_t sent_length = sendto(socket_fd, message, audio_length, send_flags,
                                 (struct sockaddr *) send_address, address_length);
    if (sent_length < 0) {
        PRINT_ERRNO();
    }
    ENSURE(sent_length == (ssize_t) audio_length);
}

bool IsLookUp(char * buffer) {
    std::string s = buffer;
    std::string prefix = s.substr(0, 18);
    if(strcmp(prefix.c_str(),"ZERO_SEVEN_COME_IN") == 0) {
        return true;
    }
    return false;

}

bool isRexmit(char * buffer) {
    std::string s = buffer;
    std::string prefix = s.substr(0, 13);
    if(strcmp(prefix.c_str(), "LOUDER_PLEASE") == 0) {
        return true;
    }
    return false;
}

void setBegin(){
    begin_data = begin_data + psize;
    if ( begin_data >= max_size ) {
        begin_data = 0;
    }

}

uint64_t read_number(char **buffer){
    std::string char_number = "";
    while (**buffer >= '0' && **buffer <='9'){
        char_number = char_number + **buffer;
        (*buffer)++;
    } 
    if (*buffer != NULL || **buffer != '\0' ) {
        (*buffer)++;
    }
    uint64_t number = strtoull(char_number.c_str(), NULL, 10);
    return number;
}

void addRetransmission (char *buffer) {
    buffer = buffer + 14;
    while (buffer != NULL && *buffer != '\0' && *buffer != ' ') {
        uint64_t num = read_number(&buffer);
        retransmission_buffer.push_back(num);
    }
}

void sendReply(int socket_fd, struct sockaddr_in *address) {
    std::string package = "BOREWICZ_HERE ";
    package += mcast_addr;
    package += " ";
    package += std::to_string(port);
    package += " ";
    package += name;
    send_message_udp(socket_fd, address, package.c_str() );
}

void initQueue(){
    begin_data = 0;
    max_size = (fsize/psize) * psize;
    queue = static_cast<char*>(std::calloc(fsize, 1));
    pthread_mutex_init(&fifo, NULL);
}

void addQueue(char * audio_package, uint64_t first_byte) {
    pthread_mutex_lock(&fifo);
    memcpy(queue + begin_data, audio_package + 16, psize);
    first_byte_num_positions.insert({first_byte, begin_data});
    setBegin();
    auto it = first_byte_num_positions.find(first_byte - (fsize - fsize % psize));
    if ( it != first_byte_num_positions.end()){
        first_byte_num_positions.erase(it);
    }
    pthread_mutex_unlock(&fifo);
}

void * retransmission( void* data) {
    (void)data;  
    char buffer[psize + 16];
    uint64_t *session_id = (uint64_t *) buffer;
    uint64_t *converted_byte = session_id +1;
    uint64_t first_byte_num = 0;
    *session_id = htobe64(start);

    int socket = open_udp_socket();

    struct sockaddr_in remote_address;
    remote_address.sin_family = AF_INET;
    remote_address.sin_port = htons(port);
    if (inet_aton(mcast_addr, &remote_address.sin_addr) == 0) {
        fprintf(stderr, "ERROR: inet_aton - invalid multicast address\n");
        exit(EXIT_FAILURE);
    }
    while(true){
        usleep(rtime * 1000);
        pthread_mutex_lock(&fifo);
        for(size_t i = 0; i < retransmission_buffer.size(); i++){
            first_byte_num = retransmission_buffer.at(i);
            auto it = first_byte_num_positions.find(first_byte_num);
            if (it != first_byte_num_positions.end()){
                *converted_byte = htobe64(first_byte_num);
                memcpy(buffer+16, queue+it->second, psize);
                send_message_udp(socket, &remote_address, buffer);
            }
        }
        retransmission_buffer.clear();
        pthread_mutex_unlock(&fifo);
    }

}

void* receiver(void* data) {
    (void)data;  
    char retransmission_buffer[BSIZE];
    int socket_fd = open_udp_socket();
    struct sockaddr_in broadcast_address;
    broadcast_address.sin_family = AF_INET;
    broadcast_address.sin_port = htons(ctrl_port);
    if (inet_aton(mcast_addr, &broadcast_address.sin_addr) == 0) {
        fprintf(stderr, "ERROR: inet_aton - invalid multicast address\n");
        exit(EXIT_FAILURE);
    }

    int optval = 1;
    CHECK_ERRNO(setsockopt(socket_fd, SOL_SOCKET, SO_BROADCAST, (void *) &optval, sizeof optval));
    optval = 4;
    CHECK_ERRNO(setsockopt(socket_fd, IPPROTO_IP, IP_MULTICAST_TTL, (void *) &optval, sizeof optval));

    struct sockaddr_in client_address;
    set_port_reuse(socket_fd);
    bind_socket(socket_fd, ctrl_port);
    do {
        memset(retransmission_buffer,0,BSIZE);
        read_message_udp(socket_fd, &client_address, retransmission_buffer, sizeof(retransmission_buffer));
        if(IsLookUp(retransmission_buffer)){
            sendReply(socket_fd, &client_address);
        }
        if(isRexmit(retransmission_buffer)){
            addRetransmission(retransmission_buffer);
        }
    } while(true);
}

int main(int argc, char *argv[]) {
    start = time(NULL);
    setValue(argc,argv);
    initQueue();
    char audio_package[psize + 2*sizeof(uint64_t)];
    uint64_t *session_id = (uint64_t *) audio_package;

    pthread_t writing_data;
    pthread_create(&writing_data, NULL, receiver, NULL);

    pthread_t retransmission_data;
    pthread_create(&retransmission_data, NULL, retransmission, NULL);


    uint64_t *converted_byte = session_id +1;
    uint64_t first_byte_num = 0;
    *session_id = htobe64(start);

    char *multicast_dotted_address = mcast_addr;

    int socket = open_udp_socket();

    struct sockaddr_in remote_address;
    remote_address.sin_family = AF_INET;
    remote_address.sin_port = htons(port);
    if (inet_aton(multicast_dotted_address, &remote_address.sin_addr) == 0) {
        fprintf(stderr, "ERROR: inet_aton - invalid multicast address\n");
        exit(EXIT_FAILURE);
    }

    int optval = 5;
    setsockopt(socket, IPPROTO_IP, IP_MULTICAST_TTL, (void *) &optval, sizeof optval);

    while( fread(audio_package + 2 * sizeof(uint64_t), sizeof(char), psize, stdin) ==psize ){
        *converted_byte = htobe64(first_byte_num);
        send_message_udp(socket, &remote_address, audio_package);
        addQueue(audio_package, first_byte_num);
        first_byte_num += psize;
    }

    return 0;
}
