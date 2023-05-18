#include "include.h"

/* DEFINE USEFUL CONSTANTS */
#define Num_elem 1000000
#define PC_buffer_len 10

// signal thread info
pthread_cond_t finished_threads_cond = PTHREAD_COND_INITIALIZER;
pthread_mutex_t finished_threads_mutex = PTHREAD_MUTEX_INITIALIZER;

// threads counter for sigterm
int NUM_THREADS = 2;
int finished_threads = 0;

// hashtable syncronization
Htable_sync hashtable_global_sync;

void manage_sigint(int signal_num) {
  // get_unique_wors_count() uses reader locks in its implementation
  fprintf(stderr, "%d\n", get_unique_words_count());
}

void manage_sigterm(int signal_num) {
  // wait threads termination
  while (finished_threads < NUM_THREADS) {
    pthread_cond_wait(&finished_threads_cond, &finished_threads_mutex);
  }

  // print unique words, deallocate hashtable
  fprintf(stderr, "%d\n", get_unique_words_count());
  hdestroy();
  exit(0);
}

// Funzione del thread che gestisce i segnali
void *signal_manager(void *arg) {
  puts("[SIGNAL MANAGER STARTED]");
  sigset_t mask;
  sigfillset(&mask);
  sigdelset(&mask, SIGINT);
  sigdelset(&mask, SIGTERM);

  while (1) {
    int sig_type;
    // wait for a new signal
    int err_code = sigwait(&mask, &sig_type);

    if (err_code != 0) {
      exit_msg("Sigwait error");
    }
    printf("Received signal: %d\n", sig_type);
    // manage signal
    if (sig_type == SIGINT) {
      manage_sigint(sig_type);
    } else if (sig_type == SIGTERM) {
      manage_sigterm(sig_type);
    }
  }
}

int main(int argc, char **argv) {

  if (argc != 3) {
    exit_msg("Wrong number of args, please use => #writers #readers");
  }

  int num_readers = atoi(argv[1]);
  int num_writers = atoi(argv[2]);

  // update total number of threads so that i can track the number of threads
  // finished in sigterm function
  NUM_THREADS += (num_readers + num_writers);
  printf("[ARCHIVIO.c] Num Threads: %d\n", NUM_THREADS);
  /*=================== HASHTABLE ===================*/

  init_hashtable_sync_variables(&hashtable_global_sync);

  int res = hcreate(Num_elem);

  if (res == 0) {
    exit_msg("Error creating hashtable");
  } else {
    puts("[ARCHIVIO.c] HASTABLE CREATED SUCCESSFULY");
  }

  /*=================== SHARED BUFFERS ===================*/

  // create shared buffers
  Buffer *lettori_buffer = malloc(sizeof(Buffer));
  Buffer *scrittori_buffer = malloc(sizeof(Buffer));

  if (lettori_buffer == NULL) {
    puts("Error creating buffer, exiting...");
    destroy_buffer(lettori_buffer);
    free(lettori_buffer);
    hdestroy();
    exit(1);
  }

  if (scrittori_buffer == NULL) {
    puts("Error creating buffer, exiting...");
    destroy_buffer(scrittori_buffer);
    free(scrittori_buffer);
    hdestroy();
    exit(1);
  }

  puts("[ARCHIVIO.c] created lettori scrittori buffer");
  //  intialize buffer struct
  init_buffer(lettori_buffer);
  init_buffer(scrittori_buffer);
  puts("[ARCHIVIO.c] initialized buffers");

  /*=================== THREADS LAUNCHER ===================*/
  // threads definition
  pthread_t signal_manager_thread, capo_scrittore_thread, capo_lettore_thread;
  pthread_t scrittori[num_writers];
  pthread_t lettori[num_readers];

  // start signal handler thread
  xpthread_create(&signal_manager_thread, NULL, signal_manager, NULL, line,
                  file);
  // puts("[ARCHIVIO.c] created signal manager thread");
  //   start caposc
  xpthread_create(&capo_scrittore_thread, NULL, capo_scrittore,
                  (void *)scrittori_buffer, line, file);
  // puts("[ARCHIVIO.c] started capo scrittore");
  //   start capolet
  xpthread_create(&capo_lettore_thread, NULL, capo_lettore,
                  (void *)lettori_buffer, line, file);
  // puts("[ARCHIVIO.c] started capo lettore");

  // start writers threads
  for (int i = 0; i < num_writers; i++) {
    int ret_code = xpthread_create(&scrittori[i], NULL, thread_scrittore,
                                   (void *)scrittori_buffer, line, file);
    if (ret_code != 0) {
      printf("[ARCHIVIO.c] Error launching scrittori\n");
    }
  }

  // printf("[ARCHIVIO.c] started scrittori\n");
  //   start readers threads
  for (int i = 0; i < num_readers; i++) {
    int ret_code = xpthread_create(&lettori[i], NULL, thread_lettore,
                                   (void *)lettori_buffer, line, file);
    if (ret_code != 0) {
      printf("[ARCHIVIO.c] Error launching lettori\n");
    }
  }
  // printf("[ARCHIVIO.c] started lettori\n");

  /*=================== THREADS JOIN ===================*/
  for (int i = 0; i < num_writers; i++) {
    int ret_code = xpthread_join(scrittori[i], NULL, line, file);
    if (ret_code == 0) {
      finished_threads++;
    }
  }

  for (int i = 0; i < num_readers; i++) {
    int ret_code = xpthread_join(lettori[i], NULL, line, file);
    if (ret_code == 0) {
      finished_threads++;
    }
  }

  // xpthread_join(signal_manager_thread, NULL, line, file);
  int ret_code_caposc = xpthread_join(capo_scrittore_thread, NULL, line, file);
  int ret_code_capolet = xpthread_join(capo_lettore_thread, NULL, line, file);

  if (ret_code_caposc == 0) {
    finished_threads++;
  }
  if (ret_code_capolet == 0) {
    finished_threads++;
  }

  // signal sigterm manager thread that all threads have finished
  xpthread_cond_signal(&finished_threads_cond, line, file);
  printf("[ARCHIVIO.c] signaled all threads finished\n");
  printf("[ARCHIVIO.c] threads finished: [%d,%d]\n", finished_threads,
         NUM_THREADS);
  /*=================== FREE MEMORY ===================*/
  destroy_buffer(scrittori_buffer);
  destroy_buffer(lettori_buffer);
  free(scrittori_buffer);
  free(lettori_buffer);
  hdestroy();

  return 0;
}
