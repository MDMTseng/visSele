OBJECTS = hw.o
LIB=
INC=-IacvImage/include/
default: doCompile

FLAGs=-w

%.o: %.c
	g++  -Wall $(FLAGs) $(LIB) $(INC) -c $< -o $@

doCompile: $(OBJECTS)
	g++  -Wall $(FLAGs) $(LIB) $(INC) $(OBJECTS) -o $@
