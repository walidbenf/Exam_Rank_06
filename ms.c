#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <stdio.h>

typedef struct s_clients {
    int     id;
    char    msg[1024];
} t_clients;

t_clients   clients[1024];
fd_set      rfds, wfds, actv;
int         mfd = 0, nid = 0;
char        rbuffer[120000], wbuffer[120000];

void    fterr(char *s) {
    if (s)
        write(2, s, strlen(s));
    else
        write(2, "Fatal error", strlen("Fatal error"));
    write(2, "\n", 1);
    exit(1);
}

void    sendAll(int not) {
    for(int i = 0; i <= mfd; i++)
        if(FD_ISSET(i, &wfds) && i != not)
            send(i, wbuffer, strlen(wbuffer), 0);
}

int main(int ac, char **av) {
    if (ac != 2)
        fterr("Wrong number of arguments");

    int sfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sfd < 0)
        fterr(NULL);

    FD_ZERO(&actv);
    bzero(&clients, sizeof(clients));
    mfd = sfd;
    FD_SET(sfd, &actv);

    struct sockaddr_in  servaddr;
    socklen_t           len;
    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(2130706433); //127.0.0.1
    servaddr.sin_port = htons(atoi(av[1]));

    if ((bind(sfd, (const struct sockaddr *)&servaddr, sizeof(servaddr))) < 0)
        fterr(NULL);
    if (listen(sfd, 10) < 0)
        fterr(NULL);

    while(1) {
        rfds = wfds = actv;
        if (select(mfd + 1, &rfds, &wfds, NULL, NULL) < 0)
            continue;
        for(int fdi = 0; fdi <= mfd; fdi++) { 
            if (FD_ISSET(fdi, &rfds) && fdi == sfd) { // si un client se connecte
                int cfd = accept(sfd, (struct sockaddr *)&servaddr, &len);
                if (cfd < 0)
                    continue;
                mfd = cfd > mfd ? cfd : mfd;
                clients[cfd].id = nid++;
                FD_SET(cfd, &actv);
                sprintf(wbuffer, "server: client %d just arrived\n", clients[cfd].id);
                sendAll(cfd);
                break;
            }
            if (FD_ISSET(fdi, &rfds) && fdi != sfd) { // on envoie un message si le client se deconnecte
                int res = recv(fdi, rbuffer, 65536, 0);
                if (res <= 0) {
                    sprintf(wbuffer, "server: client %d just left\n", clients[fdi].id);
                    sendAll(fdi);
                    FD_CLR(fdi, &actv);
                    close(fdi);
                    break;
                }

                else { // on lit le message du client et on le stocke dans le buffer
                    for (int i = 0, j = strlen(clients[fdi].msg); i < res; i++, j++) {
                        clients[fdi].msg[j] = rbuffer[i];
                        if (clients[fdi].msg[j] == '\n') {
                            clients[fdi].msg[j] = '\0';
                            sprintf(wbuffer, "client %d: %s\n", clients[fdi].id, clients[fdi].msg);
                            sendAll(fdi);
                            bzero(&clients[fdi].msg, strlen(clients[fdi].msg));
                            j = -1;
                        }
                    }
					break;
                }
            }
        }
    }
}
