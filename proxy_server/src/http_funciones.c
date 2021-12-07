#include "../inc/main.h"

int post_control(void)
{
    struct json_object *principal = NULL;
    struct json_object *status = NULL;
    char *buffer_control;

    buffer_control=obtener_string_de_file("output_post");
    
    if(buffer_control == NULL)
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

void post_http(char *medicion, time_t timestamp, char *url)
{
    char buffer_aux[128];
    int file;
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

    memset(buffer_aux, '\0', 128);
    sprintf(buffer_aux, "{ \"medicion\": { \"valor\": %s, \"timestamp\": %ld} }", medicion, timestamp);

    curl_envio_http(buffer_aux, url);
}



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

int intenta_nuevamente_http(char *url)
{
    char *buff_tx;

    buff_tx=obtener_string_de_file("error_send.json");

    if(buff_tx == NULL)
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

void post_paquete_http(char *buff_tx, char *url)
{
    int file;
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

    curl_envio_http(buff_tx, url);
}
