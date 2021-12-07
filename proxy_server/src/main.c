#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/shm.h> /* shared memory functions and structs. */
#include <sys/sem.h> /* semaphore functions and structs.     */
#include <sys/ipc.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>
#include <json-c/json.h>
#include <curl/curl.h>

#define SEM_ID 250 /* ID del semaforo */

typedef union smun
{
    int val;
    struct semid_ds *buff;
    unsigned short *array;
} smun;

volatile int bucle_activado = 1;

void handle_sigkill(int signal)
{
    bucle_activado = 0;
    printf("Ctrl C por consola\n");
}

void handle_sigchild(int s)
{
    int pidstatus; /*Variable que revisa un hijo finalizo ejecución*/
    int childpid;
    do
    {
        childpid = waitpid(0, &pidstatus, WNOHANG);
        if (childpid > 0)
        {
            bucle_activado = 0;
            printf("Nuestro hijo %d ha finalizado ejecución\n", childpid);
        }
    } while (childpid > 0);
    return;
}

char *get_url(char *ruta);
void post_http(char *medicion,time_t timestamp, char *url);
int post_control(void);
void productor_temperatura(int sem_set_id, char *shm_addr, struct sembuf sb, union smun arg);
int transmision_http(char *medicion,time_t timestamp, char *url);
int intenta_nuevamente_http(char* url);
int transmision_paquete_http(char* buff_tx,char* url);
void post_paquete_http(char* buff_tx, char* url);

