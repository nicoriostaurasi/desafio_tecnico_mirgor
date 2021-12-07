/**
 * @file http_funciones.c
 * @author Nicolas Rios Taurasi (nicoriostaurasi@frba.utn.edu.ar)
 * @brief Contiene las funciones HTTP post de paquete y de medicion
 * @version 0.1
 * @date 07-12-2021
 *
 * @copyright Copyright (c) 2021
 *
 */

#include "../inc/main.h"

/**
 * @brief funcion que se encarga de chequear la salida por consola luego de hacer un post HTTP
 *
 * @return 0: Exito, -1: Error
 */
int post_control(void)
{
    struct json_object *principal = NULL;
    struct json_object *status = NULL;
    char *buffer_control;

    buffer_control = obtener_string_de_file(Ruta_LOG_POST);

    if (buffer_control == NULL)
    {
        return -1;
    }

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

    free(buffer_control);
    return 0;
}

/**
 * @brief Post simple de una sola medicion HTTP
 *
 * @param medicion valor medido
 * @param timestamp unidad temporal
 * @param url direccion a donde se encuentra el cloud server
 */
void post_http(char *medicion, time_t timestamp, char *url)
{
    char buffer_aux[128];
    int file;
    int PID;
    PID = fork();

    if (PID == 0)
    {
        execlp("rm", "rm", "-f", Ruta_LOG_POST, NULL);
        exit(1);
    }
    wait(NULL);

    file = open(Ruta_LOG_POST, O_WRONLY | O_CREAT, 0777);
    if (file == -1)
    {
        printf("Error al abrir el archivo\n");
        exit(1);
    }
    dup2(file, STDOUT_FILENO);

    memset(buffer_aux, '\0', 128);
    sprintf(buffer_aux, "{ \"medicion\": { \"valor\": %s, \"timestamp\": %ld} }", medicion, timestamp);

    curl_envio_http(buffer_aux, url);
}

/**
 * @brief Se encarga de transmitir una unica medicion
 *
 * @param medicion valor medido
 * @param timestamp unidad temporal
 * @param url direccion a donde se encuentra el cloud server
 * @return int 0: Exito, -1: Error
 */
int transmision_http(char *medicion, time_t timestamp, char *url)
{
    int PID_2;

    PID_2 = fork();

    if (PID_2 == 0)
    {
        signal(SIGINT, SIG_IGN);
        post_http(medicion, timestamp, url);
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

/**
 * @brief Funcion que se encarga de levantar el archivo de errores y envia todo como un solo paquete
 *
 * @param url direccion a donde se encuentra el cloud server
 * @return int 0: Exito, -1: Error
 */
int intenta_nuevamente_http(char *url)
{
    char *buff_tx;

    buff_tx = obtener_string_de_file(Ruta_Archivo_Error);

    if (buff_tx == NULL)
    {
        return -1;
    }

    if (transmision_paquete_http(buff_tx, url) != 0)
    {
        return -1;
    }
    free(buff_tx);
    return 0;
}

/**
 * @brief Funcion que recibe el buffer a enviar como paquete y controla que llego correctamente
 *
 * @param buff_tx string que contiene el texto del archivo
 * @param url direccion a donde se encuentra el cloud server
 * @return int 0: Exito, -1: Error
 */
int transmision_paquete_http(char *buff_tx, char *url)
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

/**
 * @brief Proceso que va a enviar el paquete
 *
 * @param buff_tx contenido a enviar
 * @param url direccion a donde se encuentra el cloud server
 */
void post_paquete_http(char *buff_tx, char *url)
{
    int file;
    int PID;
    PID = fork();

    if (PID == 0)
    {
        execlp("rm", "rm", "-f", Ruta_LOG_POST, NULL);
        exit(1);
    }
    wait(NULL);

    file = open(Ruta_LOG_POST, O_WRONLY | O_CREAT, 0777);
    if (file == -1)
    {
        printf("Error al abrir el archivo\n");
        exit(1);
    }
    dup2(file, STDOUT_FILENO);

    curl_envio_http(buff_tx, url);
}
