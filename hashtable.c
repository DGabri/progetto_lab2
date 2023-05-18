#include "include.h"
#include <pthread.h>

void exit_msg(char *msg) {
  printf("%s\n", msg);
  exit(1);
}

/*=================== HASHTABLE SYNC ===================*/
extern Htable_sync hashtable_global_sync;

// init syncronization variables
void init_hashtable_sync_variables(Htable_sync *hashtable_sync) {

  hashtable_sync->readers = 0;
  hashtable_sync->writers = 0;
  hashtable_sync->writers_queue = 0;

  // initialize condition variable and mutex
  xpthread_cond_init(&hashtable_sync->sync_var_readers, NULL, line, file);
  xpthread_cond_init(&hashtable_sync->sync_var_writers, NULL, line, file);
  xpthread_mutex_init(&hashtable_sync->mutex, NULL, line, file);
}

// reader gets access to hashtable
void hashtable_reader_lock(Htable_sync *hashtable_sync) {
  pthread_mutex_lock(&hashtable_sync->mutex);

  // wait that the current writer finishes writing
  while (hashtable_sync->writers_queue > 0 || hashtable_sync->writers > 0) {
    pthread_cond_wait(&hashtable_sync->sync_var_readers,
                      &hashtable_sync->mutex);
  }

  hashtable_sync->readers++;
  pthread_mutex_unlock(&hashtable_sync->mutex);
}

// reader gives other access to hashtable
void hashtable_reader_unlock(Htable_sync *hashtable_sync) {
  pthread_mutex_lock(&hashtable_sync->mutex);
  // update readers count
  hashtable_sync->readers--;

  if ((hashtable_sync->readers == 0) && (hashtable_sync->writers_queue > 0)) {
    pthread_cond_signal(&hashtable_sync->sync_var_writers);
  }
  pthread_mutex_unlock(&hashtable_sync->mutex);
}

// writer gets hashtable access
void hashtable_writer_lock(Htable_sync *hashtable_sync) {
  pthread_mutex_lock(&hashtable_sync->mutex);
  hashtable_sync->writers_queue += 1;

  while (hashtable_sync->writers > 0 || hashtable_sync->readers > 0) {
    // attende fine scrittura o lettura
    pthread_cond_wait(&hashtable_sync->sync_var_writers,
                      &hashtable_sync->mutex);
  }
  hashtable_sync->writers_queue--;
  hashtable_sync->writers++;
  pthread_mutex_unlock(&hashtable_sync->mutex);
}

// writer gives other access to hashtable
void hashtable_writer_unlock(Htable_sync *hashtable_sync) {
  pthread_mutex_lock(&hashtable_sync->mutex);
  hashtable_sync->writers--;
  // check that a writer has access to hashtable
  if (hashtable_sync->writers_queue > 0) {
    pthread_cond_signal(&hashtable_sync->sync_var_writers);
    // signal to everyone waiting
  } else {
    pthread_cond_broadcast(&hashtable_sync->sync_var_readers);
  }
  pthread_mutex_unlock(&hashtable_sync->mutex);
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
    free_entry_memory(entry);
  } else {
    // printf("[HASHTABLE MISSING] => %s not found\n", s);
    word_count = 0;
  }
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