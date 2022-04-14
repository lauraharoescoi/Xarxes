from concurrent.futures import thread
import readline
import sys, os, traceback, optparse
import time, datetime
import socket
import threading
import ctypes
import random

global server
global sock_UDP
global sock_TCP
global estat_client
IP = 0 

Z = 2

Package = {
    'REG_REQ': 0xa0,
    'REG_ACK' : 0xa1,
    'REG_NACK' : 0xa2,
    'REG_REJ' : 0xa3,
    'REG_INFO' : 0xa4,
    'INFO_ACK' : 0xa5,
    'INFO_NACK' : 0xa6,
    'INFO_REJ' : 0xa7,
    'ALIVE' : 0xb0,
    'ALIVE_NACK' : 0xb1,
    'ALIVE_REJ' : 0xb2 }

Estat = {
    'DISCONNECTED' : 0xf0,
    'NOT_REGISTERED' : 0xf1,
    'WAIT_ACK_REG' : 0xf2,
    'WAIT_INFO' : 0xf3,
    'WAIT_ACK_INFO' : 0xf4,
    'REGISTERED' : 0xf5,
    'SEND_ALIVE' : 0xf6 }

class Server:
    def __init__(self):
        self.id = None
        self.UDPport = None
        self.TCPport = None

class Client:
    def __init__(self): 
        self.id = None
        self.estat = Estat.DISCONNECTED
        self.elements = None


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

def UDP_encoder(msg):
    tipus = msg.tipus
    id_transmissor = str(msg.id_transmissor).encode()
    id_comunicacio = str(msg.id_comunicacio).encode()
    dades = str(msg.dades).encode()
    return pdu_UDP_c(tipus, id_transmissor, id_comunicacio, dades)

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

def check_info_reg(pdu):
    return pdu.id_comunicacio == '0000000000' and pdu.dades == ''
  
def wait_info(sock, addr):
    sock.settimeout(Z)
    info, addr = sock.recvfrom(1024)
    if (info == None):
        print('Error en el recvfrom\n')
        Client.estat = Estat.DISCONNECTED
        exit(-1)
    recived_pdu = UDP_decoder(info)
    if (recived_pdu.tipus == Package['REG_INFO']):
        if (recived_pdu.id_communicacio == addr and recived_pdu.id_transmissor == server.id):
            Client.estat = Estat.REGISTERED
            Client.elements = recived_pdu.dades
            print('Registre correcte\n')
    


def UDP_process(info, addr):
    info = pdu_UDP_c.from_buffer_copy(info)
    recived_pdu = UDP_decoder(info)
    correct = False
    if recived_pdu.id_transmissor in clients_autoritzats:
        if recived_pdu.tipus == Package['REG_REQ']:
            correct = check_info_reg(recived_pdu)
            sock_UDP_reg = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            rand_addr = random.randint(0000000000, 9999999999)
            global IP
            IP = socket.gethostbyname("localhost")
            sock_UDP_reg.bind((IP, 0))
            send_pdu = pdu_UDP(Package['REG_ACK'], server.id, rand_addr, sock_UDP_reg.getsockname()[1])
            send_pdu = UDP_encoder(send_pdu)
            a = sock_UDP.sendto(send_pdu, addr)
            #wait_info(sock_UDP_reg, rand_addr)
            
    if correct == False:
        send_pdu = pdu_UDP(Package['REG_REJ'], server.id, '0000000000', 'Client no autoritzat')
        send_pdu = UDP_encoder(send_pdu)
        a = sock_UDP.sendto(send_pdu, addr)
        if a < 0:
            print('Error en el sendto\n')
            Client.estat = Estat.DISCONNECTED
            exit(-1)

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
    print("pito\n")
    while True:
        command = input()

