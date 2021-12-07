/**
 * @file main.h
 * @author Nicolas Rios Taurasi (nicoriostaurasi@frba.utn.edu.ar)
 * @brief Principales bibliotecas a utilizar
 * @version 0.1
 * @date 07-12-2021
 * 
 * @copyright Copyright (c) 2021
 * 
 */

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

#include "archivo_funciones.h"
#include "curl_funciones.h"
#include "http_funciones.h"

#define SEM_ID 250 /* ID del semaforo */
#define MAX_ERRORS 20

#define Ruta_Archivo_Error  "error_send.json"
#define Ruta_Archivo_CFG    "cloud_cfg.json"
#define Ruta_LOG_POST       "output_post"
#define Ruta_Temperatura    "temperatura.json"
 
typedef union smun
{
    int val;
    struct semid_ds *buff;
    unsigned short *array;
} smun;

void productor_temperatura(int sem_set_id, char *shm_addr, struct sembuf sb, union smun arg);