int main()
{
    int bucle_activado = 1;   /*Variable que gobierna el bucle principal*/
    int PID;                  /*PID que identifica nuestro hijo*/
    int semaforo_actual = 0;  /*Variable auxiliar para intercambiar de semaforo*/
    char buffer_recepcion[6]; /*Buffer que lee los datos desde la SHMEM*/
    int  paquetes_erroneos = 0;
    char buff_aux[128];

    typedef enum
    {
        Primera_Falla,
        Correcto,
        Erroneo,
        Intento_Corregir
    } tState;
    tState MEF_STATE = Correcto;

    typedef enum
    {
        Acierto,
        Falla
    } txState;
    txState TX_STATE=Acierto;

    time_t timestamp;

    FILE* file_error_log;

    int sem_set_id;   /*ID del semaforo*/
    struct sembuf sb; /*Estructura para crear un array de semaforos*/
    union smun arg;   /*Para comandar la toma de semaforo por parte del productor*/

    int shm_id;               /*ID de la Shared Memory*/
    char *shm_addr;           /*Puntero a la Shared Memory*/
    struct shmid_ds shm_desc; /*Struct para control de la Shared Memory*/

    char *url; /*URL del cloud server*/

    struct sigaction muerte;
    muerte.sa_handler = handle_sigkill;
    muerte.sa_flags = SA_RESTART;
    sigemptyset(&muerte.sa_mask);
    sigaction(SIGINT, &muerte, NULL);

    struct sigaction muere_hijo;
    muere_hijo.sa_handler = handle_sigchild;
    muere_hijo.sa_flags = 0;
    sigemptyset(&muere_hijo.sa_mask);
    sigaction(SIGCHLD, &muere_hijo, NULL);


    url = get_url("cloud_cfg.json");

    if (url == NULL)
    {
        printf("Error al leer URL\n");
        exit(1);
    }

    printf("URL del servidor: %s\n", url);

    // Seccion SHME
    /*Pido una SHMEM de 8 bytes*/
    shm_id = shmget(IPC_PRIVATE, 8, IPC_CREAT | IPC_EXCL | 0600);

    if (shm_id == -1)
    {
        printf("Error al crear un segmento de memoria compartida\n");
        exit(1);
    }

    /*Mapeo la dirección de la SHMEM a mi espacio de direccionamiento*/
    shm_addr = shmat(shm_id, NULL, 0);
    if (!shm_addr)
    {
        printf("Error al direccionar el segmento de memoria compartida\n");
        shmctl(shm_id, IPC_RMID, &shm_desc);
        exit(1);
    }

    /*Vacio la primer posición*/
    memset(shm_addr, 0, 8);

    // Seccion SEM
    // Creo el semaforo
    sem_set_id = semget(SEM_ID, 2, IPC_CREAT | 0600);
    if (sem_set_id == -1)
    {
        printf("Error al solicitar el semaforo\n");
        exit(1);
    }

    sb.sem_op = 1;
    sb.sem_flg = 0;
    arg.val = 1;

    for (sb.sem_num = 0; sb.sem_num < 2; sb.sem_num++)
    {
        if (semop(sem_set_id, &sb, 1) != 0)
        {
            printf("Error al liberar los semaforos\n");
        }
    }

    printf("Soy el Proceso Proxy mi PID es %d\n", getpid());

    PID = fork();

    if (PID == 0)
    {
        productor_temperatura(sem_set_id, shm_addr, sb, arg);
        exit(0);
    }
    if (PID == -1)
    {
        printf("Error al obtener un hijo\n");
    }

    sleep(1);

    while (bucle_activado)
    {
        sb.sem_op = -1;
        sb.sem_num = semaforo_actual;
        if (semop(sem_set_id, &sb, 1) == -1)
        {
            bucle_activado = 0;
            printf("Error al tomar el semaforo en proceso proxy\n");
        }

        memcpy(buffer_recepcion, shm_addr, sizeof(buffer_recepcion));
        buffer_recepcion[6] = '\0';
        printf("[PROXY]: Lectura de temperatura %s\n", buffer_recepcion);
        timestamp = time(NULL);

        if (transmision_http(buffer_recepcion,timestamp, url) != 0)
        {
            TX_STATE=Falla;
            paquetes_erroneos++;
            if(MEF_STATE==Correcto)
            {
                MEF_STATE=Primera_Falla;
            }
        }
        else
        {
            TX_STATE=Acierto;
        }

        // Maquina de Estados
        switch (MEF_STATE)
        {
            case Primera_Falla:
            {
                
                //Creo el archivo de errores
                file_error_log=fopen("error_send.json","w+");
                if(file_error_log == NULL)
                {
                    bucle_activado=0;
                }
                fflush(file_error_log);
                fprintf(file_error_log,"{\n"
                                        "\t\"medicionN%d\":\n"
                                        "\t\t{\"valor\":%s,\"timestamp\":%ld}", 
                        paquetes_erroneos,buffer_recepcion,timestamp);
                fclose(file_error_log);
                MEF_STATE=Erroneo;
                break;
            }
            case Correcto:
            {

                if(TX_STATE == Acierto)
                {
                    MEF_STATE=Correcto;
                }
                else
                {
                    MEF_STATE=Primera_Falla;
                }
                break;
            }
            case Intento_Corregir:
            {
                file_error_log=fopen("error_send.json","rw+");
                fseek(file_error_log,0,SEEK_END);
                fprintf(file_error_log,"\n}");
                fclose(file_error_log);

                if(intenta_nuevamente_http(url)==0)
                {
                    MEF_STATE=Correcto;
                    paquetes_erroneos=0;
                }
                else
                {
                    //retrocedo el cambio
                    file_error_log=fopen("error_send.json","rw+");
                    fseek(file_error_log,-2,SEEK_END);
                    fprintf(file_error_log,"  ");
                    fclose(file_error_log);
                    MEF_STATE=Erroneo;
                }    
                break;
            }
            case Erroneo:
            {
                int cantidad;
                if(TX_STATE==Falla)
                {
                    file_error_log=fopen("error_send.json","rw+");
                    fseek(file_error_log,0,SEEK_END);
                    fprintf(file_error_log,",\n"
                                           "\t\"medicionN%d\":\n"
                                           "\t\t{\"valor\":%s,\"timestamp\":%ld}", 
                        paquetes_erroneos,buffer_recepcion,timestamp);
                    fclose(file_error_log);
                }
                else
                {
                    MEF_STATE=Intento_Corregir;
                }
                break;
            }

            default:
            {
                MEF_STATE = Erroneo;
                break;
            }
        }

        if (paquetes_erroneos == 20)
        {
            printf("Demasiados errores de comunicacion, reintentar mas tarde!\n");
            bucle_activado = 0;
        }

        sb.sem_op = 1;

        if (semop(sem_set_id, &sb, 1) == -1)
        {
            bucle_activado = 0;
            printf("Error al liberar el semaforo en proceso proxy\n");
        }

        // Cambio el semaforo
        if (semaforo_actual == 0)
            semaforo_actual = 1;
        else
            semaforo_actual = 0;
    }
    file_error_log=fopen("error_send.json","rw+");
    fseek(file_error_log,0,SEEK_END);
    fprintf(file_error_log,"\n}");
    fclose(file_error_log);




    /* Remuevo la SHMEM de nuestro espacio de direccionamiento*/
    if (shmdt(shm_addr) == -1)
    {
        printf("Error al desalojar la shmem\n");
    }

    /* Remuevo el recurso de SHMEM dentro del S.O.*/
    if (shmctl(shm_id, IPC_RMID, &shm_desc) == -1)
    {
        printf("Error al desalojar el segmento de shmem\n");
    }

    /* Remuevo el recurso de SEM dentro del S.O. */
    if (semctl(sem_set_id, 0, IPC_RMID) == -1)
    {
        printf("Error al desalojar el semaforo\n");
    }
    printf("Liberando recursos\n");
    kill(PID, SIGUSR2);
    return 0;
}

