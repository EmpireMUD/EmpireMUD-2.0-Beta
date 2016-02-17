/* ************************************************************************
*   File: dg_scripts.h                                    EmpireMUD 2.0b3 *
*  Usage: header file for script structures and contstants, and           *
*         function prototypes for dg_scripts.c                            *
*                                                                         *
*  DG Scripts code by egreen, 1996/09/24 03:48:42, revision 3.6           *
*  EmpireMUD code base by Paul Clarke, (C) 2000-2015                      *
*  All rights reserved.  See license.doc for complete information.        *
*                                                                         *
*  EmpireMUD based upon CircleMUD 3.0, bpl 17, by Jeremy Elson.           *
*  Death's Gate MUD is based on CircleMUD, Copyright (C) 1993, 94.        *
*  CircleMUD (C) 1993, 94 by the Trustees of the Johns Hopkins University *
*  CircleMUD is based on DikuMUD, Copyright (C) 1990, 1991.               *
************************************************************************ */

#define DG_SCRIPT_VERSION "DG Scripts 1.0.12 e2"

// look for 'x_TRIGGER' for things related to this (I know that's backwards)
#define MOB_TRIGGER  0
#define OBJ_TRIGGER  1
#define WLD_TRIGGER  2
#define RMT_TRIGGER  3
#define ADV_TRIGGER  4
#define VEH_TRIGGER  5

/* unless you change this, Puff casts all your dg spells */
#define DG_CASTER_PROXY 1
/* spells cast by objects and rooms use this level */
#define DG_SPELL_LEVEL  25 

/*
 * define this if you don't want wear/remove triggers to fire when
 * a player is saved.
 */
#define NO_EXTRANEOUS_TRIGGERS

/* mob trigger types */
#define MTRIG_GLOBAL           BIT(0)      /* check even if zone empty   */
#define MTRIG_RANDOM           BIT(1)      /* checked randomly           */
#define MTRIG_COMMAND          BIT(2)	   /* character types a command  */
#define MTRIG_SPEECH           BIT(3)	   /* a char says a word/phrase  */
#define MTRIG_ACT              BIT(4)      /* word or phrase sent to act */
#define MTRIG_DEATH            BIT(5)      /* character dies             */
#define MTRIG_GREET            BIT(6)      /* something enters room seen */
#define MTRIG_GREET_ALL        BIT(7)      /* anything enters room       */
#define MTRIG_ENTRY            BIT(8)      /* the mob enters a room      */
#define MTRIG_RECEIVE          BIT(9)      /* character is given obj     */
#define MTRIG_FIGHT            BIT(10)     /* each pulse while fighting  */
#define MTRIG_HITPRCNT         BIT(11)     /* fighting and below some hp */
#define MTRIG_BRIBE	           BIT(12)     /* coins are given to mob     */
#define MTRIG_LOAD             BIT(13)     /* the mob is loaded          */
#define MTRIG_MEMORY           BIT(14)     /* mob see's someone remembered */
#define MTRIG_ABILITY          BIT(15)     /* mob targetted by ability     */
#define MTRIG_LEAVE            BIT(16)     /* someone leaves room seen   */
#define MTRIG_DOOR             BIT(17)     /* door manipulated in room   */
#define MTRIG_LEAVE_ALL        BIT(18)	// leave even if they can't see
#define MTRIG_FIGHT_CHARMED    BIT(19)	// fight trigger that fires even while charmed


/* obj trigger types */
#define OTRIG_GLOBAL           BIT(0)	     /* unused                     */
#define OTRIG_RANDOM           BIT(1)	     /* checked randomly           */
#define OTRIG_COMMAND          BIT(2)      /* character types a command  */

#define OTRIG_TIMER            BIT(5)     /* item's timer expires       */
#define OTRIG_GET              BIT(6)     /* item is picked up          */
#define OTRIG_DROP             BIT(7)     /* character trys to drop obj */
#define OTRIG_GIVE             BIT(8)     /* character trys to give obj */
#define OTRIG_WEAR             BIT(9)     /* character trys to wear obj */
#define OTRIG_REMOVE           BIT(11)    /* character trys to remove obj */

