#include <pthread.h>
#include <search.h>

/* Function to exit and print error msg */
void exit_msg(char *msg);

// hashtable syncronization struct

typedef struct htable {

  // numbers of readers currently reading
  int readers;
  int writer;
  int unique_words_count;
  // sync vars
  pthread_cond_t sync_var;
  pthread_mutex_t mutex;
} Htable_sync;

// init syncronization variables
void init_hashtable_sync_variables(Htable_sync *hashtable_sync);

// reader gets access to hashtable
void hashtable_reader_lock(Htable_sync *hashtable_sync);

// reader gives other access to hashtable
void hashtable_reader_unlock(Htable_sync *hashtable_sync);

// writer gets hashtable access
void hashtable_writer_lock(Htable_sync *hashtable_sync);

// writer gives other access to hashtable
void hashtable_writer_unlock(Htable_sync *hashtable_sync);

/* hashtable DATA MANIPULATION FUNCTIONS */

// create hashtable entry
ENTRY *create_hashtable_entry(char *string, int n);

// free hashtable entry memory
void free_entry_memory(ENTRY *entry);

// if s is not in hashtable => insert (s, 1)
// else => (s, counter++)
void aggiungi(char *s);

// return hashtable occurrences of the word s
int conta(char *s);