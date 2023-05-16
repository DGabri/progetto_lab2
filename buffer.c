#include "include.h"
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>

// Constants definition

#define line __LINE__
#define file __FILE__
#define PC_buffer_len 10
#define Max_sequence_length 2048

/*=================== BUFFER UTILS ===================*/
// initialize the buffer
void init_buffer(Buffer *buffer) {
  buffer->head = 0;
  buffer->tail = 0;
  buffer->inserted = 0;
  buffer->capo_has_finished = 0;

  // init threading vars
  xpthread_mutex_init(&buffer->lock, NULL, line, file);
  xpthread_cond_init(&buffer->not_full, NULL, line, file);
  xpthread_cond_init(&buffer->not_empty, NULL, line, file);
}

void destroy_buffer(Buffer *buffer) {
  // init threading vars
  xpthread_mutex_destroy(&buffer->lock, line, file);
  xpthread_cond_destroy(&buffer->not_full, line, file);
  xpthread_cond_destroy(&buffer->not_empty, line, file);

  // free each string pointer inside the buffer
  for (int i = 0; i < PC_buffer_len; i++) {
    if (buffer->data[i] != NULL) {
      free(buffer->data[i]);
    }
  }
}

// insert element into buffer
void insert_element(Buffer *buffer, char *elem) {

  // acquire lock
  xpthread_mutex_lock(&buffer->lock, line, file);

  // wait until there is space to insert a new element
  while ((buffer->inserted) == (PC_buffer_len)) {
    xpthread_cond_wait(&buffer->not_full, &buffer->lock, line, file);
  }

  // if elem != NULL => insert element
  // if elem == NULL => termination string so broadcast everyone
  if (elem != NULL) {
    int insert_index = buffer->tail;

    buffer->data[insert_index] = strdup(elem);
    buffer->tail = (insert_index + 1) % (PC_buffer_len);
    buffer->inserted++;

    xpthread_cond_signal(&buffer->not_empty, line, file);

  } else {
    buffer->capo_has_finished = 1;
    xpthread_cond_broadcast(&buffer->not_empty, line, file);
  }

  xpthread_mutex_unlock(&buffer->lock, line, file);
}

// remove element from buffer
char *remove_element(Buffer *buffer) {

  // acquire lock
  xpthread_mutex_lock(&buffer->lock, line, file);

  // no elements to remove, wait
  while ((buffer->inserted == 0) && (buffer->capo_has_finished == 0)) {
    xpthread_cond_wait(&buffer->not_empty, &buffer->lock, line, file);
  }

  char *elem;

  // there is at least an element, remove it
  if (buffer->inserted > 0) {

    // extract element from the 'head' of the buffer
    int head = buffer->head;
    elem = buffer->data[head];
    buffer->head = (head + 1) % (PC_buffer_len);
    buffer->inserted--;

    // signal that an element was removed
    xpthread_cond_signal(&buffer->not_full, line, file);
  } else {
    elem = NULL;
  }

  xpthread_mutex_unlock(&buffer->lock, line, file);

  return elem;
}

/*=================== CAPO SCRITTORE ===================*/
// capo scrittore function
void *capo_scrittore(void *arg) {

  // shared buffer between capo scrittore and scrittori
  Buffer *shared_buff = (Buffer *)arg;
  // open caposc pipe
  int pipe_fd = open("caposc", O_RDONLY);

  if (pipe_fd < 0) {
    printf("error opening caposc\n");
    exit(1);
  }

  // read from pipe and insert on shared buffer
  while (1) {
    int str_len;

    // read sequence length from pipe, (first 4 bytes -> sizeof(int))
    ssize_t e = read(pipe_fd, &str_len, sizeof(int));
    // error reading pipe
    if (e == 0) {
      break;
    }

    // convert from  network order to machine order the received bytes
    str_len = ntohl(str_len);
    printf(".c => STR LEN: %d\n", str_len);

    // allocate 1 more char to put '\0' at the end
    char *rcvd_string = malloc(str_len + 1);

    if (rcvd_string == NULL) {
      printf("Malloc error");
      exit(1);
    }

    // read from pipe
    ssize_t pipe_ret_val = read(pipe_fd, rcvd_string, str_len);
    // error reading from pipe
    if (pipe_ret_val == -1) {
      exit(1);
    } else if (pipe_ret_val == 0) {
      // pipe is empty => insert NULL in buffer
      insert_element(shared_buff, NULL);
      break;
    }

    /* TOKENIZATION AND BUFFER INSERTION */

    // add \0 to the end of the string
    rcvd_string[str_len] = '\0';

    printf(".c => Received string: %s\n", rcvd_string);
    // Tokenization
    char *part;
    char *rimanente = NULL;

    part = strtok_r(rcvd_string, ".,:; \n\r\t", &rimanente);

    while (part != NULL) {
      printf("Piece: %s\n", part);
      char *part_copy = strdup(part);

      if (part_copy == NULL) {
        printf("Error duplicating string\n");
        exit(1);
      }
      // insert in shared buffer
      insert_element(shared_buff, part_copy);
      part = strtok_r(NULL, ".,:; \n\r\t", &rimanente);
    }
    free(rcvd_string);
  }

  close(pipe_fd);
  return NULL;
}

