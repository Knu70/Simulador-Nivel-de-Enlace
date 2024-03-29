/* Simulator for datalink protocols. 
 *    Written by Andrew S. Tanenbaum.
 *    Revised by Shivakant Mishra.
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <stdio.h>

//NUEVO*****************************************
#include <sys/sem.h>
#include <sys/ipc.h>
#include <sys/shm.h>
//FIN NUEVO*************************************

#include "simulator.h"

#define NR_TIMERS 8             /* number of timers; this should be greater
                                   than half the number of sequence numbers. */
#define MAX_QUEUE 100000        /* max number of buffered frames */
#define NO_EVENT -1             /* no event possible */
#define FRAME_SIZE (sizeof(frame))
#define BYTE 0377               /* byte mask */
#define UINT_MAX  0xFFFFFFFF    /* maximum value of an unsigned 32-bit int */
#define INTERVAL 100000         /* interval for periodic printing */
#define AUX 2                   /* aux timeout is main timeout/AUX */

/* DEBUG MASKS */
#define SENDS        0x0001     /* frames sent */
#define RECEIVES     0x0002     /* frames received */
#define TIMEOUTS     0x0004     /* timeouts */
#define PERIODIC     0x0008     /* periodic printout for use with long runs */

#define DEADLOCK (3 * timeout_interval)	/* defines what a deadlock is */
#define MANY 256		/* big enough to clear pipe at the end */

/* Status variables used by the workers, M0 and M1. */
bigint ack_timer[NR_TIMERS];    /* ack timers */
unsigned int seqs[NR_TIMERS];   /* last sequence number sent per timer */
bigint lowest_timer;            /* lowest of the timers */
bigint aux_timer;               /* value of the auxiliary timer */
int network_layer_status;       /* 0 is disabled, 1 is enabled */
unsigned int next_net_pkt;      /* seq of next network packet to fetch */
unsigned int last_pkt_given= 0xFFFFFFFF;        /* seq of last pkt delivered*/
frame last_frame;               /* arrive frames are kept here */
int offset;                     /* to prevent multiple timeouts on same tick*/
bigint tick;                    /* current time */
int retransmitting;             /* flag that is set on a timeout */
int nseqs = NR_TIMERS;          /* must be MAX_SEQ + 1 after startup */
unsigned int oldest_frame = NR_TIMERS;  /* tells which frame timed out */

char *badgood[] = {"bad ", "good"};
char *tag[] = {"Data", "Ack ", "Nak "};

/* Statistics */
int data_sent;                  /* number of data frames sent */
int data_retransmitted;         /* number of data frames retransmitted */
int data_lost;                  /* number of data frames lost */
int data_not_lost;              /* number of data frames not lost */
int good_data_recd;             /* number of data frames received */
int cksum_data_recd;            /* number of bad data frames received */

int acks_sent;                  /* number of ack frames sent */
int acks_lost;                  /* number of ack frames lost */
int acks_not_lost;              /* number of ack frames not lost */
int good_acks_recd;             /* number of ack frames received */
int cksum_acks_recd;            /* number of bad ack frames received */

int payloads_accepted;          /* number of pkts passed to network layer */
int timeouts;                   /* number of timeouts */
int ack_timeouts;               /* number of ack timeouts */

/* Incoming frames are buffered here for later processing. */
frame queue[MAX_QUEUE];         /* buffered incoming frames */
frame *inp = &queue[0];         /* where to put the next frame */
frame *outp = &queue[0];        /* where to remove the next frame from */
int nframes;                    /* number of queued frames */

bigint tick = 0;		/* the current time, measured in events */
bigint last_tick;		/* when to stop the simulation */
int exited[2];			/* set if exited (for each worker) */
int hanging[2];			/* # times a process has done nothing */
struct sigaction act, oact;

//NUEVO*****************************************
   int sem0; //Controla la lectura y la escritura sobre la variable compartida hay_datos_0_1
   int sem1; //Controla la lectura y la escritura sobre la variable compartida hay_datos_1_0
   int *hay_datos_0_1;  //inicialmente no hay datos escritos
   int *hay_datos_1_0;  //inicialmente no hay datos escritos
   int memId1;
   int memId2;
//FIN NUEVO*************************************

/* Prototypes. */
void start_simulator(void (*p1)(), void (*p2)(), long event, int tm_out, int pk_loss, int grb, int d_flags);
void set_up_pipes(void);
void fork_off_workers(void);
void terminate(char *s);

