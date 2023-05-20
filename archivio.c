#include "include.h"

/* DEFINE USEFUL CONSTANTS */
#define Num_elem 1000000
#define PC_buffer_len 10

// capo lettore and scrittore
int num_writers;
int num_readers;
pthread_t capo_scrittore_thread, capo_lettore_thread;
pthread_t *scrittori;
pthread_t *lettori;

// create shared buffers
Buffer *lettori_buffer;
Buffer *scrittori_buffer;

// signal thread info
pthread_cond_t finished_threads_cond = PTHREAD_COND_INITIALIZER;
pthread_mutex_t finished_threads_mutex = PTHREAD_MUTEX_INITIALIZER;

// threads counter for sigterm
int NUM_THREADS = 2;
int finished_threads = 0;

// hashtable syncronization
Htable_sync hashtable_global_sync;

// extern FILE *log_file;


/* ******************* */

void free_memory() {
  destroy_buffer(scrittori_buffer);
  destroy_buffer(lettori_buffer);
  free(scrittori_buffer);
  free(lettori_buffer);
  free(scrittori);
  free(lettori);

  free_hash_memory();
}

/* ******************* */

void manage_sigint(int signal_num) {
  puts("[SIGNAL MANAGER] SIGINT received");
  
  // get_unique_wors_count() uses reader locks in its implementation
  fprintf(stderr, "[SIGINT] Unique words in hashtable: %d\n",
          get_unique_words_count());
}

void manage_sigterm(int signal_num) {
  puts("[SIGNAL MANAGER] SIGTERM received");
  
  // wait threads termination

  /*=================== THREADS JOIN ===================*/
  for (int i = 0; i < num_writers; i++) {
    xpthread_join(scrittori[i], NULL, line, file);
  }
  
  for (int i = 0; i < num_readers; i++) {
    xpthread_join(lettori[i], NULL, line, file);
  }

  xpthread_join(capo_scrittore_thread, NULL, line, file);
  xpthread_join(capo_lettore_thread, NULL, line, file);

  // print unique words, deallocate hashtable
  fprintf(stderr, "Unique words in hashtable: %d\n", get_unique_words_count());

  /*=================== FREE MEMORY ===================*/
  free_memory();
  
  exit(0);
}

// Funzione del thread che gestisce i segnali
void *signal_manager(void *arg) {
  puts("[SIGNAL MANAGER STARTED]");
  
  sigset_t mask;

  sigemptyset(&mask);        // remove all signals in the mask
  sigaddset(&mask, SIGINT);  // add SIGINT to mask
  sigaddset(&mask, SIGTERM); // add SIGTERM to mask

  while (1) {
    int sig_type;
    // wait for a new signal
    int err_code = sigwait(&mask, &sig_type);

    if (err_code != 0) {
      exit_msg("Sigwait error");
    }

    // manage signal received
    if (sig_type == SIGINT) {
      manage_sigint(sig_type);
    } else if (sig_type == SIGTERM) {
      manage_sigterm(sig_type);
    }
  }

  return NULL;
}

/* ******************* */

int main(int argc, char **argv) {
  if (argc != 3) {
    exit_msg("Wrong number of args, please use => #writers #readers");
  }

  num_readers = atoi(argv[1]);
  num_writers = atoi(argv[2]);

  // update total number of threads so that i can track the number of threads
  // finished in sigterm function
  NUM_THREADS += (num_readers + num_writers);
  printf("[ARCHIVIO.c] Num Threads: %d\n", NUM_THREADS);

  /*=================== SIGNALS INIT ===================*/
  sigset_t mask; // create signal mask struct

  sigemptyset(&mask);                      // remove all signals from mask
  sigaddset(&mask, SIGINT);                // block SIGINT
  sigaddset(&mask, SIGTERM);               // block SIGTERM
  pthread_sigmask(SIG_BLOCK, &mask, NULL); // block SIGINT and SIGTERM

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
  lettori_buffer = malloc(sizeof(Buffer));
  scrittori_buffer = malloc(sizeof(Buffer));

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

  /*=================== CLEANUP ===================*/
  unlink("lettori.log");
  
  /*=================== THREADS LAUNCHER ===================*/
  // threads definition
  pthread_t signal_manager_thread;

    // create scrittori lettori thread id
  scrittori = malloc(sizeof(pthread_t) * num_writers);

  if (scrittori == NULL) {
    exit_msg("Error allocating scrittori thread id array");
  }

  lettori = malloc(sizeof(pthread_t) * num_readers);
  if (lettori == NULL) {
    exit_msg("Error allocating lettori thread id array");
  }

  // start signal handler thread
  int ret_code_sig_manager = xpthread_create(&signal_manager_thread, NULL,
                                             signal_manager, NULL, line, file);
  if (ret_code_sig_manager != 0) {
    printf("[ARCHIVIO.c] Error launching signal_manager\n");
  }
  // puts("[ARCHIVIO.c] created signal manager thread");
  //   start caposc
  int ret_code_capo_scrittore =
      xpthread_create(&capo_scrittore_thread, NULL, capo_scrittore,
                      (void *)scrittori_buffer, line, file);
  if (ret_code_capo_scrittore != 0) {
    printf("[ARCHIVIO.c] Error launching capo_scrittore\n");
  }
  // puts("[ARCHIVIO.c] started capo scrittore");
  //   start capolet
  int ret_code_capo_lettore =
      xpthread_create(&capo_lettore_thread, NULL, capo_lettore,
                      (void *)lettori_buffer, line, file);
  if (ret_code_capo_lettore != 0) {
    printf("[ARCHIVIO.c] Error launching capo_lettore\n");
  }
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

  /*=================== THREADS JOIN ===================*/

    // wait signal thread
  xpthread_join(signal_manager_thread, NULL, line, file);

  return 0;
}
