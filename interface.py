import re
import PySimpleGUI as sg
import requests
import random as rd


def tagNotIn(tag, linhas):
    for linha in linhas:
        if tag == linha.split(",")[1][1:]:
            return False
    return True

database = open("database.csv", "r+")

sg.theme('DarkBlue')
layout = [  [sg.Text("Nome ou identificação:"), sg.Input(size=(30, 0), key="id")],
            [sg.Button("Cadastrar"), sg.Button("Deletar"), sg.Button("Dumpar EEPROM")],
            [sg.Text("Saída de informação;")],
            [sg.Output(size=(50, 4))],
            [sg.Text("Tags já cadastradas:")],
            [sg.Listbox(values=database.readlines(), size=(50, 6))]]

database.close()
window = sg.Window('Portal de Gerenciamento de Acesso', layout)

while True:
    event, values = window.read()
    if event == sg.WIN_CLOSED: # if user closes window or clicks cancel
        break

    nome = values['id'] if len(values['id']) > 0 else "Desconhecido"+str(rd.randint(0, 100001))

    if(event == "Cadastrar"):
        # Cadastrar
        database = open("database.csv", "a+")

        print(f"Cadastrando o usuário: {nome} no Controle de Acesso.")
        resultado = requests.get("http://192.168.88.19/cadastrar=")
        resultado.close()
        
        resultado = resultado.text.split("\r\n")

        if("FULL OR ALREADY SIGN" not in resultado):
            database.write(nome+","+resultado[3]+"\n")
            print(f"Usuário {nome} cadastrado com sucesso.")
        else:
            print("Erro cadastrando o usuário. Espaço insuficiente na memória ou já cadastrado.")

        database.close()

    elif(event == "Deletar"):
        database = open("database.csv", "r")
        linhas = database.readlines()
        database.close()

        database = open("database.csv", "w")
        database.write(linhas[0])
        print(f"Deletando o usuário: {nome} do Controle de Acesso")
        resultado = requests.get("http://192.168.88.19/deletar=")
        resultado.close()
        resultado = resultado.text.split("\r\n")
        
        if("Limpo" in resultado):
            print("Memória do microcontrolador limpa")
            for linha in linhas[1:]:
                if nome != linha.split(",")[0]:
                    database.write(linha)
                    resultado = requests.get("http://192.168.88.19/addtag="+linha.split(",")[1][1:])
                    resultado.close()
                    resultado = resultado.text.split("\r\n")
                    if "OK" not in resultado:
                        print("Tag "+linha.split(',')[1]+" não cadastrada por erro.")
        database.close()
        print("Tag deletada com sucesso.")

    elif(event == "Dumpar EEPROM"):
        database = open("database.csv", "r+")

        print("Dumpando a EEPROM do microcontrolador.")
        resultado = requests.get("http://192.168.88.19/atualizarserver=")
        resultado.close()

        resultado = resultado.text.split("\r\n")[3:-3]
        for indice, tag in enumerate(resultado):
            if tag != "FFFFFFFF":
                if tagNotIn(tag, database.readlines()):
                    database.write(f"Desconhecido{indice}, {tag}\n")
        
        database.close()
        print("Tags dumpadas com sucesso.")

window.close()