void init_max_seqnr(unsigned int o);
unsigned int get_timedout_seqnr(void);
void wait_for_event(event_type *event);
void init_frame(frame *s);
void queue_frames(void);
int pick_event(void);
event_type frametype(void);
void from_network_layer(packet *p);
void to_network_layer(packet *p);
void from_physical_layer(frame *r);
void to_physical_layer(frame *s);
void start_timer(seq_nr k);
void stop_timer(seq_nr k);
void start_ack_timer(void);
void stop_ack_timer(void);
void enable_network_layer(void);
void disable_network_layer(void);
int check_timers(void);
int check_ack_timer(void);
unsigned int pktnum(packet *p);
void fr(frame *f);
void recalc_timers(void);
void print_statistics(void);
void sim_error(char *s);
int parse_first_five_parameters(int argc, char *argv[], long *event, int *timeout_interval, int *pkt_loss, int *garbled, int *debug_flags);


//NUEVO*****************************************
int INICIA(int valor);
int BORRASEM(int semaforo);
int semcall(int semaforo,int operacion);
void P(int semaforo);
void V(int semaforo);
//FIN NUEVO*************************************

void start_simulator(void (*p1)(), void (*p2)(), long event, int tm_out, int pk_loss, int grb, int d_flags)
{
/* The simulator has three processes: main(this process), M0, and M1, all of
 * which run independently.  Set them all up first.  Once set up, main
 * maintains the clock (tick), and picks a process to run.  Then it writes a
 * 32-bit word to that process to tell it to run.  The process sends back an
 * answer when it is done.  Main then picks another process, and the cycle
 * repeats.
 */

  int process = 0;		/* whose turn is it */
  int rfd, wfd;			/* file descriptor for talking to workers */
  bigint word;			/* message from worker */

  act.sa_handler = SIG_IGN;
  setvbuf(stdout, (char *) 0, _IONBF, (size_t) 0);	/* disable buffering*/

  proc1 = p1;
  proc2 = p2;
  /* Each event uses DELTA ticks to make it possible for each timeout to
   * occur at a different tick.  For example, with DELTA = 10, ticks will
   * occur at 0, 10, 20, etc.  This makes it possible to schedule multiple
   * events (e.g. in protocol 5), all at unique times, e.g. 1070, 1071, 1072,
   * 1073, etc.  This property is needed to make sure timers go off in the
   * order they were set.  As a consequence, internally, the variable tick
   * is bumped by DELTA on each event. Thus asking for a simulation run of
   * 1000 events will give 1000 events, but they internally they will be
   * called 0 to 10,000.
   */
  last_tick = DELTA * event; 

  /* Convert from external units to internal units so the user does not see
   * the internal units at all.
   */
  timeout_interval = DELTA * tm_out;

  /* Packet loss takes place at the sender.  Packets selected for being lost
   * are not put on the wire at all.  Internally, pkt_loss and garbled are
   * from 0 to 990 so they can be compared to 10 bit random numbers.  The
   * inaccuracy here is about 2.4% because 1000 != 1024.  In effect, it is
   * not possible to say that all packets are lost.  The most that can be
   * be lost is 990/1024.
   */

  pkt_loss = 10 * pk_loss; /* for our purposes, 1000 == 1024 */

  /* This arg tells what fraction of arriving packets are garbled.  Thus if
   * pkt_loss is 50 and garbled is 50, half of all packets (actually,
   * 500/1024 of all packets) will not be sent at all, and of the ones that
   * are sent, 500/1024 will arrive garbled.
   */

  garbled = 10 * grb; /* for our purposes, 1000 == 1024 */



//NUEVO*****************************************
   sem0=INICIA(1);  
   sem1=INICIA(1);
   memId1 = shmget(IPC_PRIVATE,sizeof(int),0660);//IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR);
   memId2 = shmget(IPC_PRIVATE,sizeof(int),0660);//IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR);

//Inicializo memoria compartida
//Vincular la memoria compartida a las variables concretas
   hay_datos_0_1 = (int *)shmat(memId1,NULL,0);
   hay_datos_1_0 = (int *)shmat(memId2,NULL,0);
//Inicializar las variables
   (*hay_datos_0_1) = 0;
   (*hay_datos_0_1) = 0;
//Desvincular la memoria (el padre no la vuelve a usar, solo los 2 procesos hijos)
   shmdt(hay_datos_0_1);
   shmdt(hay_datos_1_0);

//FIN NUEVO*************************************


  /* Turn tracing options on or off.  The bits are defined in worker.c. */
  debug_flags = d_flags;
  printf("\n\nEvents: %u    Parameters: %u %d %u\n",
      last_tick/DELTA, timeout_interval/DELTA, pkt_loss/10, garbled/10);

  set_up_pipes();		/* create five pipes */
  fork_off_workers();		/* fork off the worker processes */

  /* Main simulation loop. */
  while (tick <last_tick) {
	process = rand() & 1;		/* pick process to run: 0 or 1 */
	tick = tick + DELTA;
	rfd = (process == 0 ? r4 : r6);
	if (read(rfd, &word, TICK_SIZE) != TICK_SIZE) terminate("");
	if (word == OK) hanging[process] = 0;
	if (word == NOTHING) hanging[process] += DELTA;
	if (hanging[0] >= DEADLOCK && hanging[1] >= DEADLOCK)
		terminate("A deadlock has been detected");

	/* Write the time to the selected process to tell it to run. */
	wfd = (process == 0 ? w3 : w5);
	if (write(wfd, &tick, TICK_SIZE) != TICK_SIZE)
		terminate("Main could not write to worker");

  }

  /* Simulation run has finished. */


//NUEVO*****************************************
   BORRASEM(sem0); //Eliminamos el semaforo 0
   BORRASEM(sem1); //Eliminamos el semaforo 1
// Borrar
   int o,oo;
   o = shmctl(memId1,IPC_RMID,NULL); //Eliminamos la memoria compartida
   oo = shmctl(memId2,IPC_RMID,NULL); //Eliminamos la memoria compartida
//FIN NUEVO*************************************

  terminate("End of simulation");
}


