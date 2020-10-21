#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

#define ROUTER_FILE_CONFIG_NAME "roteador.config"
#define LINK_FILE_CONFIG_NAME "enlace.config"
#define BUFFER_LENGTH 100 //Max length of buffer
#define TRUE 1
#define FALSE 0

#define KILL_THIS_PROCESS '0'
#define SEND_A_NEW_MESSAGE '1'
#define READ_UNREAD_MESSAGES '2'
#define READ_ALL_MESSAGES '3'

struct
{
  int id;
  int port;
  char *ip_address;
  struct sockaddr_in socket;
  int socket_number;
} typedef Router;

struct
{
  int node_start;
  int node_destination;
  int weight;
} typedef Link;

void die(char *s)
{
  perror(s);
  exit(1);
}

char *read_file(char *filename);
Router **read_router_config(int *router_list_size);
Link **read_link_config(int *link_list_size);
Link *parse_line_to_link(char *line);
Router *parse_line_to_router(char *line);

void *listen_to_messages_thread(void *data);
void *send_messages_thread(void *data);
void *menu_interface_thread(void *data);

void clear_buffer(char *buffer);
int create_udp_socket();
int set_udp_configurations(Router *router_config);
int get_message_from_socket(int *socket_created, char *buffer_to_receive_messages, struct sockaddr_in *socket_client, socklen_t *socket_length);
void print_data_received_from_client(struct sockaddr_in *socket_client, char *buffer_message);
int send_response_to_client(int *socket_created, char *buffer_to_receive_messages, int *client_message_length, struct sockaddr_in *socket_client, socklen_t *socket_length);
void wait_and_lock_thread(pthread_mutex_t *mutex, pthread_cond_t *condition);
void lock_and_wake_next_thread(pthread_mutex_t *mutex, pthread_cond_t *condition);

pthread_mutex_t mutex1 = PTHREAD_MUTEX_INITIALIZER, mutex2 = PTHREAD_MUTEX_INITIALIZER;
pthread_t Thread_listen_to_messages, Thread_send_messages, Thread_menu_interface;
pthread_cond_t send_messages_cond, menu_cond;
// pthread_mutex_t lock;

void parse_args(int argc, char const *argv[], char **ip, int *port)
{
  if (argc < 2)
    return;

  if (argc % 2 == 0)
  {
    printf("Incorrect arguments\n");
    exit(1);
  }

  for (int i = 1; i < argc; i += 2)
  {
    if (strcmp(argv[i], "-p") == 0)
    {
      char *aux = (char *)malloc(strlen(argv[i + 1]) + 1);
      *port = atoi(argv[i + 1]);
    }
    else if (strcmp(argv[i], "--port") == 0)
    {
      *port = atoi(argv[i + 1]);
    }
    if (strcmp(argv[i], "-a") == 0)
    {
      *ip = (char *)malloc(strlen(argv[i + 1]) + 1);
      strcpy(*ip, argv[i + 1]);
    }
    else if (strcmp(argv[i], "--address") == 0)
    {
      *ip = (char *)malloc(strlen(argv[i + 1]) + 1);
      strcpy(*ip, argv[i + 1]);
    }
  }
}

Router create_main_router(int argc, char const *argv[]){
  char *ip;
  int port;

  parse_args(argc, argv, &ip, &port);

  Router router_config;
  int socket_created; 
  
  socket_created = create_udp_socket();

  router_config.id = -1;
  router_config.ip_address = "127.0.0.1"; //ip;
  router_config.port = port;
  router_config.socket_number = socket_created;

  set_udp_configurations(&router_config);

  free(ip);

  return router_config;
}

int main(int argc, char const *argv[])
{

  Router router_config;

  router_config = create_main_router(argc, argv);

  // int router_list_size; //, link_list_size;
  // Router **router_list = read_router_config(&router_list_size);
  // Link **link_list = read_link_config(&link_list_size);

  pthread_create(&Thread_listen_to_messages, NULL, listen_to_messages_thread, (void *)&router_config);
  pthread_create(&Thread_send_messages, NULL, send_messages_thread, (void *)&router_config);
  pthread_create(&Thread_menu_interface, NULL, menu_interface_thread, NULL);

  pthread_join(Thread_listen_to_messages, NULL);
  pthread_join(Thread_send_messages, NULL);
  pthread_join(Thread_menu_interface, NULL);

  return 0;
}

