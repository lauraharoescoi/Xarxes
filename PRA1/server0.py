from concurrent.futures import thread
import readline
import sys, os, traceback, optparse
import time, datetime
import socket
import threading
import ctypes

global server
global sock_UDP
global sock_TCP

class Server:
    def __init__(self):
        self.id = None
        self.UDPport = None
        self.TCPport = None

class pdu_UDP:
    def __init__(self, tipus, id_transmissor, id_comunicacio, dades):
        self.tipus = tipus
        self.id_transmissor = id_transmissor
        self.id_comunicacio = id_comunicacio
        self.dades = dades

class pdu_UDP_c(ctypes.Structure):
    _fields_ = [
        ("tipus", ctypes.c_ubyte),
        ("id_transmissor", ctypes.c_char * 11),
        ("id_comunicacio", ctypes.c_char * 11),
        ("dades", ctypes.c_char * 61)
    ]
    def __init__(self, tipus, id_transmissor, id_comunicacio, dades):
        self.tipus = tipus
        self.id_transmissor = id_transmissor
        self.id_comunicacio = id_comunicacio
        self.dades = dades

def UDP_decoder(msg):
    tipus = msg.tipus
    id_transmissor = msg.id_transmissor.decode()
    id_comunicacio = msg.id_comunicacio.decode()
    dades = msg.dades.decode()
    return pdu_UDP(tipus, id_transmissor, id_comunicacio, dades)

def read_server_config():
    fs = open("server.cfg")
    x = 0
    for line in fs:
        values = line.split(' = ')
        if  x == 0:
            server.id = values[1][:-1]
        elif x == 1:
            server.UDPport = values[1][:-1]
        elif x == 2:
            server.TCPport = values[1][:-1]
        x += 1
        
def read_clients_file():
    fc = open("bbdd_dev.dat")
    clients_autoritzats = []
    i = 0
    for line in fc:
        clients_autoritzats.append(line.strip('\n'))
    return clients_autoritzats


def UDP_process(info, addr):
    info = pdu_UDP_c.from_buffer_copy(info)
    recived_pdu = UDP_decoder(info)
    print(recived_pdu.tipus, recived_pdu.id_transmissor)


def UDP_recive():
    while True:
        info, addr = sock_UDP.recvfrom(1024)
        th = threading.Thread(target = UDP_process, args = (info, addr,))
        th.start()



#def TCP_process():

if __name__ == '__main__':
    server = Server()
    read_server_config()
    clients_autoritzats = read_clients_file()

    sock_UDP = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    UDP_addr = ('', int(server.UDPport)) 
    sock_UDP.bind(UDP_addr)
    
    sock_TCP = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    TCP_addr = ('', int(server.TCPport))
    sock_TCP.bind(TCP_addr)
    sock_TCP.listen(5)

    th = threading.Thread(target = UDP_recive, args = ())
    th.start()
    """
    th = threading.Thread(target = TCP_process, args = ())
    th.start()
    """
    while True:
        command = input()