void set_up_pipes(void)
{
/* Create six pipes so main, M0 and M1 can communicate pairwise. */

  int fd[2];

  pipe(fd);  r1 = fd[0];  w1 = fd[1];	/* M0 to M1 for frames */
  pipe(fd);  r2 = fd[0];  w2 = fd[1];	/* M1 to M0 for frames */
  pipe(fd);  r3 = fd[0];  w3 = fd[1];	/* main to M0 for go-ahead */
  pipe(fd);  r4 = fd[0];  w4 = fd[1];	/* M0 to main to signal readiness */
  pipe(fd);  r5 = fd[0];  w5 = fd[1];	/* main to M1 for go-ahead */
  pipe(fd);  r6 = fd[0];  w6 = fd[1];	/* M1 to main to signal readiness */
}

void fork_off_workers(void)
{
/* Fork off the two workers, M0 and M1. */

  if (fork() != 0) {
	/* This is the Parent.  It will become main, but first fork off M1. */
	if (fork() != 0) {
		/* This is main. */
		sigaction(SIGPIPE, &act, &oact);
	        setvbuf(stdout, (char *)0, _IONBF, (size_t)0);/*don't buffer*/
		close(r1);
		close(w1);
		close(r2);
		close(w2);
		close(r3);
		close(w4);
		close(r5);
		close(w6);
		return;
	} else {
		/* This is the code for M1. Run protocol. */
		sigaction(SIGPIPE, &act, &oact);
	        setvbuf(stdout, (char *)0, _IONBF, (size_t)0);/*don't buffer*/
		close(w1);
		close(r2);
		close(r3);
		close(w3);
		close(r4);
		close(w4);
		close(w5);
		close(r6);

//NUEVO********************************
//Vinculamos la memoria compartida a las 2 variables en el proceso hijo M1
   hay_datos_0_1 = (int *)shmat(memId1,NULL,0);
   hay_datos_1_0 = (int *)shmat(memId2,NULL,0);
//FIN NUEVO****************************
	
		id = 1;		/* M1 gets id 1 */
		mrfd = r5;	/* fd for reading time from main */
		mwfd = w6;	/* fd for writing reply to main */
		prfd = r1;	/* fd for reading frames from worker 0 */
		(*proc2)();	/* call the user-defined protocol function */

//NUEVO********************************
//Desvinculamos la memoria compartida a las 2 variables en el proceso hijo M1
   shmdt(hay_datos_0_1);
   shmdt(hay_datos_1_0);
//FIN NUEVO****************************
		terminate("Impossible.  Protocol terminated");
	}
  } else {
	/* This is the code for M0. Run protocol. */
	sigaction(SIGPIPE, &act, &oact);
        setvbuf(stdout, (char *)0, _IONBF, (size_t)0);/*don't buffer*/
	close(r1);
	close(w2);
	close(w3);
	close(r4);
	close(r5);
	close(w5);
	close(r6);

//NUEVO********************************
//Vinculamos la memoria compartida a las 2 variables en el proceso hijo M0
   hay_datos_0_1 = (int *)shmat(memId1,NULL,0);
   hay_datos_1_0 = (int *)shmat(memId2,NULL,0);
//FIN NUEVO****************************

	id = 0;		/* M0 gets id 0 */
	mrfd = r3;	/* fd for reading time from main */
	mwfd = w4;	/* fd for writing reply to main */
	prfd = r2;	/* fd for reading frames from worker 1 */
	(*proc1)();	/* call the user-defined protocol function */

//NUEVO********************************
//Desvinculamos la memoria compartida a las 2 variables en el proceso hijo M1
   shmdt(hay_datos_0_1);
   shmdt(hay_datos_1_0);
//FIN NUEVO****************************
	terminate("Impossible. protocol terminated");
  }
}