#define OTRIG_LOAD             BIT(13)    /* the object is loaded       */

#define OTRIG_ABILITY          BIT(15)    /* object targetted by ability */
#define OTRIG_LEAVE            BIT(16)    /* someone leaves room seen    */

#define OTRIG_CONSUME          BIT(18)    /* char tries to eat/drink obj */


// VTRIG_x: vehicle trigger types
#define VTRIG_GLOBAL  BIT(0)	// checked even if zone empty
#define VTRIG_RANDOM  BIT(1)	// checked randomly when people nearby
#define VTRIG_COMMAND  BIT(2)	// character types a command
#define VTRIG_SPEECH  BIT(3)	// character speaks a word or phrase
// unused BIT(4)
#define VTRIG_DESTROY  BIT(5)	// called before destruction
#define VTRIG_GREET  BIT(6)	// character enters the room
// unused BIT(7)
#define VTRIG_ENTRY  BIT(8)	// vehicle enters a room
// unused BIT(9), BIT(10), BIT(11), BIT(12)
#define VTRIG_LOAD  BIT(13)	// vehicle is loaded
// unused BIT(14), BIT(15)
#define VTRIG_LEAVE  BIT(16)	// someone leaves the room


/* wld trigger types */
#define WTRIG_GLOBAL           BIT(0)      /* check even if zone empty   */
#define WTRIG_RANDOM           BIT(1)	     /* checked randomly           */
#define WTRIG_COMMAND          BIT(2)	     /* character types a command  */
#define WTRIG_SPEECH           BIT(3)      /* a char says word/phrase    */
#define WTRIG_ADVENTURE_CLEANUP  BIT(4)	// called on a map tile or room after an adventure cleans up
#define WTRIG_RESET            BIT(5)      /* zone has been reset        */
#define WTRIG_ENTER            BIT(6)	     /* character enters room      */
#define WTRIG_DROP             BIT(7)      /* something dropped in room  */

#define WTRIG_ABILITY          BIT(15)     /* ability used in room */
#define WTRIG_LEAVE            BIT(16)     /* character leaves the room */
#define WTRIG_DOOR             BIT(17)     /* door manipulated in room  */


/* obj command trigger types */
#define OCMD_EQUIP             BIT(0)	     /* obj must be in char's equip */
#define OCMD_INVEN             BIT(1)	     /* obj must be in char's inven */
#define OCMD_ROOM              BIT(2)	     /* obj must be in char's room  */

/* obj consume trigger commands */
#define OCMD_EAT    1
#define OCMD_DRINK  2
#define OCMD_QUAFF  3

#define TRIG_NEW                0	     /* trigger starts from top  */
#define TRIG_RESTART            1	     /* trigger restarting       */


#define MAX_SCRIPT_DEPTH      10          /* maximum depth triggers can recurse into each other */


// used for command triggers
#define CMDTRG_EXACT  0
#define CMDTRG_ABBREV  1


/* one line of the trigger */
struct cmdlist_element {
	char *cmd;				/* one line of a trigger */
	struct cmdlist_element *original;
	struct cmdlist_element *next;
};

struct trig_var_data {
	char *name;				/* name of variable  */
	char *value;				/* value of variable */
	long context;				/* 0: global context */

	struct trig_var_data *next;
};

/* structure for triggers */
struct trig_data {
	trig_vnum vnum;	// trigger's vnum
	int attach_type;			/* mob/obj/wld intentions          */
	int data_type;		        /* type of game_data for trig      */
	char *name;			        /* name of trigger                 */
	bitvector_t trigger_type;			/* type of trigger (for bitvector) */
	struct cmdlist_element *cmdlist;	/* top of command list             */
	struct cmdlist_element *curr_state;	/* ptr to current line of trigger  */
	int narg;				/* numerical argument              */
	char *arglist;			/* argument list                   */
	int depth;				/* depth into nest ifs/whiles/etc  */
	int loops;				/* loop iteration counter          */
	struct event *wait_event;   	/* event to pause the trigger      */
	ubyte purged;			/* trigger is set to be purged     */
	struct trig_var_data *var_list;	/* list of local vars for trigger  */