// scrittore function
void *thread_scrittore(void *arg) {
  // shared buffer between capo scrittore and scrittori
  Buffer *shared_buff = (Buffer *)arg;

  // read from buffer until NULL is read (termination string)
  while (1) {
    char *read_str = remove_element(shared_buff);

    // termination string
    if (read_str == NULL) {
      break;
    }

    // add to hashtable if read_str is not NULL
    aggiungi(read_str);
    free(read_str);
  }

  return NULL;
}

/*=================== CAPO LETTORE ===================*/
// capo lettore function
void *capo_lettore(void *arg) {

  // shared buffer between capo lettore and lettori
  Buffer *shared_buff = (Buffer *)arg;
  // open caposc pipe
  int pipe_fd = open("capolet", O_RDONLY);

  if (pipe_fd < 0) {
    printf("error opening capolet\n");
    exit(1);
  }

  // read from pipe and insert on shared buffer
  while (1) {
    int str_len;

    // read sequence length from pipe, (first 4 bytes -> sizeof(int))
    ssize_t e = read(pipe_fd, &str_len, sizeof(int));
    // error reading pipe
    if (e == 0) {
      break;
    }

    // convert from  network order to machine order the received bytes
    str_len = ntohl(str_len);
    printf(".c => STR LEN: %d\n", str_len);

    // allocate 1 more char to put '\0' at the end
    char *rcvd_string = malloc(str_len + 1);

    if (rcvd_string == NULL) {
      printf("Malloc error");
      exit(1);
    }

    // read from pipe
    ssize_t pipe_ret_val = read(pipe_fd, rcvd_string, str_len);
    // error reading from pipe
    if (pipe_ret_val < str_len) {
      exit(1);
    } else if (pipe_ret_val == 0) {
      // pipe is empty => insert NULL in buffer
      insert_element(shared_buff, NULL);
      break;
    }

    /* TOKENIZATION AND BUFFER INSERTION */

    // add \0 to the end of the string
    rcvd_string[str_len] = '\0';

    printf(".c => Received string: %s\n", rcvd_string);
    // Tokenization
    char *part;
    char *rimanente = NULL;

    part = strtok_r(rcvd_string, ".,:; \n\r\t", &rimanente);

    while (part != NULL) {
      printf("Piece: %s\n", part);
      char *part_copy = strdup(part);

      if (part_copy == NULL) {
        printf("Error duplicating string\n");
        exit(1);
      }
      // insert in shared buffer
      insert_element(shared_buff, part_copy);
      part = strtok_r(NULL, ".,:; \n\r\t", &rimanente);
    }
    free(rcvd_string);
  }

  close(pipe_fd);
  return NULL;
}

// lettore function
void *thread_lettore(void *arg) {
  // shared buffer between capo lettore and lettori
  Buffer *shared_buff = (Buffer *)arg;

  // read from buffer until NULL is read (termination string)
  while (1) {
    char *read_str = remove_element(shared_buff);

    // termination string
    if (read_str == NULL) {
      break;
    }

    // logging -> string count\n
    FILE *log_file = fopen("lettori.log", "w");

    if (log_file == NULL) {
      printf("Error opening file");
      exit(1);
    }

    // call conta and log the value counted
    int word_occurrences = conta(read_str);
    fprintf(log_file, "%s %d\n", read_str, word_occurrences);

    free(read_str);
    fclose(log_file);
  }

  return NULL;
}