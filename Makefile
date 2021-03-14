#######################
# Makefile
#######################
# source object target
SOURCE := main.cpp
OBJS   := main.o
TARGET := sim

# compile and lib parameter
CC      := g++
LIBS    :=
LDFLAGS := -L.
DEFINES :=
INCLUDE := -I.
CFLAGS  := 
CXXFLAGS:= 

all:
	$(CC) -o $(TARGET) $(SOURCE)

# clean
clean:
	rm -fr *.o
	rm -fr $(TARGET)
