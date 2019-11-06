CC = g++ -g
COMPILE.C = $(CC) -c
VPATH = ./
CXXFLAGS = -lpthread
MAKEEXE = $(CC)

OBJ = main.o GevDiscovery.o GevForceIp.o BroadcastSocket.o arping.o
EXE = GeConfigCmd

all: $(EXE)

$(EXE): ${OBJ}
	$(MAKEEXE) $^ -o $@ $(CXXFLAGS)

%.o: %.cpp
	$(COMPILE.C) $< -o $@ $(CXXFLAGS)

clean:
	rm -f *.o $(EXE)
