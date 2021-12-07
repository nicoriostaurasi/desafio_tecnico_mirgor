#include "../inc/main.h"

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

        buffer=obtener_string_de_file("temperatura.json");

        if(buffer==NULL)
        {
            bucle_temperatura=0;
        }

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
