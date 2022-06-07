import PySimpleGUI as sg
import socket

database = open("database.csv", "r")
tags_cadastradas = database.readlines()

layout = [  [sg.Text("Nome ou identificação:"), sg.Input(size=(30, 0), key="id")],
            [sg.Button("Cadastrar"), sg.Button("Deletar")],
            [sg.Listbox(values=tags_cadastradas, size=(50, 6))]]

window = sg.Window('Portal de Gerenciamento de Acesso', layout)
with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
    s.connect(("192.168.1.15", 80))
    #print(s.recv(1024))
    s.send("cadastrar")
    print(s.recv(1024))


window.close()
database.close()
s.close()