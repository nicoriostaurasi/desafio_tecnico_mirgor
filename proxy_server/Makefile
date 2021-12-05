LINKER_OBJECTS		= 
LINKER_SRIPT		= 
LINKER_DEPENDENCIES	= 
LINKER_ENTRY_POINT	= 
OUTPUT 				= ejecutable
AFLAG 				= 
CFLAG 				= -ljson-c -o 
CODIGO				= main

main: ./src/$(CODIGO).c
	@echo Generando $@.o ...				
	mkdir -p bin
	gcc ./src/$@.c $(CFLAG) ./bin/$(OUTPUT).bin 
	./bin/$(OUTPUT).bin

compile: ./src/$(CODIGO).c
	@echo Generando $(CODIGO).o ...				
	mkdir -p bin
	gcc ./src/$(CODIGO).c $(CFLAG) ./bin/$(OUTPUT).bin

clean:
	rm -f -r ./sup/*.lst ./bin/*.elf ./bin/*.bin ./obj/*.o

run:
	./bin/$(OUTPUT).bin
