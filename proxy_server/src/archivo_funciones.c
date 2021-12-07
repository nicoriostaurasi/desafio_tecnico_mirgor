#include "../inc/main.h"

char *obtener_url(char *ruta)
{
    char *buffer_cfg;
    char *buffer_return;
    struct json_object *principal_cfg = NULL; /*Variables auxiliares para recorrer el .json*/
    struct json_object *url_json = NULL;


    buffer_cfg=obtener_string_de_file(ruta);

    if(buffer_cfg == NULL)
    {
        return NULL;
    }

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

char* obtener_string_de_file(char* ruta)
{
    FILE* file_pointer;
    char ch_aux;
    int cantidad_caracteres;
    char* buffer_return;

    file_pointer = fopen(ruta, "r");
    if (file_pointer == NULL)
    {
        printf("Error al abrir el archivo %s\n",ruta);
        return NULL;
    }

    do
    {
        ch_aux = fgetc(file_pointer);
        cantidad_caracteres++;
    } while (!feof(file_pointer));

    buffer_return = malloc(cantidad_caracteres);

    if (buffer_return == NULL)
    {
        printf("Error al asignar memoria dinamica\n");
        fclose(file_pointer);
        return NULL;
    }

    rewind(file_pointer);

    if (fread(buffer_return, cantidad_caracteres,sizeof(char), file_pointer) < 0)
    {
        printf("Error al leer el archivo %s\n",ruta);
        fclose(file_pointer);
        return NULL;
    }

    return buffer_return;   
}