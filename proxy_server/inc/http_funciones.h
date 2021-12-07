int transmision_http(char *medicion, time_t timestamp, char *url);
int transmision_paquete_http(char *buff_tx, char *url);
int post_control(void);
int intenta_nuevamente_http(char *url);
void post_paquete_http(char *buff_tx, char *url);
void post_http(char *medicion, time_t timestamp, char *url);