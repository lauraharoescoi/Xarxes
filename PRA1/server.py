#!/usr/bin/env python3

import sys, os
import socket
import threading
import ctypes
import random
import select
import time
import signal

#variables globals
global server
global sock_UDP
global sock_TCP
global estat_client

global d
d = False

global num_clients
num_clients = 0

IP = 0 
Z = 2
W = 3

#diccionari per a guardar les dades dels clients autoritzats que es connectin
clients_autoritzats = {}

#dades
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

#funcio que retorna un string del paquet que li diguem
def to_string(msg):
    if msg == Package['REG_REQ']:
        return "REG_REQ"
    elif msg == Package['REG_ACK']:
        return "REG_ACK"    
    elif msg == Package['REG_NACK']:
        return "REG_NACK"
    elif msg == Package['REG_REJ']:
        return "REG_REJ"
    elif msg == Package['REG_INFO']:
        return "REG_INFO"
    elif msg == Package['INFO_ACK']:
        return "INFO_ACK"
    elif msg == Package['INFO_NACK']:
        return "INFO_NACK"
    elif msg == Package['INFO_REJ']:
        return "INFO_REJ"
    elif msg == Package['ALIVE']:
        return "ALIVE"
    elif msg == Package['ALIVE_NACK']:
        return "ALIVE_NACK"
    elif msg == Package['ALIVE_REJ']:
        return "ALIVE_REJ"
    elif msg == Estat['DISCONNECTED']:
        return "DISCONNECTED"
    elif msg == Estat['NOT_REGISTERED']:
        return "NOT_REGISTERED"
    elif msg == Estat['WAIT_ACK_REG']:
        return "WAIT_ACK_REG"
    elif msg == Estat['WAIT_INFO']:
        return "WAIT_INFO"
    elif msg == Estat['WAIT_ACK_INFO']:
        return "WAIT_ACK_INFO"
    elif msg == Estat['REGISTERED']:
        return "REGISTERED"
    elif msg == Estat['SEND_ALIVE']:
        return "SEND_ALIVE"
    else:
        return "ERROR"

#classes per a guardar les dades
class Server:
    def __init__(self):
        self.id = None
        self.UDPport = None
        self.TCPport = None

class Client:
    def __init__(self): 
        self.id = None
        self.estat = Estat['DISCONNECTED']
        self.id_comunicacio = None
        self.elements = None
        self.portTCP = None
        self.count = 0
        self.ip = 0

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

#funcio per a llegir la comanda inicial
def read_command():
    argv = sys.argv
    fs = 'server.cfg'
    fc = 'bbdd_dev.dat'
    while argv:
        if argv[0] == '-c':
            fs = argv[1]
        elif argv[0] == '-d':
            global d
            d = True
        elif argv[0] == '-u':
            fc = argv[1]
        argv = argv[1:]
    return fs, fc

#imprimir la comanda list
def print_list():
    buffer = 'Llegits {} equips autoritzats en el sistema'.format(str(num_clients))
    print_debug(buffer)
    print("-ID.-DISP- -ID.-COM.- ------ IP ---- ---- ESTAT ---- -- ELEMENTS ---------------------------------")
    for i in clients_autoritzats:
        print(i, "  ", clients_autoritzats[i].id_comunicacio, "  ", clients_autoritzats[i].ip, "  ", to_string(clients_autoritzats[i].estat), "  ", clients_autoritzats[i].elements)

#traduccio de C a python
def UDP_decoder(msg):
    tipus = msg.tipus
    id_transmissor = msg.id_transmissor.decode()
    id_comunicacio = msg.id_comunicacio.decode()
    dades = msg.dades.decode()
    return pdu_UDP(tipus, id_transmissor, id_comunicacio, dades)

#traduccio de python a C
def UDP_encoder(msg):
    tipus = msg.tipus
    id_transmissor = str(msg.id_transmissor).encode()
    id_comunicacio = str(msg.id_comunicacio).encode()
    dades = str(msg.dades).encode()
    return pdu_UDP_c(tipus, id_transmissor, id_comunicacio, dades)

#print per als missatges de debug
def print_debug(msg):
    t = time.strftime("%H:%M:%S: ")
    if(d):
        print(t, "DEBUG  =>  ", msg)

#print per als missatges en general
def print_msg(msg):
    t = time.strftime("%H:%M:%S: ")
    print(t, "MSG  =>  ", msg)

