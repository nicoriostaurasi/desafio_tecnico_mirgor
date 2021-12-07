/**
 * @file main.c
 * @author Nicolas Rios Taurasi (nicoriostaurasi@frba.utn.edu.ar)
 * @brief Archivo principal, contiene el proceso proxy
 * @version 0.1
 * @date 07-12-2021
 * 
 * @copyright Copyright (c) 2021
 * 
 */

#include "../inc/main.h"


//Variable de control para el bucle
int bucle_activado = 1;

//Handler de Control+C
void handle_sigkill(int signal)
{
    bucle_activado = 0;
    printf("Ctrl C por consola\n");
}

//Handler de Sigchild
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
    int bucle_activado = 1;     /*Variable que gobierna el bucle principal*/
    int PID;                    /*PID que identifica nuestro hijo*/
    int semaforo_actual = 0;    /*Variable auxiliar para intercambiar de semaforo*/
    char buffer_recepcion[6];   /*Buffer que lee los datos desde la SHMEM*/
    int paquetes_erroneos = 0;  /*Cantidad de paquetes erroneos*/
    int aux;                    /*Variable auxiliar*/

    typedef enum                /*Variable de la Maquina de Estados*/
    {
        Primera_Falla,
        Correcto,
        Erroneo,
        Intento_Corregir
    } tState;
    tState MEF_STATE = Correcto;

    typedef enum                /*Variable de control para el envio*/
    {
        Acierto,
        Falla
    } txState;
    txState TX_STATE = Acierto;

    time_t timestamp;           /*Variable para controlar el tiempo*/

    FILE *file_error_log;       /*File pointer para completar el archivo de paquetes no enviados*/

    int sem_set_id;             /*ID del semaforo*/
    struct sembuf sb;           /*Estructura para crear un array de semaforos*/
    union smun arg;             /*Para comandar la toma de semaforo por parte del productor*/

    int shm_id;                 /*ID de la Shared Memory*/
    char *shm_addr;             /*Puntero a la Shared Memory*/
    struct shmid_ds shm_desc;   /*Struct para control de la Shared Memory*/

    char *url;                  /*URL del cloud server*/

    struct sigaction muerte;                    /*Reasigno el handler de SIGINT*/
    muerte.sa_handler = handle_sigkill;
    muerte.sa_flags = 0;
    sigemptyset(&muerte.sa_mask);
    sigaction(SIGINT, &muerte, NULL);

    struct sigaction muere_hijo;                /*Reasigno el handler de SIGCHLD*/
    muere_hijo.sa_handler = handle_sigchild;
    muere_hijo.sa_flags = 0;
    sigemptyset(&muere_hijo.sa_mask);
    sigaction(SIGCHLD, &muere_hijo, NULL);

    url = obtener_url(Ruta_Archivo_CFG);        /*Obtengo la URL desde el archivo de configuracion*/

    if (url == NULL)
    {
        printf("Error al leer URL\n");
        exit(1);
    }

    printf("URL del servidor: %s\n", url);

    /*-------------------------------------------------------------------------------------------*/
    /*Seccion SHMEM*/
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

    /*-------------------------------------------------------------------------------------------*/
    /*Seccion SEM*/
    /*Creo el semaforo*/
    sem_set_id = semget(SEM_ID, 2, IPC_CREAT | 0600);
    if (sem_set_id == -1)
    {
        printf("Error al solicitar el semaforo\n");
        exit(1);
    }

    sb.sem_op = 1;
    sb.sem_flg = 0;
    arg.val = 1;
    
    /*Libero los semaforos*/
    for (sb.sem_num = 0; sb.sem_num < 2; sb.sem_num++)
    {
        if (semop(sem_set_id, &sb, 1) != 0)
        {
            printf("Error al liberar los semaforos\n");
        }
    }

    /*-------------------------------------------------------------------------------------------*/
    printf("Soy el Proceso Proxy mi PID es %d\n", getpid());

    PID = fork();
    /*Creo un hijo para que actue como productor de datos*/
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

    /*-------------------------------------------------------------------------------------------*/
    /*Ingreso a un superlazo para hacer las recepciones por parte del hijo y enviar los paquetes*/
    while (bucle_activado)
    {
        /*Tomo el Semaforo y me quedo durmiendo hasta que se libere*/
        aux=bucle_activado;
        sb.sem_op = -1;
        sb.sem_num = semaforo_actual;
        if (semop(sem_set_id, &sb, 1) == -1)
        {
            bucle_activado = 0;
            printf("Error al tomar el semaforo en proceso proxy\n");
        }

        /*Obtengo la informacion desde la SHMEM*/
        memcpy(buffer_recepcion, shm_addr, sizeof(buffer_recepcion));
        buffer_recepcion[6] = '\0';
        printf("[PROXY]: Lectura de temperatura %s\n", buffer_recepcion);
        timestamp = time(NULL);

        /*Hago la transmision por HTTP*/
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

        /*Maquina de Estados*/
        switch (MEF_STATE)
        {
        case Primera_Falla:
        {
            /*Creo el archivo de errores y me voy al estado de Erroneo*/
            file_error_log = fopen(Ruta_Archivo_Error, "w+");
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
            /*Detecto la primer falla y me voy al estado correspondiente*/
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
            /*Se detecto que un paquete se pudo enviar,
            por lo que intento enviar los paquetes que tuvieron error.
            Si esta operacion fue exitosa limpio mi contador de paquetes
            errados y me voy a un estado de funcionamiento perfecto.
            De fallar debo seguir completando el archivo.*/

            file_error_log = fopen(Ruta_Archivo_Error, "rw+");
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
                /*retrocedo el cambio*/
                file_error_log = fopen(Ruta_Archivo_Error, "rw+");
                fseek(file_error_log, -2, SEEK_END);
                fprintf(file_error_log, "  ");
                fclose(file_error_log);
                MEF_STATE = Erroneo;
            }
            break;
        }
        case Erroneo:
        {
            /*Estado de error permanente, voy completando el archivo y ante
            una eventual deteccion de funcionamiento correcto, intento enviar los paquetes
            anteriores nuevamente.*/
            if (TX_STATE == Falla)
            {
                file_error_log = fopen(Ruta_Archivo_Error, "rw+");
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

        /*La falla de transmision se prolongo mucho tiempo, generamos una condicion de exit*/
        if (paquetes_erroneos == MAX_ERRORS)
        {
            printf("Demasiados errores de comunicacion, reintentar mas tarde!\n");
            bucle_activado = 0;
        }

        /*Intento liberar el semaforo tomado anteriormente*/

        sb.sem_op = 1;

        if (semop(sem_set_id, &sb, 1) == -1)
        {
            bucle_activado = 0;
            printf("Error al liberar el semaforo en proceso proxy\n");
        }

        /*Switcheo de semaforo*/
        if (semaforo_actual == 0)
            semaforo_actual = 1;
        else
            semaforo_actual = 0;
    }
    /*Termino de completar el formato json en el archivo de error*/
    file_error_log = fopen(Ruta_Archivo_Error, "rw+");
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
    /*Le aviso a mi hijo que debe terminar su ejecucion para tener una salida 
    y cirre de recursos agradable.*/

    kill(PID, SIGUSR2);
    free(url);
    return 0;
}