	struct trig_data *next;  
	struct trig_data *next_in_world;    /* next in the global trigger list */
	
	UT_hash_handle hh;	// trigger_table hash handle
};


/* a complete script (composed of several triggers) */
struct script_data {
	bitvector_t types;				/* bitvector of trigger types */
	struct trig_data *trig_list;	        /* list of triggers           */
	struct trig_var_data *global_vars;	/* list of global variables   */
	ubyte purged;				/* script is set to be purged */
	long context;				/* current context for statics */

	struct script_data *next;		/* used for purged_scripts    */
};

/* The event data for the wait command */
struct wait_event_data {
	struct trig_data *trigger;
	void *go;
	int type;
};

/* used for actor memory triggers */
struct script_memory {
	int id;				/* id of who to remember */
	char *cmd;				/* command, or NULL for generic */
	struct script_memory *next;
};


/* function prototypes from triggers.c (and others) */
void adventure_cleanup_wtrigger(room_data *room);
void act_mtrigger(const char_data *ch, char *str, char_data *actor, char_data *victim, obj_data *object, obj_data *target, char *arg);  
void speech_mtrigger(char_data *actor, char *str);
void speech_wtrigger(char_data *actor, char *str);
void greet_memory_mtrigger(char_data *ch);
int greet_mtrigger(char_data *actor, int dir);
int entry_mtrigger(char_data *ch);
void entry_memory_mtrigger(char_data *ch);
int enter_wtrigger(room_data *room, char_data *actor, int dir);
int drop_otrigger(obj_data *obj, char_data *actor);
int timer_otrigger(obj_data *obj);
int get_otrigger(obj_data *obj, char_data *actor);
int drop_wtrigger(obj_data *obj, char_data *actor);
int give_otrigger(obj_data *obj, char_data *actor, char_data *victim);
int receive_mtrigger(char_data *ch, char_data *actor, obj_data *obj);
int wear_otrigger(obj_data *obj, char_data *actor, int where);
int remove_otrigger(obj_data *obj, char_data *actor);
int command_mtrigger(char_data *actor, char *cmd, char *argument, int mode);
int command_otrigger(char_data *actor, char *cmd, char *argument, int mode);
int command_wtrigger(char_data *actor, char *cmd, char *argument, int mode);
bool check_command_trigger(char_data *actor, char *cmd, char *argument, int mode);
int death_mtrigger(char_data *ch, char_data *actor);
void fight_mtrigger(char_data *ch);
void hitprcnt_mtrigger(char_data *ch);
void bribe_mtrigger(char_data *ch, char_data *actor, int amount);

void random_mtrigger(char_data *ch);
void random_otrigger(obj_data *obj);
void random_wtrigger(room_data *ch);
void reset_wtrigger(room_data *ch);

void load_mtrigger(char_data *ch);
void load_otrigger(obj_data *obj);

int ability_mtrigger(char_data *actor, char_data *ch, any_vnum abil);
int ability_otrigger(char_data *actor, obj_data *obj, any_vnum abil);
int ability_wtrigger(char_data *actor, char_data *vict, obj_data *obj, any_vnum abil);

int leave_mtrigger(char_data *actor, int dir);
int leave_wtrigger(room_data *room, char_data *actor, int dir);
int leave_otrigger(room_data *room, char_data *actor, int dir);

int door_mtrigger(char_data *actor, int subcmd, int dir);
int door_wtrigger(char_data *actor, int subcmd, int dir);

int consume_otrigger(obj_data *obj, char_data *actor, int cmd);