void terminate(char *s)
{
/* End the simulation run by sending each worker a 32-bit zero command. */

  int n, k1, k2, res1[MANY], res2[MANY], eff, acc, sent;

  for (n = 0; n < MANY; n++) {res1[n] = 0; res2[n] = 0;}
  write(w3, &zero, TICK_SIZE);
  write(w5, &zero, TICK_SIZE);
  sleep(2);

  /* Clean out the pipe.  The zero word indicates start of statistics. */
  n = read(r4, res1, MANY*sizeof(int));
  k1 = 0;
  while (res1[k1] != 0) k1++;
  k1++;				/* res1[k1] = accepted, res1[k1+1] = sent */

  /* Clean out the other pipe and look for statistics. */
  n = read(r6, res2, MANY*sizeof(int));
  k2 = 0;
  while (res1[k2] != 0) k2++;
  k2++;				/* res1[k2] = accepted, res1[k2+1] = sent */

  if (strlen(s) > 0) {
	acc = res1[k1] + res2[k2];
	sent = res1[k1+1] + res2[k2+1];
	if (sent > 0) {
		eff = (100 * acc)/sent;
 	        printf("\nEfficiency (payloads accepted/data pkts sent) = %d%c\n", eff, '%');
	}
	printf("%s.  Time=%u\n",s, tick/DELTA);
  }
  exit(1);
 }

void init_max_seqnr(unsigned int o)
{
  nseqs = oldest_frame = o;
}

unsigned int get_timedout_seqnr(void)
{
  return(oldest_frame);
}

void wait_for_event(event_type *event)
{
/* Wait_for_event reads the pipe from main to get the time.  Then it
 * fstat's the pipe from the other worker to see if any
 * frames are there.  If so, if collects them all in the queue array.
 * Once the pipe is empty, it makes a decision about what to do next.
 */
 
 bigint ct, word = OK;

  offset = 0;			/* prevents two timeouts at the same tick */
  retransmitting = 0;		/* counts retransmissions */
  while (true) {
	queue_frames();		/* go get any newly arrived frames */
	if (write(mwfd, &word, TICK_SIZE) != TICK_SIZE) print_statistics();
	if (read(mrfd, &ct, TICK_SIZE) != TICK_SIZE) print_statistics();
	if (ct == 0) print_statistics();
	tick = ct;		/* update time */
	if ((debug_flags & PERIODIC) && (tick%INTERVAL == 0))
		printf("Tick %u. Proc %d. Data sent=%d  Payloads accepted=%d  Timeouts=%d\n", tick/DELTA, id, data_sent, payloads_accepted, timeouts);

	/* Now pick event. */
	*event = pick_event();
	if (*event == NO_EVENT) {
		word = (lowest_timer == 0 ? NOTHING : OK);
		continue;
	}
	word = OK;
	if (*event == timeout) {
		timeouts++;
		retransmitting = 1;	/* enter retransmission mode */
		if (debug_flags & TIMEOUTS)
		      printf("Tick %u. Proc %d got timeout for frame %d\n",
					       tick/DELTA, id, oldest_frame);
	}

	if (*event == ack_timeout) {
		ack_timeouts++;
		if (debug_flags & TIMEOUTS)
		      printf("Tick %u. Proc %d got ack timeout\n",
					       tick/DELTA, id);
	}
	return;
  }
}

void init_frame(frame *s)
{
  /* Fill in fields that that the simulator expects. Protocols may update
   * some of these fields. This filling is not strictly needed, but makes the
   * simulation trace look better, showing unused fields as zeros.
   */

  s->seq = 0;
  s->ack = 0;
  s->kind = (id == 0 ? data : ack);
  s->info.data[0] = 0;
  s->info.data[1] = 0;
  s->info.data[2] = 0;
  s->info.data[3] = 0;
}

