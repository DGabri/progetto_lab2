#include "include.h"

// Constants definition

#define line __LINE__
#define file __FILE__
#define PC_buffer_len 10

// initialize the buffer
void init_buffer(Buffer *buffer, int buffer_len) {
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
