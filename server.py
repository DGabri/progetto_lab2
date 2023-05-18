#!/usr/bin/env python3
from concurrent.futures import ThreadPoolExecutor
import subprocess
import argparse
import logging
import signal
import struct
import socket
import sys
import os


# connection constants
HOST = '127.0.0.1'
PORT = 54901

# setup basic logging
logging.basicConfig(filename='server.log',
                    level=logging.DEBUG, datefmt='%d/%m/%y %H:%M:%S',
                    format='%(asctime)s - %(levelname)s - %(message)s')

######################################################################################################

# create the two pipes if they do not exist already
def check_create_pipes():
    if (not os.path.exists('capolet')):
        os.mkfifo('capolet')
    if (not os.path.exists('caposc')):
        os.mkfifo('caposc')

# function to delete pipes at the end of execution
def delete_pipes():
    os.unlink('capolet')
    os.unlink('caposc')

# termination sequence function, delete pipes and send termination signal to C script
def termination_sequence():
    # delete pipes and kill archivio.c
    print("[SERVER.PY] deleting pipes")
    delete_pipes()
    print("[SERVER.PY] pipes deleted, sending termination signal")
    archivio_launcher.send_signal(signal.SIGTERM)
    print("[SERVER.PY] sent termination signal")
    print("[SERVER.PY] Server shut down")
    sys.exit()
    
# signal handler
def sigint_handler(signum, frame):
    print("*** SIGINT RECEIVED***")
    termination_sequence()
    


# logging to file
def log_bytes_written(client_connection_type, caposc_bytes_written, capolet_bytes_written):

    if (client_connection_type == "A"):
        logging.info(f"Connessione: {client_connection_type}, bytes scritti in capolet: {capolet_bytes_written}")
    else:
        logging.info(f"Connessione: {client_connection_type}, bytes scritti in caposc: {caposc_bytes_written}")

######################################################################################################


def client_connection(conn, capolet_fd, caposc_fd):
    bytes_written_caposc = 0
    bytes_written_capolet = 0

    # receive messages
    with conn:
        while True:
            # get client type letter
            connection_code_type = conn.recv(1)

            if (not connection_code_type):
                print(connection_code_type)
                print(f"[server.py] error rcv connection type closing...")
                break
            
            # decode client type
            client_type = connection_code_type.decode()
            # get msg length
            str_len_bytes = conn.recv(4)
            
            # convert sequence length from bytes to int
            str_len_number = struct.unpack('!I', str_len_bytes)[0]
            
            
            # read sequence of length received before
            message = conn.recv(str_len_number)
            #print(f"[server.py] TYPE: {client_type}, LEN: {str_len_number}, MSG: {message}")
            
            # i received 0 so i have to write 0 in the pipe to terminate the sequence
            if (str_len_number == 0):
                print(f"[server.py] RECEIVED 0: {client_type} closing...")
                if client_type == "A":
                    os.write(capolet_fd, struct.pack('!I', 0))
                else:
                    os.write(caposc_fd, struct.pack('!I', 0))
                conn.close()
                break
                
            if (not message):
                print("[server.py] Closing...")
                conn.close()
                break

            if (client_type == "A"):
                # write in capolet
                # write 4 bytes to indicate string length
                os.write(capolet_fd, struct.pack('!I', str_len_number))
                # write the line
                os.write(capolet_fd, message)
                bytes_written_capolet += (4 + str_len_number)
            elif (client_type == "B"):
                # write in caposc
                # write 4 bytes to indicate string length
                os.write(caposc_fd, struct.pack('!I', str_len_number))
                # write the line
                os.write(caposc_fd, message)
                bytes_written_caposc += (4 + str_len_number)

        log_bytes_written(client_type, bytes_written_caposc, bytes_written_capolet)

if __name__ == '__main__':

    parser = argparse.ArgumentParser()
    # max thread num to handle connections
    parser.add_argument('thread_num', help='Max threads num', nargs='?')
    parser.add_argument('-r', type = int, default = 3, help = 'Num threads reader (archivio.c)')
    parser.add_argument('-w', type = int, default = 3, help = 'Num threads write (archivio.c)')
    parser.add_argument('-v', action='store_true', help ='starts valgrind if -v is present (archivio.c)')
    args = parser.parse_args()

    # create pipes
    check_create_pipes()

    # call archivio.c using subprocess
    # check if valgrind has to be started
    if (args.v == True):
        c_launcher = ['valgrind', '--leak-check=full', '--show-leak-kinds=all', '--log-file=valgrind-%p.log', './archivio', str(args.r), str(args.w)]
    else:
        c_launcher = ['./archivio', str(args.r), str(args.w)]

    archivio_launcher = subprocess.Popen(c_launcher)
    print(f"[SERVER.PY] Started: {archivio_launcher.pid}")

    capolet = os.open("capolet", os.O_RDWR)
    caposc = os.open("caposc", os.O_RDWR)

    if (args.thread_num is None):
        parser.error("The input value is required.")

    signal.signal(signal.SIGINT, sigint_handler)
    
    # server initialization
    try:
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as server:
            try:
                server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
                server.bind((HOST, PORT))
                server.listen(int(args.thread_num))

                with ThreadPoolExecutor(max_workers=int(args.thread_num)) as executor:
                    while True:
                        conn, addr = server.accept()
                        executor.submit(client_connection, conn, capolet, caposc)

            except KeyboardInterrupt:
                print("------------- EXCEPTION ------------")
                pass

            server.shutdown(socket.SHUT_RDWR)

    except Exception as e:
        pass

