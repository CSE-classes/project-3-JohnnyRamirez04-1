
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define NUM_THREADS	4
#define MAX 1024

void *sub_string(void *);
int readf(FILE *fp);
int total=0;
int nlocal,n1,n2;
char *s1,*s2;
FILE *fp;
pthread_mutex_t total_lock;

int main(int argc, char *argv[])
{
	int i,rc;
	long t;
	pthread_t threads[NUM_THREADS];

	pthread_mutex_init(&total_lock,NULL);
	readf(fp);
	for(i=0;i<NUM_THREADS;i++){
		t = (long)i;
		rc=pthread_create(&threads[i],NULL,sub_string,(void *)t);
		if (rc){
			printf("ERROR: return error from pthread_create() is %d\n", rc);
			exit(-1);
		}
	}

	for(i=0; i<NUM_THREADS; i++){
		rc = pthread_join(threads[i], NULL);
		if (rc){
			printf("ERROR: return error from pthread_join() is %d\n", rc);
			exit(-1);
		}
	}
	printf("the occurences of s2 in s1 is %d\n",total);
	pthread_mutex_destroy(&total_lock);
	pthread_exit(0);
}



int readf(FILE *fp)
{
	if((fp=fopen("strings.txt", "r"))==NULL){
		printf("ERROR: can't open string.txt!\n");
		return 0;
	}
	s1=(char *)malloc(sizeof(char)*MAX);
	if(s1==NULL){
		printf("ERROR: Out of memory!\n");
		return -1;
	}
	s2=(char *)malloc(sizeof(char)*MAX);
	if(s1==NULL){
		printf("ERROR: Out of memory\n");
		return -1;
	}
	/*read s1 s2 from the file*/
	s1=fgets(s1, MAX, fp);
	s2=fgets(s2, MAX, fp);
	n1=strlen(s1);  /*length of s1*/
	if(s1[n1-1] == '\n') n1--; /*strip newline from s1*/
	n2=strlen(s2);
	if(s2[n2-1] == '\n') n2--; /*strip newline from s2*/
	nlocal=n1/NUM_THREADS;  /*data length held by process*/
	if(s1==NULL || s2==NULL ||n1<n2)  /*when error exit*/
		return -1;
	fclose(fp);
	return 0;
}

void *sub_string(void *threadid) 	/*each thread searches its assigned chunk of s1*/
	/*starting positions: tid*nlocal .. (tid+1)*nlocal - 1, capped at n1-n2*/
{
	long tid = (long)threadid;
	int start = tid * nlocal;
	int end   = start + nlocal - 1;     /*last starting index this thread owns*/
	int local_count = 0;
	int i, j, k, count;

	/*the absolute last possible starting index for a match is n1 - n2*/
	if (end > n1 - n2) end = n1 - n2;

	for (i = start; i <= end; i++) {
		count = 0;
		for (j = i, k = 0; k < n2; j++, k++) {
			if (*(s1+j) != *(s2+k)) {
				break;
			} else {
				count++;
			}
		}
		if (count == n2) {
			local_count++;
		}
	}

	/*add local count to global total under lock*/
	pthread_mutex_lock(&total_lock);
	total += local_count;
	pthread_mutex_unlock(&total_lock);

	pthread_exit(NULL);
}