#llegir la configuracio del servidor
def read_server_config(f):
    fs = open(f, 'r')
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
        
#llegir quins clients estan autoritzats
def read_clients_file(f):
    fs = open(f, 'r')
    for line in fs:
        clients_autoritzats[line.strip('\n')] = Client()
        global num_clients
        num_clients += 1

#revisio del paquet info_reg
def check_info_reg(pdu):
    return pdu.id_comunicacio == '0000000000' and pdu.dades == ''

#funcio per al proces wait info
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
        buffer = "Dispositiu: {} , passa a l'estat: {}".format(id_transmissor, to_string(Estat['WAIT_INFO']))
        print_msg(buffer)
        if (recived_pdu.tipus == Package['REG_INFO']):
            if str(recived_pdu.id_comunicacio) == str(id_comunicacio) and str(recived_pdu.id_transmissor) == str(id_transmissor):
                clients_autoritzats[id_transmissor].estat = Estat['REGISTERED']
                recived_pdu = pdu_UDP(Package['INFO_ACK'], server.id, id_comunicacio, server.TCPport)
                #creem un nou socket per a seguir gestionan el registre del client
                sock.sendto(UDP_encoder(recived_pdu), addr)
                buffer = "Enviat: bytes= 84 , comanda= {} , Id.= {} , Id. Com.= {} , Dades= {}".format(to_string(recived_pdu.tipus), recived_pdu.id_transmissor, recived_pdu.id_comunicacio, recived_pdu.dades)
                print_debug(buffer)
                buffer = "Dispositiu: {} , passa a l'estat: {}".format(id_transmissor, to_string(Estat['REGISTERED']))
                print_msg(buffer)
            else:
                clients_autoritzats[recived_pdu.id_transmissor].estat = Estat['DISCONNECTED']
                recived_pdu = pdu_UDP(Package['INFO_NACK'], server.id, id_comunicacio, "id comunicacio o id transmissor incorrecte")
                sock.sendto(UDP_encoder(recived_pdu), addr)
                buffer = "Enviat: bytes= 84 , comanda= {} , Id.= {} , Id. Com.= {} , Dades= {}".format(to_string(recived_pdu.tipus), recived_pdu.id_transmissor, recived_pdu.id_comunicacio, recived_pdu.dades)
                print_debug(buffer)
                buffer = "Dispositiu: {} , passa a l'estat: {}".format(id_transmissor, to_string(Estat['DISCONNECTED']))
                print_msg(buffer)
                exit(-1)
        else:
            print('Error en el tipus de paquet\n')
            clients_autoritzats[recived_pdu.id_transmissor].estat = Estat['DISCONNECTED']
            exit(-1)

#funcio per a enviar els alives
def send_alives(sock, id_comunicacio, id_transmissor, recived_pdu, addr):
    if recived_pdu.tipus == Package['ALIVE'] and (clients_autoritzats[recived_pdu.id_transmissor].estat == Estat['REGISTERED'] or clients_autoritzats[recived_pdu.id_transmissor].estat == Estat['SEND_ALIVE']):
        if str(recived_pdu.id_comunicacio) == str(id_comunicacio) and str(recived_pdu.id_transmissor) == str(id_transmissor) and str(recived_pdu.dades) == '':
            if (clients_autoritzats[recived_pdu.id_transmissor].estat == Estat['REGISTERED']):
                buffer = "Dispositiu: {} , passa a l'estat: {}".format(id_transmissor, to_string(Estat['SEND_ALIVE']))
                print_msg(buffer)
            recived_pdu = pdu_UDP(Package['ALIVE'], server.id, id_comunicacio, recived_pdu.id_transmissor)
            sock.sendto(UDP_encoder(recived_pdu), addr)
            buffer = "Enviat: bytes= 84 , comanda= {} , Id.= {} , Id. Com.= {} , Dades= {}".format(to_string(recived_pdu.tipus), recived_pdu.id_transmissor, recived_pdu.id_comunicacio, recived_pdu.dades)
            print_debug(buffer)
            clients_autoritzats[id_transmissor].estat = Estat['SEND_ALIVE']
        else:
            recived_pdu = pdu_UDP(Package['ALIVE_REJ'], server.id, id_comunicacio, "id comunicacio o id transmissor incorrecte")
            sock.sendto(UDP_encoder(recived_pdu), addr)
            buffer = "Enviat: bytes= 84 , comanda= {} , Id.= {} , Id. Com.= {} , Dades= {}".format(to_string(recived_pdu.tipus), recived_pdu.id_transmissor, recived_pdu.id_comunicacio, recived_pdu.dades)
            print_debug(buffer)
            clients_autoritzats[id_transmissor].estat = Estat['DISCONNECTED']
            exit(-1)
    else:
        print('Error en el tipus de paquet\n')
        clients_autoritzats[recived_pdu.id_transmissor].estat = Estat['DISCONNECTED']
        exit(-1)

