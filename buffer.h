#include <stdlib.h>

// buffer lenght definition
#define PC_buffer_len 10

typedef struct {
  char *data[PC_buffer_len];
  int inserted;
  int head;
  int tail;
  int capo_has_finished;
  pthread_mutex_t lock;
  pthread_cond_t not_full;
  pthread_cond_t not_empty;

} Buffer;

// initialize the buffer
int init_buffer(Buffer *buffer, int buffer_len, int producers_count);

// free buffer memory
void destroy_buffer(Buffer *buffer);

// insert element into buffer
void insert_element(Buffer *buffer, int elem);

// remove element from buffer
int remove_element(Buffer *buffer);

// capo scrittore function
void *capo_scrittore(void *arg);

// thread scrittore function
void *thread_scrittore(void *arg);

// capo lettore function
void *capo_lettore(void *arg);

// thread scrittore function
void *thread_lettore(void *arg);