void *menu_interface_thread(void *data)
{
  char option;

  do
  {
    puts(
        "TYPE:\n"
        "0: KILL THIS PROCESS\n"
        "1: SEND A NEW MESSAGE\n"
        "2: READ UNREAD MESSAGES\n"
        "3: READ ALL MESSAGES\n");

    scanf("%c", &option);

    getchar();

    switch (option)
    {
    case SEND_A_NEW_MESSAGE:
      // Wakes the send message thread
      lock_and_wake_next_thread(&mutex2, &send_messages_cond);
      // make this thread wait
      wait_and_lock_thread(&mutex2, &menu_cond);
      //clean buffer
      getchar();
      break;

    default:
      break;
    }

  } while (option != '0');

  // TODO: kill all process!

  pthread_exit(NULL);
}

void *listen_to_messages_thread(void *data)
{
  Router router_config;
  router_config = *((Router *)data);

  int client_message_length;
  char buffer_to_receive_messages[BUFFER_LENGTH];
  struct sockaddr_in socket_address;
  socklen_t socket_length;

  socket_length = sizeof(router_config.socket);

  while (TRUE)
  {
    printf("Waiting for data...");

    clear_buffer(buffer_to_receive_messages);

    // TODO: implement a mutex here!

    client_message_length = get_message_from_socket(
        &router_config.socket_number,
        buffer_to_receive_messages,
        &router_config.socket,
        &socket_length);

    print_data_received_from_client(&router_config.socket, buffer_to_receive_messages);

    send_response_to_client(
        &router_config.socket_number,
        buffer_to_receive_messages,
        &client_message_length,
        &router_config.socket,
        &socket_length);
  }

  pthread_exit(NULL);
}

void *send_messages_thread(void *data)
{
  Router router_config;
  router_config = *((Router *)data);

  struct sockaddr_in socket_to_send_message;
  int socket_to_send_message_length, message_success;
  int port_to_send_message;
  char buffer_to_receive_message[BUFFER_LENGTH];
  char message[BUFFER_LENGTH];

  socket_to_send_message_length = sizeof(socket_to_send_message);

  memset((char *)&socket_to_send_message, 0, sizeof(socket_to_send_message));

  socket_to_send_message.sin_family = AF_INET;

  if (inet_aton(router_config.ip_address, &socket_to_send_message.sin_addr) == 0)
  {
    fprintf(stderr, "inet_aton() failed\n");
    exit(1);
  }


  while (1)
  {
    wait_and_lock_thread(&mutex1, &send_messages_cond);

    puts("Enter a port");
    scanf("%i", &port_to_send_message);
    socket_to_send_message.sin_port = htons(port_to_send_message);

    printf("Enter a message : ");

    scanf("%s", message);

    message_success = sendto(
        router_config.socket_number,
        message,
        strlen(message),
        0,
        (struct sockaddr *)&socket_to_send_message,
        socket_to_send_message_length);

    //send the message
    if (message_success == -1)
    {
      die("sendto()");
    }

    //receive a reply and print it
    //clear the buffer by filling null, it might have previously received data
    memset(buffer_to_receive_message, '\0', BUFFER_LENGTH);
    //try to receive some data, this is a blocking call

    message_success = recvfrom(
        router_config.socket_number,
        buffer_to_receive_message,
        BUFFER_LENGTH,
        0,
        (struct sockaddr *)&socket_to_send_message,
        &socket_to_send_message_length);

    if (message_success == -1)
    {
      die("recvfrom()");
    }

    puts(buffer_to_receive_message);

    lock_and_wake_next_thread(&mutex1, &menu_cond);
  }

  pthread_exit(NULL);
}

void wait_and_lock_thread(pthread_mutex_t *mutex, pthread_cond_t *condition)
{
  pthread_mutex_lock(mutex);
  pthread_cond_wait(condition, mutex);
}

void lock_and_wake_next_thread(pthread_mutex_t *mutex, pthread_cond_t *condition)
{
  pthread_mutex_unlock(mutex);
  pthread_cond_signal(condition);
}

