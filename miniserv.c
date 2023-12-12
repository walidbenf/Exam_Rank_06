// Inclusions des bibliothèques nécessaires
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <stdio.h>

// Définition de la structure pour les clients
typedef struct Client {
    int     id;
    char    message[1024];
} ClientInfo;

// Tableau de clients
ClientInfo   clients[1024];

// Ensembles de descripteurs de fichiers pour les actions sur les sockets
fd_set      readSet, writeSet, activeSet;

// Variables globales pour le suivi des sockets et des messages
int         maxSocket = 0, nextClientId = 0;
char        readBuffer[120000], writeBuffer[120000];

// Fonction d'affichage d'erreur
void showError(char *errorMsg) {
    if (errorMsg)
        write(2, errorMsg, strlen(errorMsg));
    else
        write(2, "Fatal error", strlen("Fatal error"));
    write(2, "\n", 1);
    exit(1);
}

// Fonction d'envoi à tous les clients sauf à celui spécifié
void sendToAllExcept(int notSocket) {
    for(int i = 0; i <= maxSocket; i++)
        if(FD_ISSET(i, &writeSet) && i != notSocket)
            send(i, writeBuffer, strlen(writeBuffer), 0);
}

// Fonction principale (main)
int main(int argc, char **argv) {
    // Vérification du nombre d'arguments
    if (argc != 2)
        showError("Wrong number of arguments");

    // Création du socket de lecture pour le serveur
    int serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket < 0)
        showError(NULL);

    // Initialisation des ensembles de descripteurs de fichiers et des variables
    FD_ZERO(&activeSet);
    bzero(&clients, sizeof(clients));
    maxSocket = serverSocket;
    FD_SET(serverSocket, &activeSet);

    // Configuration de l'adresse du serveur
    struct sockaddr_in  serverAddress;
    socklen_t           addrLen;
    bzero(&serverAddress, sizeof(serverAddress));
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_addr.s_addr = inet_addr("127.0.0.1");
    serverAddress.sin_port = htons(atoi(argv[1]));

    // Liaison du socket
    if (bind(serverSocket, (const struct sockaddr *)&serverAddress, sizeof(serverAddress)) < 0)
        showError(NULL);

    // Mise en écoute du socket
    if (listen(serverSocket, 10) < 0)
        showError(NULL);

    // Boucle principale
    while(1) {
        // Initialisation des ensembles de descripteurs de fichiers pour la sélection des sockets
        readSet = writeSet = activeSet;

        // Attente d'une action sur un des descripteurs de fichiers
        if (select(maxSocket + 1, &readSet, &writeSet, NULL, NULL) < 0)
            continue;

        // Gestion des sockets
        for(int socketId = 0; socketId <= maxSocket; socketId++) {
            // Nouvelle connexion client
            if (FD_ISSET(socketId, &readSet) && socketId == serverSocket) {
                int clientSocket = accept(serverSocket, (struct sockaddr *)&serverAddress, &addrLen);
                if (clientSocket < 0)
                    continue;
                maxSocket = clientSocket > maxSocket ? clientSocket : maxSocket;
                clients[clientSocket].id = nextClientId++;
                FD_SET(clientSocket, &activeSet);
                sprintf(writeBuffer, "server: client %d just arrived\n", clients[clientSocket].id);
                sendToAllExcept(clientSocket);
                break;
            }
            // Lecture du message client
            if (FD_ISSET(socketId, &readSet) && socketId != serverSocket) {
                int bytesRead = recv(socketId, readBuffer, 65536, 0);
                if (bytesRead <= 0) {
                    // Déconnexion d'un client
                    sprintf(writeBuffer, "server: client %d just left\n", clients[socketId].id);
                    sendToAllExcept(socketId);
                    FD_CLR(socketId, &activeSet);
                    close(socketId);
                    break;
                } else {
                    // Lecture et traitement du message
                    for (int i = 0, j = strlen(clients[socketId].message); i < bytesRead; i++, j++) {
                        clients[socketId].message[j] = readBuffer[i];
                        if (clients[socketId].message[j] == '\n') {
                            clients[socketId].message[j] = '\0';
                            sprintf(writeBuffer, "client %d: %s\n", clients[socketId].id, clients[socketId].message);
                            sendToAllExcept(socketId);
                            bzero(&clients[socketId].message, strlen(clients[socketId].message));
                            j = -1;
                        }
                    }
                    break;
                }
            }
        }
    }
    return (0);
}
