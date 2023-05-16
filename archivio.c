#include "include.h"

/* DEFINE USEFUL CONSTANTS */
#define Num_elem 1000000
#define PC_buffer_len 10

// thread
pthread_t signal_manager_thread, capo_scrittore_thread, capo_lettore_thread;

void manage_sigint() { fprintf(stderr, "%d\n", get_unique_words_count()); }

void manage_sigterm() {
  // wait thread termination
  xpthread_join(capo_scrittore_thread, NULL, line, file);
  xpthread_join(capo_lettore_thread, NULL, line, file);
  // print unique words, deallocate hashtable
  fprintf(stderr, "%d\n", get_unique_words_count());
  hdestroy();
}

int main(int argc, char **argv) {

  if (argc != 3) {
    exit_msg("Wrong number of args, please use => #writers #readers");
  }

  int num_writers = atoi(argv[1]);
  int num_readers = atoi(argv[2]);

  /*=================== HASHTABLE ===================*/

  // create hashtable syncronization variables struct
  Htable_sync *htable_sync_data = malloc(sizeof(Htable_sync));
  int hash_table = hcreate(Num_elem);

  if (hash_table == 0) {
    exit_msg("Error creating hashtable");
  }

  if (htable_sync_data == NULL) {
    exit_msg("Error allocating hashtable sync var struct, exiting...");
  }

  /*=================== SHARED BUFFERS ===================*/

  // create shared buffers
  Buffer *lettori_buffer = malloc(sizeof(Buffer));
  Buffer *scrittori_buffer = malloc(sizeof(Buffer));

  if (lettori_buffer == NULL) {
    puts("Error creating buffer, exiting...");
    destroy_buffer(lettori_buffer);
    free(lettori_buffer);
    free(htable_sync_data);
    hdestroy();
    exit(1);
  }

  if (scrittori_buffer == NULL) {
    puts("Error creating buffer, exiting...");
    destroy_buffer(scrittori_buffer);
    free(scrittori_buffer);
    free(htable_sync_data);
    hdestroy();
    exit(1);
  }

  // intialize buffer struct
  init_buffer(lettori_buffer);
  init_buffer(scrittori_buffer);

  /*=================== THREADS LAUNCHER ===================*/
  // threads definition
  pthread_t scrittori[num_writers];
  pthread_t lettori[num_readers];

  // start signal handler thread
  // xpthread_create(&signal_manager_thread, NULL, signal_manager, NULL, line,
  // file);
  // start caposc
  xpthread_create(&capo_scrittore_thread, NULL, capo_scrittore,
                  (void *)scrittori_buffer, line, file);
  // start capolet
  xpthread_create(&capo_lettore_thread, NULL, capo_lettore,
                  (void *)lettori_buffer, line, file);

  // start writers threads
  for (int i = 0; i < num_writers; i++) {
    printf("started scrittori: %d\n", i);
    xpthread_create(&scrittori[i], NULL, thread_scrittore,
                    (void *)scrittori_buffer, line, file);
  }
  // start readers threads
  for (int i = 0; i < num_readers; i++) {
    printf("started lettori: %d\n", i);
    xpthread_create(&lettori[i], NULL, thread_lettore, (void *)lettori_buffer,
                    line, file);
  }

  /*=================== THREADS JOIN ===================*/
  for (int i = 0; i < num_writers; i++) {
    xpthread_join(scrittori[i], NULL, line, file);
  }

  for (int i = 0; i < num_readers; i++) {
    xpthread_join(lettori[i], NULL, line, file);
  }

  // xpthread_join(signal_manager_thread, NULL, line, file);
  xpthread_join(capo_scrittore_thread, NULL, line, file);
  xpthread_join(capo_lettore_thread, NULL, line, file);

  /*=================== FREE MEMORY ===================*/
  destroy_buffer(scrittori_buffer);
  destroy_buffer(lettori_buffer);
  free(htable_sync_data);
  free(scrittori_buffer);
  free(lettori_buffer);
  hdestroy();

  return 0;
}