Router **read_router_config(int *router_list_size)
{
  char *text;

  text = read_file(ROUTER_FILE_CONFIG_NAME);
  char *token = strtok(text, "\n");

  // this needs to return a list of pointers of routers!
  Router **router_list = (Router **)malloc(sizeof(Router **));

  for (*router_list_size = 0; token != NULL; token = strtok(NULL, "\n"))
  {
    Router *router_aux;

    router_aux = parse_line_to_router(token);
    router_list[*router_list_size] = router_aux;
    router_list = realloc(router_list, ++(*router_list_size) * sizeof(**router_list)); 
  }

  return router_list;
}

Link **read_link_config(int *link_list_size)
{
  char *text;

  text = read_file(LINK_FILE_CONFIG_NAME);
  char *token = strtok(text, "\n");

  // this needs to return a list of pointers of routers!
  Link **link_list = (Link **)malloc(sizeof(Link **));

  for (*link_list_size = 0; token != NULL; token = strtok(NULL, "\n"))
  {
    Link *link_aux;

    link_aux = parse_line_to_link(token);

    link_list[*link_list_size] = link_aux;
    link_list = realloc(link_list, ++(*link_list_size) * sizeof(**link_list));
  }

  return link_list;
}

Link *parse_line_to_link(char *line)
{
  int first_node_last_index = 0;
  int first_letter_appear, weight_start_index, weight_last_index;

  int destination_last_index, destination_start_index;

  // Find the index of the id in the array
  for (first_node_last_index = 0; line[first_node_last_index] != ' '; first_node_last_index++)
    ;

  //find the start index of port and the last index of it. It ignores blank spaces too
  for (destination_last_index = first_node_last_index, first_letter_appear = 1;; destination_last_index++)
  {
    if (line[destination_last_index] != ' ' && first_letter_appear)
    {
      destination_start_index = destination_last_index;
      first_letter_appear = 0;
    }
    if (line[destination_last_index + 1] == ' ' && line[destination_last_index] != ' ')
    {
      break;
    }
  }

  //find the start index of ip and the last index of it. It ignores blank spaces too
  for (weight_last_index = destination_last_index + 1, first_letter_appear = 1;; weight_last_index++)
  {
    if (line[weight_last_index] != ' ' && first_letter_appear)
    {
      weight_start_index = weight_last_index;
      first_letter_appear = 0;
    }
    if (line[weight_last_index] == '\0')
      break;
  }

  //allocs the space to the new strings
  char *first_node = (char *)malloc(sizeof(char) * (first_node_last_index + 1));
  char *weight = (char *)malloc(sizeof(char) * (weight_last_index - weight_last_index + 1));
  char *destination_node = (char *)malloc(sizeof(char) * (destination_last_index - destination_start_index + 1));

  // copy the substrings
  strncpy(first_node, line, first_node_last_index);
  strncpy(destination_node, line + destination_start_index, destination_last_index - destination_start_index + 1);
  strncpy(weight, line + weight_start_index, weight_last_index - weight_start_index);

  first_node[first_node_last_index] = '\0';                                      // place the null terminator
  destination_node[destination_last_index - destination_start_index + 1] = '\0'; // place the null terminator
  weight[weight_last_index - weight_start_index] = '\0';                         // place the null terminator

  Link *link = (Link *)malloc(sizeof(Link));

  link->node_destination = atoi(destination_node);
  link->node_start = atoi(first_node);
  link->weight = atoi(weight);

  //free the space;
  free(weight);
  free(first_node);
  free(destination_node);

  return link;
};

Router *parse_line_to_router(char *line)
{
  int id_position = 0, port_start_position, port_end_position;
  int first_letter_appear, ip_start_position, ip_end_position;

  // Find the index of the id in the array
  for (id_position = 0; line[id_position] != ' '; id_position++)
    ;

  //find the start index of port and the last index of it. It ignores blank spaces too
  for (port_end_position = id_position, first_letter_appear = 1;; port_end_position++)
  {
    //TODO: fix it
    if (line[port_end_position] != ' ' && first_letter_appear)
    {
      port_start_position = port_end_position;
      first_letter_appear = 0;
    }
    if (line[port_end_position + 1] == ' ' && line[port_end_position] != ' ')
    {
      break;
    }
  }

  //find the start index of ip and the last index of it. It ignores blank spaces too
  for (ip_end_position = port_end_position + 1, first_letter_appear = 1; TRUE; ip_end_position++)
  {
    if (line[ip_end_position] != ' ' && first_letter_appear)
    {
      ip_start_position = ip_end_position;
      first_letter_appear = 0;
    }
    if (line[ip_end_position] == '\0')
      break;
  }

  //allocs the space to the new strings
  char *id = (char *)malloc(sizeof(char) * (id_position + 1));
  char *ip = (char *)malloc(sizeof(char) * (ip_end_position - ip_start_position + 1));
  char *port = (char *)malloc(sizeof(char) * (port_end_position - port_start_position + 1));

  // copy the substrings
  strncpy(id, line, id_position);
  strncpy(port, line + port_start_position, port_end_position - port_start_position + 1);
  strncpy(ip, line + ip_start_position, ip_end_position - ip_start_position);

  id[id_position] = '\0';                                   // place the null terminator
  port[port_end_position - port_start_position + 1] = '\0'; // place the null terminator
  ip[ip_end_position - ip_start_position] = '\0';           // place the null terminator

  Router *router = (Router *)malloc(sizeof(Router));

  router->id = atoi(id);
  router->ip_address = ip;
  router->port = atoi(port);

  //free the space of ip and port;
  free(id);
  free(port);

  return router;
};

