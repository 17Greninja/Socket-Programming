#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <pthread.h>
#include <time.h>
#include <sys/time.h>

#define BUFFER_SIZE 128

typedef struct {
    char title[31];
    char artist[31];
    char album[31];
    char genre[31];
} ID3Tags;

int max_num_client;
int curr = 0;

void extractID3Tags(FILE *mp3File, ID3Tags *tags) {
    fseek(mp3File, -128, SEEK_END); // ID3 tag size is usually 128 bytes from the end of the file

    // Read the last 128 bytes of the file
    char buffer[BUFFER_SIZE];
    fread(buffer, BUFFER_SIZE, 1, mp3File);

    // Check if the buffer contains an ID3v1 tag
    if (memcmp(buffer, "TAG", 3) == 0) {
        strncpy(tags->title, buffer + 3, 30);
        strncpy(tags->artist, buffer + 33, 30);
        strncpy(tags->album, buffer + 63, 30);
        strncpy(tags->genre, buffer + 93, 30);
    } else {
        strcpy(tags->title, "Unknown");
        strcpy(tags->artist, "Unknown");
        strcpy(tags->album, "Unknown");
        strcpy(tags->genre, "Unknown");
    }
}

void extract(char * filename){
    FILE *mp3File = fopen(filename, "rb");
    if (!mp3File) {
        printf("Error opening file.\n");
        return;
    }

    ID3Tags tags;
    extractID3Tags(mp3File, &tags);

    printf("\nTitle: %s\n", tags.title);
    printf("Artist: %s\n", tags.artist);
    printf("Album: %s\n", tags.album);
    // printf("Genre: %s\n", tags.genre);

    fclose(mp3File);
}

pthread_mutex_t mutex;
pthread_mutex_t mutex1;


int portNumber = 8800;
// int max_num_client = 10;
char song_directory[100] = "./songs/";

int create_socket(){
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    return sock;
}

char* songs[10];

void *handleClient(void *client_sock){
    pthread_mutex_lock(&mutex1);
    curr++;
    pthread_mutex_unlock(&mutex1);
    int client_fd = *((int *)client_sock);
    char msg[500];
    int len;
    len = recv(client_fd,msg,500,0);
    msg[len] = '\0';
    int songNumber = atoi(msg);
    printf("Song played: %s", songs[songNumber-1]);
    extract(songs[songNumber-1]);
    // send_mp3_file(client_fd);
    FILE *file = fopen(songs[songNumber-1], "rb");
    if (!file) {
        perror("Error opening file");
        exit(EXIT_FAILURE);
    }
    char buffer[4096];
    ssize_t bytes_read;

    while ((bytes_read = fread(buffer, 1, sizeof(buffer), file)) > 0) {
        if (send(client_fd, buffer, bytes_read, MSG_NOSIGNAL) != bytes_read) {
            // perror("Error sending data");
            // exit(EXIT_FAILURE);
            // return (void *);
            continue;;
        }
    }

    pthread_mutex_lock(&mutex1);
    curr--;
    pthread_mutex_unlock(&mutex1);

    // if (bytes_read == -1) {
    //     perror("Error reading file");
    //     // exit(EXIT_FAILURE);
    // }

    fclose(file);
    pthread_exit(NULL);


    // pthread_mutex_lock(&mutex);
    // curr--;
    // pthread_mutex_unlock(&mutex);
    // close(client_fd);
}

int main(int argc, char* argv[]){
    if(argc != 4){
        perror("Incorrect number of arguments.");
        exit(EXIT_FAILURE);
    }

    int portNumber = atoi(argv[1]);
    // portNumber = 8800;
    curr++;// printf("%s", song_dir);
    max_num_client = atoi(argv[2]);
    char song_dir[100];

    strcpy(song_dir, argv[3]);

    // printf("%s", song_dir);

    for(int i=0;i<9;i++){
        songs[i] = (char *)malloc(100);
        char mySong[50];
        strcpy(mySong, song_dir);
        strcat(mySong, "/song_");
        char str[20]; // Assuming a sufficiently large array to store the string

        sprintf(str, "%d", i+1);
        strcat(mySong, str);
        strcat(mySong, ".mp3");
        strcpy(songs[i], mySong);
    }

    int server_fd;
    struct sockaddr_in server_addr;
    int option = 1;
    int addrlen = sizeof(server_addr);

    int newSocket;
	struct sockaddr_in newAddr;
    socklen_t addr_size;

    server_fd = create_socket();
    if(server_fd == -1){
        perror("Server: Socket creation failed");
        exit(EXIT_FAILURE);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(portNumber);
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    // Bind to the set port and IP:
    if(bind(server_fd, (struct sockaddr*)&server_addr, sizeof(server_addr))<0){
        printf("Couldn't bind to the port\n");
        return -1;
    }

    if(listen(server_fd, max_num_client) == 0){
		printf("[+]Listening....\n");
	}
    else{
		perror("[-]Error in binding.\n");
	}


    int check = 0;

    while(1){
        // printf("%d - value of current", curr);
        if(curr >= max_num_client){
            pthread_mutex_lock(&mutex);
            if(check == 0)printf("Max limit reached, please try again later");
            check = 1;
            pthread_mutex_unlock(&mutex);
            continue;
        }
        pthread_mutex_lock(&mutex);
        check = 0;
        pthread_mutex_unlock(&mutex);
        struct sockaddr_in address;
        int addrlen = sizeof(address);
        if(curr < max_num_client){
            newSocket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen);
            // ip address of the client has to be displayed.
            char client_ip[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &(address.sin_addr), client_ip, INET_ADDRSTRLEN);
            printf("Client IP address: %s\n", client_ip);

            pthread_mutex_lock(&mutex);
            // curr++;
            pthread_t myThread;
            pthread_create(&myThread,NULL,(void *)handleClient,&newSocket);
            pthread_mutex_unlock(&mutex);
        }
    }
    close(server_fd);
    return 0;
}