void queue_frames(void)
{
/* See if any frames from the peer have arrived; if so get and queue them.
 * Queue_frames() sucks frames out of the pipe into the circular buffer,
 * queue[]. It first fstats the pipe, to avoid reading from an empty pipe and
 * thus blocking.  If inp is near the top of queue[], a single call here
 * may read a few frames into the top of queue[] and then some more starting
 * at queue[0].  This is done in two read operations.
 */

  int prfd, frct, k;
  frame *top;
  struct stat statbuf;
  frame *a;
  frame aa;


  prfd = (id == 0 ? r2 : r1);	/* which file descriptor is pipe on */

//Modificado
//Tengo que leer primeramente todos los datos de la tuber�a y me quedo con la direccion de inicio
//compruebo si he leido datos y el numero e tramas que se reciben
//paso a ejecutar la lectura pero copiando desde la direccion de inicio en lugar de leyendo desde la tuberia
 
  
  int readbytes;
  frame queue_aux[MAX_QUEUE];
  a = &queue_aux[0];

//NUEVO*****************************************
  readbytes=0;
  frct = 0;

  if (id == 0)
  {
	  if((*hay_datos_1_0) > 0)//hay_datos_1_0 > 0)
	  {
		  //Leo datos
		  if( (readbytes=read(prfd, &queue_aux, FRAME_SIZE * MAX_QUEUE )) > 0)   
		  {
				frct = readbytes/FRAME_SIZE;
				a = &queue_aux[0];

		  }
		  //Actualizo variable
		  P(sem1); //Bloqueo la variable hay_datos_1_0
		  (*hay_datos_1_0) = 0;
		  V(sem1); //Desbloqueo la variable hay_datos_1_0
	  }

  }	 
  else
	  if (id==1)
	  {
			if((*hay_datos_0_1) > 0)
			{
				//Leo datos

				if( (readbytes=read(prfd, &queue_aux, FRAME_SIZE * MAX_QUEUE )) > 0)   
				{
					frct = readbytes/FRAME_SIZE;
					a = &queue_aux[0];
				}
				//Actualizo variable
				P(sem1); //Bloqueo la variable hay_datos_1_0
				(*hay_datos_0_1) = 0;
				V(sem1); //Desbloqueo la variable hay_datos_1_0
			}
	  }
//FIN NUEVO*************************************





//No cambia
  if (nframes + frct >= MAX_QUEUE)	/* check for possible queue overflow*/
	sim_error("Out of queue space. Increase MAX_QUEUE and re-make.");  



//Modificaciones SUSTITUIR el mecanismo de lectura por la asignacion desde el puntero leido
  /* If frct is 0, the pipe is empty, so don't read from it. */
  if (frct > 0) 
  {

	/* How many frames can be read consecutively? */
	top = (outp <= inp ? &queue[MAX_QUEUE] : outp);/* how far can we rd?*/
	k = top - inp;	/* number of frames that can be read consecutively */
	if (k > frct) k = frct;	/* how many frames to read from peer */


//Modificada lectura 

/*	if (read(prfd, inp, k * FRAME_SIZE) != k * FRAME_SIZE)
		sim_error("Error reading frames from peer");*/

	memmove(inp,a,k*FRAME_SIZE);

	a+= k; //a pasa a apuntar a donde apuntaba antes + k posiciones  


	frct -= k;		/* residual frames not yet read */
	inp += k;
	if (inp == &queue[MAX_QUEUE]) inp = queue;
	nframes += k;

	/* If frct is still > 0, the queue has been filled to the upper
	 * limit, but there is still space at the bottom.  Continue reading
	 * there.  This mechanism makes queue a circular buffer.
	 */
	if (frct > 0) 
	{

//Modificada lectura
/*		if (read(prfd, queue, frct * FRAME_SIZE) != frct*FRAME_SIZE)
			sim_error("Error 2 reading frames from peer");*/
		//equivale a meter en queue todos los datos que hay en a

		memmove(queue,a,frct*FRAME_SIZE);

		nframes += frct;
		inp = &queue[frct];
	}
  }
}


int pick_event(void)
{
/* Pick a random event that is now possible for the process.
 * Note that the order in which the tests is made is critical, as it gives
 * priority to some events over others.  For example, for protocols 3 and 4
 * frames will be delivered before a timeout will be caused.  This is probably
 * a reasonable strategy, and more closely models how a real line works.
 */

  if (check_ack_timer() > 0) return(ack_timeout);
  if (nframes > 0) return((int)frametype());
  if (network_layer_status) return(network_layer_ready);
  if (check_timers() >= 0) return(timeout);	/* timer went off */
  return(NO_EVENT);
}


