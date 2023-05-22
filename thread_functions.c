#include "include.h"

pthread_mutex_t log_file_mutex = PTHREAD_MUTEX_INITIALIZER;

/*=================== CAPO SCRITTORE [client2] ===================*/
// capo scrittore function

void *capo_scrittore(void *arg) {
  //  shared buffer between capo scrittore and scrittori
  Buffer *shared_buff = (Buffer *)arg;
  // open caposc pipe
  int pipe_fd = open("caposc", O_RDONLY);

  if (pipe_fd < 0) {
    printf("error opening caposc\n");
    exit(1);
  }

  //   read from pipe and insert on shared buffer

  while (1) {
    int str_len;

    // read sequence length from pipe, (first 4 bytes -> sizeof(int))
    ssize_t e = read(pipe_fd, &str_len, sizeof(int));

    // error reading pipe
    if (e == 0) {
      printf("[ERROR] reading pipe caposc\n");
      break;
    }

    // convert from  network order to machine order the received bytes
    if (str_len == 0) {
      // termination string => read 0
      insert_element(shared_buff, NULL);
      break;

    } else {
      // Allocate memory for the string
      char *rcvd_string = malloc(str_len + 1);

      if (rcvd_string == NULL) {
        printf("Malloc error\n");
        exit(1);
      }

      // Read the string from the pipe
      ssize_t pipe_ret_val = read(pipe_fd, rcvd_string, str_len);

      // Error reading from pipe
      if (pipe_ret_val <= 0) {
        printf("Error reading pipe\n");
        free(rcvd_string);
        break;
      }

      /* TOKENIZATION AND BUFFER INSERTION */

      // add \0 to the end of the string
      rcvd_string[str_len] = '\0';
      // printf("[CAPO SCRITTORE] LEN: %d, STR: %s\n", str_len, rcvd_string);

      // Tokenization
      char *part;
      char *rimanente = NULL;

      part = strtok_r(rcvd_string, ".,:; \n\r\t", &rimanente);

      while (part != NULL) {
        // printf("Piece: %s\n", part);
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
  }

  close(pipe_fd);

  return NULL;
}

// scrittore function
void *thread_scrittore(void *arg) {
  // shared buffer between capo scrittore and scrittori
  Buffer *shared_buff = (Buffer *)arg;

  //    read from buffer until NULL is read (termination string)
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

/* *************************************************************** */

/*=================== CAPO LETTORE [client1] ===================*/
// capo lettore function
void *capo_lettore(void *arg) {
  //  shared buffer between capo scrittore and scrittori
  Buffer *shared_buff = (Buffer *)arg;
  // open caposc pipe
  int pipe_fd = open("capolet", O_RDONLY);

  if (pipe_fd < 0) {
    printf("error opening capolet\n");
    exit(1);
  }

  //    read from pipe and insert on shared buffer
  while (1) {
    int str_len;

    // read sequence length from pipe, (first 4 bytes -> sizeof(int))
    ssize_t e = read(pipe_fd, &str_len, sizeof(int));
    // error reading pipe
    if (e == 0) {
      printf("[ERROR] reading pipe capolet\n");
      break;
    }

    // convert from  network order to machine order the received bytes
    if (str_len == 0) {
      // termination string => read 0
      insert_element(shared_buff, NULL);
      break;
    } else {
      // Allocate memory for the string
      char *rcvd_string = malloc(str_len + 1);

      if (rcvd_string == NULL) {
        printf("[ERROR] Malloc error\n");
        exit(1);
      }

      // Read the string from the pipe
      ssize_t pipe_ret_val = read(pipe_fd, rcvd_string, str_len);

      // Error reading from pipe
      if (pipe_ret_val <= 0) {
        printf("[ERROR] reading pipe\n");
        free(rcvd_string);
        break;
      }

      /* TOKENIZATION AND BUFFER INSERTION */

      // add \0 to the end of the string
      rcvd_string[str_len] = '\0';

      // printf("[CAPO LETTORE] LEN: %d, STR: %s\n", str_len, rcvd_string);

      //   Tokenization
      char *part;
      char *rimanente = NULL;

      part = strtok_r(rcvd_string, ".,:; \n\r\t", &rimanente);

      while (part != NULL) {
        // printf("Piece: %s\n", part);
        char *part_copy = strdup(part);

        if (part_copy == NULL) {
          printf("Error duplicating string\n");
          exit(1);
        }
        // insert in shared buffer
        insert_element(shared_buff, part_copy);
        // printf("[CAPO LETTORE] Inserted: %s\n", part_copy);
        part = strtok_r(NULL, ".,:; \n\r\t", &rimanente);
      }
      free(rcvd_string);
    }
  }

  close(pipe_fd);

  return NULL;
}

/* ****************************************************************** */

// lettore function
void *thread_lettore(void *arg) {
  // shared buffer between capo lettore and lettori
  Buffer *shared_buff = (Buffer *)arg;

  //  read from buffer until NULL is read (termination string)
  while (1) {
    char *read_str = remove_element(shared_buff);

    // termination string
    if (read_str == NULL) {
      break;
    }

    // call conta and log the value counted
    int word_occurrences = conta(read_str);
    // printf("[LETTORE] conta: %s -> %d\n", read_str, word_occurrences);
    xpthread_mutex_lock(&log_file_mutex, line, file);

    // logging -> string count\n
    FILE *log_file = fopen("lettori.log", "a");

    if (log_file == NULL) {
      printf("Error opening file");
      exit(1);
    }

    fprintf(log_file, "%s %d\n", read_str, word_occurrences);

    free(read_str);
    fclose(log_file);
    xpthread_mutex_unlock(&log_file_mutex, line, file);
  }

  return NULL;
}
