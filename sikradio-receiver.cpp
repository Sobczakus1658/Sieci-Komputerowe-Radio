#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <semaphore.h>
#include <string>
#include <map>
#include <algorithm>
#include <unistd.h>
#include <stdint.h>
#include <sys/mman.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdlib.h>
#include <inttypes.h>
#include <netdb.h>
#include <time.h>
#include <fcntl.h>
#include <poll.h>
#include "utils.h"
#include "common.h"

#define BUFFER_SIZE 20
#define DATA_PORT 20009
#define BSIZE 65536
#define NAZWA "Nienazwany Nadajnik"
#define BROADCAST "255.255.255.255"
#define CTRL_PORT 30009
#define UI_PORT 10009
#define RTIME 250
#define CONNECTIONS 10
#define ARROW_SIZE 3
#define QUEUE_LENGTH 5
#define TIMEOUT 5000
#define character_mode "\377\375\042\377\373\001"

char *favourite_radio;
bool started;
bool finished = 0;
uint64_t usefull_bytes;
char *buffer;
bool *received_package;
bool set_byte0;
uint64_t reading_data;
uint64_t writing_data;
uint64_t max_received_package;
pthread_mutex_t sem;
pthread_mutex_t change_value;
pthread_mutex_t pipe_sem;
pthread_cond_t cond;
uint16_t port = DATA_PORT;
uint16_t psize = (uint16_t)BSIZE;
uint32_t bsize = (uint32_t)BSIZE;
uint64_t max_package;
bool psize_set = 0;
uint64_t counter;
int socket_fd_ctrl;
uint16_t ctrl_port = CTRL_PORT;
uint16_t UI_port = UI_PORT;
uint16_t rtime = RTIME;
const char *mcast_addr = BROADCAST;
bool first_station;
pthread_mutex_t playingMutex;
pthread_cond_t startPlaying;
int pipe_dsc[2];
int pipe_change[2];
uint64_t actual_session = 0;
char *retransmission_address;
uint64_t first_byte_num;
uint64_t BYTE0;
//localhost + sprawdzić czasy
const std::string HEADER =
    "------------------------------------------------------------------------"
    "\n\r\n\rSIK "
    "Radio\n\r\n\r-----------------------------------------------------------------"
    "--"
    "-----\n\r\n\r";
const std::string DOWN =
    "------------------------------------------------------------------------"
    "\n\r";
const std::string ARROW = " > ";
struct Station
{
    std::string address;
    std::string name;
    uint64_t port;

    bool operator==(const Station &other) const
    {
        return name == other.name && address == other.address && port == other.port;
    }

    bool operator<(const Station &other) const
    {
        if (name == other.name)
        {
            if (address == other.address)
            {
                if (port < other.port)
                {
                    return true;
                }
                else
                {
                    return false;
                }
            }
            else
            {
                if (address < other.address)
                {
                    return true;
                }
                else
                {
                    return false;
                }
            }
        }
        else
        {
            if (name < other.name)
            {
                return true;
            }
            else
            {
                return false;
            }
        }
    }
};

std::map<Station, size_t> availableStation;

struct Station currentStation;

int bind_socket(uint16_t port)
{
    int socket_fd = socket(AF_INET, SOCK_DGRAM, 0);
    ENSURE(socket_fd > 0);

    struct sockaddr_in server_address;
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = htonl(INADDR_ANY);
    server_address.sin_port = htons(port);

    CHECK_ERRNO(bind(socket_fd, (struct sockaddr *)&server_address,
                     (socklen_t)sizeof(server_address)));

    return socket_fd;
}

ssize_t read_audio(int socket_fd, struct sockaddr_in *client_address, char *buffer, size_t max_length)
{
    socklen_t address_length = (socklen_t)sizeof(*client_address);
    int flags = 0;
    errno = 0;
    ssize_t len = recvfrom(socket_fd, buffer, max_length, flags,
                           (struct sockaddr *)client_address, &address_length);

    if (len < 0)
    {
        PRINT_ERRNO();
    }
    return (ssize_t)len;
}

void send_message(int socket_fd, const struct sockaddr_in *client_address, const char *message, size_t length)
{
    socklen_t address_length = (socklen_t)sizeof(*client_address);
    int flags = 0;
    ssize_t sent_length = sendto(socket_fd, message, length, flags,
                                 (struct sockaddr *)client_address, address_length);
    ENSURE(sent_length == (ssize_t)length);
}