event_type frametype(void)
{
/* This function is called after it has been decided that a frame_arrival
 * event will occur.  The earliest frame is removed from queue[] and copied
 * to last_frame.  This copying is needed to avoid messing up the simulation
 * in the event that the protocol does not actually read the incoming frame.
 * For example, in protocols 2 and 3, the senders do not call
 * from_physical_layer() to collect the incoming frame. If frametype() did
 * not remove incoming frames from queue[], they never would be removed.
 * Of course, one could change sender2() and sender3() to have them call
 * from_physical_layer(), but doing it this way is more robust.
 *
 * This function determines (stochastically) whether the arrived frame is good
 * or bad (contains a checksum error).
 */

  int n, i;
  event_type event;

  /* Remove one frame from the queue. */
  last_frame = *outp;		/* copy the first frame in the queue */
  outp++;
  if (outp == &queue[MAX_QUEUE]) outp = queue;
  nframes--;

  /* Generate frames with checksum errors at random. */
  n = rand() & 01777;
  if (n < garbled) {
	/* Checksum error.*/
	event = cksum_err;
	if (last_frame.kind == data) cksum_data_recd++;
	if (last_frame.kind == ack) cksum_acks_recd++;
	i = 0;
  } else {
	event = frame_arrival;
	if (last_frame.kind == data) good_data_recd++;
	if (last_frame.kind == ack) good_acks_recd++;
	i = 1;
  }

  if (debug_flags & RECEIVES) {
	printf("Tick %u. Proc %d got %s frame:  ",
						tick/DELTA,id,badgood[i]);
	fr(&last_frame);
  }
  return(event);
}


void from_network_layer(packet *p)
{
/* Fetch a packet from the network layer for transmission on the channel. */

  p->data[0] = (next_net_pkt >> 24) & BYTE;
  p->data[1] = (next_net_pkt >> 16) & BYTE;
  p->data[2] = (next_net_pkt >>  8) & BYTE;
  p->data[3] = (next_net_pkt      ) & BYTE;
  next_net_pkt++;
}


void to_network_layer(packet *p)
{
/* Deliver information from an inbound frame to the network layer. A check is
 * made to see if the packet is in sequence.  If it is not, the simulation
 * is terminated with a "protocol error" message.
 */

  unsigned int num;

  num = pktnum(p);
  if (num != last_pkt_given + 1) {
	printf("Tick %u. Proc %d got protocol error.  Packet delivered out of order.\n", tick/DELTA, id); 
	printf("Expected payload %d but got payload %d\n",last_pkt_given+1,num);
	exit(0);
  }
  last_pkt_given = num;
  payloads_accepted++;
}

  
void from_physical_layer (frame *r)
{
/* Copy the newly-arrived frame to the user. */
 *r = last_frame;
}

void to_physical_layer(frame *s)
{
/* Pass the frame to the physical layer for writing on pipe 1 or 2. 
 * However, this is where bad packets are discarded: they never get written.
 */

  int fd, got, k;



  /* The following statement is essential to later on determine the timed
   * out sequence number, e.g. in protocol 6. Keeping track of
   * this information is a bit tricky, since the call to start_timer()
   * does not tell what the sequence number is, just the buffer.  The
   * simulator keeps track of sequence numbers using the array seqs[],
   * which records the sequence number of each data frame sent, so on a
   * timeout, knowing the buffer number makes it possible to determine
   * the sequence number.
   */
  if (s->kind==data) seqs[s->seq % (nseqs/2)] = s->seq; 

  if (s->kind == data) data_sent++;
  if (s->kind == ack) acks_sent++;
  if (retransmitting) data_retransmitted++;

  /* Bad transmissions (checksum errors) are simulated here. */
  k = rand() & 01777;		/* 0 <= k <= about 1000 (really 1023) */
  if (k < pkt_loss) {	/* simulate packet loss */
	if (debug_flags & SENDS) {
		printf("Tick %u. Proc %d sent frame that got lost: ",
							    tick/DELTA, id);
		fr(s);
	}
	if (s->kind == data) data_lost++;	/* statistics gathering */
	if (s->kind == ack) acks_lost++;	/* ditto */
	return;

  }
  if (s->kind == data) data_not_lost++;		/* statistics gathering */
  if (s->kind == ack) acks_not_lost++;		/* ditto */
  fd = (id == 0 ? w1 : w2);


  frame b;
  b=*s;


  got = write(fd, &b,FRAME_SIZE);  //s, FRAME_SIZE);

//NUEVO*****************************************

  if (id == 1)
  {
	  P(sem1); //Bloqueo la variable hay_datos_1_0
	  (*hay_datos_1_0) += 1;
	  V(sem1); //Desbloqueo la variable hay_datos_1_0

  }	 
  else
	  if (id==0)
	  {

		P(sem0); //Bloqueo la variable hay_datos_0_1
		(*hay_datos_0_1) += 1;
		V(sem0); //Desbloqueo la variable hay_datos_0_1

	  }
//FIN NUEVO*************************************



  if (got != FRAME_SIZE) print_statistics();	/* must be done */

  if (debug_flags & SENDS) {
	printf("Tick %u. Proc %d sent frame: ", tick/DELTA, id);
	fr(s);
  }

}


