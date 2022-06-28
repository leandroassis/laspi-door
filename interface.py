from art import *
import PIL.Image
from os import system
import platform
import pandas as pd

def logo_to_ASCII(path):
    try:
        img = PIL.Image.open(path)
        img_flag = True
    except:
        print(path, "Unable to find image ");
    
    width, height = img.size
    aspect_ratio = height/width
    new_width = 40
    new_height = aspect_ratio * new_width * 0.3
    img = img.resize((new_width, int(new_height)))
    
    img = img.convert('L')
    
    chars = ["@", "J", "D", "%", "*", "P", "+", "Y", "$", ",", ".", "^", "-", "+", "="]
    
    pixels = img.getdata()
    new_pixels = [chars[pixel//25] for pixel in pixels]
    new_pixels = ''.join(new_pixels)
    new_pixels_count = len(new_pixels)
    ascii_image = [new_pixels[index:index + new_width] for index in range(0, new_pixels_count, new_width)]
    ascii_image = "\n".join(ascii_image)
    
    return ascii_image

class interface():
    _ARDUINO_ERRO = "NOP"
    _ARDUINO_SUCESS = "OK"
    _MAXIMO_TAGS = 45

    def __init__(self):
        self.apresentacao_inicial()
        self.tags = pd.read_csv("database.csv")

        while True:
            self.menu()

    def apresentacao_inicial(self):
        logo_ASCII = logo_to_ASCII("logo_laspi.jfif")

        if platform.system() == 'Linux':
            system("clear")
        else:
            system("cls")

        print(str("-"*200).center(200))

        for linha in logo_ASCII.split('\n'):
            print(linha.center(210))

        print(text2art('''
             CERBERUS
          LASPI - DOOR''', font="univers", ))
        print("Feito por: assissantosleandro@poli.ufrj.br".rjust(200))

        print(str("-"*200).center(200))

    def menu(self):
        print('''
            Menu de Gerenciamento do CERBERUS:

            [C] = Cadastrar novo usuário.
            [D] = Deletar usuário.
            [A] = Dumpar tags para o servidor.
            [E] = Entrar em estado de erro temporário (sai ao reiniciar)
            [S] = Sair.
        ''')
        try:
            opt = str(input("Escolha uma opção: ")).upper()

            if opt in ["C", "1", "CADASTRAR", "ADD", "NOVO"]:
                self.cadastrarUsuario()
            elif opt in ["D", "2", "DELETAR", "REMOVER", "REMOVE"]:
                self.deletarUsuario()
            elif opt in ["A", "3", "ATUALIZAR", "DUMPAR"]:
                self.AtualizarServidor()
            elif opt in ["S", "4", "SAIR", "QUIT"]:
                raise KeyboardInterrupt
            elif opt in ["E", "5", "ENCERRAR", "ERRO", "ERROR"]:
                self.EntraEstadoErro()
            else:
                print("\nOpção inválida. Leia a documentação disponibilizada e tente novamente.")
                raise KeyboardInterrupt

        except KeyboardInterrupt:
            print("\nSaindo da aplicação...")
            exit()
        
    def cadastrarUsuario(self):
        # Pede um nome pro usuario e avisa para a pessoa ir encostar o cartão
        # mostrar a tag e o nome inserido e o código de erro
        # mostra as tags salvas no database e a quantidade máxima de tags possiveis
        pass

    def deletarUsuario(self):
        # Mostra as tags salvas no databse e pede o nome de um para deletar
        # Pede uma senha para prosseguir com a operação e avisa que será deletado permanentemente
        # Mostra o código de erro
        pass
    
    def AtualizarServidor(self):
        # le todas as tags do arduino e as apresenta
        # atualiza o database 
        #Mostra as tags salvas no databse atualizadas
        # mostra o código de erro
        pass
    
    def EntraEstadoErro(self):
        # apresenta código de erro
        # sai do programa
        pass

objeto = interface()