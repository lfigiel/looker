#!/usr/bin/env python
import urllib.request
from time import sleep, strftime
import sys
import socket
from html.parser import HTMLParser
import csv

i = 1

class MyHTMLParser(HTMLParser):
    def handle_data(self, data):
        global i
        l = data.split(": ")
        if len(l) == 2 and l[0] == "Temp [C]":
#            print(l[1])
            f = open(sys.argv[2], "a")
            try:
                writer = csv.writer(f)
                writer.writerow( (i, strftime("%c"), l[1]) )
                i = i + 1
            finally:
                f.close()

parser = MyHTMLParser()

host = socket.gethostbyname(sys.argv[1])

while 1:
    parser.feed(urllib.request.urlopen("http://" + host).read().decode("utf-8"))
    sleep(1)