void start_timer(seq_nr k)
{
/* Start a timer for a data frame. */

  ack_timer[k] = tick + timeout_interval + offset;
  offset++;
  recalc_timers();		/* figure out which timer is now lowest */
}


void stop_timer(seq_nr k)
{
/* Stop a data frame timer. */

  ack_timer[k] = 0;
  recalc_timers();		/* figure out which timer is now lowest */
}


void start_ack_timer(void)
{
/* Start the auxiliary timer for sending separate acks. The length of the
 * auxiliary timer is arbitrarily set to half the main timer.  This could
 * have been another simulation parameter, but that is unlikely to have
 * provided much extra insight.
 */

  aux_timer = tick + timeout_interval/AUX;
  offset++;
}


void stop_ack_timer(void)
{
/* Stop the ack timer. */

  aux_timer = 0;
}


void enable_network_layer(void)
{
/* Allow network_layer_ready events to occur. */

  network_layer_status = 1;
}


void disable_network_layer(void)
{
/* Prevent network_layer_ready events from occuring. */

  network_layer_status = 0;
}


int check_timers(void)
{
/* Check for possible timeout.  If found, reset the timer. */

  int i;

  /* See if a timeout event is even possible now. */
  if (lowest_timer == 0 || tick < lowest_timer) return(-1);

  /* A timeout event is possible.  Find the lowest timer. Note that it is
   * impossible for two frame timers to have the same value, so that when a
   * hit is found, it is the only possibility.  The use of the offset variable
   * guarantees that each successive timer set gets a higher value than the
   * previous one.
   */
  for (i = 0; i < NR_TIMERS; i++) {
	if (ack_timer[i] == lowest_timer) {
		ack_timer[i] = 0;	/* turn the timer off */
		recalc_timers();	/* find new lowest timer */
                oldest_frame = seqs[i];	/* timed out sequence number */
		return(i);
	}
  }
  printf("Impossible.  check_timers failed at %d\n", lowest_timer);
  exit(1);
}


int check_ack_timer()
{
/* See if the ack timer has expired. */

  if (aux_timer > 0 && tick >= aux_timer) {
	aux_timer = 0;
	return(1);
  } else {
	return(0);
  }
}


unsigned int pktnum(packet *p)
{
/* Extract packet number from packet. */

  unsigned int num, b0, b1, b2, b3;

  b0 = p->data[0] & BYTE;
  b1 = p->data[1] & BYTE;
  b2 = p->data[2] & BYTE;
  b3 = p->data[3] & BYTE;
  num = (b0 << 24) | (b1 << 16) | (b2 << 8) | b3;
  return(num);
}


void fr(frame *f)
{
/* Print frame information for tracing. */

  printf("type=%s  seq=%d  ack=%d  payload=%d\n",
	tag[f->kind], f->seq, f->ack, pktnum(&f->info));
}

void recalc_timers(void)
{
/* Find the lowest timer */

  int i;
  bigint t = UINT_MAX;

  for (i = 0; i < NR_TIMERS; i++) {
	if (ack_timer[i] > 0 && ack_timer[i] < t) t = ack_timer[i];
  }
  lowest_timer = t;
}


void print_statistics(void)
{
/* Display statistics. */

  int word[3];

  sleep(1);
  printf("\nProcess %d:\n", id);
  printf("\tTotal data frames sent:  %9d\n", data_sent);
  printf("\tData frames lost:        %9d\n", data_lost);
  printf("\tData frames not lost:    %9d\n", data_not_lost);
  printf("\tFrames retransmitted:    %9d\n", data_retransmitted);
  printf("\tGood ack frames rec'd:   %9d\n", good_acks_recd);
  printf("\tBad ack frames rec'd:    %9d\n\n", cksum_acks_recd);

  printf("\tGood data frames rec'd:  %9d\n", good_data_recd);
  printf("\tBad data frames rec'd:   %9d\n", cksum_data_recd);
  printf("\tPayloads accepted:       %9d\n", payloads_accepted);
  printf("\tTotal ack frames sent:   %9d\n", acks_sent);
  printf("\tAck frames lost:         %9d\n", acks_lost);
  printf("\tAck frames not lost:     %9d\n", acks_not_lost);

  printf("\tTimeouts:                %9d\n", timeouts);
  printf("\tAck timeouts:            %9d\n", ack_timeouts);
  fflush(stdin);

  word[0] = 0;
  word[1] = payloads_accepted;
  word[2] = data_sent;
  write(mwfd, word, 3*sizeof(int));	/* tell main we are done printing */
  sleep(1);
  exit(0);
}