char *read_file(char *filename)
{
  char *buffer = NULL;
  int string_size, read_size;
  FILE *file_pointer = fopen(filename, "r");

  if (file_pointer)
  {
    // Seek the last byte of the file
    fseek(file_pointer, 0, SEEK_END);
    // Offset from the first to the last byte, or in other words, filesize
    string_size = ftell(file_pointer);
    // go back to the start of the file
    rewind(file_pointer);

    // Allocate a string that can hold it all
    buffer = (char *)malloc(sizeof(char) * (string_size + 1));

    // Read it all in one operation
    read_size = fread(buffer, sizeof(char), string_size, file_pointer);

    // fread doesn't set it so put a \0 in the last position
    // and buffer is now officially a string
    buffer[string_size] = '\0';

    if (string_size != read_size)
    {
      free(buffer);
      buffer = NULL;
    }

    // Always remember to close the file.
    fclose(file_pointer);
  }

  return buffer;
}

void clear_buffer(char *buffer)
{
  fflush(stdout);
  memset(buffer, '\0', BUFFER_LENGTH);
};

int create_udp_socket()
{
  int socket_created = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

  if (socket_created == -1)
  {
    die("socket");
  }

  return socket_created;
}

int set_udp_configurations(Router *router_config)
{
  // zero out the structure
  memset((char *)&router_config->socket, 0, sizeof(router_config->socket));

  // UDP
  router_config->socket.sin_family = AF_INET;
  // Set the port
  router_config->socket.sin_port = htons(router_config->port);
  // Set the IP ADDRESS
  router_config->socket.sin_addr.s_addr = htonl(INADDR_ANY);

  // TODO: before send a message verify if its a big endian or little endian and send it
  // bind socket to port
  // it could be a pointer... set it to free after usage...

  int bind_success = bind(router_config->socket_number, (struct sockaddr *)&router_config->socket, sizeof(router_config->socket));

  if (bind_success == -1)
  {
    die("bind");
  }

  return bind_success;
}

int get_message_from_socket(int *socket_created, char *buffer_to_receive_messages, struct sockaddr_in *socket_client, socklen_t *socket_length)
{
  int client_message_length;

  client_message_length = recvfrom(
      *socket_created,
      buffer_to_receive_messages,
      BUFFER_LENGTH,
      0,
      (struct sockaddr *)socket_client,
      socket_length);

  if (client_message_length == -1)
  {
    die("recvfrom()");
  }

  return client_message_length;
}

void print_data_received_from_client(struct sockaddr_in *socket_client, char *buffer_message)
{
  printf("Received packet from %s:%d\n", inet_ntoa(socket_client->sin_addr), ntohs(socket_client->sin_port));
  //print details of the client/peer and the data received
  printf("Data: %s\n", buffer_message);
}

int send_response_to_client(int *socket_created, char *buffer_to_receive_messages, int *client_message_length, struct sockaddr_in *socket_client, socklen_t *socket_length)
{
  //now reply the client with the same data
  int send_response_message_success;

  send_response_message_success = sendto(
      *socket_created,
      buffer_to_receive_messages,
      *client_message_length,
      0,
      (struct sockaddr *)socket_client,
      *socket_length);

  if (send_response_message_success == -1)
  {
    die("sendto()");
  }

  return send_response_message_success;
}
