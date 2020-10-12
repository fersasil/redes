#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ROUTER_FILE_CONFIG_NAME "roteador.config"
#define LINK_FILE_CONFIG_NAME "enlace.config"

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

char *read_file(char *filename);
Router **read_router_config(int *router_list_size);
Link **read_link_config(int *link_list_size);
Link *parse_line_to_link(char *line);
Router *parse_line_to_router(char *line);

void *listen_to_messages_thread(void *data);
void *send_messages_thread(void *data);
void *menu_interface_thread(void *data);

pthread_mutex_t mutex1 = PTHREAD_MUTEX_INITIALIZER, mutex2 = PTHREAD_MUTEX_INITIALIZER;
pthread_t Thread_listen_to_messages, Thread_send_messages, Thread_menu_interface;

int main()
{
  int router_list_size, link_list_size;
  Router **router_list = read_router_config(&router_list_size);
  Link **link_list = read_link_config(&link_list_size);

  // for (int i = 0; i < router_list_size; i++)
  //   printf("id: %i port: %i address: %s \n", router_list[i]->id, router_list[i]->port, router_list[i]->ip_address);

  // for (int i = 0; i < link_list_size; i++)
  //   printf("first: %i destination: %i weight: %i \n", link_list[i]->node_start, link_list[i]->node_destination, link_list[i]->weight);

  pthread_create(&Thread_listen_to_messages, NULL, listen_to_messages_thread, NULL);
  pthread_create(&Thread_send_messages, NULL, send_messages_thread, NULL);
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
  // unsigned long i, j;
  // if (pthread_mutex_lock(&mutex1) == 0)
  // {
  //   printf("Thread ID%ld got mutex1.\n", pthread_self());
  //   for (i = 0; i < 10000000; ++i)
  //     ; // just for wasting some time
  //   if (pthread_mutex_lock(&mutex2) == 0)
  //   {
  //     printf("Thread ID%ld got mutex2.\n", pthread_self());
  //     for (i = 0; i < 10000000; ++i)
  //       ; // just for wasting some time
  //     pthread_mutex_unlock(&mutex2);
  //   }
  //   else
  //   {
  //     printf("\nThread ID%ld did not get mutex2.\n", pthread_self());
  //     pthread_mutex_unlock(&mutex2);
  //   }
  //   pthread_mutex_unlock(&mutex1);
  // }
  // else
  // {
  //   printf("\nThread ID%ld did not get mutex1.\n", pthread_self());
  //   pthread_mutex_unlock(&mutex1);
  // }
  pthread_exit(NULL);
}

void *send_messages_thread(void *data)
{
  // unsigned long i, j;
  // if (pthread_mutex_lock(&mutex2) == 0)
  // {
  //   printf("Thread ID%ld got mutex2.\n", pthread_self());
  //   for (i = 0; i < 10000000; ++i)
  //     ; // just for wasting some time
  //   if (pthread_mutex_lock(&mutex1) == 0)
  //   {
  //     printf("Thread ID%ld got mutex1.\n", pthread_self());
  //     for (i = 0; i < 10000000; ++i)
  //       ; // just for wasting some time
  //     pthread_mutex_unlock(&mutex1);
  //   }
  //   else
  //   {
  //     printf("\nThread ID%ld did not get mutex1.\n", pthread_self());
  //     pthread_mutex_unlock(&mutex1);
  //   }
  //   pthread_mutex_unlock(&mutex2);
  // }
  // else
  // {
  //   printf("\nThread ID%ld did not get mutex2.\n", pthread_self());
  //   pthread_mutex_unlock(&mutex2);
  // }
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