int command_vtrigger(char_data *actor, char *cmd, char *argument, int mode);
int destroy_vtrigger(vehicle_data *veh);
int entry_vtrigger(vehicle_data *veh);
int leave_vtrigger(char_data *actor, int dir);
void load_vtrigger(vehicle_data *veh);
int greet_vtrigger(char_data *actor, int dir);
void random_vtrigger(vehicle_data *veh);
void speech_vtrigger(char_data *actor, char *str);

/* function prototypes from scripts.c */
void script_trigger_check(void);
void add_trigger(struct script_data *sc, trig_data *t, int loc);
char_data *get_char(char *name);
char_data *get_char_by_obj(obj_data *obj, char *name);
empire_data *get_empire(char *name);
obj_data *get_obj(char *name);

void do_stat_trigger(char_data *ch, trig_data *trig);
void do_sstat_room(char_data *ch);
void do_sstat_object(char_data *ch, obj_data *j);
void do_sstat_character(char_data *ch, char_data *k);

void script_vlog(const char *format, va_list args);
void script_log(const char *format, ...) __attribute__ ((format (printf, 1, 2)));
void mob_log(char_data *mob, const char *format, ...) __attribute__ ((format (printf, 2, 3)));
void obj_log(obj_data *obj, const char *format, ...) __attribute__ ((format (printf, 2, 3)));
void wld_log(room_data *room, const char *format, ...) __attribute__ ((format (printf, 2, 3)));
void dg_read_trigger(char *line, void *proto, int type);
void dg_obj_trigger(char *line, obj_data *obj);
void assign_triggers(void *i, int type);
void parse_trigger(FILE *trig_f, int nr);
extern trig_data *real_trigger(trig_vnum vnum);
void extract_script(void *thing, int type);
void extract_script_mem(struct script_memory *sc);
void free_proto_script(void *thing, int type);
void free_script(void *thing, int type);
void free_trigger(trig_data *trig);
void copy_proto_script(void *source, void *dest, int type);
void copy_script(void *source, void *dest, int type);
void trig_data_copy(trig_data *this_data, const trig_data *trg);

trig_data *read_trigger(int nr);
void add_var(struct trig_var_data **var_list, char *name, char *value, int id);
room_data *dg_room_of_obj(obj_data *obj);
room_data *do_dg_add_room_dir(room_data *from, int dir, bld_data *bld);
void do_dg_affect(void *go, struct script_data *sc, trig_data *trig, int type, char *cmd);
void script_damage(char_data *vict, char_data *killer, int level, int dam_type, double modifier);
void script_damage_over_time(char_data *vict, int level, int dam_type, double modifier, int dur_seconds, int max_stacks, char_data *cast_by);

void extract_value(struct script_data *sc, trig_data *trig, char *cmd);

/* To maintain strict-aliasing we'll have to do this trick with a union */
union script_driver_data_u {
	char_data *c;
	room_data *r;
	obj_data *o;
	vehicle_data *v;
};
int script_driver(union script_driver_data_u *sdd, trig_data *trig, int type, int mode);
//int script_driver(void *go, trig_data *trig, int type, int mode);

int trgvar_in_room(room_vnum vnum);
struct cmdlist_element *find_done(struct cmdlist_element *cl);
struct cmdlist_element * find_case(trig_data *trig, struct cmdlist_element *cl, void *go, struct script_data *sc, int type, char *cond);
int find_eq_pos_script(char *arg);
char_data *get_char_near_obj(obj_data *obj, char *name);
obj_data *get_obj_near_obj(obj_data *obj, char *name);
char_data *get_char_near_vehicle(vehicle_data *veh, char *name);
obj_data *get_obj_near_vehicle(vehicle_data *veh, char *name);


/* defines for valid_dg_target */
int valid_dg_target(char_data *ch, int bitvector);
#define DG_ALLOW_GODS BIT(0)

/* Macros for scripts */