bool checkAddress(char *address)
{
    for( int i = 0; i < 4; i++){
        int16_t number = readNumberAddress(address);
        if( number > 255 || number ==- 1 ){
            exit(1);
        }
        if ( i != 3) {
            while(*address != '.'){
                address++;
            }
            address++;
        }
    }
    return 1;
}

void set_value(int argc, char *argv[])
{
    for (int i = 1; i < argc; i += 2)
    {
        if (!checkIfExist(argc, i + 1))
        {
            exit(1);
        }
        if (!strcmp(argv[i], "-d"))
        {
            if(strcmp(argv[i + 1], "localhost") == 0){
                mcast_addr = argv[i + 1];
            }
            else {
                if (checkAddress(argv[i + 1]))
                {
                    mcast_addr = argv[i + 1];
                }
                else
                {
                    exit(1);
                }
            }
        }
        else if (!strcmp(argv[i], "-C"))
        {
            if (checkPositiveNumber(argv[i + 1]))
            {
                ctrl_port = read_port(argv[i + 1]);
            }
            else
            {
                exit(1);
            }
        }
        else if (!strcmp(argv[i], "-U"))
        {
            if (checkPositiveNumber(argv[i + 1]))
            {
                UI_port = read_port(argv[i + 1]);
            }
            else
            {
                exit(1);
            }
        }
        else if (!strcmp(argv[i], "-b"))
        {
            if (checkPositiveNumber(argv[i + 1]))
            {
                bsize = strtoull(argv[i + 1], NULL, 10);
            }
            else
            {
                exit(1);
            }
        }
        else if (!strcmp(argv[i], "-R"))
        {
            if (checkPositiveNumber(argv[i + 1]))
            {
                rtime = strtoull(argv[i + 1], NULL, 10);
            }
            else
            {
                exit(1);
            }
        }
        else if (!strcmp(argv[i], "-n"))
        {
            if (checkName(argv[i + 1]))
            {
                favourite_radio = argv[i + i];
            }
            else
            {
                exit(1);
            }
        }
        else
        {
            exit(1);
        }
    }
    if (favourite_radio == NULL) {
        char empty[1];
        empty[0] = '\0';
        favourite_radio = empty;
    }
}

void reset_data()
{
    pthread_mutex_lock(&change_value);
    memset(buffer, 0, bsize);
    writing_data = 0;
    reading_data = 0;
    // psize = (uint16_t)BSIZE;
    psize_set = 0;
    memset(received_package, 0, bsize);
    actual_session = 0;
    max_received_package = 0;
    set_byte0 = false;
    started = 0;
    counter = 0;
    pthread_mutex_unlock(&change_value);
}

int interested(char *buffer)
{
    uint64_t *session_id = (uint64_t *)buffer;
    pthread_mutex_lock(&change_value);
    if (*session_id < actual_session)
    {
        pthread_mutex_unlock(&change_value);
        return 0;
    }
    if (*session_id > actual_session)
    {
        pthread_mutex_unlock(&change_value);
        reset_data();
        BYTE0 = be64toh(*(((uint64_t *)buffer) + 1));
        first_byte_num = BYTE0;
        pthread_mutex_lock(&change_value);
        actual_session = *session_id;
        pthread_mutex_unlock(&change_value);
        return 1;
    }
    pthread_mutex_unlock(&change_value);
    return 1;
}

void missing_package(uint64_t actual_byte)
{
    received_package[actual_byte % (bsize / psize)] = 1;
    for (uint64_t i = max_received_package + 1; i < actual_byte; i++)
    {
        received_package[i % (bsize / psize)] = 0;
    }
}

uint64_t set_buffer(char *temporary_buffer, char *buffer, uint64_t index)
{
    uint64_t first_num_byte = be64toh(*(((uint64_t *)temporary_buffer) + 1));
    pthread_mutex_lock(&change_value);
    if(first_num_byte >= BYTE0){
        missing_package((first_num_byte-BYTE0)/ psize);
    } else {
        pthread_mutex_unlock(&change_value);
        return first_num_byte;
    }
    memcpy(buffer + index, temporary_buffer + 16, psize);
    (writing_data) = ((writing_data) + psize) % usefull_bytes;
    if (first_num_byte > BYTE0 &&  (first_num_byte - BYTE0) / psize > max_received_package)
    {
        max_received_package = (first_num_byte - BYTE0) / psize;
    }
    first_byte_num = first_num_byte;
    pthread_mutex_unlock(&change_value);
    return first_num_byte;
}