char *get_url(char *ruta)
{
    FILE *fp_cfg;
    fp_cfg = fopen(ruta, "r");
    char ch_cfg;
    char *buffer_cfg;
    char *buffer_return;
    int c_cfg;
    struct json_object *principal_cfg = NULL; /*Variables auxiliares para recorrer el .json*/
    struct json_object *url_json = NULL;

    if (fp_cfg == NULL)
    {
        printf("Error al abrir el archivo cloud_cfg.json\n");
        return NULL;
    }

    do
    {
        ch_cfg = fgetc(fp_cfg);
        c_cfg++;
    } while (!feof(fp_cfg));

    buffer_cfg = malloc(c_cfg);

    if (buffer_cfg == NULL)
    {
        printf("Error al asignar memoria dinamica\n");
        return NULL;
    }

    rewind(fp_cfg);

    if (fread(buffer_cfg, c_cfg, 1, fp_cfg) < 0)
    {
        printf("Error al leer el archivo\n");
        return NULL;
    }

    fclose(fp_cfg);

    principal_cfg = json_tokener_parse(buffer_cfg);

    if (principal_cfg == NULL)
    {
        printf("Error al parsear el .json\n");
        return NULL;
    }

    json_object_object_get_ex(principal_cfg, "url", &url_json);

    if (url_json == NULL)
    {
        printf("Error al parsear el .json\n");
        return NULL;
    }

    buffer_return = malloc(json_object_get_string_len(url_json));

    if (buffer_return == NULL)
    {
        printf("Error al parsear el .json\n");
        return NULL;
    }

    sprintf(buffer_return, "%s", json_object_get_string(url_json));

    return buffer_return;
}

int post_control(void)
{
    struct json_object *principal = NULL;
    struct json_object *status = NULL;
    FILE *colector_salida;
    char *buffer_control;
    char ch;
    int c;

    colector_salida = fopen("output_post", "r");

    if (colector_salida == NULL)
    {
        printf("Error al abrir el archivo\n");
        return -1;
    }

    do
    {
        ch = fgetc(colector_salida);
        c++;
    } while (!feof(colector_salida));

    buffer_control = malloc(c);

    if (buffer_control == NULL)
    {
        printf("Error al asignar memoria dinamica\n");
        return -1;
    }

    rewind(colector_salida);

    if (fread(buffer_control, c, 1, colector_salida) < 0)
    {
        printf("Error al leer el archivo\n");
        return -1;
    }

    fclose(colector_salida);

    principal = json_tokener_parse(buffer_control);

    if (principal == NULL)
    {
        printf("Error al parsear el request\n");
        return -1;
    }

    json_object_object_get_ex(principal, "status", &status);

    if (status == NULL)
    {
        printf("Error al parsear el request\n");
        return -1;
    }

    if (strcmp(json_object_get_string(status), "Awesome!") != 0)
    {
        printf("Error de respuesta\n");
        return -1;
    }

    return 0;
}