void sim_error(char *s)
{
/* A simulator error has occurred. */

  int fd;

  printf("%s\n", s);
  fd = (id == 0 ? w4 : w6);
  write(fd, &zero, TICK_SIZE);
  exit(1);
}

int parse_first_five_parameters(int argc, char *argv[], long *event, int *timeout_interval, int *pkt_loss, int *garbled, int *debug_flags)
{
/* Help function for protocol writers to parse first five command-line
 * parameters that the simulator needs.
 */

  if (argc < 6) {
        printf("Need at least five command-line parameters.\n");
        return(0);
  }
  *event = atol(argv[1]);
  if (*event < 0) {
        printf("Number of simulation events must be positive\n");
        return(0);
  }
  *timeout_interval = atoi(argv[2]);
  if (*timeout_interval < 0){
        printf("Timeout interval must be positive\n");
        return(0);
  }
  *pkt_loss = atoi(argv[3]);     /* percent of sends that chuck pkt out */
  if (*pkt_loss < 0 || *pkt_loss > 99) {
        printf("Packet loss rate must be between 0 and 99\n");
        return(0);
  }
  *garbled = atoi(argv[4]);
  if (*garbled < 0 || *garbled > 99) {
        printf("Packet cksum rate must be between 0 and 99\n");
        return(0);
  }
  *debug_flags = atoi(argv[5]);
  if (*debug_flags < 0) {
        printf("Debug flags may not be negative\n");
        return(0);
  }
  return(1);
}


//NUEVO********************* FUNCIONES PARA SEMAFOROS ********************

int INICIA(int valor) /*---->Funcion que crea un semaforo,inicializa su contador
			 asociado con un valor y devuelve su identificador*/
  {
   int semid;/*------------------------->Identificador del semaforoa ,entero*/
   unsigned short semval;
   int temporal;
//Semget permite crear o acceder a un conjunto de semaforos bajo un identificador comun
//En este caso creamos un semaforo
//El conjunto es de solo un miembro
//La key es IPC_PRIVATE
//Con IPC_CREAT indicamos que creamos
//0666 son los permisos que damos a usuario, grupo y otros
   semid=semget(IPC_PRIVATE,1,(IPC_CREAT | 0666));
   if (semid==-1)
      return(-1);
//Inicializamos el semaforo al valor que se nos pasa
   semval=valor;
//Semctl nos permite poner en el semaforo identificado por semid el valor 
   //Semctl permite realizar operaciones de control sobre el semaforo
   temporal=semctl(semid,0,SETVAL,semval);/*--->Inicializacion del semaforo*/
   if (temporal==-1)
      return(-2);/*-------------------------------->Error de inicializacion*/
   return(semid);/*----------------->Devuelve el identificador del semaforo*/
  }

int BORRASEM(int semaforo) /*------->Funcion que elimina el semaforo del sistema*/
  
  {
//Liberamos el semaforo con semctl
   semctl(semaforo,0,IPC_RMID,0);/*----------------->Liberacion del semaforo*/
  }



int semcall(int semaforo,int operacion) /*---->Funcion que utilizan los procesos P y V*/

  {
   struct sembuf sb;
//realizamos la operacion que nos indican los parametros en el semaforo que se nos indica
//Semop usa una estructura sem_buf con la posicion en el array, la operacion y parametros de la op
//Se utiliza para hacer los wait y los signal
   sb.sem_num=0;
   sb.sem_op=operacion;
   sb.sem_flg=0;
   return((semop(semaforo,&sb,1))); /*--------------->Devuelve -1 si error*/
  }

/*A continuacion vienen las rutinas P y V : */

void P(int semaforo)
  
  {
//Operacion wait sobre el semaforo, restamos una unidad de recurso
   semcall(semaforo,-1);
  }

void V(int semaforo)
  
  {
//Operacion signal sobre el semaforo, sumamos una unidad de recurso
   semcall(semaforo,1);
  }

//FIN NUEVO*************** FUNCIONES PARA SEMAFOROS *********************
