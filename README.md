Durante lo sviluppo del progetto finale ho fatto delle scelte implementative che verranno brevemente discusse in questa relazione. Ogni parte del progetto e' stata divisa in un file contente le funzionalita' richieste.

- archivio.c: in questo file .c, che svolge la funzione di main, ho creato come variabili globali gli identificatori dei thread in modo da essere visibili dentro la funzione che si  occupa della gestione di sigterm, dato che thread_join deve essere svolta da tale funzione. Inoltre anche i buffer condivisi e la struct contente le variabili di sincronizzazione per la hastable sono state dichiarate come variabili globali in modo da creare una funzione `void free_memory()` da chiamare a fine esecuzione e liberare la memoria utilizzata. Come da specifica nel main ho verificato che ci fosse i due parametri che indicano il numero di thread lettori e scrittori. Successivamente ho avviato i thread capo lettore e capo scrittore, il thread che si occupa della gestione dei segnali e infine i thread lettori e scrittori.

- buffer.c: in questo file sono state implementate le funzionalita' del buffer condiviso. In particolare:
    - `void init_buffer(Buffer *buffer)` in cui inizializzo il mutex e le due variabili di condizione necessarie per la sincronizzazione della struttura condivisa buffer.
    - `void destroy_buffer(Buffer *buffer)` utilizzata per eliminare mutex e condition variables. 
    - `void insert_element(Buffer *buffer, char *elem)` tale funzione e' usata per inserire un char *elem all'interno del buffer, sia buffer che stringa da inserire sono passati come parametro
    - `char *remove_element(Buffer *buffer)` la quale passato un buffer da cui estrarre rimuove l'elento in posizione head ritornando un puntatore a char

- hashtable.c: in questo file ho implementato le funzioni di sincronizzazione per la hashtable e le funzioni aggiungi() e conta(). Come per buffer.c ho creato due funzioni per inizializzare e cancellare la struct contenente le variabili di sincronizzazione. Ho scelto una politica che favorisce i lettori come spiegato a lezione di sistemi operativi. Per avere la lock prima di accedere alla hashtable sono state create funzioni specifiche come `hashtable_reader_lock` e `hashtable_writer_lock` e le simmetriche funzioni per i writer
    - `ENTRY *create_hashtable_entry(char *string, int n)` tale funzione ritorna una entry da usare successivamente per eseguire inserimenti e ricerche nella hashtable
    - `void aggiungi(char *s)` la quale aggiunge una entry nella hashtable, se la chiave e' presente incrementa il valore nella hashtable. Alternativamente se la chiave non e' presente la inserisce con valore uguale a 1 ed aumenta il contatore globale nella struct di sincronizzazione della hashtable in modo da poter stampare il numero di parole uniche inserite nella hashtable
    - `int conta(char *s)` tale funzione ritorna il numero di occorrenze della stringa s passata come parametro
    - `int get_unique_words_count()` questa funzione prende la lock come reader sulla hastable_global_sync (struct per sincronizzare la hastable) e ritorna il numero di parole distinte presenti nella hashtable al momento dell'invocazione

- include.h: e' un header centrale in cui sono stati fatti tutti gli import in modo da includerlo nei vari file senza importare nuovamente le varie librerie di sistema necessarie o anche soltanto gli headers da me scritti

- thread functions.c: contiene le funzioni dei thread quindi 
    - `void *capo_scrittore(void *arg)` tale funzione prende come parametro il buffer condiviso con i consumatori (thread_scrittore), successivamente apre in lettura la pipe caposc e legge continuamente dati dalla pipe. Per la lettura leggo 4 byte in un intero "str_len", tale intero rappresenta la lunghezza della sequenza da leggere successivamente. Eseguo quindi una seconda read inserendo i dati letti all'interno di una stringa allocata dinamicamente, visto che non ho una dimensione fissa per tutte le sequenze, aggiungo +1 alla lunghezz in modo da inserire '\0' alla fine della string come richiesto nel testo. Infine faccio la tokenizzazione con della sequenza letta e inserisco i singoli token nel buffer. Come valore di terminazione ho scelto di considerare str_len == 0 come valore di terminazione e inserire NULL nel buffer invece di una stringa
    - `void *thread_scrittore(void *arg)` riceve come input il buffer condiviso con il capo scrittore, legge dal buffer una parola fino a che non trova NULL (valore di terminazione) ed inserisce ogni parola letta nella hastable
    - `void *capo_lettore(void *arg)` si comporta come la funzione capo_scrittore, l'unica differenza e' la pipe da cui vengono lette le sequenze ed il buffer condiviso, il quale viene condiviso con i thread lettori.
    - `void *thread_lettore(void *arg)` si occupa di estrarre le stringhe dal buffer condiviso, invocare la funzione conta sulla stringa letta e scrivere in un file 'lettori.log' la linea "parola occorrenze\n"