// #define UID_CHAR   '\x1b'
#define UID_CHAR   '}'
#define GET_TRIG_NAME(t)          ((t)->name)
#define GET_TRIG_VNUM(t)	  ((t)->vnum)
#define GET_TRIG_TYPE(t)          ((t)->trigger_type)
#define GET_TRIG_DATA_TYPE(t)	  ((t)->data_type)
#define GET_TRIG_NARG(t)          ((t)->narg)
#define GET_TRIG_ARG(t)           ((t)->arglist)
#define GET_TRIG_VARS(t)	  ((t)->var_list)
#define GET_TRIG_WAIT(t)	  ((t)->wait_event)
#define GET_TRIG_DEPTH(t)         ((t)->depth)
#define GET_TRIG_LOOPS(t)         ((t)->loops)

/* player id's: 0 to MOB_ID_BASE - 1            */
/* mob id's: MOB_ID_BASE to EMPIRE_ID_BASE - 1      */
// empire ids: EMPIRE_ID_BASE to ROOM_ID_BASE - 1
/* room id's: ROOM_ID_BASE to OBJ_ID_BASE - 1    */
/* object id's: OBJ_ID_BASE and higher           */
#define MOB_ID_BASE	  10000000  /* 10000000 player IDNUMS should suffice */
#define EMPIRE_ID_BASE  (10000000 + MOB_ID_BASE) /* 10000000 Mobs */
#define ROOM_ID_BASE    (10000000 + EMPIRE_ID_BASE)	// 10000000 empire id limit?
#define VEHICLE_ID_BASE  ((MAP_SIZE * 5) + ROOM_ID_BASE)	// Lots o' Rooms
#define OBJ_ID_BASE  (1000000 + VEHICLE_ID_BASE)	// Plenty o' Vehicles

#define SCRIPT(o)		  ((o)->script)
#define SCRIPT_MEM(c)             ((c)->memory)

#define SCRIPT_TYPES(s)		  ((s)->types)				  
#define TRIGGERS(s)		  ((s)->trig_list)

#define GET_SHORT(ch)    ((ch)->player.short_descr)


#define SCRIPT_CHECK(go, type)   (SCRIPT(go) && IS_SET(SCRIPT_TYPES(SCRIPT(go)), type))
#define TRIGGER_CHECK(t, type)   (IS_SET(GET_TRIG_TYPE(t), type) && !GET_TRIG_DEPTH(t))

#define ADD_UID_VAR(buf, trig, go, name, context) do { \
			sprintf(buf, "%c%d", UID_CHAR, GET_ID(go)); \
			add_var(&GET_TRIG_VARS(trig), name, buf, context); } while (0)

#define ADD_ROOM_UID_VAR(buf, trig, go, name, context) do { \
			sprintf(buf, "%c%d", UID_CHAR, GET_ROOM_VNUM(go) + ROOM_ID_BASE); \
			add_var(&GET_TRIG_VARS(trig), name, buf, context); } while (0)


#define ABILITY_TRIGGERS(actor, vict, obj, abil)  (!ability_wtrigger((actor), (vict), (obj), (abil)) || !ability_mtrigger((actor), (vict), (abil)) || !ability_otrigger((actor), (obj), (abil)))

/* needed for new %load% handling */
int can_wear_on_pos(obj_data *obj, int pos);

/* find_char helpers */
void init_lookup_table(void);
char_data *find_char_by_uid_in_lookup_table(int uid);
obj_data *find_obj_by_uid_in_lookup_table(int uid);
vehicle_data *find_vehicle_by_uid_in_lookup_table(int uid);
void add_to_lookup_table(int uid, void *c);
void remove_from_lookup_table(int uid);

// find helpers
extern char_data *find_char(int n);
extern empire_data *find_empire_by_uid(int n);
extern room_data *find_room(int n);

// purge helpers
extern int dg_owner_purged;
extern char_data *dg_owner_mob;
extern obj_data *dg_owner_obj;
extern vehicle_data *dg_owner_veh;
extern room_data *dg_owner_room;
