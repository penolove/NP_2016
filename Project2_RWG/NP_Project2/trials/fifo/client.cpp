#include <stdio.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

int main()
{
   int client_to_server;
   char *myfifo = "/tmp/client_to_server_fifo";

   int server_to_client;
   char *myfifo2 = "/tmp/server_to_client_fifo";

   char str[BUFSIZ];
   printf("Input message to serwer: ");
   scanf("%s", str);


   /* write str to the FIFO */
   client_to_server = open(myfifo, O_WRONLY);
   server_to_client = open(myfifo2, O_RDONLY);
   write(client_to_server, str, sizeof(str));

   perror("Write:"); //Very crude error check

   read(server_to_client,str,sizeof(str));

   perror("Read:"); // Very crude error check

   printf("...received from the server: %s\n",str);
   close(client_to_server);
   close(server_to_client);

   /* remove the FIFO */

   return 0;
}
