from concurrent.futures import thread
from fileinput import close
import readline
import sys, os, traceback, optparse
import time, datetime
import socket
import threading
import ctypes
import random
import select
import signal

global server
global sock_UDP
global sock_TCP
global estat_client

IP = 0 

Z = 2
W = 3

clients_autoritzats = {}

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

def signal_handler(signal, frame):
    sock_TCP.close()
    sock_UDP.close()
    sys.exit(0)

signal.signal(signal.SIGINT, signal_handler)

class Server:
    def __init__(self):
        self.id = None
        self.UDPport = None
        self.TCPport = None

class Client:
    def __init__(self): 
        self.id = None
        self.estat = Estat['DISCONNECTED']
        self.elements = None
        self.portTCP = None
        self.count = 0

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
    i = 0
    for line in fc:
        clients_autoritzats[line.strip('\n')] = Client()
    return clients_autoritzats

def check_info_reg(pdu):
    return pdu.id_comunicacio == '0000000000' and pdu.dades == ''
  
def wait_info(sock, id_comunicacio, id_transmissor):
    r, _, _ = select.select([sock], [], [], Z)
    if r:
        info, addr = sock.recvfrom(1024)
        info = pdu_UDP_c.from_buffer_copy(info)
        if (info == None):
            print('Error en el recvfrom\n')
            clients_autoritzats[info.id_transmissor].estat = Estat['DISCONNECTED']
            exit(-1)
        recived_pdu = UDP_decoder(info)
        if (recived_pdu.tipus == Package['REG_INFO']):
            if str(recived_pdu.id_comunicacio) == str(id_comunicacio) and str(recived_pdu.id_transmissor) == str(id_transmissor):
                recived_pdu = pdu_UDP(Package['INFO_ACK'], server.id, id_comunicacio, server.TCPport)
                sock.sendto(UDP_encoder(recived_pdu), addr)
                print('Registre correcte\n')
                clients_autoritzats[id_transmissor].estat = Estat['REGISTERED']
            else:
                recived_pdu = pdu_UDP(Package['INFO_NACK'], server.id, id_comunicacio, "id comunicacio o id transmissor incorrecte")
                sock.sendto(UDP_encoder(recived_pdu), addr)
                print('Registre incorrecte\n')
                clients_autoritzats[recived_pdu.id_transmissor].estat = Estat['DISCONNECTED']
                exit(-1)
        else:
            print('Error en el tipus de paquet\n')
            clients_autoritzats[recived_pdu.id_transmissor].estat = Estat['DISCONNECTED']
            exit(-1)

def send_alives(sock, id_comunicacio, id_transmissor, recived_pdu, addr):
    if recived_pdu.tipus == Package['ALIVE'] and (clients_autoritzats[recived_pdu.id_transmissor].estat == Estat['REGISTERED'] or clients_autoritzats[recived_pdu.id_transmissor].estat == Estat['SEND_ALIVE']):
        if str(recived_pdu.id_comunicacio) == str(id_comunicacio) and str(recived_pdu.id_transmissor) == str(id_transmissor):
            recived_pdu = pdu_UDP(Package['ALIVE'], server.id, id_comunicacio, recived_pdu.id_transmissor)
            print('Enviat alive\n')
            sock.sendto(UDP_encoder(recived_pdu), addr)
            clients_autoritzats[id_transmissor].estat = Estat['SEND_ALIVE']
        else:
            recived_pdu = pdu_UDP(Package['ALIVE_REJ'], server.id, id_comunicacio, "id comunicacio o id transmissor incorrecte")
            sock.sendto(UDP_encoder(recived_pdu), addr)
            print('Alive incorrecte\n')
            clients_autoritzats[id_transmissor].estat = Estat['DISCONNECTED']
            exit(-1)
    else:
        print('Error en el tipus de paquet\n')
        clients_autoritzats[recived_pdu.id_transmissor].estat = Estat['DISCONNECTED']
        exit(-1)


def UDP_process(info, addr):
    info = pdu_UDP_c.from_buffer_copy(info)
    recived_pdu = UDP_decoder(info)
    correct = False
    if recived_pdu.id_transmissor in clients_autoritzats:
        id_transmissor = recived_pdu.id_transmissor
        if recived_pdu.tipus == Package['REG_REQ'] and clients_autoritzats[recived_pdu.id_transmissor].estat == Estat['DISCONNECTED']:
            correct = check_info_reg(recived_pdu)
            if correct:
                sock_UDP_reg = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
                rand_addr = random.randint(0000000000, 9999999999)
                clients_autoritzats[id_transmissor].id_comunicacio = str(rand_addr)
                global IP
                IP = socket.gethostbyname("localhost")
                sock_UDP_reg.bind((IP, 0))
                send_pdu = pdu_UDP(Package['REG_ACK'], server.id, rand_addr, sock_UDP_reg.getsockname()[1])
                send_pdu = UDP_encoder(send_pdu)
                a = sock_UDP.sendto(send_pdu, addr)
                clients_autoritzats[recived_pdu.id_transmissor].estat = Estat['WAIT_INFO']
                wait_info(sock_UDP_reg, rand_addr, id_transmissor)
        elif recived_pdu.tipus == Package['ALIVE']:
            correct = True
            send_alives(sock_UDP, recived_pdu.id_comunicacio, id_transmissor, recived_pdu, addr)
        else:
            print('Error en el estat\n')
            clients_autoritzats[recived_pdu.id_transmissor].estat = Estat['DISCONNECTED']
            exit(-1)

        
    if correct == False:
        send_pdu = pdu_UDP(Package['REG_REJ'], server.id, '0000000000', 'Client no autoritzat')
        send_pdu = UDP_encoder(send_pdu)
        a = sock_UDP.sendto(send_pdu, addr)
        if a < 0:
            print('Error en el sendto\n')
            clients_autoritzats[info.id_transmissor].estat = Estat['DISCONNECTED']
            exit(-1)

def UDP_recive():
    while True:
        info, addr = sock_UDP.recvfrom(1024)
        th = threading.Thread(target = UDP_process, args = (info, addr,))
        th.start()

#def TCP_process():

if __name__ == '__main__':
    
    try:
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
    except KeyboardInterrupt:
        sock_UDP.close()
        sock_TCP.close()
        print("S'ha interromput la connexió")
        os._exit(0)