void post_http(char *medicion,time_t timestamp , char *url)
{
    char buffer_aux[128];
    CURL *curl;
    CURLcode res;
    int err, file;
    int PID;
    PID = fork();

    if (PID == 0)
    {
        execlp("rm", "rm", "-f", "output_post", NULL);
        exit(1);
    }
    wait(NULL);

    file = open("output_post", O_WRONLY | O_CREAT, 0777);
    if (file == -1)
    {
        printf("Error al abrir el archivo\n");
        exit(1);
    }
    dup2(file, STDOUT_FILENO);

    memset(buffer_aux,'\0',128);
    sprintf(buffer_aux,"{ \"medicion\": { \"valor\": %s, \"timestamp\": %ld} }",medicion,timestamp);

    /* In windows, this will init the winsock stuff */
    curl_global_init(CURL_GLOBAL_ALL);

    /* get a curl handle */
    curl = curl_easy_init();
    if (curl)
    {
        /* First set the URL that is about to receive our POST. This URL can
           just as well be a https:// URL if that is what should receive the
           data. */
        curl_easy_setopt(curl, CURLOPT_URL, url);
        /* Now specify the POST data */
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, buffer_aux);

        /* Perform the request, res will get the return code */
        res = curl_easy_perform(curl);
        /* Check for errors */
        if (res != CURLE_OK)
            printf("FALLA: curl_easy_perform() : %s\n",
                   curl_easy_strerror(res));

        /* always cleanup */
        curl_easy_cleanup(curl);
    }
    curl_global_cleanup();
}

volatile int bucle_temperatura = 1;

void handler_productor_sigusr2(int signal)
{
    bucle_temperatura = 0;
}

void productor_temperatura(int sem_set_id, char *shm_addr, struct sembuf sb, union smun arg)
{
    int pid;                              /*PID que identifica nuestro hijo lector del proceso sensors*/
    FILE *fp;                             /*File Pointer para recorrer el archivo*/
    char *buffer;                         /*Buffer que almacena temporalmente el file*/
    int c = 0;                            /*Variable auxiliar para contar los caracteres*/
    char ch = 0;                          /*variable auxiliar para almacenar temporalmente caracteres*/
    struct json_object *principal = NULL; /*Variables auxiliares para recorrer el .json*/
    struct json_object *core_temp = NULL;
    struct json_object *pack_id = NULL;
    struct json_object *temp_1 = NULL;
    int nbytes = 0;             /*Return del fread*/
    char temperatura_vector[6]; /*Vector que almacena la temperatura leida del .json*/

    // handler para recargar el handler
    struct sigaction muerte;
    muerte.sa_handler = handler_productor_sigusr2;
    muerte.sa_flags = 0;
    sigemptyset(&muerte.sa_mask);
    sigaction(SIGUSR2, &muerte, NULL);

    signal(SIGINT, SIG_DFL);
    signal(SIGCHLD, SIG_IGN);

    printf("Soy el Proceso Colector de temperatura mi PID es %d\n", getpid());

    sb.sem_num = 0;
    arg.val = 0;
    semctl(sem_set_id, sb.sem_num, SETVAL, arg);

    while (bucle_temperatura)
    {
        pid = fork();
        if (pid == 0)
        {
            int err, file;
            file = open("temperatura.json", O_WRONLY | O_CREAT, 0777);
            if (file == -1)
            {
                printf("Error al abrir el archivo\n");
                kill(getppid(), SIGUSR2);
                exit(1);
            }
            dup2(file, STDOUT_FILENO);
            err = execlp("sensors", "sensors", "-j", NULL);
            if (err == -1)
            {
                printf("No se encontro el paquete sensors dentro del dispositivo\n");
                kill(getppid(), SIGUSR2);
                exit(1);
            }
            close(file);
            exit(0);
        }
        if (pid == -1)
        {
            bucle_temperatura = 0;
            printf("Error al obtener un hijo\n");
        }

        wait(NULL);

        fp = fopen("temperatura.json", "r");

        if (fp == NULL)
        {
            bucle_temperatura = 0;
            printf("Error al abrir el archivo temperatura.json\n");
        }

        do
        {
            ch = fgetc(fp);
            c++;
        } while (!feof(fp));

        buffer = malloc(c);

        if (buffer == NULL)
        {
            bucle_temperatura = 0;
            printf("Error al asignar memoria dinamica\n");
        }

        rewind(fp);

        nbytes = fread(buffer, c, 1, fp);
        if (nbytes < 0)
        {
            bucle_temperatura = 0;
            printf("Error al leer el archivo\n");
        }

        fclose(fp);

        principal = json_tokener_parse(buffer);

        if (principal == NULL)
        {
            bucle_temperatura = 0;
            printf("Error al parsear el .json\n");
        }

        json_object_object_get_ex(principal, "coretemp-isa-0000", &core_temp);

        if (core_temp == NULL)
        {
            bucle_temperatura = 0;
            printf("Error al parsear dentro de los nodos principales del .json\n");
        }

        json_object_object_get_ex(core_temp, "Package id 0", &pack_id);

        if (pack_id == NULL)
        {
            bucle_temperatura = 0;
            printf("Error al parsear dentro de \"coretemp-isa-0000\"\n");
        }

        json_object_object_get_ex(pack_id, "temp1_input", &temp_1);

        if (temp_1 == NULL)
        {
            bucle_temperatura = 0;
            printf("Error al parsear dentro de \"Package id 0\"\n");
        }

        sprintf(temperatura_vector, "%s", json_object_get_string(temp_1));

        // cambio de semaforo
        memcpy(shm_addr, temperatura_vector, sizeof(temperatura_vector));
        sleep(1);
        // Libero el semaforo
        arg.val = 1;
        if (semctl(sem_set_id, sb.sem_num, SETVAL, arg) != 0)
        {
            bucle_temperatura = 0;
            printf("Error al liberar el semaforo\n");
        }

        // Toggleo el numero de semaforo
        if (sb.sem_num == 0)
            sb.sem_num = 1;
        else
            sb.sem_num = 0;

        // Tomo el otro semaforo
        arg.val = 0;
        if (semctl(sem_set_id, sb.sem_num, SETVAL, arg) != 0)
        {
            bucle_temperatura = 0;
            printf("Error al tomar el semaforo\n");
        }
    }
    free(buffer);
}

