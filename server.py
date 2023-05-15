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
logging.basicConfig(level=logging.INFO)

# create the two pipes if they do not exist already
def check_create_pipes():
    if not os.path.exists('capolet'):
        os.mkfifo('capolet')
    if not os.path.exists('caposc'):
        os.mkfifo('caposc')

# function to delete pipes at the end of execution
def delete_pipes():
    os.unlink('capolet')
    os.unlink('caposc')

def client_connection(conn):
    # get client type letter
    client_type_code = conn.recv(1)
    client_type = client_type_code.decode()

    # wait for new messages
    while True:
        # receive msg len
        str_len_bytes = conn.recv(4)
        #print(f"STR LEN: {str_len_bytes}")

        if (not str_len_bytes):
            print("conn close")
            conn.close()  
        else:
            str_len_number = struct.unpack('!I', str_len_bytes)[0]

            # get message reading as many bytes as the length received at line 45
            message = conn.recv(str_len_number).decode()
            print("-----")
            print(f"Conn type: {client_type}")

            # received data
            if (not message):
                print("conn close")
                conn.close()  
            
            else:
                if (client_type == "A"):
                    # write in capolet
                    print("Received msg A:", message)

                elif (client_type == "B"):
                    #write in caposc
                    print("Received msg B:", message)   

if __name__ == '__main__':

    parser = argparse.ArgumentParser()
    parser.add_argument('-t', type = int, required = True, help = 'Max threads num')
    #parser.add_argument('-r', type = int, default = 3, help = 'Num threads reader (archivio.c)')
    #parser.add_argument('-w', type = int, default = 3, help = 'Num threads write (archivio.c)')
    #parser.add_argument('-v', type = int, default = 0, action = range_action(0, 1) help ='starts valgrind if 1 (archivio.c)')
    args = parser.parse_args()

    # create pipes
    #check_create_pipes()

    # call archivio.c using subprocess
    """
    if (args.v):
        c_launcher = ['valgrind', '--leak-check=full', '--show-leak-kinds=all', '--log-file=valgrind-%p.log', './archivio', '-r', str(args.r), '-w', str(args.w)]
    else:
        c_launcher = ['./archivio', '-r', str(args.r), '-w', str(args.w)]
    """

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
            #delete_pipes()            
            #archive.send_signal(signal.SIGTERM)