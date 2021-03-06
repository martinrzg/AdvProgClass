/**
 * @copyright (c) 2017 Abelardo López Lagunas
 *
 * @file    5_complexPipes.c
 *
 * @author  Abelardo López Lagunas
 *
 * @date    Wed 25 Jan 2017 10:13 CST
 *
 * @brief   A set of processes randomly messaging each
 *          other using pipes.
 *
 * References:
 *          Based on code by Mij <mij@bitchx.it> on 05/01/05.
 *          Original source file:
 *               http://mij.oltrelinux.com/devel/unixprg/
 *
 * Restrictions:
 *          There is little or no error checking
 *
 * Revision history:
 *          Wed 25 Jan 2017 10:13 CST -- File created
 *
 * @note    Intended for TC2025 students
 *
 */

#include <stdio.h>
#include <sys/types.h>              // for read() and write()
#include <sys/uio.h>
#include <string.h>                 // for strlen and others
#include <unistd.h>                 // for pipe()
#include <stdlib.h>                 // for [s]random()
#include <time.h>                   // for time() [seeding srandom()]
#include <signal.h>                 //for kill() and SIGTERM

#define PROCS_NUM 15       // 1 < number of processes involved <= 255
#define MAX_PAYLOAD_LENGTH 50  // Message length
const int DEAD_PROC = -1;      // Mark a dead process' file descriptors

typedef int proc_addr;              // A process address */

struct message_s {                  // Message structure
    proc_addr src_id;
    short int length;
    char *payload;
};

/* send message to process with id dest */
int send_proc_message(proc_addr dest, char *message);
/* receive a message in the process' queue of received ones */
int receive_proc_message(struct message_s *msg);
/* mark process file descriptors closed */
void mark_proc_closed(proc_addr process);


/*              ***             GLOBAL VARS                 ***     */

proc_addr my_address = 0;           /* stores the id of the process */
int proc_pipes[PROCS_NUM][2]; /* store the pipes of every process   */

int main(int argc, char *argv[])
{
  pid_t child_pid;
  pid_t my_children[PROCS_NUM];            /* PIDs of the children */
  int i, ret;
  char msg_text[MAX_PAYLOAD_LENGTH]; /* payload of the message to send */
  proc_addr msg_recipient;
  struct message_s msg;

  pipe(proc_pipes[0]);       /* create a pipe for me (the parent) */

  /* initializing proc_pipes struct */
  for (i = 1; i < PROCS_NUM; i++) {
	/* creating one pipe for every (future) process */
	ret = pipe(proc_pipes[i]);
	if (ret) {
	  perror("Error creating pipe");
	  abort();
	}
  }


  /* fork [1..NUM_PROCS] children. 0 is me. */
  for (i = 1; i < PROCS_NUM; i++) {
	/* setting the child address */
	my_address = my_address + 1;

	child_pid = fork();
	if (! child_pid) {
	  /* child */
	  sleep(5);

	  /* closing other process' pipes read ends */
	  for (i = 0; i < PROCS_NUM; i++) {
		if (i != my_address)
		  close(proc_pipes[i][0]);
	  }

	  /* init random num generator */
	  srandom(time(NULL));

	 /* my_address is now my address, and will hereby become a "constant" */
	 /* producing some message for the other processes */
	  while (random() % (2*PROCS_NUM)) {

		/* interleaving... */
		sleep((unsigned int)(random() % 2));

		/* choosing a random recipient (including me) */
		msg_recipient = (proc_addr)(random() % PROCS_NUM);

		/* preparing and sending the message */
		sprintf(msg_text,"hello from process %u.", (int)my_address);

		ret = send_proc_message(msg_recipient, msg_text);
		if (ret > 0) {
		  /* message has been correctly sent */
		  printf("   --> %d: sent message to %u\n",
				 my_address, msg_recipient);
		} else {
		  /* the child we tried to message does no longer exist */
		  mark_proc_closed(msg_recipient);
		  printf("    --> %d: recipient %u is no longer available\n",
				 my_address, msg_recipient);
		}
	  }

	  printf ("Process %d exited the while\n", my_address);

	  /* now, reading the first 2 messages we've been sent */
	  for (i = 0; i < 2; i++) {
		ret = receive_proc_message(&msg);
		if (ret < 0) break;
		printf("<--     Process %d, received message from %u: \"%s\".\n",
			   my_address, msg.src_id, msg.payload);
	  };

	  /* i'm exiting. making my pipe widowed */
	  close(proc_pipes[my_address][0]);

	  printf("# %d: i am exiting.\n", my_address);
	  exit(0);
	}

	/* saving the child pid (for future killing) */
	my_children[my_address] = child_pid;

	/* parent. I don't need the read descriptor of the pipe */
	close(proc_pipes[my_address][0]);

	/* this is for making srandom() consistent */
	sleep(1);
  }

  /* expecting the user request to terminate... */
  printf("Please press ENTER when you like me to flush the children...\n");
  getchar();

  printf("Ok, terminating dandling processes...\n");
  /* stopping freezed children */
  for (i = 1; i < PROCS_NUM; i++) {
	kill(my_children[i], SIGTERM);
  }
  printf("Done. Exiting.\n");

  return 0;
}


int send_proc_message(proc_addr dest, char *message)
{
  int ret;
  char *msg = (char *)malloc(sizeof(message) + 2);


  /* the write should be atomic. Doing our best */
  msg[0] = (char)dest;
  memcpy((void *)&(msg[1]), (void *)message, strlen(message)+1);

  /* send message, including the "header" the trailing '\0' */
  ret = write(proc_pipes[dest][1], msg, strlen(msg)+2);
  free(msg);

  return ret;
}


int receive_proc_message(struct message_s *msg)
{
  char c = 'x';
  char temp_string[MAX_PAYLOAD_LENGTH];
  int ret, i = 0;


  /* first, getting the message sender */
  ret = read(proc_pipes[my_address][0], &c, 1);
  if (ret == 0) {
	return 0;
  }
  msg->src_id = (proc_addr)c;

  do {
	ret = read(proc_pipes[my_address][0], &c, 1);
	temp_string[i++] = c;
  } while ((ret > 0) && (c != '\0') && (i < MAX_PAYLOAD_LENGTH));

  if (c == '\0') {
	/* msg correctly received. Preparing message packet */

	msg->payload = (char *)malloc(strlen(temp_string) + 1);
	strncpy(msg->payload, temp_string, strlen(temp_string) + 1);

	return 0;
  }

  return -1;
}


void mark_proc_closed(proc_addr process)
{
  proc_pipes[process][0] = DEAD_PROC;
  proc_pipes[process][1] = DEAD_PROC;
}
