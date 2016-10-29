#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <Math.h>
#include <time.h>
#include <pthread.h>
#include "kv511.h"

#define PORT "9088"
#define BUFSIZE 100

char* char_pool = "1234567890!@#$^&*()abcdefghijklmnopqrstuvwsyzABCDEFGHIJKLMNOPQISTUVWXYZ,./;':<>?";

void *get_in_addr(struct sockaddr *sa){	// get IP address
	if(sa->sa_family == AF_INET){
		return &(((struct sockaddr_in*)sa)->sin_addr);
	}
	return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

v_t *get(k_t *key, int sockfd){
	char msg[BUFSIZE];
	char buf[BUFSIZE];
	v_t *res = malloc(1);
	int numbytes;
	memset(msg,'\0',80);
	sprintf(msg,"GET:%c",*key);
	if((numbytes = send(sockfd,msg,strlen(msg),0)) == -1){
		perror("send");
		exit(1);
	}
	if((numbytes = recv(sockfd,buf,BUFSIZE-1,0)) == -1){
		perror("recv");
		exit(1);
	}
	buf[numbytes] = '\0';
	printf("client: received '%s'\n", buf);
	*res = buf[numbytes-1];
	return res;
}

int put(k_t *key, v_t *val, int sockfd){
	char msg[BUFSIZE];
	char buf[BUFSIZE];
	int numbytes;
	memset(msg,'\0',80);
	sprintf(msg,"PUT:%c%c",*key,*val);
	if((numbytes = send(sockfd,msg,strlen(msg),0)) == -1){
		perror("send");
		exit(1);
	}
	if((numbytes = recv(sockfd,buf,BUFSIZE-1,0)) == -1){
		perror("recv");
		exit(1);
	}
	buf[numbytes] = '\0';
	printf("client: received '%s'\n", buf);
	return 0;
}

double exponential_gen(int lambda){
	srand(time(NULL));
	int rv = rand()%10000;
	double u = ((double)rv)/10000;
	return -log(u)/lambda;
}

char *rand_reqest_gen(){	// generate random request
	char* req = malloc(10);
	memset(req,'\0',10);
	if(rand()%2 == 0){	// PUT request
		memcpy(req,"PUT:",4);
		memcpy(req,char_pool+rand()%79,1);
		memcpy(req+1,char_pool+rand()%79,1);
		return req;
	}else{
		memcpy(req,"GET:",4);
		memcpy(req,char_pool+rand()%79,1);
		memcpy(req+1,char_pool+rand()%79,1);
		return req;
	}
}

void client_thread(void *arg1){
	arg_t *arg = (arg_t*)arg1;
	int snum = arg->nsession;
	int lambda = arg->lambda;
	int rnum = arg->nreq;
	int i,j,rv;
	int sockfd;
	char buf[BUFSIZE];
	char s[INET_ADDRSTRLEN];
	const char* addr = arg->iaddr;
	struct addrinfo hints, *p, *servinfo;

	k_t *key = malloc(1);
	v_t *val = malloc(1);
	
	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	/*if((rv = getaddrinfo(addr,PORT,&hints,&servinfo)) != 0){
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		pthread_exit(NULL);
	}*/
	for(i = 0; i<snum; i++){
		double tmp = exponential_gen(lambda);	// make the interval time to be exponential to make a Poisson distr
		int sec = (int)tmp;
		int usec = ((int)(tmp*1000000))%1000000;
		sleep(sec);
		usleep(usec);
		if((rv = getaddrinfo(addr,PORT,&hints,&servinfo)) != 0){
			fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
			pthread_exit(NULL);
		}
		for(p = servinfo; p != NULL; p = p->ai_next){	// pick first type of socket available
			if((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1){
				perror("client: socket");
				continue;
			}
			if(connect(sockfd, p->ai_addr, p->ai_addrlen) == -1){
				close(sockfd);
				perror("client: connect");
				continue;
			}
			break;
		}
		if(p == NULL){
			fprintf(stderr, "client: failed to connect\n");
			break;
		}	
		inet_ntop(p->ai_family,get_in_addr((struct sockaddr *)p->ai_addr),s,sizeof s);
		printf("client: connecting to %s\n",s);
		freeaddrinfo(servinfo);
		for(j = 0; j < rnum; j++){
			if(rand()%2 == 0){	// PUT command	
				key = char_pool+rand()%79;
				val = char_pool+rand()%79;
				//memcpy(key,req+rand()%79,1);
				//memcpy(val,req+rand()%79,1);
				put(key,val,sockfd);
				printf("put request: put <%c,%c>\n", *key, *val);
			}else{				// GET command
				key = char_pool+rand()%79;
				printf("get request: get %c\n", *key);
				v_t *tmpval = get(key,sockfd);
			}
		}
		close(sockfd);		
	}
}

/**
*	argv[1]: dest ip addr; 
* 	argv[2]: number of client threads;
* 	argv[3]: session arrival rate for each of the threads (lambda)
* 	argv[4]: number of sessions for each thread
*	argv[5]: number of request per session
*/

int main(int argc, char const *argv[])
{
	arg_t* arg = malloc(sizeof(arg_t));
	arg->iaddr = argv[1];
	arg->lambda = (int)strtol(argv[3],NULL,10);
	arg->nsession = (int)strtol(argv[4],NULL,10);
	arg->nreq = (int)strtol(argv[5],NULL,10);

	client_thread(arg);
	/*char buf[BUFSIZE];
	char s[INET_ADDRSTRLEN];
	char *msg1 = "PUT:a1";
	char *msg2 = "GET:a";
	char *msg3 = "GET:b";
	memset(buf,'\0',BUFSIZE);
	struct addrinfo hints, *p, *servinfo;
	int sockfd;
	int rv;
	int numbytes;

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	if((rv = getaddrinfo("127.0.0.1",PORT,&hints,&servinfo)) != 0){
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		return 1;
	}

	for(p = servinfo; p != NULL; p = p->ai_next){	// pick first type of socket available
		if((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1){
			perror("client: socket");
			continue;
		}
		if(connect(sockfd, p->ai_addr, p->ai_addrlen) == -1){
			close(sockfd);
			perror("client: connect");
			continue;
		}
		break;
	}

	if(p == NULL){
		fprintf(stderr, "client: failed to connect\n");
		return 2;
	}

	inet_ntop(p->ai_family,get_in_addr((struct sockaddr *)p->ai_addr),s,sizeof s);
	printf("client: connecting to %s\n",s);

	freeaddrinfo(servinfo);
	k_t *key = malloc(1);
	v_t *val = malloc(1);
	memcpy(key,"a",1);
	memcpy(val,"1",1);

	put(key,val,sockfd);
	v_t *tmpval = get(key,sockfd);
	printf("main: %c\n",*tmpval);
	*/
/*	if((numbytes = send(sockfd,msg1,6,0)) == -1){
		perror("send");
		exit(1);
	}
	//printf("send out %d bytes\n",numbytes);
	if((numbytes = recv(sockfd,buf,BUFSIZE-1,0)) == -1){
		perror("recv");
		exit(1);
	}
	buf[numbytes] = '\0';

	printf("client: received '%s'\n", buf);

//////////////////////////////////////////////////
	
	if((numbytes = send(sockfd,msg2,5,0)) == -1){
		perror("send");
		exit(1);
	}
	//printf("send out %d bytes\n",numbytes);
	if((numbytes = recv(sockfd,buf,BUFSIZE-1,0)) == -1){
		perror("recv");
		exit(1);
	}
	buf[numbytes] = '\0';

	printf("client: received '%s'\n", buf);

//////////////////////////////////////////////////
	
	if((numbytes = send(sockfd,msg3,5,0)) == -1){
		perror("send");
		exit(1);
	}
	//printf("send out %d bytes\n",numbytes);
	if((numbytes = recv(sockfd,buf,BUFSIZE-1,0)) == -1){
		perror("recv");
		exit(1);
	}
	buf[numbytes] = '\0';

	printf("client: received '%s'\n", buf);
*/
	//close(sockfd);
	return 0;
}