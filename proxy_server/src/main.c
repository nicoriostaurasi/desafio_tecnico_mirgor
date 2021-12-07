#include "../inc/main.h"

int bucle_activado = 1;

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


int main()
{
    int bucle_activado = 1;   /*Variable que gobierna el bucle principal*/
    int PID;                  /*PID que identifica nuestro hijo*/
    int semaforo_actual = 0;  /*Variable auxiliar para intercambiar de semaforo*/
    char buffer_recepcion[6]; /*Buffer que lee los datos desde la SHMEM*/
    int paquetes_erroneos = 0;
    int aux;

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
    txState TX_STATE = Acierto;

    time_t timestamp;

    FILE *file_error_log;

    int sem_set_id;   /*ID del semaforo*/
    struct sembuf sb; /*Estructura para crear un array de semaforos*/
    union smun arg;   /*Para comandar la toma de semaforo por parte del productor*/

    int shm_id;               /*ID de la Shared Memory*/
    char *shm_addr;           /*Puntero a la Shared Memory*/
    struct shmid_ds shm_desc; /*Struct para control de la Shared Memory*/

    char *url; /*URL del cloud server*/

    struct sigaction muerte;
    muerte.sa_handler = handle_sigkill;
    muerte.sa_flags = 0;
    sigemptyset(&muerte.sa_mask);
    sigaction(SIGINT, &muerte, NULL);

    struct sigaction muere_hijo;
    muere_hijo.sa_handler = handle_sigchild;
    muere_hijo.sa_flags = 0;
    sigemptyset(&muere_hijo.sa_mask);
    sigaction(SIGCHLD, &muere_hijo, NULL);

    url = obtener_url("cloud_cfg.json");

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
        aux=bucle_activado;
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

        if (transmision_http(buffer_recepcion, timestamp, url) != 0)
        {
            TX_STATE = Falla;
            paquetes_erroneos++;
            if (MEF_STATE == Correcto)
            {
                MEF_STATE = Primera_Falla;
            }
        }
        else
        {
            TX_STATE = Acierto;
        }

        // Maquina de Estados
        switch (MEF_STATE)
        {
        case Primera_Falla:
        {
            // Creo el archivo de errores
            file_error_log = fopen("error_send.json", "w+");
            if (file_error_log == NULL)
            {
                bucle_activado = 0;
            }
            fflush(file_error_log);
            fprintf(file_error_log, "{\n"
                                    "\t\"medicionN%d\":\n"
                                    "\t\t{\"valor\":%s,\"timestamp\":%ld}",
                    paquetes_erroneos, buffer_recepcion, timestamp);
            fclose(file_error_log);
            MEF_STATE = Erroneo;
            break;
        }
        case Correcto:
        {
            if (TX_STATE == Acierto)
            {
                MEF_STATE = Correcto;
            }
            else
            {
                MEF_STATE = Primera_Falla;
            }
            break;
        }
        case Intento_Corregir:
        {
            file_error_log = fopen("error_send.json", "rw+");
            fseek(file_error_log, 0, SEEK_END);
            fprintf(file_error_log, "\n}");
            fclose(file_error_log);

            if (intenta_nuevamente_http(url) == 0)
            {
                MEF_STATE = Correcto;
                paquetes_erroneos = 0;
            }
            else
            {
                // retrocedo el cambio
                file_error_log = fopen("error_send.json", "rw+");
                fseek(file_error_log, -2, SEEK_END);
                fprintf(file_error_log, "  ");
                fclose(file_error_log);
                MEF_STATE = Erroneo;
            }
            break;
        }
        case Erroneo:
        {
            int cantidad;
            if (TX_STATE == Falla)
            {
                file_error_log = fopen("error_send.json", "rw+");
                fseek(file_error_log, 0, SEEK_END);
                fprintf(file_error_log, ",\n"
                                        "\t\"medicionN%d\":\n"
                                        "\t\t{\"valor\":%s,\"timestamp\":%ld}",
                        paquetes_erroneos, buffer_recepcion, timestamp);
                fclose(file_error_log);
            }
            else
            {
                MEF_STATE = Intento_Corregir;
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
    file_error_log = fopen("error_send.json", "rw+");
    fseek(file_error_log, 0, SEEK_END);
    fprintf(file_error_log, "\n}");
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
    free(url);
    return 0;
}