void create_message_lookup(char *buffer)
{
    strcpy(buffer, "ZERO_SEVEN_COME_IN");
}

void create_message_rexmit(char *buffer)
{
    strcpy(buffer, "LOUDER_PLEASE 512,1024,1536");
}

void send_message_udp(int socket_fd, const struct sockaddr_in *client_address, const char *message, size_t length)
{
    socklen_t address_length = (socklen_t)sizeof(*client_address);
    int flags = 0;
    ssize_t sent_length = sendto(socket_fd, message, length, flags,
                                 (struct sockaddr *)client_address, address_length);
    ENSURE(sent_length == (ssize_t)length);
}

void *sender(void *data)
{
    (void) data;
    char buffer[BSIZE];
    struct sockaddr_in remote_address;
    remote_address.sin_family = AF_INET;
    remote_address.sin_port = htons(ctrl_port);
    inet_aton(mcast_addr, &remote_address.sin_addr);
    int optval = 0;
    setsockopt(socket_fd_ctrl, SOL_IP, IP_MULTICAST_LOOP, (void *)&optval, sizeof(optval));
    create_message_lookup(buffer);

    while (true)
    {
        send_message_udp(socket_fd_ctrl, &remote_address, buffer, 20);
        sleep(1);
    }
}

void addStation(std::string address, uint16_t port, std::string name)
{   
    Station newStation = {address, name, port};
    if (availableStation.empty())
    {
        currentStation = newStation;
        std::pair<Station, size_t> new_station = {newStation, time(NULL)};
        availableStation.insert(new_station);
        first_station = true;
        usleep(10);

        pthread_mutex_lock(&pipe_sem);
        ASSERT_SYS_OK(write(pipe_dsc[1], "new_station", 11));
        ASSERT_SYS_OK(write(pipe_change[1], "reload", 6));
        pthread_mutex_unlock(&pipe_sem);
        return;
    }
    auto it = availableStation.find(newStation);
        if (it != availableStation.end()) {
             it->second = time(NULL);
             return;
        }
        std::pair<Station, time_t> station_with_time = {
        {address,name, port}, time(NULL)};
        availableStation.insert(station_with_time);
        usleep(10);
        pthread_mutex_lock(&pipe_sem);
        ASSERT_SYS_OK(write(pipe_dsc[1], "new_station", 11));
        pthread_mutex_unlock(&pipe_sem);
        if (strcmp(name.c_str(), favourite_radio) == 0) {
            currentStation = newStation;
            usleep(10);
            pthread_mutex_lock(&pipe_sem);
            ASSERT_SYS_OK(write(pipe_change[1], "reload", 6));
            pthread_mutex_unlock(&pipe_sem);
        }
}

void *receiver_reply(void *data)
{
    (void) data;
    char buffer[10000];
    do
    {
        read_message_udp(socket_fd_ctrl, NULL, buffer, 10000);
        if (buffer[0] == 'B')
        {
            std::string line(buffer + 14);
            size_t space_position = line.find(' ');
            
            std::string address = line.substr(0, space_position);
            
            line = line.substr(space_position + 1);
            
            space_position = line.find(' ');
            
            uint16_t port = std::stoi(line.substr(0, space_position));
            
            std::string name = line.substr(space_position + 1);
            
            addStation(address, port, name);
        }
    } while (true);
}

void *writing(void *data)
{
    (void)data;
    while (true)
    {
        pthread_cond_wait(&cond, &sem);
        pthread_mutex_lock(&change_value);
        while (writing_data != reading_data)
        {
            if(!received_package[reading_data/psize])
            {
                pthread_mutex_unlock(&change_value);
                reset_data();
                break;
            }
            fwrite(buffer + (reading_data), sizeof(char), psize, stdout);
            (reading_data) = ((reading_data) + psize) % usefull_bytes;
            pthread_mutex_unlock(&change_value);
            pthread_mutex_lock(&change_value);
        }
        pthread_mutex_unlock(&change_value);
    }
}

bool check(struct sockaddr_in send_address, struct sockaddr_in excpeted_address)
{
    return send_address.sin_addr.s_addr == excpeted_address.sin_addr.s_addr;
}

