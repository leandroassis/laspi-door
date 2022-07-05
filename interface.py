from art import *
import PIL.Image
from os import system
import platform
import pandas as pd
import socket

'''
MELHORIAS PARA OS PROXIMOS DESAVISADOS QUE CAIREM AQ:
-> Faça uma função para parsear o valores de retorno do client (OK, NOP ou a TAG em caso de cadastro), isso vai deixar mais bonito do q só chamar o while 5 vezes...
-> Faça uma função para verificar se o usuario/tag ja esta cadastrada pra evitar comunicação atoa com o arduino nos casos de deletar e cadastrar...
-> Use os atributos _MAXIMO_TAGS, _ARDUINO_SUCESSO e _ARDUINO_ERRO pra tornar o código mais dinâmico e fácil de manutenção
-> Troca esse .csv por um .db e usa o pySQL, vai ficar mais bonito aos olhos...

Horas gastas nisso: 40 
Boa sorte!
'''

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
    _MAXIMO_TAGS = 45

    def __init__(self):
        self.apresentacao_inicial()

        print("Inicializando a aplicação...")
        try:
            self.tags = pd.read_csv("database.csv")
            self.server = socket.socket()
            self.server.bind(("172.16.15.74", 80))
            print("Inicializando conexão via socket.")
            self.server.listen(5)
            self.conexao, endereco = self.server.accept()

            print("Conectado com o client remoto.")
        except:
            pass
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
            self.EncerraConsole()
            exit()
        
    def cadastrarUsuario(self):
        # Pede um nome pro usuario e avisa para a pessoa ir encostar o cartão
        # mostrar a tag e o nome inserido e o código de erro
        # mostra as tags salvas no database e a quantidade máxima de tags possiveis

        usuario = input("Insira a identificação (nome) do usuário que será cadastrado: ")

        self.conexao.send(b"/cadastrar=\n")

        recebidos = []
        dado_in = ""
        while dado_in not in ["OK", "NOP"] or len(dado_in) >= 3: # Se o dado que chegar for OK, NOP ou uma tag (que possui tamanho mínimo 3)
            dado_in = self.conexao.recv(1024).decode("utf-8").split("\r\n")[0]
            if dado_in == "HTTP 200 OK":
                print("Aproxime um cartão RFID ao painel do leitor...")
            recebidos.append(dado_in)

        if "OK" in recebidos:
            print("Tag %s associada ao usuário %s com sucesso." %recebidos[1] %usuario)
            self.tags.append({"NOME":usuario, "TAG":recebidos[1]})
            print("Número de tags cadastradas: %d/45" %len(self.tags.index))
        else:
            print("Erro ao cadastrar tag, verifique o limite de tags já cadastradas e se a tag já está cadastrada.")

    def deletarUsuario(self):
        # Mostra as tags salvas no databse e pede o nome de um para deletar
        # Pede uma senha para prosseguir com a operação e avisa que será deletado permanentemente
        # Mostra o código de erro

        self.conexao.send(b"/deletar=\n")
        dado_in = ""
        while dado_in not in ["OK", "NOP"] or len(dado_in) >= 3: # Se o dado que chegar for OK, NOP ou uma tag (que possui tamanho mínimo 3)
            dado_in = self.conexao.recv(1024).decode("utf-8").split("\r\n")[0]

        if dado_in == "OK":
            print(self.tags.to_string())
            usuario = input("Insira a identificação (nome) do usuário que será deletado: ")
            for index, linha in self.tags.iterrows():
                if(linha["NOME"] != usuario):
                    self.conexao.send(b"/addtag=%s\n"%linha["TAG"])
                    # recebe codigo de retorno e parseia ele
                else:
                    self.tags.drop([linha["NOME"]], inplace=True)
                    deletado = True
            if deletado:
                print("Usuario %s deletado com sucesso." %usuario)
                print("Tags cadastradas: %d/45" %len(self.tags.index))
            else:
                print("O usuário %s nao foi encontrado." %usuario)
        else:
            print("Ocorreu um erro durante o processo de deletar um usuário")
    
    def AtualizarServidor(self):
        # atualiza o database 
        #Mostra as tags salvas no databse atualizadas
        # mostra o código de erro
        self.conexao.send(b"/atualizarserver=\n")
        dado_in = ""
        while dado_in not in ["OK", "NOP"]:
            dado_in = self.conexao.recv(1024).decode("utf-8").split("\r\n")[0]
            if dado_in != "HTTP 200 OK":
                self.adicionaTagNoDatabase(dado_in)
        if dado_in == "OK":
            print("Tags dumpadas com sucesso. Número de tags cadastradas: %d/45" %len(self.tags.index))
            print(self.tags.to_string())
        else:
            print("Erro dumpando tags.")
    
    def EntraEstadoErro(self):
        # apresenta código de erro
        # sai do programa
        self.conexao.send(b"/desligarsistema=\n")
        while dado_in not in ["OK", "NOP"]:
            dado_in = self.conexao.recv(1024).decode("utf-8").split("\r\n")[0]
        if dado_in == "OK":
            print("Sistema desabilitado até o próximo reset manual. Saindo da aplicação.")
            self.EncerraConsole()

    def EncerraConsole(self):
        self.conexao.close()
        self.server.shutdown(socket.SHUT_RDWR)
        self.server.close()

objeto = interface()