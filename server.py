#!/usr/bin/env python3
from concurrent.futures import ThreadPoolExecutor
import subprocess
import argparse
import logging
import signal
import struct
import socket
import os

# connection constants
HOST = '127.0.0.1'
PORT = 54901

# setup basic logging
logging.basicConfig(filename='server.log',
                    level=logging.DEBUG, datefmt='%d/%m/%y %H:%M:%S',
                    format='%(asctime)s - %(levelname)s - %(message)s')

# create the two pipes if they do not exist already
def check_create_pipes():
    if (not os.path.exists('capolet')):
        os.mkfifo('capolet')
        #os.chmod('capolet', 0o777)
        print(".py => created capolet")
    if (not os.path.exists('caposc')):
        os.mkfifo('caposc')
        #os.chmod('caposc', 0o777)
        print(".py => created caposc")

# function to delete pipes at the end of execution
def delete_pipes():
    os.unlink('capolet')
    os.unlink('caposc')

def write_zero_sequence_to_pipe(client_code, sc_fd, let_fd):
    if (client_code == "A"):
        os.write(let_fd, b'')
    else:
        os.write(sc_fd, b'')

# logging to file
def log_bytes_written(client_connection_type, caposc_bytes_written, capolet_bytes_written):

    if (client_connection_type == "A"):
        logging.info(f"Connessione: {client_connection_type}, bytes scritti in capolet: {capolet_bytes_written}")
    else:
        logging.info(f"Connessione: {client_connection_type}, bytes scritti in caposc: {caposc_bytes_written}")

def client_connection(conn):
    # logging stats
    bytes_written_caposc = 0
    bytes_written_capolet = 0
    client_type = ""

    # get client type letter
    connection_code_type = conn.recv(1)

    if (not connection_code_type):
        return
    
    # decode client type
    client_type = connection_code_type.decode()

    # open pipes
    if (client_type == "A"):
        capolet_fd = os.open("capolet", os.O_RDWR)
        print(".py => opened capolet FIFO")
    else:
        caposc_fd = os.open("caposc", os.O_RDWR)
        print(".py => opened caposc FIFO")


    while True:

        # get msg length
        str_len_bytes = conn.recv(4)

        if (not str_len_bytes):
            print("Zero seq len")
            write_zero_sequence_to_pipe(client_type, caposc_fd, capolet_fd)
            break

        #convert sequence length from bytes to int
        str_len_number = struct.unpack('!I', str_len_bytes)[0]
        
        # read sequnce of length received before
        message = conn.recv(str_len_number)
        
        if (not message):
            print("Zero seq msg")
            write_zero_sequence_to_pipe(client_type, caposc_fd, capolet_fd)
            break
        
        if (client_type == "A"):
            # write in capolet
            bytes_written_capolet += (4 + str_len_number)
            # write 4 bytes to indicate string length
            #print(".py => Received msg A:", message.decode())
            os.write(capolet_fd, str_len_bytes)
            # write the line
            os.write(capolet_fd, message)

        elif (client_type == "B"):
            #write in caposc
            bytes_written_caposc += (4 + str_len_number)
            # write 4 bytes to indicate string length
            #print(".py => Received msg B:", message.decode())   
            os.write(caposc_fd, str_len_bytes)
            # write the line
            os.write(caposc_fd, message)

        #print(f"Client code: {client_type}, len: {str_len_number}, msg: {message.decode()}")
        

    # Connection closed
    conn.close()
    print("CLOSED CONNECTION")
    log_bytes_written(client_type, bytes_written_caposc, bytes_written_capolet)

if __name__ == '__main__':
    print(".py => starting server")

    parser = argparse.ArgumentParser()
    parser.add_argument('-t', type = int, required = True, help = 'Max threads num')
    parser.add_argument('-r', type = int, default = 3, help = 'Num threads reader (archivio.c)')
    parser.add_argument('-w', type = int, default = 3, help = 'Num threads write (archivio.c)')
    parser.add_argument('-v', action='store_true', help ='starts valgrind if -v is present (archivio.c)')
    args = parser.parse_args()

    # create pipes
    check_create_pipes()
    print(".py => created pipes")

    # call archivio.c using subprocess
    
    if (args.v == True):
        c_launcher = ['valgrind', '--leak-check=full', '--show-leak-kinds=all', '--log-file=valgrind-%p.log', './archivio', str(args.r), str(args.w)]
    else:
        c_launcher = ['./archivio', str(args.r), str(args.w)]
    

    archivio_launcher = subprocess.Popen(c_launcher)
    print(".py => launched archivio", archivio_launcher.pid)
    print(".py => listening")
    print(".py => args thread", args.t)

    # server initialization
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as server:
        try:  
            server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)            
            server.bind((HOST, PORT))
            server.listen(args.t)

            with ThreadPoolExecutor(max_workers=args.t) as executor:
                while True:
                    conn, _ = server.accept()
                    executor.submit(client_connection, conn)

        except KeyboardInterrupt:
            # delete pipes and kill archivio.c
            print("deleting pipes")
            delete_pipes()       
            print("pipes deleted, sending termination signal")     
            archivio_launcher.send_signal(signal.SIGTERM)
            print("sent termination signal")
            print("Server shut down")

        server.shutdown(socket.SHUT_RDWR)
        server.close()