void display_stations(pollfd *poll_descriptors)
{
    for (int i = 2; i < CONNECTIONS; ++i)
    {
        if (poll_descriptors[i].fd != -1)
        {
            send_message(poll_descriptors[i].fd, "\033[2J\033[H", 8, 0);
            send_message(poll_descriptors[i].fd, HEADER.c_str(),
                         HEADER.size(), 0);

            auto it = availableStation.begin();
            while (it != availableStation.end())
            {
                std::string name = it->first.name;
                if (it->first == currentStation)
                {
                    name = ARROW + name;
                }
                name += "\n\r\n\r";
                send_message(poll_descriptors[i].fd, name.c_str(), name.size(), 0);
                it++;
            }

            send_message(poll_descriptors[i].fd, DOWN.c_str(),
                         DOWN.size(), 0);
        }
    }
}

void *delete_stations(void *data) {
    (void)data;  

    while (true) {
        sleep(1);
        bool reload = false;
        pthread_mutex_lock(&playingMutex);
        for (auto it = availableStation.begin(); it != availableStation.end();) {
            if (time(NULL) - it->second > 20) {
                bool current = false;
                if (it->first == currentStation) {
                    current = true;
                }
                it = availableStation.erase(it);
                reload = true;
                if (current) {
                    if (availableStation.empty()) {
                        currentStation = {"", "", 0};
                    } else {
                        currentStation = availableStation.begin()->first;
                    }
                }
                pthread_mutex_lock(&pipe_sem);
                ASSERT_SYS_OK(write(pipe_dsc[1], "reload", 6));
                pthread_mutex_unlock(&pipe_sem);
            } else {
                it++;
            }
        }
        pthread_mutex_unlock(&playingMutex);
        if(reload) {
            usleep(10);
            pthread_mutex_lock(&pipe_sem);
            ASSERT_SYS_OK(write(pipe_change[1], "reload", 6));
            pthread_mutex_unlock(&pipe_sem);
        }
    }
}

void *telnet(void *data)
{   (void) data;
    char buf[CONNECTIONS][ARROW_SIZE];
    char buffer[11];

    struct pollfd poll_descriptors[CONNECTIONS];
    for (int i = 0; i < CONNECTIONS; ++i)
    {
        poll_descriptors[i].fd = -1;
        poll_descriptors[i].events = POLLIN;
        poll_descriptors[i].revents = 0;
    }
    size_t active_clients = 0;

    poll_descriptors[0].fd = open_tcp_socket();

    bind_socket(poll_descriptors[0].fd, UI_port);

    start_listening(poll_descriptors[0].fd, QUEUE_LENGTH);

    poll_descriptors[1].fd = pipe_dsc[0];

    do
    {
        for (int i = 0; i < CONNECTIONS; ++i)
        {
            poll_descriptors[i].revents = 0;
        }

        int poll_status = poll(poll_descriptors, CONNECTIONS, TIMEOUT);
        if (poll_status == -1)
        {
            if (errno == EINTR)
                fprintf(stderr, "Interrupted system call\n");
            else
                PRINT_ERRNO();
        }
        else if (poll_status > 0)
        {
            if (poll_descriptors[0].revents & POLLIN)
            {
                int client_fd = accept_connection(poll_descriptors[0].fd, NULL);

                CHECK_ERRNO(fcntl(client_fd, F_SETFL, O_NONBLOCK)); /* tryb nieblokujący */

                bool accepted = false;
                for (int i = 2; i < CONNECTIONS; ++i)
                {
                    if (poll_descriptors[i].fd == -1)
                    {
                        poll_descriptors[i].fd = client_fd;
                        poll_descriptors[i].events = POLLIN;
                        active_clients++;
                        accepted = true;
                        send_message(poll_descriptors[i].fd, character_mode, 6, 0);
                        display_stations(poll_descriptors);
                        break;
                    }
                }
                if (!accepted)
                {
                    CHECK_ERRNO(close(client_fd));
                }
            }
            for (int i = 2; i < CONNECTIONS; ++i)
            {
                if (poll_descriptors[i].fd != -1 &&
                    (poll_descriptors[i].revents & (POLLIN)))
                {
                    int flags = 0;
                    size_t read_length = receive_message(
                        poll_descriptors[i].fd, buf[i], ARROW_SIZE, flags);
                    if (read_length > 0)
                    {
                        if (buf[i][0] == '\033' && buf[i][1] == '[')
                        {
                            if (buf[i][2] == 'A')
                            {
                                if(availableStation.empty() || availableStation.size() == 1){
                                    continue;
                                }
                                auto it = availableStation.find(currentStation);
                                if (it == availableStation.begin())
                                {
                                    it = availableStation.end();
                                }
                                it--;
                                currentStation = it->first;
                                usleep(10);
                                pthread_mutex_lock(&pipe_sem);
                                ASSERT_SYS_OK(write(pipe_change[1], "reload", 6));
                                pthread_mutex_unlock(&pipe_sem);
                                display_stations(poll_descriptors);
                            }
                            else if (buf[i][2] == 'B')
                            {
                                if(availableStation.empty() || availableStation.size() == 1){
                                    continue;
                                }
                                auto it = availableStation.find(currentStation);
                                it++;
                                if (it == availableStation.end())
                                {
                                    it = availableStation.begin();
                                }
                                currentStation = it->first;
                                usleep(10);
                                pthread_mutex_lock(&pipe_sem);
                                ASSERT_SYS_OK(write(pipe_change[1], "reload", 6));
                                pthread_mutex_unlock(&pipe_sem);
                                display_stations(poll_descriptors);
                            }
                        }
                    }
                }
            }
            if (poll_descriptors[1].fd != -1 &&
                (poll_descriptors[1].revents & (POLLIN)))
            {
                memset(buffer, 0, 11);
                ASSERT_SYS_OK(read(poll_descriptors[1].fd, buffer, 11));
                display_stations(poll_descriptors);
            }
        }
    } while (true);

    if (poll_descriptors[0].fd >= 0)
        CHECK_ERRNO(close(poll_descriptors[0].fd));
    exit(EXIT_SUCCESS);
}

