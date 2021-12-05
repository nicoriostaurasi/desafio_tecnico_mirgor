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

#define SEM_ID 250 /* ID del semaforo */

typedef union smun
{
    int val;
    struct semid_ds *buff;
    unsigned short *array;
} smun;

void productor_temperatura(int sem_set_id, char *shm_addr, struct sembuf sb, union smun arg);

volatile int bucle_activado = 1;

void handle_sigkill(int signal)
{
    bucle_activado = 0;
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
            printf("Nuestro hijo tuvo una falla\n");
        }
    } while (childpid > 0);
    return;
}

int main()
{
    int bucle_activado = 1;   /*Variable que gobierna el bucle principal*/
    int PID;                  /*PID que identifica nuestro hijo*/
    int semaforo_actual = 0;  /*Variable auxiliar para intercambiar de semaforo*/
    char buffer_recepcion[6]; /*Buffer que lee los datos desde la SHMEM*/

    int sem_set_id;   /*ID del semaforo*/
    struct sembuf sb; /*Estructura para crear un array de semaforos*/
    union smun arg;   /*Para comandar la toma de semaforo por parte del productor*/

    int shm_id;               /*ID de la Shared Memory*/
    char *shm_addr;           /*Puntero a la Shared Memory*/
    struct shmid_ds shm_desc; /*Struct para control de la Shared Memory*/

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
        printf("[PROXY]: Lectura de temperatura %s\n", buffer_recepcion);
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

        if (fp < 0)
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