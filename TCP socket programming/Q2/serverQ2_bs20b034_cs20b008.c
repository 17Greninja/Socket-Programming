#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <fcntl.h>
#include <ctype.h>

#define BUFFER_SIZE 1024
#define MAX_REQUEST_SIZE 2048
char buffer[MAX_REQUEST_SIZE] = {0};
char root_dir[500];
char check[100] = "name=\"message\"";

char* extract_data(const char* input) {
    const char* start_delimiter = "%**%";
    const char* end_delimiter = "%**%";

    // Find the start delimiter
    const char* start_ptr = strstr(input, start_delimiter);
    if (start_ptr == NULL) {
        return NULL; // Start delimiter not found
    }
    start_ptr += strlen(start_delimiter);

    // Find the end delimiter
    const char* end_ptr = strstr(start_ptr, end_delimiter);
    if (end_ptr == NULL) {
        return NULL; // End delimiter not found
    }

    // Calculate the length of the data between delimiters
    size_t data_length = end_ptr - start_ptr;

    // Allocate memory for the extracted data (+1 for null terminator)
    char* extracted_data = (char*)malloc(data_length + 1);
    if (extracted_data == NULL) {
        return NULL; // Memory allocation failed
    }

    // Copy the data between delimiters
    strncpy(extracted_data, start_ptr, data_length);
    extracted_data[data_length] = '\0'; // Null-terminate the string

    return extracted_data;
}

// Function to send a file over the socket
void send_file(int client_socket, const char *file_path) {
    char buffer[BUFFER_SIZE];
    ssize_t bytes_read;
    int fd = open(file_path, O_RDONLY);
    // if (fd == -1) {
    //     perror("open");
    //     exit(EXIT_FAILURE);
    // }
    while ((bytes_read = read(fd, buffer, BUFFER_SIZE)) > 0) {
        if (send(client_socket, buffer, bytes_read, 0) == -1) {
            perror("send");
            exit(EXIT_FAILURE);
        }
    }
    close(fd);
}

// Function to handle a GET request
void handle_get_request(int client_socket, const char *file_path) {
    // char final_file_path[500] = "./webroot/web1/";
    char final_file_path[500];
    strcpy(final_file_path, root_dir);
    if (strcmp(file_path, "/") == 0) {
        strcat(final_file_path, "/index.html");    
    }
    else{
        strcat(final_file_path, file_path);
    }
    // file_path = "./webroot/web1/images/name.jpg";

    // Determine MIME type based on file extension
    const char *mime_type = "text/plain";
    if (strstr(final_file_path, ".html") != NULL) {
        mime_type = "text/html";
    } else if (strstr(final_file_path, ".css") != NULL) {
        mime_type = "text/css";
    } else if (strstr(final_file_path, ".js") != NULL) {
        mime_type = "application/javascript";
    } else if (strstr(final_file_path, ".jpg") != NULL || strstr(final_file_path, ".jpeg") != NULL) {
        mime_type = "image/jpeg";
    } else if (strstr(final_file_path, ".png") != NULL) {
        mime_type = "image/png";
    }

    // Check if the requested file exists, if not, serve 404.html
    if (access(final_file_path, F_OK) != -1) {
        char response_header[512];
        sprintf(response_header, "HTTP/1.1 200 OK\r\nContent-Type: %s\r\n\r\n", mime_type);
        send(client_socket, response_header, strlen(response_header), 0);
        send_file(client_socket, final_file_path);
    } 
    else {
        send(client_socket, "HTTP/1.1 404 Not Found\r\n\r\n", 26, 0);
        char file_404[500];
        strcpy(file_404, root_dir);
        strcat(file_404, "/404.html");
        send_file(client_socket, file_404);
    }
}

typedef struct {
    int num_characters;
    int num_words;
    int num_sentences;
} TextInfo;

TextInfo analyze_text(const char *text) {
    TextInfo info = {0};

    int text_length = strlen(text);
    int in_word = 0; // Flag to indicate if currently in a word

    for(int i=0;i<text_length;i++){
        if(text[i] == ' '){ 
            info.num_words++;
        }
        else if(text[i] == '\n'){
            info.num_sentences++;
            info.num_words++;//since with every next line new words start. 
        }
        else if(text[i] != ' ' && text[i] != '\n'){
            info.num_characters++;
        }
    }
    if(info.num_characters > 0)
    {
        info.num_words++;
        info.num_sentences++;
    }
    info.num_characters -= (info.num_sentences-1);
    return info;
}

void handle_post_request(int client_socket) {
    char buffer[1024] = {0};
    read(client_socket, buffer, 1024);
    if(strstr(buffer, check) != NULL){
        char* extracted_data = extract_data(buffer);
        printf("Extracted data: \n%s\n", extracted_data);
        TextInfo info = analyze_text(extracted_data);
        printf("Number of characters: %d\n", info.num_characters);
        printf("Number of words: %d\n", info.num_words);
        printf("Number of sentences: %d\n", info.num_sentences);
    } 
}

// Main function to handle client connections
void *handle_connection(void *arg) {
    int client_socket = *((int *)arg);
    char request_buffer[BUFFER_SIZE];
    int len;
    char msg[500];

    len = recv(client_socket, msg, 500, 0);
    msg[len] = '\0';
    char method[5], file_path[256];
    sscanf(msg, "%s %s", method, file_path);
    handle_get_request(client_socket, file_path);

    handle_post_request(client_socket);
    close(client_socket);
    free(arg);
    pthread_exit(NULL);
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <port> <root_directory>\n", argv[0]);
        return EXIT_FAILURE;
    }

    int port = atoi(argv[1]);
    char *root_directory = argv[2];

    strcpy(root_dir, argv[2]);

    int server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == -1) {
        perror("socket");
        return EXIT_FAILURE;
    }

    struct sockaddr_in server_address, client_address;
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = INADDR_ANY;
    server_address.sin_port = htons(port);

    if (bind(server_socket, (struct sockaddr *)&server_address, sizeof(server_address)) == -1) {
        perror("bind");
        return EXIT_FAILURE;
    }

    if (listen(server_socket, 10) == -1) {
        perror("listen");
        return EXIT_FAILURE;
    }

    printf("Server listening on port %d\n", port);

    while (1) {
        socklen_t client_address_size = sizeof(client_address);
        int *client_socket = malloc(sizeof(int));
        *client_socket = accept(server_socket, (struct sockaddr *)&client_address, &client_address_size);
        if (*client_socket == -1) {
            perror("accept");
            continue;
        }

        pthread_t tid;
        if (pthread_create(&tid, NULL, handle_connection, client_socket) != 0) {
            perror("pthread_create");
            close(*client_socket);
            free(client_socket);
        }
    }

    close(server_socket);
    return EXIT_SUCCESS;
}
