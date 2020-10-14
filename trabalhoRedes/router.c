#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define ROUTER_FILE_CONFIG_NAME "roteador.config"
#define LINK_FILE_CONFIG_NAME "enlace.config"
#define BUFFER_LENGTH 100 //Max length of buffer
#define TRUE 1
#define FALSE 0

struct
{
  int id;
  int port;
  char *ip_address;
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

void clean_buffer(char *buffer);
int create_udp_socket();
int set_udp_configurations(struct sockaddr_in *socket_address_pointer, Router* router_config, int socket_created_info);

pthread_mutex_t mutex1 = PTHREAD_MUTEX_INITIALIZER, mutex2 = PTHREAD_MUTEX_INITIALIZER;
pthread_t Thread_listen_to_messages, Thread_send_messages, Thread_menu_interface;

int main()
{
  int router_list_size, link_list_size;
  Router **router_list = read_router_config(&router_list_size);
  Link **link_list = read_link_config(&link_list_size);

  pthread_create(&Thread_listen_to_messages, NULL, listen_to_messages_thread, (void *)router_list[0]);
  // pthread_create(&Thread_send_messages, NULL, send_messages_thread, NULL);
  pthread_create(&Thread_menu_interface, NULL, menu_interface_thread, NULL);

  //now join them
  pthread_join(Thread_listen_to_messages, NULL);

  // printf("Thread id %ld returned\n", Thread_listen_to_messages);

  // I think this thread should be created only when the user wants to send a new message
  // pthread_join(Thread_send_messages, NULL);
  // printf("Thread id %ld returned\n", Thread_send_messages);

  pthread_join(Thread_menu_interface, NULL);
  printf("Thread id %ld returned\n", Thread_menu_interface);

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
        "1: READ UNREAD MESSAGES\n"
        "2: READ ALL MESSAGES\n");
    scanf("%c", &option);
    getchar();
  } while (option != '0');

  pthread_exit(NULL);
}

void *listen_to_messages_thread(void *data)
{
  Router router_config;
  router_config = *((Router *)data);

  //Is it really necessary?
  free((int *)data);

  struct sockaddr_in socket_address, socket_client;
  int socket_created, i, socket_length, client_message_length;
  int bind_success, send_response_message_success;
  char buffer_to_receive_messages[BUFFER_LENGTH];

  socket_length = sizeof(socket_client);

  //create a UDP socket
  socket_created = create_udp_socket();

  set_udp_configurations(&socket_address, &router_config, socket_created);

  //keep listening for data
  while (TRUE)
  {
    printf("Waiting for data...");

    clean_buffer(buffer_to_receive_messages);

    // TODO: implement a mutex here!
    // TODO: this flag can have a name more meaningful
    //try to receive some data, this is a blocking call
    client_message_length = recvfrom(
        socket_created,
        buffer_to_receive_messages,
        BUFFER_LENGTH,
        0,
        (struct sockaddr *)&socket_client,
        &socket_length);

    if (client_message_length == -1)
    {
      die("recvfrom()");
    }

    //print details of the client/peer and the data received
    printf("Received packet from %s:%d\n", inet_ntoa(socket_client.sin_addr), ntohs(socket_client.sin_port));
    printf("Data: %s\n", buffer_to_receive_messages);

    //now reply the client with the same data
    send_response_message_success = sendto(
        socket_created,
        buffer_to_receive_messages,
        client_message_length,
        0,
        (struct sockaddr *)&socket_client,
        socket_length);

    if (send_response_message_success == -1)
    {
      die("sendto()");
    }
  }

  close(socket_created);
  pthread_exit(NULL);
}

void *send_messages_thread(void *data)
{

#define SERVER "127.0.0.1"
#define BUFLEN 512 //Max length of buffer
#define PORT 8888  //The port on which to send data

  struct sockaddr_in si_other;
  int s, i, slen = sizeof(si_other);
  char buf[BUFLEN];
  char message[BUFLEN];

  if ((s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
  {
    die("socket");
  }

  memset((char *)&si_other, 0, sizeof(si_other));
  si_other.sin_family = AF_INET;
  si_other.sin_port = htons(PORT);

  if (inet_aton(SERVER, &si_other.sin_addr) == 0)
  {
    fprintf(stderr, "inet_aton() failed\n");
    exit(1);
  }

  while (1)
  {
    printf("Enter message : ");
    //gets(message);
    scanf("%s", message);

    //send the message
    if (sendto(s, message, strlen(message), 0, (struct sockaddr *)&si_other, slen) == -1)
    {
      die("sendto()");
    }

    //receive a reply and print it
    //clear the buffer by filling null, it might have previously received data
    memset(buf, '\0', BUFLEN);
    //try to receive some data, this is a blocking call
    if (recvfrom(s, buf, BUFLEN, 0, (struct sockaddr *)&si_other, &slen) == -1)
    {
      die("recvfrom()");
    }

    puts(buf);
  }

  close(s);
  return 0;
  pthread_exit(NULL);
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
    ;
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
  for (ip_end_position = port_end_position + 1, first_letter_appear = 1;; ip_end_position++)
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

void clean_buffer(char *buffer)
{
  fflush(stdout);
  //receive a reply and print it
  //clear the buffer by filling null, it might have previously received data
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

int set_udp_configurations(struct sockaddr_in *socket_address_pointer, Router* router_config, int socket_created_info)
{
  // zero out the structure
  memset((char *)socket_address_pointer, 0, sizeof(*socket_address_pointer));

  // UDP
  socket_address_pointer->sin_family = AF_INET;
  // Set the port
  socket_address_pointer->sin_port = htons(router_config->port);
  // Set the IP ADDRESS
  socket_address_pointer->sin_addr.s_addr = htonl(INADDR_ANY);

  // TODO: before send a message verify if its a big endian or little endian and send it
  // bind socket to port
  // it could be a pointer... set it to free after usage...

  int bind_success = bind(socket_created_info, (struct sockaddr *)socket_address_pointer, sizeof(*socket_address_pointer));

  if (bind_success == -1)
  {
    die("bind");
  }

  return bind_success;
}