- server.py: e' lo script che si occupa di ricevere le sequenze lette dai client e scriverli nelle due pipes. Ho diviso le varie funzionalita' in funzioni in modo da avere codice piu' organizzato ed evitare ripetizioni. Ho usato il modulo argparse come richiesto per prendere da linea di comando i parametri richiesti in modo tale da avviare archivio.c usando os.popopen(). Come prima operazione controllo che esistano le pipe e in caso contrario le creo. Successivamente apro le pipe in cui scrivero' ed avvio archivio.c ed infine avvio la threadpool con un numero di thread passato come parametro in argv. Ogni thread si occupa di gestire una singola connessione. Poiche' le connessioni erano gestite con thread, durante i test mi sono accorto che in base allo scheduling, il messaggio di terminazione ricevuto da client1 poteva essere letto prima dei messaggi effettivi da inviare nella pipe. Per risolvere questo problema ho deciso di scrivere sulla pipe ogni messaggio diverso dalla stringa vuota dato che l'ordine dei messaggi in questo progetto non era necessario che venisse mantenuto. Come spiegato nella funzione dei thread ho deciso di scrivere nella pipe un messaggio formato da 4 byte per indicare la lunghezza e concatenare il messaggio effettivamente ricevuto dal client. 
`handle_client(conn, capolet_fd, caposc_fd)` si occupa di gestire la connessione stabilita con il client, in particolare come protocollo di comunicazione con i client ho deciso di inviare come primo byte di ogni messaggio l'identificatore del tipo di connessione (A oppure B), successivamente ricevo 4 bytes che indicano la lunghezza della sequenza ed infine la sequenza stessa ricevendo tanti bytes quanto specificato dal valore letto precedentemente. Scrivo quindi il messaggio: lunghezza+messaggio dentro la pipe controllando di aver scritto effettivamente tutti i byte richiesti (funzione: `write_to_pipe`) salvando in due contatori globali il numero di bytes scritti nelle varie pipes per eseguire logging alla fine dell'esecuzione. Per la gestione dei segnali ho deciso di terminare la scrittura nelle pipes quando ricevevo il segnale `SIGINT`, scrivendo 0 nella pipe in modo che i thread capo lettore e scrittore capissero che non ci sono piu' dati da leggere. Successivamente chiudo le pipes, invio il segnale di terminazione ad archivio.c ed attendo la sua terminazione all'interno del while usando la funzione `.poll()`. Infine elimino le pipes, faccio il log del numero di bytes scritti nelle rispettive pipes ed esco.

- client1: riceve come input da linea di comando il nome di un file, apre tale file e per ogni riga diversa dalla riga vuota chiama la funzione `send_line` che prende come input la linea, calcola la sua lunghezza e prepara il messaggio del formato "A+lunghezza+linea" in quanto e' stato definito questo formato come protocollo di comunicazione del server. Send_line invoca la funzione send_raw che apre una connessione ed invia la linea del file. Questo processo viene iterato per tutte le linee del file. Prima di uscire da client1 invio 0 al server in modo che capisca che tutti i dati necessari sono stati inviati

- client2: si comporta esattamente come client1, con la differenza che accetta 1+ files come argomento; per ogni file crea un thread che si occupa di leggere il file ed inviare al server ogni linea letta usando 1 connessione per file e non 1 connessione per linea letta come in client1. Il protocollo di comunicazione col server rimane invariato rispetto a client1 con l'unica differenza che il primo byte del messaggio e' B per indicare una connessione di tipo B come richiesto nella specifica