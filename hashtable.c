#include "include.h"

void exit_msg(char *msg) {
  printf("%s\n", msg);
  exit(1);
}

Htable_sync hashtable_global_sync;

// init syncronization variables
void init_hashtable_sync_variables(Htable_sync *hashtable_sync) {

  hashtable_sync->readers = 0;
  hashtable_sync->writer = 0;

  // initialize condition variable and mutex
  xpthread_cond_init(&hashtable_sync->sync_var, NULL, line, file);
  xpthread_mutex_init(&hashtable_sync->mutex, NULL, line, file);
}

// reader gets access to hashtable
void hashtable_reader_lock(Htable_sync *hashtable_sync) {

  pthread_mutex_lock(&hashtable_sync->mutex);

  // wait that the current writer finishes writing
  while (hashtable_sync->writer > 0) {
    pthread_cond_wait(&hashtable_sync->sync_var, &hashtable_sync->mutex);
  }

  hashtable_sync->readers++;
  pthread_mutex_unlock(&hashtable_sync->mutex);
}

// reader gives other access to hashtable
void hashtable_reader_unlock(Htable_sync *hashtable_sync) {

  if ((hashtable_sync->readers > 0) && (hashtable_sync->writer != 0)) {
    pthread_mutex_lock(&hashtable_sync->mutex);

    // update readers count
    hashtable_sync->readers--;
    if (hashtable_sync->readers == 0) {
      // signal waiting writer
      pthread_cond_signal(&hashtable_sync->sync_var);
    }

    pthread_mutex_unlock(&hashtable_sync->mutex);
  }
}

// writer gets hashtable access
void hashtable_writer_lock(Htable_sync *hashtable_sync) {
  pthread_mutex_lock(&hashtable_sync->mutex);

  while (hashtable_sync->writer || hashtable_sync->readers > 0) {
    // attende fine scrittura o lettura
    pthread_cond_wait(&hashtable_sync->sync_var, &hashtable_sync->mutex);
  }

  hashtable_sync->writer = 1;
  pthread_mutex_unlock(&hashtable_sync->mutex);
}

// writer gives other access to hashtable
void hashtable_writer_unlock(Htable_sync *hashtable_sync) {
  // check that a writer has access to hashtable
  if (hashtable_sync->writer) {
    pthread_mutex_lock(&hashtable_sync->mutex);
    hashtable_sync->writer = 0;

    // signal to everyone waiting
    pthread_cond_broadcast(&hashtable_sync->sync_var);
    pthread_mutex_unlock(&hashtable_sync->mutex);
  }
}

ENTRY *create_hashtable_entry(char *string, int n) {
  ENTRY *new_entry = malloc(sizeof(ENTRY));

  if (new_entry == NULL) {
    termina("errore malloc entry 1");
  }

  new_entry->key = strdup(string);
  new_entry->data = (int *)malloc(sizeof(int));
  if (new_entry->key == NULL || new_entry->data == NULL)
    termina("errore malloc entry 2");
  *((int *)new_entry->data) = n;
  return new_entry;
}

void free_entry_memory(ENTRY *entry) {
  free(entry->key);
  free(entry->data);
  free(entry);
}

void aggiungi(char *s) {

  hashtable_writer_lock(&hashtable_global_sync);
  ENTRY *entry = create_hashtable_entry(s, 1);
  ENTRY *result = hsearch(*entry, FIND);

  // search if word to insert is already in the hashtable
  // entry was not present
  if (result == NULL) {

    // insert element
    result = hsearch(*entry, ENTER);

    if (result == NULL) {
      exit_msg("Hashtable search error");
    }

    // update unique_word_count
    hashtable_global_sync.unique_words_count++;

  } else {

    assert(strcmp(entry->key, result->key) == 0);
    int *counter = (int *)result->data;
    *counter += 1;
    free_entry_memory(entry);
  }
  hashtable_writer_unlock(&hashtable_global_sync);
}

int conta(char *s) {

  int word_count = 0;
  hashtable_reader_lock(&hashtable_global_sync);

  // search word in hashtable
  ENTRY *entry = create_hashtable_entry(s, 1);
  ENTRY *result = hsearch(*entry, FIND);

  // s is present inside the hashtable
  if (result != NULL) {
    word_count = *((int *)result->data);
  } else {
    word_count = 0;
  }
  free_entry_memory(entry);
  hashtable_reader_unlock(&hashtable_global_sync);

  return word_count;
}
