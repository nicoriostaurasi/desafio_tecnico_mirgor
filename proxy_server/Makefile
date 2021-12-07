LINKER_OBJECTS		= -lcurl
SRC_PATH			= ./src/
OUTPUT 				= ejecutable
CFLAG 				= -ljson-c -o 
OBJS				= $(SRC_PATH)main.c $(SRC_PATH)archivo_funciones.c $(SRC_PATH)curl_funciones.c $(SRC_PATH)http_funciones.c $(SRC_PATH)temperatura.c 
CURL_PATH			= -L/usr/lib/x86_64-linux-gnu

main: 
	@echo Generando $(OUTPUT).bin ...				
	mkdir -p bin
	gcc $(CURL_PATH) $(OBJS) $(CFLAG) ./bin/$(OUTPUT).bin $(LINKER_OBJECTS) 
	./bin/$(OUTPUT).bin

compile: 
	@echo Compilando Fuentes... 				
	mkdir -p bin
	gcc $(CURL_PATH) $(OBJS) $(CFLAG) ./bin/$(OUTPUT).bin $(LINKER_OBJECTS)

clean:
	rm -f -r ./sup/*.lst ./bin/*.elf ./bin/*.bin ./obj/*.o

run:
	./bin/$(OUTPUT).bin
