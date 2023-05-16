#!/usr/bin/env python3
import os
import logging
import argparse
import subprocess
import signal
import struct
from concurrent.futures import ThreadPoolExecutor
import socket

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
    if (not os.path.exists('caposc')):
        os.mkfifo('caposc')

# function to delete pipes at the end of execution
def delete_pipes():
    os.unlink('capolet')
    os.unlink('caposc')

# logging to file
def log_bytes_written(client_connection_type, caposc_bytes_written, capolet_bytes_writte):

    if (client_connection_type == "A"):
        logging.info(f"Connessione: {client_connection_type}, bytes scritti in capolet: {capolet_bytes_writte}")
    else:
        logging.info(f"Connessione: {client_connection_type}, bytes scritti in caposc: {caposc_bytes_written}")

def client_connection(conn):
    # get client type letter
    client_type_code = conn.recv(1)
    client_type = client_type_code.decode()
    
    # open pipes
    if (client_type == "A"):
        capolet_fd = os.open("capolet", os.O_WRONLY)
    else:
        caposc_fd = os.open("caposc", os.O_WRONLY)

    # logging stats
    bytes_written_caposc = 0
    bytes_written_capolet = 0

    # wait for new messages
    while True:
        # receive msg len
        str_len_bytes = conn.recv(4)
        #print(f"STR LEN: {str_len_bytes}")

        if (not str_len_bytes):
            log_bytes_written(client_type, bytes_written_caposc, bytes_written_capolet)
            conn.close()  
        else:
            str_len_number = struct.unpack('!I', str_len_bytes)[0]

            # get message reading as many bytes as the length received at line 45
            message = conn.recv(str_len_number)
            #print(".py => -----")
            #print(f".py => Conn type: {client_type}")

            # received data
            if (not message):
                log_bytes_written(client_type, bytes_written_caposc, bytes_written_capolet)
                conn.close()  
            
            else:
                if (client_type == "A"):
                    # write in capolet
                    bytes_written_capolet += (4 + str_len_bytes)
                    # write 4 bytes to indicate string length
                    #print(".py => Received msg A:", message.decode())
                    os.write(capolet_fd, str_len_bytes)
                    # write the line
                    os.write(capolet_fd, message)
                    #print(f".py => Wrote string: {message}")

                elif (client_type == "B"):
                    #write in caposc
                    bytes_written_caposc += (4 + str_len_bytes)
                    # write 4 bytes to indicate string length
                    #print(".py => Received msg B:", message.decode())   
                    os.write(caposc_fd, str_len_bytes)
                    # write the line
                    os.write(caposc_fd, message)
                    #print(f".py => Wrote string: {message}")

if __name__ == '__main__':

    parser = argparse.ArgumentParser()
    parser.add_argument('-t', type = int, required = True, help = 'Max threads num')
    parser.add_argument('-r', type = int, default = 3, help = 'Num threads reader (archivio.c)')
    parser.add_argument('-w', type = int, default = 3, help = 'Num threads write (archivio.c)')
    parser.add_argument('-v', type = int, default = 0, action = range(0, 1), help ='starts valgrind if 1 (archivio.c)')
    args = parser.parse_args()

    # create pipes
    check_create_pipes()

    # call archivio.c using subprocess
    
    if (args.v):
        c_launcher = ['valgrind', '--leak-check=full', '--show-leak-kinds=all', '--log-file=valgrind-%p.log', './archivio', '-r', str(args.r), '-w', str(args.w)]
    else:
        c_launcher = ['./archivio', '-r', str(args.r), '-w', str(args.w)]
    

    archivio_launcher = subprocess.Popen(c_launcher)
    print("Ho lanciato il processo:", archivio_launcher.pid)

    # server initialization
    server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    server.bind((HOST, PORT))
    server.listen(args.t)
    
    #start connection manager threads
    
    with ThreadPoolExecutor(max_workers=args.t) as executor:
        try:
            while True:
                # accept new conn
                conn, _ = server.accept()
                executor.submit(client_connection, conn)

        except KeyboardInterrupt:
            print("Shutting down...")
            server.shutdown(socket.SHUT_RDWR)
            server.close()

            # delete pipes and kill archivio.c
            delete_pipes()            
            archivio_launcher.send_signal(signal.SIGTERM)