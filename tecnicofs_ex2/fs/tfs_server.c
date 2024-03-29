#include "tfs_server.h"
#include "operations.h"
#include "common/common.h"
#include <signal.h>

#define MAX_REQUEST_LEN 40

session_t session[S];
pthread_cond_t pode_enviar[S],pode_processar[S];
pthread_mutex_t session_lock[S];
pthread_t tarefas_trabalhadoras[S];

void *tarefa_trabalhadora(void *args){
    int session_id = ((int*) args)[0];
    int fclient= -1;
    while(1) {
        pthread_mutex_lock(&session_lock[session_id]);
        while(session[session_id].count == 0)
            pthread_cond_wait(&pode_processar[session_id], &session_lock[session_id]);
        
        void *return_message,*buffer;
        char *name,*client_pipe_path;
        int fhandle;
        size_t len;
        switch (session[session_id].op_code){
            case 1:
                client_pipe_path = malloc(sizeof(char)*MAX_PIPE_LEN);
                return_message = malloc(sizeof(int));
                memcpy(client_pipe_path , session[session_id].new_command, (size_t) MAX_PIPE_LEN);
                fclient = open_failure_retry(client_pipe_path,O_WRONLY);
                if(fclient < 0)
                    ((int*)return_message)[0] = -1,session[session_id].valid = 0;
                else
                    ((int*)return_message)[0] = 0,session[session_id].valid = 1;
                free(client_pipe_path);
                break;
            case 2:
                return_message = malloc(sizeof(int));
                ((int*)return_message)[0] = 0;
                session[session_id].valid = 0;
                close(fclient);
                break;
            case 3:
                name = malloc(sizeof(char)*40);
                return_message = malloc(sizeof(int));
                int flags;
                memcpy(name , session[session_id].new_command , (size_t) 40);
                flags = ((int*) (name+40))[0];

                ((int*)return_message)[0] = tfs_open(name , flags);
                free(name);
                break;
            case 4:
                fhandle = ((int*)session[session_id].new_command)[0];
                return_message = malloc(sizeof(int));
                ((int*)return_message)[0] = tfs_close(fhandle);
                break;
            case 5:
                fhandle = ((int*)session[session_id].new_command)[0];
                len = ((size_t*)session[session_id].new_command)[1];
                buffer = malloc(sizeof(char)*len);
                memcpy(buffer , ((char*) (((int*)session[session_id].new_command)+ 2)), len*sizeof(char));
                return_message = malloc(sizeof(int));
                ((int*)return_message)[0] = (int) tfs_write(fhandle, buffer, len);
                free(buffer);
                break;
            case 6:
                fhandle = ((int*)session[session_id].new_command)[0];
                len = ((size_t*)session[session_id].new_command)[1];
                buffer = malloc(sizeof(char)*len);
                int bytes_read = (int) tfs_read(fhandle , buffer , len);
                if(bytes_read == -1)
                    return_message = malloc(sizeof(int)),((int*)return_message)[0] = -1;
                else{
                    return_message = malloc(sizeof(int)+ sizeof(char)*((size_t)bytes_read));
                    ((int*)return_message)[0] = (int) bytes_read;
                    memcpy((char*) (((int*)return_message) + 1) , buffer , (size_t) bytes_read);
                }
                free(buffer);
                break;
            case 7:
                return_message = malloc(sizeof(int));   
                break;
            
            default:
                return_message = malloc(sizeof(int));   
                break;
        }
        send_to_pipe(fclient , return_message , sizeof(int));
        free(return_message);
        free(session[session_id].new_command);
        /* Processar o pedido session[session_id].new_command*/
        /*responder diretamente ao client*/

        session[session_id].count--;
        pthread_cond_signal(&pode_enviar[session_id]);
        pthread_mutex_unlock(&session_lock[session_id]);
    }
}

void init(){
    for(int i = 0 ; i < S ; i++){
        pthread_cond_init(&pode_enviar[i], NULL);
        pthread_cond_init(&pode_processar[i], NULL);
        pthread_mutex_init(&session_lock[i], NULL);
        pthread_create(&tarefas_trabalhadoras[i], NULL , tarefa_trabalhadora, (void*) &i);
    }
    for(int i = 0 ; i < S ;i++)
        session[i].valid = 0,session[i].count = 0;
}


int find_session_id() {

    for(int id = 0; id < S; id++) {
        if(session[id].valid == 0) {
            return id;
        }
    }
    return -1;
}

int main(int argc, char **argv) {


    if (argc < 2) {
        printf("Please specify the pathname of the server's pipe.\n");
        return 1;
    }
    init();

    signal(SIGPIPE,SIG_IGN);
    
    char *pipename = argv[1];
    void *buffer = (void*) malloc(sizeof(char)*(MAX_COMMAND_LENGTH + 1) + sizeof(int));
    int fserv;
    printf("Starting TecnicoFS server with pipe called %s\n", pipename);
    

    unlink(pipename);
    if (mkfifo (pipename, 0666) < 0)
        exit (1);

    if ((fserv = open_failure_retry(pipename, O_RDONLY)) < 0) 
        exit(1);
        
    while(1){


        int session_id = -1;
        if(receive_from_pipe(fserv, buffer ,sizeof(char)*(MAX_COMMAND_LENGTH+1) + sizeof(int)) < 0) { // ???
            perror("tfs_server: failed to read \n");
            return -1;
        }
        printf("NEW COMMAND %c\n", ((char*)buffer)[0]);
        if(((char*)buffer)[0] == '1') {

            printf("STILL OK -1\n");
            /*calculate new session_id*/
            session_id = find_session_id(); //erro se der -1
            pthread_mutex_lock(&session_lock[session_id]);

            session[session_id].op_code = 1;
            session[session_id].new_command = malloc(sizeof(char)*(MAX_PIPE_LEN - 5));
            memcpy(session[session_id].new_command , (char*) buffer+1, (MAX_PIPE_LEN-5));

            session[session_id].count++;
            pthread_cond_signal(&pode_processar[session_id]);
            pthread_mutex_unlock(&session_lock[session_id]);
        }
        else{

            printf("STILL OK -1\n");
            void *last_pos = buffer;
            printf("STILL OK 0");
            int op = ((char*)last_pos)[0]- '0';

            printf("STILL OK 1");
            last_pos = (void*)(((char*)last_pos) + 1);
            printf("STILL OK 2");
            session_id = ((int*)last_pos)[0]; 
            printf("STILL OK 3");
            
            pthread_mutex_lock(&session_lock[session_id]);
            while(session[session_id].count != 0)
                pthread_cond_wait(&pode_enviar[session_id], &session_lock[session_id]); 

            printf("STILL OK 4");
            last_pos = (void*)(((int*)last_pos) + 1);
            session[session_id].op_code = op;

            session[session_id].new_command = malloc(sizeof(char)*(MAX_COMMAND_LENGTH-5));
            memcpy(session[session_id].new_command,last_pos,MAX_COMMAND_LENGTH-5);

            session[session_id].count++;
            pthread_cond_signal(&pode_processar[session_id]);
            pthread_mutex_unlock(&session_lock[session_id]);
        }
    }
    free(buffer);
    
    return 0;
}