void create_retransmission(char ** message){
    std::string napis = "LOUDER_PLEASE ";

    int start = BYTE0; 
    if (first_byte_num - BYTE0 > max_package * psize) {
        start = first_byte_num - max_package * psize;
    }
    bool first = true;
    for (uint64_t i = start; i < first_byte_num; i += psize ) {
        if (!received_package[(i/psize)%max_package]) {
            if (!first) napis += ",";
            else first = false;
            napis += std::to_string(i);
        }
    }
        *message = new char[napis.length() + 1];
    
    strcpy(*message, napis.c_str());
}

void *retransmission(void *data){
    (void) data;
    while(true){
        usleep(rtime * 1000);
        char *message;
        create_retransmission(&message);
        if(strlen(message) > 14 && strcmp(retransmission_address, "") != 0){
            struct sockaddr_in remote_address;
            remote_address.sin_family = AF_INET;
            remote_address.sin_port = htons(ctrl_port);
            inet_aton(retransmission_address, &remote_address.sin_addr);
            send_message_udp(socket_fd_ctrl, &remote_address, message, strlen(message));
            free(message);
        }
    }
    while(true){}
}

void initial_data()
{
    reading_data = 0;
    writing_data = 0;
    max_received_package = 0;
    pthread_cond_init(&cond, NULL);
    pthread_mutex_init(&sem, NULL);
    pthread_mutex_init(&change_value, NULL);
    pthread_mutex_init(&playingMutex, NULL);
    pthread_mutex_init(&pipe_sem, NULL);
    retransmission_address =static_cast<char *>(std::calloc(16, 1));
    set_byte0 = false;
    started = 0;
    pthread_t writing_thread;
    pthread_t sender_data;
    pthread_t reply_data;
    pthread_t telnet_thread;
    pthread_t delete_thread;
    pthread_t retransmission_thread;
    socket_fd_ctrl = open_udp_socket();
    int optval = 1;
    first_station = false;
    CHECK_ERRNO(setsockopt(socket_fd_ctrl, SOL_SOCKET, SO_BROADCAST, (void *)&optval, sizeof optval));
    pthread_create(&reply_data, NULL, receiver_reply, NULL);
    pthread_create(&writing_thread, NULL, writing, NULL);
    pthread_create(&sender_data, NULL, sender, NULL);
    pthread_create(&telnet_thread, NULL, telnet, NULL);
    pthread_create(&delete_thread, NULL, delete_stations, NULL);
    pthread_create(&retransmission_thread, NULL, retransmission, NULL);
    ASSERT_SYS_OK(pipe(pipe_change));
    ASSERT_SYS_OK(pipe(pipe_dsc));
}