#funcio que tramita els paquets que arriben pel primer socket
def UDP_process(info, addr):
    info = pdu_UDP_c.from_buffer_copy(info)
    recived_pdu = UDP_decoder(info)
    correct = False
    buffer = "Rebut: bytes= 84 , comanda= {} , Id.= {} , Id. Com.= {} , Dades= {}".format(to_string(recived_pdu.tipus), recived_pdu.id_transmissor, recived_pdu.id_comunicacio, recived_pdu.dades)
    print_debug(buffer)
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
                buffer = "Enviat: bytes= 84 , comanda= {} , Id.= {} , Id. Com.= {} , Dades= {}".format(to_string(recived_pdu.tipus), recived_pdu.id_transmissor, recived_pdu.id_comunicacio, recived_pdu.dades)
                print_debug(buffer)
                clients_autoritzats[recived_pdu.id_transmissor].estat = Estat['WAIT_INFO']
                wait_info(sock_UDP_reg, rand_addr, id_transmissor)
        elif recived_pdu.tipus == Package['ALIVE']:
            correct = True
            send_alives(sock_UDP, recived_pdu.id_comunicacio, id_transmissor, recived_pdu, addr)
        else:
            print_msg('Error en el estat\n')
            clients_autoritzats[recived_pdu.id_transmissor].estat = Estat['DISCONNECTED']
            exit(-1)

        
    if correct == False:
        send_pdu = pdu_UDP(Package['REG_REJ'], server.id, '0000000000', 'Client no autoritzat')
        send_pdu = UDP_encoder(send_pdu)
        a = sock_UDP.sendto(send_pdu, addr)
        buffer = "Enviat: bytes= 84 , comanda= {} , Id.= {} , Id. Com.= {} , Dades= {}".format(to_string(recived_pdu.tipus), recived_pdu.id_transmissor, recived_pdu.id_comunicacio, recived_pdu.dades)
        print_debug(buffer)
        if a < 0:
            print('Error en el sendto\n')
            clients_autoritzats[info.id_transmissor].estat = Estat['DISCONNECTED']
            exit(-1)

#funcio que crea el thread per a gestionar els paquets UDP
def UDP_recive():
    while True:
        info, addr = sock_UDP.recvfrom(1024)
        th = threading.Thread(target = UDP_process, args = (info, addr,))
        th.start()


if __name__ == '__main__':
    
    try:
        #configuracio de les dades del servidor
        fs, fc = read_command()
        server = Server()
        read_server_config(fs)
        read_clients_file(fc)
        if(d):
            print_list()

        #configuracio dels sockets
        sock_UDP = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        UDP_addr = ('', int(server.UDPport)) 
        sock_UDP.bind(UDP_addr)
        buffer = 'Socket UDP actiu'
        print_debug(buffer)
        
        sock_TCP = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        TCP_addr = ('', int(server.TCPport))
        sock_TCP.bind(TCP_addr)
        sock_TCP.listen(5)
        buffer = 'Socket TPC actiu'
        print_debug(buffer)

        #creacio de thread
        th = threading.Thread(target = UDP_recive, args = ())
        buffer = 'Creat fill per gestionar la BBDD del dispositiu'
        print_debug(buffer)
        buffer = 'Establert temporitzador per la gestió de la BBDD'
        print_msg(buffer)
        th.start()

        #llegir les comandes pel terminal
        while True:
            command = input()
            if command == 'quit':
                signal.raise_signal(signal.SIGINT)
            elif command == 'list':
                print_list()
    #que passsa si es fa control + c o si es crida la funcio quit        
    except KeyboardInterrupt:
        sock_UDP.close()
        sock_TCP.close()
        print("\nS'ha interromput la connexió")
        os._exit(0)