int transmision_http(char *medicion,time_t timestamp, char *url)
{
    int PID_2;

    PID_2 = fork();

    if (PID_2 == 0)
    {
        signal(SIGINT, SIG_IGN);
        post_http(medicion,timestamp , url);
        exit(1);
    }
    wait(NULL);

    if (post_control() != 0)
    {
        printf("Error en la comunicacion\n");
        return -1;
    }
    return 0;
}

int intenta_nuevamente_http(char* url)
{
    char* buff_tx;
    char ch_aux;
    int cantidad_caracteres;
    FILE* archivo_error;

    archivo_error=fopen("error_send.json","r");

    if(archivo_error == NULL)
    {
        printf("Error al abrir el archivo");
        return -1;
    }

    do
    {
        ch_aux=fgetc(archivo_error);
        cantidad_caracteres++;
    } while (!feof(archivo_error));
    rewind(archivo_error);

    buff_tx=malloc(cantidad_caracteres*sizeof(char));
    if(buff_tx == NULL)
    {
        printf("Error al asignar memoria dinamica\n");
        return -1;
    }
    
    if(fread(buff_tx,cantidad_caracteres,sizeof(char),archivo_error)<0)
    {   
        printf("Error al leer el archivo\n");
        return -1;
    }
    if(transmision_paquete_http(buff_tx,url)!= 0)
    {
        return -1;
    }

    return 0;
}

int transmision_paquete_http(char* buff_tx,char* url)
{
    int PID_2;
    PID_2 = fork();

    if (PID_2 == 0)
    {
        signal(SIGINT, SIG_IGN);
        post_paquete_http(buff_tx, url);
        exit(1);
    }
    wait(NULL);

    if (post_control() != 0)
    {
        printf("Error en la comunicacion\n");
        return -1;
    }
    return 0;
}

void post_paquete_http(char* buff_tx, char* url)
{
    CURL *curl;
    CURLcode res;
    int err, file;
    int PID;
    PID = fork();

    if (PID == 0)
    {
        execlp("rm", "rm", "-f", "output_post", NULL);
        exit(1);
    }
    wait(NULL);

    file = open("output_post", O_WRONLY | O_CREAT, 0777);
    if (file == -1)
    {
        printf("Error al abrir el archivo\n");
        exit(1);
    }
    dup2(file, STDOUT_FILENO);

    /* In windows, this will init the winsock stuff */
    curl_global_init(CURL_GLOBAL_ALL);

    /* get a curl handle */
    curl = curl_easy_init();
    if (curl)
    {
        /* First set the URL that is about to receive our POST. This URL can
           just as well be a https:// URL if that is what should receive the
           data. */
        curl_easy_setopt(curl, CURLOPT_URL, url);
        /* Now specify the POST data */
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, buff_tx);

        /* Perform the request, res will get the return code */
        res = curl_easy_perform(curl);
        /* Check for errors */
        if (res != CURLE_OK)
            printf("FALLA: curl_easy_perform() : %s\n",
                   curl_easy_strerror(res));

        /* always cleanup */
        curl_easy_cleanup(curl);
    }
    curl_global_cleanup();

}