int main(int argc, char *argv[])
{
    set_value(argc, argv);
    initial_data();
    char temporary_buffer[bsize];
    received_package = static_cast<bool *>(std::calloc(bsize + 1, 1));
    buffer = static_cast<char *>(std::calloc(bsize + 1, 1));
    BYTE0 = 0;

    uint64_t first_byte;
    counter = 0;

    struct pollfd poll_descriptors[2];
    poll_descriptors[0].fd = pipe_change[0];
    poll_descriptors[1].fd = -1;

    for (int i = 0; i < 2; ++i) {
        poll_descriptors[i].events = POLLIN;
        poll_descriptors[i].revents = 0;
    }

    struct ip_mreq ip_mreq;
    int64_t audio_length;
    char* mcast_addr;
    do
    {
        for(int i = 0; i<2; i++) {
            poll_descriptors[i].revents = 0;
        }
        int poll_status = poll(poll_descriptors, 2, -1);
        if (poll_status == -1) {
            if (errno == EINTR){}
        } else if (poll_status > 0) {
             if (poll_descriptors[0].revents & POLLIN) {
                char buffer[7];
                memset(buffer, 0, 7);
                ASSERT_SYS_OK(read(poll_descriptors[0].fd, buffer, 6));
                if (strcmp(buffer, "reload") == 0) {
                    if (poll_descriptors[1].fd != -1) {
                        CHECK_ERRNO(setsockopt(poll_descriptors[1].fd,
                                               IPPROTO_IP, IP_DROP_MEMBERSHIP,
                                               (void *)&ip_mreq,
                                               sizeof(ip_mreq)));
                        CHECK_ERRNO(close(poll_descriptors[1].fd));
                        poll_descriptors[1].fd = -1;
                    }
                    poll_descriptors[1].fd = open_udp_socket();

                    set_port_reuse(poll_descriptors[1].fd);

                    pthread_mutex_lock(&playingMutex);
                    mcast_addr = const_cast<char*>(currentStation.address.c_str());
                    uint16_t data_port = currentStation.port;
                    pthread_mutex_unlock(&playingMutex);

                    if(strcmp(mcast_addr, "")!=0){
                        ip_mreq.imr_interface.s_addr = htonl(INADDR_ANY);
                        if (inet_aton(mcast_addr, &ip_mreq.imr_multiaddr) == 0) {
                            fatal("inet_aton - invalid multicast address\n");
                        }

                        CHECK_ERRNO(setsockopt(poll_descriptors[1].fd, IPPROTO_IP,
                                           IP_ADD_MEMBERSHIP, (void *)&ip_mreq,
                                           sizeof ip_mreq));
                        struct timeval timeout;
                        timeout.tv_sec = 2;
                        timeout.tv_usec = 0; 
                        setsockopt(poll_descriptors[1].fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));    

                        reset_data();

                        bind_socket(poll_descriptors[1].fd, data_port);
                    } else {
                        poll_descriptors[1].fd = -1;
                    }
                    
                }
            }
        if (poll_descriptors[1].fd != -1 &&
                ((poll_descriptors[1].revents & POLLIN)))
        {   
            struct sockaddr_in client_address;
            audio_length = read_message_udp(poll_descriptors[1].fd, &client_address, temporary_buffer, sizeof(temporary_buffer));
            if(audio_length <= 16) 
                continue;
            int decision = interested(temporary_buffer);
            if (decision == 1)
            {
                retransmission_address = inet_ntoa(client_address.sin_addr);
                if (!psize_set)
                {
                    pthread_mutex_lock(&change_value);
                    psize_set = 1;
                    psize = audio_length - 2 * sizeof(uint64_t);
                    psize = std::max(static_cast<uint16_t>(1), psize);
                    usefull_bytes = (bsize / psize) * psize;
                    max_package = bsize / psize;
                    pthread_mutex_unlock(&change_value);
                }
                if (!set_byte0)
                {
                    uint64_t first_num_byte = be64toh(*(((uint64_t *)temporary_buffer) + 1));
                    pthread_mutex_lock(&change_value);
                    max_received_package = (first_num_byte-BYTE0)/ psize;
                    set_byte0 = 1;
                    pthread_mutex_unlock(&change_value);
                }
                first_byte = set_buffer(temporary_buffer, buffer, counter);
                counter = (counter + psize) % usefull_bytes;
                if (started)
                {
                    pthread_cond_signal(&cond);
                }

                if (BYTE0 + bsize * 3 / 4 < first_byte + psize && !started)
                {
                    pthread_cond_signal(&cond);
                    started = 1;
                }
            }
        }
        }
    } while (true);
    pthread_cond_signal(&cond);
    free(buffer);
    free(received_package);
    pthread_cond_init(&cond, NULL);
    pthread_mutex_destroy(&sem);
    pthread_mutex_destroy(&change_value);
    pthread_cond_destroy(&cond);
    return 0;
}