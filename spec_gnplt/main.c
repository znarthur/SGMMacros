#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<time.h>
//#include"spec_shm.h"
#include"sps.h"

int main(int argc, char *argv[]){
	char *spec_version=NULL;
	double *buffer=NULL;
	int i, rows, cols, flag, type;
	struct timespec sleep_dur;
	FILE *gp, *tmp;
	sleep_dur.tv_sec = 0;
	if(argc > 1){
		sscanf(argv[1], "%ld", &sleep_dur.tv_nsec); 
	} else {
		sleep_dur.tv_nsec = 10000;
	}

	tmp = fopen("spec_gnplt.out","w+");
	gp = popen("gnuplot -persist > /dev/null 2>&1", "w");
	//gp = popen("gnuplot -persist", "w");
	if (gp==NULL){
		printf("The pipe to GNUplot could not open.\n");
		return(0);
	}
	//If gnuplot pipe hasn't failed, than setup plot parameters.
	fprintf(gp, "set term x11\n set view map\n set nokey\n set palette defined (0 \"blue\", 1 \"red\")\n set size ratio 1\n set title \"CSCAN MAP\"\n");
	//Get spec instance for shared memory
	while(spec_version != NULL){
		spec_version = SPS_GetNextSpec(0);
	}
	SPS_GetArrayInfo (spec_version, "gnplt_out", &rows, &cols, &type, &flag);
	//printf("Row %d, Column %d, Type: %d, Flag %d\n", rows, cols, type, flag);
	//Allocate local array for reading in spec shared memory.
	buffer = malloc(cols*sizeof(double));
	// Get the x and y range for plot and set it.
	SPS_CopyFromShared(spec_version, "gnplt_out",buffer,SPS_DOUBLE, cols);
	fprintf(gp, "set xrange [%f:%f]\n", buffer[4], buffer[5]);
	fprintf(gp, "set yrange [%f:%f]\n", buffer[6], buffer[7]);
	fprintf(gp, "set offset graph 0.3, 0.3\n");
	while(buffer[3]){
		if (SPS_IsUpdated (spec_version,"gnplt_out") == 1) {
		    for(i=0;i<3;i++){
		  	 if(SPS_CopyFromShared (spec_version, "gnplt_out", buffer, SPS_DOUBLE, cols) != -1) {
				if(buffer[2] != -20000){
					fprintf(tmp, "%f ", buffer[i]);
					if(i == 2){
						fprintf(tmp,"\n");
		   				fflush(tmp);
						fprintf (gp, "splot \"spec_gnplt.out\" using 1:2:3 with points pointtype 5 palette pointsize 3\n");
        	   				fflush(gp);
					}
				}
		   	 }
		  }	
		  //printf("Changed %f %f %f\n", buffer[0], buffer[1], buffer[2]);
		  }
        	  nanosleep(&sleep_dur, NULL);
		
	}
	fclose(gp);
	fclose(tmp);

return(0);
}
