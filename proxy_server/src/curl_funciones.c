/**
 * @file curl_funciones.c
 * @author Nicolas Rios Taurasi (nicoriostaurasi@frba.utn.edu.ar)
 * @brief Archivo que contiene las funciones de CURL
 * @version 0.1
 * @date 07-12-2021
 * 
 * @copyright Copyright (c) 2021
 * 
 */

#include "../inc/main.h"

/**
 * @brief Funcion ejemplo por parte de la libreria, la misma recibe un string y lo envia mediante una url pasada por parametro
 * 
 * @param buff_tx informacion a transmitir
 * @param url direccion a donde se encuentra el cloud server
 */
void curl_envio_http(char *buff_tx, char *url)
{
    CURL *curl;
    CURLcode res;
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
