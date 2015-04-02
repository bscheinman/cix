#!/usr/bin/python

import sys

import socket

ADDRESS = 'localhost'
PORT = 13579
SYMBOL = 'GOOG'

def get_connection():
    s = socket.socket()
    s.connect((ADDRESS, PORT))
    return s

def int_to_network(n):
    result = []
    for i in range(4):
        result.append((n >> (i * 8)) & 0xff)
    return result

class Order(object):
    def __init__(self, side, quantity, price, symbol=SYMBOL):
        self.side = side
        self.quantity = int(quantity)
        self.price = int(price)
        self.symbol = symbol

        assert len(self.symbol) < 8

    def to_network(self):
        data = []
        data.append(0)
        data += map(ord, self.symbol)
        data += [0] * (8 - len(self.symbol))
        data += int_to_network(self.quantity)
        data += int_to_network(self.price)
        data.append(0 if self.side == 'B' else 1)
        data += [0] * 16
        return ''.join(map(chr, data))
        

def process_line(line, connection):
    if not line.strip():
        return
    fields = [f.strip() for f in line.split()]
    order = Order(*fields)
    connection.send(order.to_network())

if __name__ == '__main__':
    input_path = sys.argv[1] if len(sys.argv) >= 2 else sys.stdout
    conn = get_connection()

    with open(input_path, 'r') as input_file:
        for line in input_file:
            process_line(line, conn)

