# The makefile to compile FDMNES
# Works with gaussian solver and without any call to libraries. Sequential.
# Works with the gfortran Linux Compiler (for other compiler change the FC command)

FC = gcc
OPTLVL = 3 

# For intel compiler, it seems that probems at execution are avoided when compiling
# sphere.f90 with O1 option and the other routines with O2 option.

EXEC = data_recorder

FFLAGS = -c  -O$(OPTLVL) 

INCLUDE = -I"nexus/hdf5-1.8.17-linux-centos6-x86_64-gcc447-shared/include"   

LLIBRARY = -L"/home/sgm/lib64" -lxml2 -lNeXus -lhdf5 

OBJ = main.o sps.o

all: $(EXEC)

$(EXEC): $(OBJ) 
	$(FC) -o $@ $^ $(LLIBRARY)

%.o: %.c
	$(FC) -o $@ $(FFLAGS) $? $(INCLUDE)

clean:
	rm -f *.o $(EXEC)
	rm -f *.mod	

