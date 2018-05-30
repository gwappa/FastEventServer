import json
import struct
import socket

def open_socket(cfgfile='service.cfg', host='localhost'):
    """opens a (UDP) socket by reading necessary info from the config file."""
    with open(cfgfile, 'r') as fp:
        cfg     = json.load(fp)
    sock        = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.connect((host, cfg['port']))
    return sock

def bind_socket(cfgfile='service.cfg'):
    """binds a (UDP) socket to the localhost port specified in the cfgfile.
    returns the (socket, port) tuple."""
    with open(cfgfile, 'r') as fp:
        cfg     = json.load(fp)
    sock        = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    port        = cfg['port']
    sock.bind(('localhost', port))
    return sock, port

def new_packet():
    return struct.Struct('Bc')

