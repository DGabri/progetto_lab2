#include "include.h"
#include <bits/pthreadtypes.h>
#include <pthread.h>

void exit_msg(char *msg) {
  printf("%s\n", msg);
  exit(1);
}

struct queue_list {
  ENTRY *entry;
  struct queue_list *next;
};

struct queue_list *head = NULL;

void queue_allocated_memory(ENTRY *entry) {
  struct queue_list *e = (struct queue_list *)malloc(sizeof(struct queue_list));

  if(e != NULL) {
    e->entry = entry;
    e->next = head;
    head = e;
  }
}

/* ******************* */

void free_hash_memory() {
  /* printf("Freeing hash memory...\n"); */

  while(head != NULL) {
    struct queue_list *next = head->next;

    /* printf("Freeing %s\n", (char*)head->entry->key); */
    free_entry_memory(head->entry);
    free(head);
    head = next;
  }

  hdestroy();
  
  /* printf("Hash memory has been freed succesfully\n"); */
}

/*=================== HASHTABLE SYNC ===================*/


extern Htable_sync hashtable_global_sync;

// init syncronization variables
void init_hashtable_sync_variables(Htable_sync *hashtable_sync) {
  hashtable_sync->readers = 0;
  hashtable_sync->waiting_readers = 0;
  hashtable_sync->writers = 0;
  hashtable_sync->waiting_writers = 0;
  hashtable_sync->unique_words_count = 0;

  // initialize condition variable and mutex
  xpthread_cond_init(&hashtable_sync->can_read, NULL, line, file);
  xpthread_cond_init(&hashtable_sync->can_write, NULL, line, file);
  xpthread_mutex_init(&hashtable_sync->mutex, NULL, line, file);
}

// reader gets access to hashtable
void hashtable_reader_lock(Htable_sync *hashtable_sync) {
  xpthread_mutex_lock(&hashtable_sync->mutex, line, file);
  hashtable_sync->waiting_readers++;
  while (hashtable_sync->writers > 0) {
    xpthread_cond_wait(&hashtable_sync->can_read, &hashtable_sync->mutex, line,
                       file);
  }

  hashtable_sync->waiting_readers--;
  hashtable_sync->readers++;
  xpthread_mutex_unlock(&hashtable_sync->mutex, line, file);
}

// reader gives other access to hashtable
void hashtable_reader_unlock(Htable_sync *hashtable_sync) {
  xpthread_mutex_lock(&hashtable_sync->mutex, line, file);
  hashtable_sync->readers--;
  while ((hashtable_sync->readers == 0) &&
         (hashtable_sync->waiting_writers > 0)) {
    xpthread_cond_signal(&hashtable_sync->can_write, line, file);
  }

  xpthread_mutex_unlock(&hashtable_sync->mutex, line, file);
}

// writer gets hashtable access
void hashtable_writer_lock(Htable_sync *hashtable_sync) {
  xpthread_mutex_lock(&hashtable_sync->mutex, line, file);
  hashtable_sync->waiting_writers++;
  while ((hashtable_sync->writers > 0) || (hashtable_sync->readers > 0)) {
    xpthread_cond_wait(&hashtable_sync->can_write, &hashtable_sync->mutex, line,
                       file);
  }

  hashtable_sync->waiting_writers--;
  hashtable_sync->writers++;
  xpthread_mutex_unlock(&hashtable_sync->mutex, line, file);
}

// writer gives other access to hashtable
void hashtable_writer_unlock(Htable_sync *hashtable_sync) {
  xpthread_mutex_lock(&hashtable_sync->mutex, line, file);
  hashtable_sync->writers--;
  if (hashtable_sync->readers > 0) {
    xpthread_cond_broadcast(&hashtable_sync->can_read, line, file);
  } else {
    xpthread_cond_signal(&hashtable_sync->can_write, line, file);
  }

  xpthread_mutex_unlock(&hashtable_sync->mutex, line, file);
}

/*=================== HASHTABLE FUNCTIONS ===================*/

ENTRY *create_hashtable_entry(char *string, int n) {
  ENTRY *new_entry = malloc(sizeof(ENTRY));
  
  if (new_entry == NULL) {
    termina("errore malloc entry 1");
  }

  new_entry->key = strdup(string);
  new_entry->data = (int *)malloc(sizeof(int));

  if (new_entry->key == NULL || new_entry->data == NULL) {
    exit_msg("errore malloc entry 2");
  }
  
  *((int *)new_entry->data) = n;
  return new_entry;
}

void free_entry_memory(ENTRY *entry) {
  free(entry->key);
  free(entry->data);
  free(entry);
}

/* ****************** */

void aggiungi(char *s) {
  ENTRY *entry = create_hashtable_entry(s, 1);

  // get exclusive access to the hashtable
  hashtable_writer_lock(&hashtable_global_sync);

  ENTRY *result = hsearch(*entry, FIND);
  if (result == NULL) {
    // entry was not found, insert it
    result = hsearch(*entry, ENTER);

    if (result == NULL) {
      hashtable_writer_unlock(&hashtable_global_sync);
      exit_msg("Hashtable insert error");
    }
    // printf("[HASHTABLE INSERTED] => %s:%d\n", result->key,
    //        *((int *)result->data));
    hashtable_global_sync.unique_words_count++;
    queue_allocated_memory(entry);
  } else {
    // entry already exists, increment occurrence count
    int *counter = (int *)result->data;
    *counter += 1;
    // printf("[HASHTABLE UPDATED] => %s:%d\n", result->key,
    //        *((int *)result->data));
    free_entry_memory(entry);
  }

  hashtable_writer_unlock(&hashtable_global_sync);
}

int conta(char *s) {
  int word_count = 0;
  // create new entry
  ENTRY *entry = create_hashtable_entry(s, 0);

  // get exclusive access to hashtable
  hashtable_reader_lock(&hashtable_global_sync);
  // search word in hashtable
  ENTRY *result = hsearch(*entry, FIND);

  if (result != NULL) {
    word_count = *((int *)result->data);
    // printf("[HASHTABLE FOUND] => %s:%d\n", result->key, word_count);
  } else {
    // printf("[HASHTABLE MISSING] => %s not found\n", s);
    word_count = 0;
  }
  free_entry_memory(entry);
  hashtable_reader_unlock(&hashtable_global_sync);

  return word_count;
}

int get_unique_words_count() {
  // get exclusive access to hashtable
  hashtable_reader_lock(&hashtable_global_sync);
  // get the number of unique words
  int count = hashtable_global_sync.unique_words_count;
  hashtable_reader_unlock(&hashtable_global_sync);

  return count;
}



#ifdef UNIT_TEST
Htable_sync hashtable_global_sync;

int main(int argc, char *argv[]) {
  int i;
  
  init_hashtable_sync_variables(&hashtable_global_sync);
  hcreate(10000);

  for (i = 0; i < 100; i++) {
    aggiungi("ciao");
    if(i% 2) aggiungi("bello");
  }

  if(conta("ciao") != 100) printf("ERROR\n");

  hdestroy();
  
  return (0);
}
#endif
