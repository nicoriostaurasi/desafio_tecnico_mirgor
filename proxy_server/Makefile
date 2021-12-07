LINKER_OBJECTS		= -lcurl
LINKER_SRIPT		= 
LINKER_DEPENDENCIES	= 
LINKER_ENTRY_POINT	= 
OUTPUT 				= ejecutable
AFLAG 				= 
CFLAG 				= -ljson-c -o 
CODIGO				= main
CURL_PATH			= -L/usr/lib/x86_64-linux-gnu

main: ./src/$(CODIGO).c
	@echo Generando $@.o ...				
	mkdir -p bin
	gcc $(CURL_PATH) ./src/$@.c $(CFLAG) ./bin/$(OUTPUT).bin $(LINKER_OBJECTS) 
	./bin/$(OUTPUT).bin

compile: ./src/$(CODIGO).c
	@echo Generando $(CODIGO).o ...				
	mkdir -p bin
	gcc $(CURL_PATH) ./src/$(CODIGO).c $(CFLAG) ./bin/$(OUTPUT).bin $(LINKER_OBJECTS)

clean:
	rm -f -r ./sup/*.lst ./bin/*.elf ./bin/*.bin ./obj/*.o

run:
	./bin/$(OUTPUT).bin
