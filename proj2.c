//	IOS - 2. projekt
//	Author: Jakub Svoboda
//	Login: xsvobo0z
//	Date: 13.4.2016

#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <unistd.h>
#include <time.h>
#include <semaphore.h>
#include <fcntl.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/ipc.h>

int arguments(int P,int C,int PT,int RT){
	// Validates values of passed arguments. Returns 0 if arguments are legit.
	if (P <= 0){									// Check if P is ok.
		fprintf(stderr, "P (number of passegers) must be > 0. %d was passed. \n",P);
		exit (1);
	}
	if ((C <= 0) || (P <= C) || ((P % C) !=0)){		// Check if C is ok.
		fprintf(stderr, "C (cart capacity) must be: C > 0, P > C, P mod C =0. %d was passed. \n",C);
		exit (1);
	}
	if ((PT < 0) || (PT >= 5001)){					//Check if PT is ok.
		fprintf(stderr, "PT (pause time) must be: PT >= 0, PT < 5001. %d was passed. \n",PT);
		exit(1);
	}
	if ((RT < 0) || (RT >= 5001)){					//Check if RT is ok.
		fprintf(stderr, "RT (ride time) must be: RT >= 0, RT < 5001. %d was passed. \n",RT);
		exit(1);
	}

	return 0;
}
int clear_semaphores(sem_t *mutex1,sem_t *mutex2,sem_t *board_queue,sem_t *unboard_queue,sem_t *all_aboard,sem_t *all_ashore,sem_t *output_sem,sem_t *kill_all){
	//This function first closes and then unlinks all semaphores.
	sem_close(mutex1);
	sem_unlink("mutex1");
	sem_close(mutex2);
	sem_unlink("mutex2");
	sem_close(board_queue);
	sem_unlink("board_queue");
	sem_close(unboard_queue);
	sem_unlink("unboard_queue");
	sem_close(all_ashore);
	sem_unlink("all_ashore");
	sem_close(all_aboard);
	sem_unlink("all_aboard");
	sem_close(output_sem);
	sem_unlink("output_sem");
	sem_close(kill_all);
	sem_unlink("kill_all");
	return 0;
}
int do_car_stuff(int *A,int C, int RT,sem_t *board_queue,sem_t *unboard_queue,sem_t *all_aboard,sem_t *all_ashore,sem_t *output_sem,FILE *fp){
	sem_wait(output_sem);				//operation LOAD begins
	fprintf (fp,"%d 	: C 1	: load\n",*A);
	fflush(fp);
	(*A)++;
	sem_post(output_sem);
	for(int k =0; k<C; k++){
		sem_post(board_queue);
	}
	sem_wait(all_aboard);		// Waits for last passenger to singal all_aboard, LOAD ends.

	sem_wait(output_sem);		//print run
	fprintf (fp,"%d 	: C 1	: run\n",*A);
	fflush(fp);
	(*A)++;
	sem_post(output_sem);

	if (RT>0) usleep((rand() % RT) *1000);				//sleep for RT miliseconds
	
	sem_wait(output_sem);		//operation UNLOAD begins
	fprintf (fp,"%d 	: C 1	: unload\n",*A);
	fflush(fp);
	(*A)++;
	sem_post(output_sem);

	for(int k = C; k>0; k--){
		sem_post(unboard_queue);	//Sends C signals to passengers to unboard.
	}
	sem_wait(all_ashore);	//Waits for last unboarding passenger to singnal all_ashore
	return 0;
}
int do_passenger_stuff(int *A, int *order,int C,int i,int *boarders,int *unboarders, sem_t *mutex1,sem_t *mutex2,sem_t *board_queue,sem_t *unboard_queue,sem_t *all_aboard,sem_t *all_ashore,sem_t *output_sem,sem_t *kill_all,FILE *fp){
	*order=*order;			//to block -Werror if order is not implemented
	sem_wait(output_sem);
	fprintf(fp,"%d 	: P %d 	: started\n",*A,i);		//Prints when passenger get created.
	fflush(fp);
	(*A)++;
	sem_post(output_sem);

	sem_wait(board_queue);					//Waits for car to send signal to board.

	sem_wait(mutex1);		// operation LOAD begins
	sem_wait(output_sem);
	(*boarders)++;
	fprintf(fp,"%d 	: P %d 	: board \n",*A,i);	//Print board message.
	fflush(fp);
	(*A)++;
	if (*boarders == C){
		fprintf(fp,"%d 	: P %d 	: board last\n",*A,i);	//output if last boarder
		fflush(fp);
		(*A)++;
		sem_post(all_aboard);
		*boarders=0;								//reset boarders count
	}else{											//output if not last boarder
		fprintf(fp,"%d 	: P %d 	: board order %d\n",*A,i, *boarders);
		fflush(fp);
		(*A)++;
	}
	sem_post(output_sem);
	sem_post(mutex1);		// operation LOAD ends

	sem_wait(unboard_queue);
	
	sem_wait(mutex2);		// operation UNLOAD begins
	sem_wait(output_sem);
	(*unboarders)++;
	fprintf(fp,"%d 	: P %d 	: unboard \n",*A,i);
	fflush(fp);
	(*A)++;
	
		if (*unboarders == C){
			fprintf(fp,"%d 	: P %d 	: unboard last\n",*A,i);		//print if last boarder
			fflush(fp);
			(*A)++;
			sem_post(all_ashore);
			*unboarders=0;							//reset unboarders counter
		}else{						//output for unboarders that are not last
			fprintf(fp,"%d 	: P %d 	: unboard order %d\n",*A,i,*unboarders);
			fflush(fp);
			(*A)++;
		}
	sem_post(output_sem);
	sem_post(mutex2);		// operation UNLOAD ends

	sem_wait(kill_all);		//waits for creator to send signal to die
	sem_wait(output_sem);
	fprintf(fp,"%d 	: P %d 	: finished \n",*A,i);	//prints final message
	fflush(fp);
	(*A)++;
	sem_post(output_sem);
	return 0;
}
int main (int argc, char *argv[]){		//Create shared variables.
	int *unboarders= mmap(NULL, sizeof(int), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
	int *boarders = mmap(NULL, sizeof(int), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
	int *A = mmap(NULL, sizeof(int), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
	int *order = mmap(NULL, sizeof(int), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
	*boarders=0;				//Number of processes that boarded the cart.
	*unboarders=0;				//Number of processes that unboarded the cart.
	*A=1;						//Event number (first number in output).
	*order=1;					//Not used atm.

	FILE *fp;						//Open output file for write.
	fp=fopen("proj2.out","w+");

	int P = 0,C = 0, PT = 0, RT =0;
	if (argc != 5){			//Exactly 4 additional arguments must be passed.
		fprintf(stderr, "Incorrect number of arguments: %d, should be 5.\n",argc);
		exit(1);
	}
	else{					//Assign aruments to int variables.
		P = strtol(argv[1], NULL, 10);
		C = strtol(argv[2], NULL, 10);
		PT = strtol(argv[3], NULL, 10);
		RT = strtol(argv[4], NULL, 10);
		arguments(P,C,PT,RT);		//Validate values of arguments.
	}
	//Creates all semaphores.
	sem_t *mutex1 = sem_open("mutex1", O_CREAT | O_EXCL, 0644, 1);	
	sem_t *mutex2 = sem_open("mutex2", O_CREAT | O_EXCL, 0644, 1);	
	sem_t *board_queue = sem_open("board_queue", O_CREAT | O_EXCL, 0644, 0);	
	sem_t *unboard_queue = sem_open("unboard_queue", O_CREAT | O_EXCL, 0644, 0);	
	sem_t *all_aboard = sem_open("all_aboard", O_CREAT | O_EXCL, 0644, 0);	
	sem_t *all_ashore = sem_open("all_ashore", O_CREAT | O_EXCL, 0644, 0);	
	sem_t *output_sem = sem_open("output_sem", O_CREAT | O_EXCL, 0644, 1);
	sem_t *kill_all = sem_open("kill_all", O_CREAT | O_EXCL, 0644, 0);

	pid_t child_pid_or_zero = fork();

	if (child_pid_or_zero < 0){				// Error when fork fails.
		perror("fork() error");
		exit (2);
	}
	if (child_pid_or_zero != 0){			// Main process.
		waitpid(child_pid_or_zero,&child_pid_or_zero,0);	//Waits for car to finish.
		
		for(int i=0; i < (P+1) ; i++) sem_post(kill_all);	//Allows passengers to end.	
		wait(NULL);											//waits for passengers to end.
		munmap(boarders, sizeof(int));		//Free shared memory.
		munmap(unboarders, sizeof(int));
		munmap(A, sizeof(int));
		munmap(order, sizeof(int));
											//Free semaphores.
		clear_semaphores(mutex1,mutex2,board_queue,unboard_queue,all_aboard,all_ashore,output_sem,kill_all);
		fclose(fp);		//close output file.

	}else{			// Creator process
		srand(time(NULL));			//	Seed for rand() function.
		int car = fork();			//	Fork of car process
		if (car  <0){				//	When fork fails, prints error message.
			perror("fork error ");	
			exit(2);
		}else if (car == 0 ){				//Car process
			sem_wait(output_sem);	
			fprintf(fp,"%d 	: C 1	: started\n",*A);	
			fflush(fp);
			(*A)++;
			sem_post(output_sem);
			for(int j=0; j<(P/C);j++){		//Car rides P/C times
				do_car_stuff(A,C,RT,board_queue,unboard_queue,all_aboard,all_ashore,output_sem,fp);
			}
				
			sem_wait(output_sem);			//Output when finished	
			fprintf(fp,"%d 	: C 1 	: finished \n",*A);
			fflush(fp);
			(*A)++;
			sem_post(output_sem);
		
			exit (0);
		}else{								//Creator process
			for(int i=1; i<=P;i++){				//Passagers creation
				if (PT> 0) usleep((rand() % PT) * 1000);	//Sleep for PT
				int passenger = fork();						
				if (passenger  <0){							//error when fork fails			
					perror("fork error");	
					exit(2);
				}else if (passenger == 0 ){		// Each passager goes for one ride.
					do_passenger_stuff(A, order,C, i,boarders, unboarders, mutex1,mutex2,board_queue,unboard_queue,all_aboard,all_ashore,output_sem,kill_all,fp);
					exit (0);
				}else{
				//	wait(NULL);
				}
			}
			wait(NULL);
		}
	//	wait(NULL);
	//	waitpid(car,&car,0);
	}
	return 0;
}