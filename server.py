#!/usr/bin/env python3
from concurrent.futures import ThreadPoolExecutor
import subprocess
import threading
import argparse
import logging
import signal
import struct
import socket
import sys
import os
import time

# syncronization for bytes written logging
written_bytes_caposc_lock = threading.Lock()
written_bytes_capolet_lock = threading.Lock()

written_bytes_caposc = 0
written_bytes_capolet = 0

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
    global written_bytes_caposc, written_bytes_capolet

    # send zero termination sequence
    zero = 0
    zero_data = zero.to_bytes(4, byteorder='little')
    os.write(caposc,  zero_data)
    os.write(capolet, zero_data)
     
    # delete pipes and kill archivio.c
    #delete_pipes()

    # send sigterm to archivio.c
    archivio_launcher.send_signal(signal.SIGTERM)
    while (archivio_launcher.poll() == None):
        time.sleep(1)

    delete_pipes()
    # log in server.log the number of bytes written in pipes
    log_bytes_written(written_bytes_caposc, written_bytes_capolet)

    sys.exit(0)
    
# signal handler
def sigint_handler(signum, frame):
    termination_sequence()
    
# logging to file
def log_bytes_written(caposc_bytes_written, capolet_bytes_written):
    logging.info(f"Connessione: A, bytes scritti in capolet: {capolet_bytes_written}")
    logging.info(f"Connessione: B, bytes scritti in caposc: {caposc_bytes_written}")

# function to correctly write data_len bytes to pipe, it writes the message until all bytes have been written
def write_to_pipe(pipe_fd, data_len, data):
    written_bytes = 0

    while (written_bytes < data_len):
        # write to pipe data until the message is all consumed
        pipe_written_bytes = os.write(pipe_fd, data[written_bytes:])
        written_bytes += pipe_written_bytes

def handle_client(conn, capolet_fd, caposc_fd):
    global written_bytes_caposc, written_bytes_capolet

    with conn:
        while True:
            # Receive the first byte indicating the client connection
            client_type = conn.recv(1).decode()

            if not client_type:
                break
            seq_size = struct.unpack("!I", conn.recv(4))[0]

            if seq_size > 0:
                data = conn.recv(seq_size)
                data_len = len(data)
                str_len = data_len.to_bytes(4, byteorder="little")
                msg = str_len + data

                # A = CAPOLET
                if (client_type == "A"):
                    # client1 -> capolet
                    write_to_pipe(capolet_fd, data_len, msg)

                    with written_bytes_capolet_lock:
                        written_bytes_capolet += (4 + len(data))

                elif (client_type == "B"):
                    # client2 -> caposc
                    write_to_pipe(caposc_fd, data_len, msg)

                    with written_bytes_caposc_lock:
                        written_bytes_caposc += (4 + len(data))

                else:
                    print(f"*********** [server.py] Received unknown client type {client_type}")
                    
            else:
                break

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

    if (args.thread_num is None):
        parser.error("The input value is required.")

    # open pipes
    capolet = os.open("capolet", os.O_RDWR)
    caposc  = os.open("caposc", os.O_RDWR)

    # start archivio
    archivio_launcher = subprocess.Popen(c_launcher)

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
                        executor.submit(handle_client, conn, capolet, caposc)

            except KeyboardInterrupt:
                pass

            server.shutdown(socket.SHUT_RDWR)

    except Exception as e:
        pass
