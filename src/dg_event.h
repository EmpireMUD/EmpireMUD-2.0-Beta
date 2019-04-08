/* ************************************************************************
*   File: dg_event.h                                      EmpireMUD 2.0b5 *
*  Usage: structures and prototypes for events                            *
*                                                                         *
*  DG Scripts code by Eric Green (ejg3@cornell.edu)                       *
*  EmpireMUD code base by Paul Clarke, (C) 2000-2015                      *
*  All rights reserved.  See license.doc for complete information.        *
*                                                                         *
*  EmpireMUD based upon CircleMUD 3.0, bpl 17, by Jeremy Elson.           *
*  Death's Gate MUD is based on CircleMUD, Copyright (C) 1993, 94.        *
*  CircleMUD (C) 1993, 94 by the Trustees of the Johns Hopkins University *
*  CircleMUD is based on DikuMUD, Copyright (C) 1990, 1991.               *
************************************************************************ */

/*
** how often will heartbeat() call the 'wait' event function?
*/
#define PULSE_DG_EVENT 1


/********** Event related section *********/

/*
** define event related structures
*/
struct dg_event {
	EVENTFUNC(*func);
	void *event_obj;
	struct q_element *q_el;
};

/****** End of Event related info ********/

/***** Queue related info ******/

/* number of queues to use (reduces enqueue cost) */
#define NUM_EVENT_QUEUES    500

struct queue {
	struct q_element *head[NUM_EVENT_QUEUES], *tail[NUM_EVENT_QUEUES];
};

struct q_element {
	void *data;
	long key;
	struct q_element *prev, *next;
};
/****** End of Queue related info ********/

/* - events - function protos need by other modules */
void dg_event_init(void);
struct dg_event *dg_event_create(EVENTFUNC(*func), void *event_obj, long when);
void dg_event_cancel(struct dg_event *event, EVENT_CANCEL_FUNC(*func));
void dg_event_process(void);
long dg_event_time(struct dg_event *event);
void dg_event_free_all(void);

/* - queues - function protos need by other modules */
struct queue *queue_init(void);
struct q_element *queue_enq(struct queue *q, void *data, long key);
void queue_deq(struct queue *q, struct q_element *qe);
void *queue_head(struct queue *q);
long queue_key(struct queue *q);
long queue_elmt_key(struct q_element *qe);
void queue_free(struct queue *q);
int  dg_event_is_queued(struct dg_event *event);
