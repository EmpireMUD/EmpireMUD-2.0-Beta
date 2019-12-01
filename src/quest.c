/* ************************************************************************
*   File: quest.c                                         EmpireMUD 2.0b5 *
*  Usage: quest loading, saving, OLC, and processing                      *
*                                                                         *
*  EmpireMUD code base by Paul Clarke, (C) 2000-2015                      *
*  All rights reserved.  See license.doc for complete information.        *
*                                                                         *
*  EmpireMUD based upon CircleMUD 3.0, bpl 17, by Jeremy Elson.           *
*  CircleMUD (C) 1993, 94 by the Trustees of the Johns Hopkins University *
*  CircleMUD is based on DikuMUD, Copyright (C) 1990, 1991.               *
************************************************************************ */

#include <math.h>

#include "conf.h"
#include "sysdep.h"

#include "structs.h"
#include "utils.h"
#include "interpreter.h"
#include "db.h"
#include "comm.h"
#include "olc.h"
#include "skills.h"
#include "handler.h"
#include "dg_scripts.h"
#include "vnums.h"

/**
* Contents:
*   Helpers
*   Lookup Handlers
*   Quest Handlers
*   Utilities
*   Database
*   OLC Handlers
*   Displays
*   Edit Modules
*/

// local data
const char *default_quest_name = "Unnamed Quest";
const char *default_quest_description = "This quest has no description.\r\n";
const char *default_quest_complete_msg = "You have completed the quest.\r\n";

// external consts
extern const char *action_bits[];
extern const char *quest_flags[];
extern const char *quest_giver_types[];
extern const char *quest_reward_types[];
extern const bool requirement_amt_type[];
extern const char *requirement_types[];
extern const char *olc_type_bits[NUM_OLC_TYPES+1];

// external funcs
extern int count_cities(empire_data *emp);
extern int count_diplomacy(empire_data *emp, bitvector_t dip_flags);
extern struct req_data *copy_requirements(struct req_data *from);
extern bool delete_requirement_from_list(struct req_data **list, int type, any_vnum vnum);
void drop_quest(char_data *ch, struct player_quest *pq);
extern struct instance_data *find_instance_by_room(room_data *room, bool check_homeroom, bool allow_fake_loc);
extern bool find_requirement_in_list(struct req_data *list, int type, any_vnum vnum);
extern struct instance_data *get_instance_by_id(any_vnum instance_id);
void get_requirement_display(struct req_data *list, char *save_buffer);
void get_script_display(struct trig_proto_list *list, char *save_buffer);
extern room_data *obj_room(obj_data *obj);
void olc_process_requirements(char_data *ch, char *argument, struct req_data **list, char *command, bool allow_tracker_types);
extern char *requirement_string(struct req_data *req, bool show_vnums);

// external vars

// local protos
void add_quest_lookup(struct quest_lookup **list, quest_data *quest);
void add_to_quest_temp_list(struct quest_temp_list **list, quest_data *quest, struct instance_data *instance);
bool char_meets_prereqs(char_data *ch, quest_data *quest, struct instance_data *instance);
int count_owned_buildings(empire_data *emp, bld_vnum vnum);
int count_owned_homes(empire_data *emp);
int count_owned_vehicles(empire_data *emp, any_vnum vnum);
int count_owned_vehicles_by_flags(empire_data *emp, bitvector_t flags);
void count_quest_tasks(struct req_data *list, int *complete, int *total);
bool find_quest_giver_in_list(struct quest_giver *list, int type, any_vnum vnum);
void free_player_quests(struct player_quest *list);
void free_quest_givers(struct quest_giver *list);
void free_quest_temp_list(struct quest_temp_list *list);
struct player_completed_quest *has_completed_quest_any(char_data *ch, any_vnum quest);
struct player_completed_quest *has_completed_quest(char_data *ch, any_vnum quest, int instance_id);
struct player_quest *is_on_quest(char_data *ch, any_vnum quest);
bool remove_quest_lookup(struct quest_lookup **list, quest_data *quest);
void update_mob_quest_lookups(mob_vnum vnum);
void update_obj_quest_lookups(obj_vnum vnum);
void update_veh_quest_lookups(any_vnum vnum);
void write_daily_quest_file();


 //////////////////////////////////////////////////////////////////////////////
//// HELPERS /////////////////////////////////////////////////////////////////

/**
* Searches for a quest giver at a particular location. This does NOT guarantee
* the quest is complete or completable.
*
* @param char_data *ch The person looking.
* @param room_data *loc The location to check.
* @param quest_data *quest The quest to check.
* @param empire_data **giver_emp Somewhere to store the loyalty of the quest-giver.
* @return bool TRUE if ch can turn it in there; FALASE if not.
*/
bool can_turn_in_quest_at(char_data *ch, room_data *loc, quest_data *quest, empire_data **giver_emp) {
	struct quest_giver *giver;
	vehicle_data *veh;
	char_data *mob;
	obj_data *obj;
	
	*giver_emp = NULL;
	
	LL_FOREACH(QUEST_ENDS_AT(quest), giver) {
		// QG_x: find quest giver here
		switch (giver->type) {
			case QG_BUILDING: {
				if (GET_BUILDING(loc) && GET_BLD_VNUM(GET_BUILDING(loc)) == giver->vnum) {
					*giver_emp = ROOM_OWNER(loc);
					return TRUE;
				}
				break;
			}
			case QG_MOBILE: {
				LL_FOREACH2(ROOM_PEOPLE(IN_ROOM(ch)), mob, next_in_room) {
					if (IS_NPC(mob) && GET_MOB_VNUM(mob) == giver->vnum && CAN_SEE(ch, mob)) {
						*giver_emp = GET_LOYALTY(mob);
						return TRUE;
					}
				}
				break;
			}
			case QG_OBJECT: {
				LL_FOREACH2(ch->carrying, obj, next_content) {
					if (GET_OBJ_VNUM(obj) == giver->vnum && CAN_SEE_OBJ(ch, obj)) {
						*giver_emp = GET_LOYALTY(ch);
						return TRUE;
					}
				}
				LL_FOREACH2(ROOM_CONTENTS(loc), obj, next_content) {
					if (GET_OBJ_VNUM(obj) == giver->vnum && CAN_SEE_OBJ(ch, obj)) {
						*giver_emp = CAN_WEAR(obj, ITEM_WEAR_TAKE) ? GET_LOYALTY(ch) : ROOM_OWNER(loc);
						return TRUE;
					}
				}
			}
			case QG_ROOM_TEMPLATE: {
				if (GET_ROOM_TEMPLATE(loc) && GET_RMT_VNUM(GET_ROOM_TEMPLATE(loc)) == giver->vnum) {
					*giver_emp = ROOM_OWNER(loc);
					return TRUE;
				}
				break;
			}
			case QG_VEHICLE: {
				LL_FOREACH2(ROOM_VEHICLES(IN_ROOM(ch)), veh, next_in_room) {
					if (VEH_VNUM(veh) == giver->vnum && CAN_SEE_VEHICLE(ch, veh)) {
						*giver_emp = VEH_OWNER(veh);
						return TRUE;
					}
				}
				break;
			}
			// case QG_TRIGGER: never local
			// case QG_QUEST: never local
		}
	}
	
	// nope
	return FALSE;
}


/**
* Copies progress from one list to another list, if they have any common
* entries. This is useful is tasks are changed on a quest, but players are
* already on the quest.
*
* @param struct req_data *to_list List to copy progress TO.
* @param struct req_data *from_list List to copy progress FROM.
*/
void copy_quest_progress(struct req_data *to_list, struct req_data *from_list) {
	struct req_data *to_iter, *from_iter;
	
	LL_FOREACH(to_list, to_iter) {
		LL_FOREACH(from_list, from_iter) {
			if (to_iter->type != from_iter->type) {
				continue;
			}
			if (to_iter->vnum != from_iter->vnum) {
				continue;
			}
			if (to_iter->misc != from_iter->misc) {
				continue;
			}
			
			// seems to be the same
			to_iter->current = MAX(to_iter->current, from_iter->current);
		}
	}
}


/**
* Counts how many different crops a list of items has, based on the <plants>
* field and the PLANTABLE flag.
*
* @param obj_data *list The set of items to search for crops.
*/
int count_crop_variety_in_list(obj_data *list) {
	obj_data *obj;
	any_vnum vnum;
	int count = 0;
	
	// helper type
	struct tmp_crop_data {
		any_vnum crop;
		UT_hash_handle hh;
	};
	struct tmp_crop_data *tcd, *next_tcd, *hash = NULL;
	
	LL_FOREACH2(list, obj, next_content) {
		if (!OBJ_FLAGGED(obj, OBJ_PLANTABLE)) {
			continue;
		}
		
		vnum = GET_OBJ_VAL(obj, VAL_FOOD_CROP_TYPE);
		HASH_FIND_INT(hash, &vnum, tcd);
		if (!tcd) {
			++count;	// found a unique
			CREATE(tcd, struct tmp_crop_data, 1);
			tcd->crop = vnum;
			HASH_ADD_INT(hash, crop, tcd);
		}
		// else: not unique
	}
	
	// free temporary data
	HASH_ITER(hh, hash, tcd, next_tcd) {
		HASH_DEL(hash, tcd);
		free(tcd);
	}
	
	return count;
}


/**
* Number of buildings owned by an empire.
*
* @param empire_data *emp Any empire.
* @param bld_vnum vnum The building to search for.
* @return int The number of completed buildings with that vnum, owned by emp.
*/
int count_owned_buildings(empire_data *emp, bld_vnum vnum) {
	struct empire_territory_data *ter, *next_ter;
	int count = 0;	// ah ah ah
	
	if (!emp || vnum == NOTHING) {
		return count;
	}
	
	HASH_ITER(hh, EMPIRE_TERRITORY_LIST(emp), ter, next_ter) {
		if (!IS_COMPLETE(ter->room) || !GET_BUILDING(ter->room)) {
			continue;
		}
		if (GET_BLD_VNUM(GET_BUILDING(ter->room)) != vnum) {
			continue;
		}
		
		// found
		++count;
	}
	
	return count;
}


/**
* Number of buildings owned by an empire that have a specific set of function flag(s).
*
* @param empire_data *emp Any empire.
* @param bitvector_t flags One or more flags to check for (if there's more than one flag, it has to have ALL of them).
* @return int The number of completed buildings with the flag(s), owned by emp.
*/
int count_owned_buildings_by_function(empire_data *emp, bitvector_t flags) {
	struct empire_territory_data *ter, *next_ter;
	int count = 0;	// ah ah ah
	
	if (!emp || flags == NOBITS) {
		return count;
	}
	
	HASH_ITER(hh, EMPIRE_TERRITORY_LIST(emp), ter, next_ter) {
		if (!IS_COMPLETE(ter->room) || !GET_BUILDING(ter->room)) {
			continue;
		}
		if ((GET_BLD_FUNCTIONS(GET_BUILDING(ter->room)) & flags) != flags) {
			continue;
		}
		
		// found
		++count;
	}
	
	return count;
}


/**
* Number of citizen homes owned by an empire.
*
* @param empire_data *emp Any empire.
* @return int The number of completed homes owned by emp.
*/
int count_owned_homes(empire_data *emp) {
	struct empire_territory_data *ter, *next_ter;
	int count = 0;	// ah ah ah
	
	if (!emp) {
		return count;
	}
	
	HASH_ITER(hh, EMPIRE_TERRITORY_LIST(emp), ter, next_ter) {
		if (!IS_COMPLETE(ter->room) || !GET_BUILDING(ter->room)) {
			continue;	// must be a completed building
		}
		if (ROOM_BLD_FLAGGED(ter->room, BLD_ROOM)) {
			continue;	// don't count interiors
		}
		if (GET_BLD_CITIZENS(GET_BUILDING(ter->room)) < 1) {
			continue;	// must have a citizen
		}
		
		// found
		++count;
	}
	
	return count;
}


/**
* Number of sector tiles owned by an empire.
*
* @param empire_data *emp Any empire.
* @param sector_vnum vnum The sector to search for.
* @return int The number of tiles with that sector vnum, owned by emp.
*/
int count_owned_sector(empire_data *emp, sector_vnum vnum) {
	room_data *room, *next_room;
	int count = 0;	// ah ah ah
	
	if (!emp || vnum == NOTHING) {
		return count;
	}
	
	HASH_ITER(hh, world_table, room, next_room) {
		if (ROOM_OWNER(room) == emp && GET_SECT_VNUM(SECT(room)) == vnum) {
			++count;
		}
	}
	
	return count;
}


/**
* Number of vehicles owned by an empire.
*
* @param empire_data *emp Any empire.
* @param any_vnum vnum The vehicle to search for.
* @return int The number of completed vehicles with that vnum, owned by emp.
*/
int count_owned_vehicles(empire_data *emp, any_vnum vnum) {
	vehicle_data *veh;
	int count = 0;
	
	if (!emp || vnum == NOTHING) {
		return count;
	}
	
	LL_FOREACH(vehicle_list, veh) {
		if (!VEH_IS_COMPLETE(veh) || VEH_OWNER(veh) != emp) {
			continue;
		}
		if (VEH_VNUM(veh) != vnum) {
			continue;
		}
		
		// found
		++count;
	}
	
	return count;
}


/**
* Number of vehicles with specific flag(s) owned by an empire.
*
* @param empire_data *emp Any empire.
* @param bitvector_t flags The flag(s) to match (all flags must be present).
* @return int The number of completed vehicles that match, owned by emp.
*/
int count_owned_vehicles_by_flags(empire_data *emp, bitvector_t flags) {
	vehicle_data *veh;
	int count = 0;
	
	if (!emp || flags == NOBITS) {
		return count;
	}
	
	LL_FOREACH(vehicle_list, veh) {
		if (!VEH_IS_COMPLETE(veh) || VEH_OWNER(veh) != emp) {
			continue;
		}
		if ((VEH_FLAGS(veh) & flags) != flags) {
			continue;
		}
		
		// found
		++count;
	}
	
	return count;
}


/**
* Counts how many components a player has available for a quest.
*
* @param char_data *ch The player.
* @param int type CMP_ type
* @param bitvector_t flags CMPF_ flags to match
* @param bool skip_keep If TRUE, skips items marked (keep) because they can't be taken.
* @return int The number of matching component items.
*/
int count_quest_components(char_data *ch, int type, bitvector_t flags, bool skip_keep) {
	obj_data *obj;
	int count = 0;
	
	LL_FOREACH2(ch->carrying, obj, next_content) {
		if (GET_OBJ_CMP_TYPE(obj) != type) {
			continue;
		}
		if ((GET_OBJ_CMP_FLAGS(obj) & flags) != flags) {
			continue;
		}
		if (skip_keep && OBJ_FLAGGED(obj, OBJ_KEEP)) {
			continue;
		}
		
		++count;
	}
	
	return count;
}


/**
* Counts how many items a player has available for a quest.
*
* @param char_data *ch The player.
* @param obj_vnum vnum The vnum of the item to look for.
* @param bool skip_keep If TRUE, skips items marked (keep) because they can't be taken.
* @return int The number of matching items.
*/
int count_quest_objects(char_data *ch, obj_vnum vnum, bool skip_keep) {
	obj_data *obj;
	int count = 0;
	
	LL_FOREACH2(ch->carrying, obj, next_content) {
		if (GET_OBJ_VNUM(obj) != vnum) {
			continue;
		}
		if (skip_keep && OBJ_FLAGGED(obj, OBJ_KEEP)) {
			continue;
		}
		
		++count;
	}
	
	return count;
}


/**
* Ends all quests that are marked QST_EXPIRES_AFTER_INSTANCE.
*
* @param struct instance_data *inst The instance to check quests for.
*/
void expire_instance_quests(struct instance_data *inst) {
	struct player_quest *pq, *next_pq;
	descriptor_data *desc;
	quest_data *quest;
	char_data *ch;
	
	LL_FOREACH(descriptor_list, desc) {
		if (STATE(desc) != CON_PLAYING || !(ch = desc->character) || IS_NPC(ch)) {
			continue;
		}
		
		LL_FOREACH_SAFE(GET_QUESTS(ch), pq, next_pq) {
			if (pq->instance_id != INST_ID(inst) || pq->adventure != GET_ADV_VNUM(INST_ADVENTURE(inst))) {
				continue;
			}
			if (!(quest = quest_proto(pq->vnum))) {
				continue;
			}
			if (!QUEST_FLAGGED(quest, QST_EXPIRES_AFTER_INSTANCE)) {
				continue;
			}
			
			msg_to_char(ch, "You fail %s because the adventure instance ended.\r\n", QUEST_NAME(quest));
			drop_quest(ch, pq);
		}
	}
}


/**
* For REQ_CROP_VARIETY on quest tasks, takes away 1 of each crop.
*
* @param char_data *ch The person whose inventory has the crops.
* @param int amount How many unique crops are needed.
*/
void extract_crop_variety(char_data *ch, int amount) {
	obj_data *obj, *next_obj;
	any_vnum vnum;
	int count = 0;
	
	// helper type
	struct tmp_crop_data {
		any_vnum crop;
		UT_hash_handle hh;
	};
	struct tmp_crop_data *tcd, *next_tcd, *hash = NULL;
	
	LL_FOREACH_SAFE2(ch->carrying, obj, next_obj, next_content) {
		if (!OBJ_FLAGGED(obj, OBJ_PLANTABLE)) {
			continue;
		}
		
		vnum = GET_OBJ_VAL(obj, VAL_FOOD_CROP_TYPE);
		HASH_FIND_INT(hash, &vnum, tcd);
		if (!tcd) {
			++count;	// found a unique
			CREATE(tcd, struct tmp_crop_data, 1);
			tcd->crop = vnum;
			HASH_ADD_INT(hash, crop, tcd);
			
			extract_obj(obj);
		}
		// else: not unique
		
		if (count >= amount) {
			break;	// done!
		}
	}
	
	// free temporary data
	HASH_ITER(hh, hash, tcd, next_tcd) {
		HASH_DEL(hash, tcd);
		free(tcd);
	}
}


/**
* Quick way to turn a vnum into a name, safely.
*
* @param any_vnum vnum The quest vnum to look up.
* @return char* A name for the vnum, or "UNKNOWN".
*/
char *get_quest_name_by_proto(any_vnum vnum) {
	quest_data *proto = quest_proto(vnum);
	return proto ? QUEST_NAME(proto) : "UNKNOWN";
}


/**
* Builds the tracker display for requirements.
*
* @param struct req_data *tracker The tracker to show.
* @param char *save_buffer The string to save it to.
*/
void get_tracker_display(struct req_data *tracker, char *save_buffer) {
	extern const bool requirement_amt_type[];
	
	int lefthand, count = 0, sub = 0;
	char buf[MAX_STRING_LENGTH];
	struct req_data *task;
	char last_group = 0;
	
	*save_buffer = '\0';
	
	LL_FOREACH(tracker, task) {
		if (last_group != task->group) {
			if (task->group) {
				sprintf(save_buffer + strlen(save_buffer), "  %sAll of:\r\n", (count > 0 ? "or " : ""));
			}
			last_group = task->group;
			sub = 0;
		}
		
		++count;	// total iterations
		++sub;	// iterations inside this sub-group
		
		// REQ_AMT_x: display based on amount type
		switch (requirement_amt_type[task->type]) {
			case REQ_AMT_NUMBER: {
				lefthand = task->current;
				lefthand = MIN(lefthand, task->needed);	// may be above the amount needed
				lefthand = MAX(0, lefthand);	// in some cases, current may be negative
				sprintf(buf, " (%d/%d)", lefthand, task->needed);
				break;
			}
			case REQ_AMT_REPUTATION:
			case REQ_AMT_THRESHOLD:
			case REQ_AMT_NONE: {
				if (task->current >= task->needed) {
					strcpy(buf, " (complete)");
				}
				else {
					strcpy(buf, " (not complete)");
				}
				break;
			}
			default: {
				*buf = '\0';
				break;
			}
		}
		sprintf(save_buffer + strlen(save_buffer), "  %s%s%s%s\r\n", (task->group ? "  " : ""), ((sub > 1 && !task->group) ? "or " : ""), requirement_string(task, FALSE), buf);
	}
}


/**
* Gives out the actual quest rewards. This is also used by the event system.
*
* @param char_data *ch The person receiving the reward.
* @param struct quest_reward *list The list of 0 or more quest rewards.
* @param int reward_level The scale level of any rewards (pass get_approximate_level(ch) if you're not sure what to pass).
* @param empire_data *quest_giver_emp Optional: If applicable, the empire that gave the quest. (For events, this can be the player's own empire.)
* @param int instance_id Optional: If the quest is associated with an instance, pass its id. Otherwise, 0 is fine.
*/
void give_quest_rewards(char_data *ch, struct quest_reward *list, int reward_level, empire_data *quest_giver_emp, int instance_id) {
	void clear_char_abilities(char_data *ch, any_vnum skill);
	void scale_item_to_level(obj_data *obj, int level);
	void start_quest(char_data *ch, quest_data *qst, struct instance_data *inst);
	
	char buf[MAX_STRING_LENGTH];
	struct quest_reward *reward;
	
	LL_FOREACH(list, reward) {
		// QR_x: reward the rewards
		switch (reward->type) {
			case QR_BONUS_EXP: {
				msg_to_char(ch, "\tyYou gain %d bonus experience point%s!\t0\r\n", reward->amount, PLURAL(reward->amount));
				SAFE_ADD(GET_DAILY_BONUS_EXPERIENCE(ch), reward->amount, 0, UCHAR_MAX, FALSE);
				break;
			}
			case QR_COINS: {
				empire_data *coin_emp = (reward->vnum == OTHER_COIN ? NULL : quest_giver_emp);
				msg_to_char(ch, "\tyYou receive %s!\t0\r\n", money_amount(coin_emp, reward->amount));
				increase_coins(ch, coin_emp, reward->amount);
				break;
			}
			case QR_CURRENCY: {
				generic_data *gen;
				if ((gen = find_generic(reward->vnum, GENERIC_CURRENCY))) {
					msg_to_char(ch, "\tyYou receive %d %s!\t0\r\n", reward->amount, reward->amount != 1 ? GET_CURRENCY_PLURAL(gen) : GET_CURRENCY_SINGULAR(gen));
					add_currency(ch, reward->vnum, reward->amount);
				}
				break;
			}
			case QR_OBJECT: {
				obj_data *obj = NULL;
				int iter;
				for (iter = 0; iter < reward->amount; ++iter) {
					obj = read_object(reward->vnum, TRUE);
					scale_item_to_level(obj, reward_level);
					if (CAN_WEAR(obj, ITEM_WEAR_TAKE)) {
						obj_to_char(obj, ch);
					}
					else {
						obj_to_room(obj, IN_ROOM(ch));
					}
					
					// ensure binding
					if (!IS_NPC(ch) && OBJ_FLAGGED(obj, OBJ_BIND_FLAGS)) {
						bind_obj_to_player(obj, ch);
						reduce_obj_binding(obj, ch);
					}
					
					load_otrigger(obj);
				}
				
				// mark gained
				if (GET_LOYALTY(ch)) {
					add_production_total(GET_LOYALTY(ch), reward->vnum, reward->amount);
				}
				
				if (reward->amount > 1) {
					snprintf(buf, sizeof(buf), "\tyYou receive $p (x%d)!\t0", reward->amount);
				}
				else {
					snprintf(buf, sizeof(buf), "\tyYou receive $p!\t0");
				}
				
				if (obj) {
					act(buf, FALSE, ch, obj, NULL, TO_CHAR);
				}
				break;
			}
			case QR_SET_SKILL: {
				int val = MAX(0, MIN(CLASS_SKILL_CAP, reward->amount));
				bool loss;
				
				loss = (val < get_skill_level(ch, reward->vnum));
				set_skill(ch, reward->vnum, val);
				
				msg_to_char(ch, "\tyYour %s is now level %d!\t0\r\n", get_skill_name_by_vnum(reward->vnum), val);
				
				if (loss) {
					clear_char_abilities(ch, reward->vnum);
				}
				
				break;
			}
			case QR_SKILL_EXP: {
				msg_to_char(ch, "\tyYou gain %s skill experience!\t0\r\n", get_skill_name_by_vnum(reward->vnum));
				gain_skill_exp(ch, reward->vnum, reward->amount);
				break;
			}
			case QR_SKILL_LEVELS: {
				if (gain_skill(ch, find_skill_by_vnum(reward->vnum), reward->amount)) {
					// sends its own message
					// msg_to_char(ch, "Your %s is now level %d!\r\n", get_skill_name_by_vnum(reward->vnum), get_skill_level(ch, reward->vnum));
				}
				break;
			}
			case QR_QUEST_CHAIN: {
				quest_data *start = quest_proto(reward->vnum);
				struct instance_data *inst = get_instance_by_id(instance_id);
				
				if (start && QUEST_FLAGGED(start, QST_TUTORIAL) && PRF_FLAGGED(ch, PRF_NO_TUTORIALS)) {
					break;	// player does not want tutorials to auto-chain
				}
				
				// shows nothing if the player doesn't qualify
				if (start && !is_on_quest(ch, reward->vnum) && char_meets_prereqs(ch, start, inst)) {
					if (!PRF_FLAGGED(ch, PRF_COMPACT)) {
						msg_to_char(ch, "\r\n");	// add some spacing
					}
					start_quest(ch, start, inst);
				}
				
				break;
			}
			case QR_REPUTATION: {
				gain_reputation(ch, reward->vnum, reward->amount, FALSE, TRUE);
				break;
			}
			case QR_EVENT_POINTS: {
				extern int gain_event_points(char_data *ch, any_vnum event_vnum, int points);
				gain_event_points(ch, reward->vnum, reward->amount);
				break;
			}
		}
	}
}


/**
* Gets standard string display like "4x lumber" for a quest giver (starts/ends
* at).
*
* @param struct quest_giver *giver The quest giver to show.
* @param bool show_vnums If TRUE, adds [1234] at the start of the string.
* @return char* The string display.
*/
char *quest_giver_string(struct quest_giver *giver, bool show_vnums) {
	static char output[256];
	char vnum[256];
	
	*output = '\0';
	if (!giver) {
		return output;
	}
	
	if (show_vnums) {
		snprintf(vnum, sizeof(vnum), "%s [%d] ", quest_giver_types[giver->type], giver->vnum);
	}
	else {
		*vnum = '\0';
	}
	
	// QG_x
	switch (giver->type) {
		case QG_BUILDING: {
			bld_data *bld = building_proto(giver->vnum);
			snprintf(output, sizeof(output), "%s%s", vnum, bld ? GET_BLD_NAME(bld) : "UNKNOWN");
			break;
		}
		case QG_MOBILE: {
			snprintf(output, sizeof(output), "%s%s", vnum, skip_filler(get_mob_name_by_proto(giver->vnum)));
			break;
		}
		case QG_OBJECT: {
			snprintf(output, sizeof(output), "%s%s", vnum, skip_filler(get_obj_name_by_proto(giver->vnum)));
			break;
		}
		case QG_ROOM_TEMPLATE: {
			room_template *rmt = room_template_proto(giver->vnum);
			snprintf(output, sizeof(output), "%s%s", vnum, rmt ? skip_filler(GET_RMT_TITLE(rmt)) : "UNKNOWN");
			break;
		}
		case QG_TRIGGER: {
			trig_data *trig = real_trigger(giver->vnum);
			snprintf(output, sizeof(output), "%s%s", vnum, trig ? skip_filler(GET_TRIG_NAME(trig)) : "UNKNOWN");
			break;
		}
		case QG_QUEST: {
			quest_data *qq = quest_proto(giver->vnum);
			snprintf(output, sizeof(output), "%s%s", vnum, qq ? skip_filler(QUEST_NAME(qq)) : "UNKNOWN");
			break;
		}
		case QG_VEHICLE: {
			snprintf(output, sizeof(output), "%s%s", vnum, skip_filler(get_vehicle_name_by_proto(giver->vnum)));
			break;
		}
		default: {
			snprintf(output, sizeof(output), "%d %sUNKNOWN", giver->type, vnum);
			break;
		}
	}
	
	return output;
}


/**
* Gets standard string display like "4x lumber" for a quest reward.
*
* @param struct quest_reward *reward The reward to show.
* @param bool show_vnums If TRUE, adds [1234] at the start of the string.
* @return char* The string display.
*/
char *quest_reward_string(struct quest_reward *reward, bool show_vnums) {
	static char output[256];
	char vnum[256];
	
	*output = '\0';
	if (!reward) {
		return output;
	}
	
	if (show_vnums) {
		snprintf(vnum, sizeof(vnum), "[%d] ", reward->vnum);
	}
	else {
		*vnum = '\0';
	}
	
	// QR_x
	switch (reward->type) {
		case QR_BONUS_EXP: {
			// has no vnum
			snprintf(output, sizeof(output), "%d bonus exp", reward->amount);
			break;
		}
		case QR_COINS: {
			// vnum not relevant
			snprintf(output, sizeof(output), "%d %s coin%s", reward->amount, reward->vnum == OTHER_COIN ? "misc" : "empire", PLURAL(reward->amount));
			break;
		}
		case QR_CURRENCY: {
			snprintf(output, sizeof(output), "%s%d %s", vnum, reward->amount, get_generic_string_by_vnum(reward->vnum, GENERIC_CURRENCY, WHICH_CURRENCY(reward->amount)));
			break;
		}
		case QR_OBJECT: {
			snprintf(output, sizeof(output), "%s%dx %s", vnum, reward->amount, skip_filler(get_obj_name_by_proto(reward->vnum)));
			break;
		}
		case QR_SET_SKILL: {
			snprintf(output, sizeof(output), "%s%d %s", vnum, reward->amount, get_skill_name_by_vnum(reward->vnum));
			break;
		}
		case QR_SKILL_EXP: {
			snprintf(output, sizeof(output), "%s%d%% %s", vnum, reward->amount, get_skill_name_by_vnum(reward->vnum));
			break;
		}
		case QR_SKILL_LEVELS: {
			snprintf(output, sizeof(output), "%s%dx %s", vnum, reward->amount, get_skill_name_by_vnum(reward->vnum));
			break;
		}
		case QR_QUEST_CHAIN: {
			snprintf(output, sizeof(output), "%s%s", vnum, get_quest_name_by_proto(reward->vnum));
			break;
		}
		case QR_REPUTATION: {
			faction_data *fct = find_faction_by_vnum(reward->vnum);
			snprintf(output, sizeof(output), "%s%+d rep to %s", vnum, reward->amount, (fct ? FCT_NAME(fct) : "UNKNOWN"));
			break;
		}
		case QR_EVENT_POINTS: {
			event_data *event = find_event_by_vnum(reward->vnum);
			snprintf(output, sizeof(output), "%+d event point%s to %s%s", reward->amount, PLURAL(reward->amount), vnum, (event ? EVT_NAME(event) : "UNKNOWN"));
			break;
		}
		default: {
			snprintf(output, sizeof(output), "%s%dx UNKNOWN", vnum, reward->amount);
			break;
		}
	}
	
	return output;
}


/**
* Redetects counts for any quest task type that CAN be redetected.
* (Some, like mob kills, cannot).
*
* @param char_data *ch The player.
* @param struct player_quest *pq The player's quest to refresh.
*/
void refresh_one_quest_tracker(char_data *ch, struct player_quest *pq) {
	quest_data *quest = quest_proto(pq->vnum);
	struct req_data *task;
	int iter;
	
	LL_FOREACH(pq->tracker, task) {
		// REQ_x: refreshable types only
		switch (task->type) {
			case REQ_COMPLETED_QUEST: {
				task->current = has_completed_quest(ch, task->vnum, pq->instance_id) ? task->needed : 0;
				break;
			}
			case REQ_GET_COMPONENT: {
				task->current = count_quest_components(ch, task->vnum, task->misc, QUEST_FLAGGED(quest, QST_EXTRACT_TASK_OBJECTS));
				break;
			}
			case REQ_GET_OBJECT: {
				task->current = count_quest_objects(ch, task->vnum, QUEST_FLAGGED(quest, QST_EXTRACT_TASK_OBJECTS));
				break;
			}
			case REQ_NOT_COMPLETED_QUEST: {
				task->current = has_completed_quest(ch, task->vnum, pq->instance_id) ? 0 : task->needed;
				break;
			}
			case REQ_NOT_ON_QUEST: {
				task->current = is_on_quest(ch, task->vnum) ? 0 : task->needed;
				break;
			}
			case REQ_OWN_BUILDING: {
				task->current = count_owned_buildings(GET_LOYALTY(ch), task->vnum);
				break;
			}
			case REQ_OWN_BUILDING_FUNCTION: {
				task->current = count_owned_buildings_by_function(GET_LOYALTY(ch), task->misc);
				break;
			}
			case REQ_OWN_VEHICLE: {
				task->current = count_owned_vehicles(GET_LOYALTY(ch), task->vnum);
				break;
			}
			case REQ_OWN_VEHICLE_FLAGGED: {
				task->current = count_owned_vehicles_by_flags(GET_LOYALTY(ch), task->misc);
				break;
			}
			case REQ_SKILL_LEVEL_OVER: {
				if (get_skill_level(ch, task->vnum) >= task->needed) {
					task->current = task->needed;	// full
				}
				else {
					task->current = 0;
				}
				break;
			}
			case REQ_SKILL_LEVEL_UNDER: {
				if (get_skill_level(ch, task->vnum) <= task->needed) {
					task->current = MAX(0, task->needed);	// full
				}
				else {
					task->current = -1;	// must set below 0 because 0 is a valid "needed"
				}
				break;
			}
			case REQ_VISIT_BUILDING: {
				if (GET_BUILDING(IN_ROOM(ch)) && GET_BLD_VNUM(GET_BUILDING(IN_ROOM(ch))) == task->vnum) {
					task->current = task->needed;	// full
				}
				// else can't detect this
				break;
			}
			case REQ_VISIT_ROOM_TEMPLATE: {
				if (GET_ROOM_TEMPLATE(IN_ROOM(ch)) && GET_RMT_VNUM(GET_ROOM_TEMPLATE(IN_ROOM(ch))) == task->vnum) {
					task->current = task->needed;	// full
				}
				// else can't detect this
				break;
			}
			case REQ_VISIT_SECTOR: {
				if (GET_SECT_VNUM(SECT(IN_ROOM(ch))) == task->vnum) {
					task->current = task->needed;	// full
				}
				// else can't detect this
				break;
			}
			case REQ_HAVE_ABILITY: {
				task->current = has_ability(ch, task->vnum) ? task->needed : 0;	// full
				break;
			}
			case REQ_REP_OVER: {
				struct player_faction_data *pfd = get_reputation(ch, task->vnum, FALSE);
				faction_data *fct = find_faction_by_vnum(task->vnum);
				task->current = (compare_reptuation((pfd ? pfd->rep : (fct ? FCT_STARTING_REP(fct) : REP_NEUTRAL)), task->needed) >= 0) ? task->needed : 0;	// full
				break;
			}
			case REQ_REP_UNDER: {
				struct player_faction_data *pfd = get_reputation(ch, task->vnum, FALSE);
				faction_data *fct = find_faction_by_vnum(task->vnum);
				task->current = (compare_reptuation((pfd ? pfd->rep : (fct ? FCT_STARTING_REP(fct) : REP_NEUTRAL)), task->needed) <= 0) ? task->needed : 0;	// full
				break;
			}
			case REQ_WEARING: {
				task->current = 0;
				
				for (iter = 0; iter < NUM_WEARS; ++iter) {
					if (GET_EQ(ch, iter) && GET_OBJ_VNUM(GET_EQ(ch, iter)) == task->vnum) {
						++(task->current);
					}
				}
				break;
			}
			case REQ_WEARING_OR_HAS: {
				task->current = 0;
				
				for (iter = 0; iter < NUM_WEARS; ++iter) {
					if (GET_EQ(ch, iter) && GET_OBJ_VNUM(GET_EQ(ch, iter)) == task->vnum) {
						++(task->current);
					}
				}
				
				task->current += count_quest_objects(ch, task->vnum, FALSE);
				break;
			}
			case REQ_GET_CURRENCY: {
				task->current = get_currency(ch, task->vnum);
				break;
			}
			case REQ_GET_COINS: {
				task->current = count_total_coins_as(ch, REAL_OTHER_COIN);
				break;
			}
			case REQ_CAN_GAIN_SKILL: {
				task->current = check_can_gain_skill(ch, task->vnum) ? task->needed : 0;
				break;
			}
			case REQ_CROP_VARIETY: {
				task->current = count_crop_variety_in_list(ch->carrying);
				break;
			}
			case REQ_OWN_HOMES: {
				task->current = count_owned_homes(GET_LOYALTY(ch));
				break;
			}
			case REQ_OWN_SECTOR: {
				task->current = count_owned_sector(GET_LOYALTY(ch), task->vnum);
				break;
			}
			case REQ_EMPIRE_WEALTH: {
				task->current = GET_LOYALTY(ch) ? GET_TOTAL_WEALTH(GET_LOYALTY(ch)) : 0;
				break;
			}
			case REQ_EMPIRE_FAME: {
				task->current = GET_LOYALTY(ch) ? EMPIRE_FAME(GET_LOYALTY(ch)) : 0;
				break;
			}
			case REQ_EMPIRE_MILITARY: {
				task->current = GET_LOYALTY(ch) ? EMPIRE_MILITARY(GET_LOYALTY(ch)) : 0;
				break;
			}
			case REQ_EMPIRE_GREATNESS: {
				task->current = GET_LOYALTY(ch) ? EMPIRE_GREATNESS(GET_LOYALTY(ch)) : 0;
				break;
			}
			case REQ_DIPLOMACY: {
				task->current = GET_LOYALTY(ch) ? count_diplomacy(GET_LOYALTY(ch), task->misc) : 0;
				break;
			}
			case REQ_HAVE_CITY: {
				task->current = GET_LOYALTY(ch) ? count_cities(GET_LOYALTY(ch)) : 0;
				break;
			}
			case REQ_EMPIRE_PRODUCED_OBJECT: {
				task->current = GET_LOYALTY(ch) ? get_production_total(GET_LOYALTY(ch), task->vnum) : 0;
				break;
			}
			case REQ_EMPIRE_PRODUCED_COMPONENT: {
				task->current = GET_LOYALTY(ch) ? get_production_total_component(GET_LOYALTY(ch), task->vnum, task->misc) : 0;
				break;
			}
			case REQ_EVENT_RUNNING: {
				task->current = find_running_event_by_vnum(task->vnum) ? task->needed : 0;
				break;
			}
			case REQ_EVENT_NOT_RUNNING: {
				task->current = find_running_event_by_vnum(task->vnum) ? 0 : task->needed;
				break;
			}
		}
	}
}


/**
* Makes sure all of a player's quest objectives are current. Also cleans up
* bad data from deleted quests.
*
* @param char_data *ch The player to check.
*/
void refresh_all_quests(char_data *ch) {
	struct player_completed_quest *pcq, *next_pcq;
	struct player_quest *pq, *next_pq;
	struct instance_data *inst;
	struct req_data *old;
	quest_data *quest;
	
	if (IS_NPC(ch)) {
		return;
	}
	
	// current quests
	LL_FOREACH_SAFE(GET_QUESTS(ch), pq, next_pq) {
		// remove entirely
		if (!(quest = quest_proto(pq->vnum)) || (QUEST_FLAGGED(quest, QST_IN_DEVELOPMENT) && !IS_IMMORTAL(ch))) {
			drop_quest(ch, pq);
			continue;
		}
		// check instance expiry
		if (QUEST_FLAGGED(quest, QST_EXPIRES_AFTER_INSTANCE) && (!(inst = get_instance_by_id(pq->instance_id)) || GET_ADV_VNUM(INST_ADVENTURE(inst)) != pq->adventure)) {
			drop_quest(ch, pq);
			continue;
		}
		
		// reload objectives
		if (pq->version < QUEST_VERSION(quest)) {
			old = pq->tracker;
			pq->tracker = copy_requirements(QUEST_TASKS(quest));
			copy_quest_progress(pq->tracker, old);
			free_requirements(old);
		}
		
		// check tracker tasks now
		refresh_one_quest_tracker(ch, pq);
	}
	
	// completed quests
	HASH_ITER(hh, GET_COMPLETED_QUESTS(ch), pcq, next_pcq) {
		if (!quest_proto(pcq->vnum)) {
			HASH_DEL(GET_COMPLETED_QUESTS(ch), pcq);
			free(pcq);
		}
	}
}


/**
* Checks a character and removes all items for quests they're not on.
*
* Does not affect immortals.
*
* @param char_data *ch The player.
*/
void remove_quest_items(char_data *ch) {
	obj_data *obj, *next_obj;
	int iter;
	
	if (IS_NPC(ch) || IS_IMMORTAL(ch)) {
		return;
	}
	
	for (iter = 0; iter < NUM_WEARS; ++iter) {
		if ((obj = GET_EQ(ch, iter)) && GET_OBJ_REQUIRES_QUEST(obj) != NOTHING && !is_on_quest(ch, GET_OBJ_REQUIRES_QUEST(obj))) {
			act("You lose $p because you are not on the right quest.", FALSE, ch, obj, NULL, TO_CHAR);
			extract_obj(obj);
		}
	}
	
	LL_FOREACH_SAFE2(ch->carrying, obj, next_obj, next_content) {
		if (GET_OBJ_REQUIRES_QUEST(obj) != NOTHING && !is_on_quest(ch, GET_OBJ_REQUIRES_QUEST(obj))) {
			act("You lose $p because you are not on the right quest.", FALSE, ch, obj, NULL, TO_CHAR);
			extract_obj(obj);
		}
	}
}


/**
* Silently removes all items related to the quest.
*
* Unlike the all-quest-items version, this one DOES apply to immortals because
* they ended the quest.
*
* @param char_data *ch The player to take items from.
* @param any_vnum vnum The quest to remove items for.
*/
void remove_quest_items_by_quest(char_data *ch, any_vnum vnum) {
	obj_data *obj, *next_obj;
	int iter;
	
	if (vnum == NOTHING) {
		syslog(SYS_ERROR, LVL_CIMPL, TRUE, "SYSERR: remove_quest_items_by_quest called with NOTHING vnum, which would remove ALL non-quest items (%s)", GET_NAME(ch));
		return;
	}
	
	for (iter = 0; iter < NUM_WEARS; ++iter) {
		if ((obj = GET_EQ(ch, iter)) && GET_OBJ_REQUIRES_QUEST(obj) == vnum) {
			extract_obj(obj);
		}
	}
	
	LL_FOREACH_SAFE2(ch->carrying, obj, next_obj, next_content) {
		if (GET_OBJ_REQUIRES_QUEST(obj) == vnum) {
			extract_obj(obj);
		}
	}
}


/**
* This turns quests on and off based on shared 'daily cycles'. Only quests with
* the DAILY flag and a dailycycle id are affected.
*
* @param int only_cycle Only updates quests on this cycle, if any (NOTHING to do all quests).
*/
void setup_daily_quest_cycles(int only_cycle) {
	// mini type for tracking
	struct sdqc_t {
		int cycle;
		int found;
		quest_data *last_active;
		UT_hash_handle hh;
	};
	
	quest_data *qst, *next_qst;
	struct sdqc_t *entry, *next_entry, *list = NULL;
	
	HASH_ITER(hh, quest_table, qst, next_qst) {
		if (only_cycle != NOTHING && QUEST_DAILY_CYCLE(qst) != only_cycle) {
			continue;	// only doing 1
		}
		
		QUEST_DAILY_ACTIVE(qst) = TRUE;	// by default
		
		if (QUEST_FLAGGED(qst, QST_IN_DEVELOPMENT)) {
			continue;	// not active
		}
		if (!QUEST_FLAGGED(qst, QST_DAILY) || QUEST_DAILY_CYCLE(qst) == NOTHING) {
			continue;	// not a cycling daily
		}
		
		// find or add entry
		HASH_FIND_INT(list, &QUEST_DAILY_CYCLE(qst), entry);
		if (!entry) {
			CREATE(entry, struct sdqc_t, 1);
			entry->cycle = QUEST_DAILY_CYCLE(qst);
			HASH_ADD_INT(list, cycle, entry);
		}
		
		// check for active
		if (!number(0, entry->found++)) {
			// success!
			if (entry->last_active) {
				QUEST_DAILY_ACTIVE(entry->last_active) = FALSE;
			}
			QUEST_DAILY_ACTIVE(qst) = TRUE;
			entry->last_active = qst;
		}
		else {
			// not active
			QUEST_DAILY_ACTIVE(qst) = FALSE;
		}
	}
	
	// free data
	HASH_ITER(hh, list, entry, next_entry) {
		HASH_DEL(list, entry);
		free(entry);
	}
	
	// save to file
	write_daily_quest_file();
}


/**
* Copies entries from one list into another, only if they are not already in
* the to_list.
*
* @param struct quest_giver **to_list A pointer to the destination list.
* @param struct quest_giver *from_list The list to copy from.
*/
void smart_copy_quest_givers(struct quest_giver **to_list, struct quest_giver *from_list) {
	struct quest_giver *iter, *search, *giver;
	bool found;
	
	LL_FOREACH(from_list, iter) {
		// ensure not already in list
		found = FALSE;
		LL_FOREACH(*to_list, search) {
			if (search->type == iter->type && search->vnum == iter->vnum) {
				found = TRUE;
				break;
			}
		}
		
		// add it
		if (!found) {
			CREATE(giver, struct quest_giver, 1);
			giver->type = iter->type;
			giver->vnum = iter->vnum;
			LL_APPEND(*to_list, giver);
		}
	}
}


/**
* Copies entries from one list into another, only if they are not already in
* the to_list.
*
* @param struct quest_reward **to_list A pointer to the destination list.
* @param struct quest_reward *from_list The list to copy from.
*/
void smart_copy_quest_rewards(struct quest_reward **to_list, struct quest_reward *from_list) {
	struct quest_reward *iter, *search, *reward;
	bool found;
	
	LL_FOREACH(from_list, iter) {
		// ensure not already in list
		found = FALSE;
		LL_FOREACH(*to_list, search) {
			if (search->type == iter->type && search->amount == iter->amount && search->vnum == iter->vnum) {
				found = TRUE;
				break;
			}
		}
		
		// add it
		if (!found) {
			CREATE(reward, struct quest_reward, 1);
			reward->type = iter->type;
			reward->amount = iter->amount;
			reward->vnum = iter->vnum;
			LL_APPEND(*to_list, reward);
		}
	}
}


 //////////////////////////////////////////////////////////////////////////////
//// LOOKUP HANDLERS /////////////////////////////////////////////////////////


/**
* Processes all the quest givers into lookup hint tables.
*
* @param quest_data *quest The quest to update givers for.
* @param bool add If TRUE, adds quest lookup hints. If FALSE, removes them.
*/
void add_or_remove_all_quest_lookups_for(quest_data *quest, bool add) {
	struct quest_giver *list[2], *giver;
	room_template *rmt;
	vehicle_data *veh;
	char_data *mob;
	bld_data *bld;
	obj_data *obj;
	int iter;
	
	if (!quest) {	// somehow
		return;
	}
	
	// work on 2 lists
	list[0] = QUEST_STARTS_AT(quest);
	list[1] = QUEST_ENDS_AT(quest);
	
	for (iter = 0; iter < 2; ++iter) {
		LL_FOREACH(list[iter], giver) {
			// QG_x -- except trigger, quest
			switch (giver->type) {
				case QG_BUILDING: {
					if ((bld = building_proto(giver->vnum))) {
						if (add) {
							add_quest_lookup(&GET_BLD_QUEST_LOOKUPS(bld), quest);
						}
						else {
							remove_quest_lookup(&GET_BLD_QUEST_LOOKUPS(bld), quest);
						}
						// does not require live update
					}
					break;
				}
				case QG_MOBILE: {
					if ((mob = mob_proto(giver->vnum))) {
						if (add) {
							add_quest_lookup(&MOB_QUEST_LOOKUPS(mob), quest);
						}
						else {
							remove_quest_lookup(&MOB_QUEST_LOOKUPS(mob), quest);
						}
						update_mob_quest_lookups(GET_MOB_VNUM(mob));
					}
					break;
				}
				case QG_OBJECT: {
					if ((obj = obj_proto(giver->vnum))) {
						if (add) {
							add_quest_lookup(&GET_OBJ_QUEST_LOOKUPS(obj), quest);
						}
						else {
							remove_quest_lookup(&GET_OBJ_QUEST_LOOKUPS(obj), quest);
						}
						update_obj_quest_lookups(GET_OBJ_VNUM(obj));
					}
					break;
				}
				case QG_ROOM_TEMPLATE: {
					if ((rmt = room_template_proto(giver->vnum))) {
						if (add) {
							add_quest_lookup(&GET_RMT_QUEST_LOOKUPS(rmt), quest);
						}
						else {
							remove_quest_lookup(&GET_RMT_QUEST_LOOKUPS(rmt), quest);
						}
						// does not require live update
					}
					break;
				}
				case QG_VEHICLE: {
					if ((veh = vehicle_proto(giver->vnum))) {
						if (add) {
							add_quest_lookup(&VEH_QUEST_LOOKUPS(veh), quest);
						}
						else {
							remove_quest_lookup(&VEH_QUEST_LOOKUPS(veh), quest);
						}
						update_veh_quest_lookups(VEH_VNUM(veh));
					}
					break;
				}
			}
		}
	}
}


/**
* Adds a quest lookup hint to a list (e.g. on a mob).
*
* Note: For mob/obj/veh quests, run update_mob_quest_lookups() etc after this.
*
* @param struct quest_lookup **list A pointer to the list to add to.
* @param quest_data *quest The quest to add.
*/
void add_quest_lookup(struct quest_lookup **list, quest_data *quest) {
	struct quest_lookup *ql;
	bool found = FALSE;
	
	if (list && quest) {
		// no dupes
		LL_FOREACH(*list, ql) {
			if (ql->quest == quest) {
				found = TRUE;
			}
		}
		
		if (!found) {
			CREATE(ql, struct quest_lookup, 1);
			ql->quest = quest;
			LL_PREPEND(*list, ql);
		}
	}
}


/**
* Builds all the quest lookup tables on startup.
*/
void build_all_quest_lookups(void) {
	quest_data *quest, *next_quest;
	HASH_ITER(hh, quest_table, quest, next_quest) {
		add_or_remove_all_quest_lookups_for(quest, TRUE);
	}
}


/**
* Adds a quest lookup hint to a list (e.g. on a mob).
*
* Note: For mob/obj/veh quests, run update_mob_quest_lookups() etc after this.
*
* @param struct quest_lookup **list A pointer to the list to add to.
* @param quest_data *quest The quest to add.
* @return bool TRUE if it removed an entry, FALSE for no matches.
*/
bool remove_quest_lookup(struct quest_lookup **list, quest_data *quest) {
	struct quest_lookup *ql, *next_ql;
	bool any = FALSE;
	
	if (list && *list && quest) {
		LL_FOREACH_SAFE(*list, ql, next_ql) {
			if (ql->quest == quest) {
				LL_DELETE(*list, ql);
				free(ql);
				any = TRUE;
			}
		}
	}
	
	return any;
}


/**
* Fixes quest lookup pointers on live copies of mobs -- this should ALWAYS
* point to the proto.
*/
void update_mob_quest_lookups(mob_vnum vnum) {
	char_data *proto, *mob;
	
	if (!(proto = mob_proto(vnum))) {
		return;
	}
	
	LL_FOREACH(character_list, mob) {
		if (IS_NPC(mob) && GET_MOB_VNUM(mob) == vnum) {
			// re-set the pointer
			MOB_QUEST_LOOKUPS(mob) = MOB_QUEST_LOOKUPS(proto);
		}
	}
}


/**
* Fixes quest lookup pointers on live copies of objs -- this should ALWAYS
* point to the proto.
*/
void update_obj_quest_lookups(obj_vnum vnum) {
	obj_data *proto, *obj;
	
	if (!(proto = obj_proto(vnum))) {
		return;
	}
	
	LL_FOREACH(object_list, obj) {
		if (GET_OBJ_VNUM(obj) == vnum) {
			// re-set the pointer
			GET_OBJ_QUEST_LOOKUPS(obj) = GET_OBJ_QUEST_LOOKUPS(proto);
		}
	}
}


/**
* Fixes quest lookup pointers on live copies of vehicles -- this should ALWAYS
* point to the proto.
*/
void update_veh_quest_lookups(any_vnum vnum) {
	vehicle_data *proto, *veh;
	
	if (!(proto = vehicle_proto(vnum))) {
		return;
	}
	
	LL_FOREACH(vehicle_list, veh) {
		if (VEH_VNUM(veh) == vnum) {
			// re-set the pointer
			VEH_QUEST_LOOKUPS(veh) = VEH_QUEST_LOOKUPS(proto);
		}
	}
}


 //////////////////////////////////////////////////////////////////////////////
//// QUEST HANDLERS //////////////////////////////////////////////////////////

/**
* @param char_data *ch Any player playing.
* @param char_data *mob Any mob.
* @param struct quest_temp_list **build_list Optional: Builds a temp list of quests available.
* @return bool TRUE if the mob has a quest the character can get; FALSE otherwise.
*/
bool can_get_quest_from_mob(char_data *ch, char_data *mob, struct quest_temp_list **build_list) {
	struct instance_data *inst;
	struct quest_lookup *ql;
	bool any = FALSE;
	bool dailies;
	
	if (IS_NPC(ch) || !MOB_QUEST_LOOKUPS(mob) || !CAN_SEE(ch, mob)) {
		return FALSE;
	}
	
	dailies = GET_DAILY_QUESTS(ch) < config_get_int("dailies_per_day");
	
	LL_FOREACH(MOB_QUEST_LOOKUPS(mob), ql) {
		// make sure they're a giver
		if (!find_quest_giver_in_list(QUEST_STARTS_AT(ql->quest), QG_MOBILE, GET_MOB_VNUM(mob))) {
			continue;
		}
		// hide tutorials
		if (QUEST_FLAGGED(ql->quest, QST_TUTORIAL) && PRF_FLAGGED(ch, PRF_NO_TUTORIALS)) {
			continue;
		}
		// hide dailies
		if (QUEST_FLAGGED(ql->quest, QST_DAILY) && !dailies) {
			continue;
		}
		// matching empire
		if (QUEST_FLAGGED(ql->quest, QST_EMPIRE_ONLY) && GET_LOYALTY(mob) != GET_LOYALTY(ch)) {
			continue;
		}
		if (!can_use_room(ch, IN_ROOM(ch), QUEST_FLAGGED(ql->quest, QST_NO_GUESTS) ? MEMBERS_ONLY : GUESTS_ALLOWED)) {
			continue;	// room permissions
		}
		// already on quest?
		if (is_on_quest(ch, QUEST_VNUM(ql->quest))) {
			continue;
		}
		
		// success
		inst = (MOB_INSTANCE_ID(mob) != NOTHING ? get_instance_by_id(MOB_INSTANCE_ID(mob)) : NULL);
		
		// pre-reqs?
		if (char_meets_prereqs(ch, ql->quest, inst)) {
			if (build_list) {
				any = TRUE;
				add_to_quest_temp_list(build_list, ql->quest, inst);
			}
			else {
				// only need 1
				return TRUE;
			}
		}
	}
	
	// nope
	return any;
}


/**
* @param char_data *ch Any player playing.
* @param obj_data *obj Any obj.
* @param struct quest_temp_list **build_list Optional: Builds a temp list of quests available.
* @return bool TRUE if the obj has a quest the character can get; FALSE otherwise.
*/
bool can_get_quest_from_obj(char_data *ch, obj_data *obj, struct quest_temp_list **build_list) {
	struct instance_data *inst;
	struct quest_lookup *ql;
	bool any = FALSE;
	room_data *room;
	bool dailies;
	
	if (IS_NPC(ch) || !GET_OBJ_QUEST_LOOKUPS(obj) || !CAN_SEE_OBJ(ch, obj) || !bind_ok(obj, ch)) {
		return FALSE;
	}
	
	dailies = GET_DAILY_QUESTS(ch) < config_get_int("dailies_per_day");
	
	LL_FOREACH(GET_OBJ_QUEST_LOOKUPS(obj), ql) {
		// make sure they're a giver
		if (!find_quest_giver_in_list(QUEST_STARTS_AT(ql->quest), QG_OBJECT, GET_OBJ_VNUM(obj))) {
			continue;
		}
		// hide tutorials
		if (QUEST_FLAGGED(ql->quest, QST_TUTORIAL) && PRF_FLAGGED(ch, PRF_NO_TUTORIALS)) {
			continue;
		}
		// hide dailies
		if (QUEST_FLAGGED(ql->quest, QST_DAILY) && !dailies) {
			continue;
		}
		// already on quest?
		if (is_on_quest(ch, QUEST_VNUM(ql->quest))) {
			continue;
		}
		room = obj_room(obj);
		if (QUEST_FLAGGED(ql->quest, QST_EMPIRE_ONLY) && !obj->carried_by && room && ROOM_OWNER(room) != GET_LOYALTY(ch)) {
			continue;	// matching empire
		}
		if (room && !obj->carried_by && !can_use_room(ch, room, QUEST_FLAGGED(ql->quest, QST_NO_GUESTS) ? MEMBERS_ONLY : GUESTS_ALLOWED)) {
			continue;	// room permissions
		}
		
		// success
		inst = (room ? find_instance_by_room(room, FALSE, TRUE) : NULL);
		
		// pre-reqs?
		if (char_meets_prereqs(ch, ql->quest, inst)) {
			if (build_list) {
				any = TRUE;
				add_to_quest_temp_list(build_list, ql->quest, inst);
			}
			else {
				// only need 1
				return TRUE;
			}
		}
	}
	
	// nope
	return any;
}


/**
* @param char_data *ch Any player playing.
* @param room_data *room Any room.
* @param struct quest_temp_list **build_list Optional: Builds a temp list of quests available.
* @return bool TRUE if the room has a quest the character can get; FALSE otherwise.
*/
bool can_get_quest_from_room(char_data *ch, room_data *room, struct quest_temp_list **build_list) {
	struct quest_lookup *ql, *list[2];
	struct instance_data *inst;
	bool any = FALSE;
	bool dailies;
	int iter;
	
	if (IS_NPC(ch)) {
		return FALSE;
	}
	
	dailies = GET_DAILY_QUESTS(ch) < config_get_int("dailies_per_day");
	
	// two places to look
	list[0] = GET_BUILDING(room) ? GET_BLD_QUEST_LOOKUPS(GET_BUILDING(room)) : NULL;
	list[1] = GET_ROOM_TEMPLATE(room) ? GET_RMT_QUEST_LOOKUPS(GET_ROOM_TEMPLATE(room)) : NULL;
	
	for (iter = 0; iter < 2; ++iter) {
		LL_FOREACH(list[iter], ql) {
			// make sure they're a giver
			if (iter == 0 && !find_quest_giver_in_list(QUEST_STARTS_AT(ql->quest), QG_BUILDING, GET_BLD_VNUM(GET_BUILDING(room)))) {
				continue;
			}
			// hide tutorials
			if (QUEST_FLAGGED(ql->quest, QST_TUTORIAL) && PRF_FLAGGED(ch, PRF_NO_TUTORIALS)) {
				continue;
			}
			if (iter == 1 && !find_quest_giver_in_list(QUEST_STARTS_AT(ql->quest), QG_ROOM_TEMPLATE, GET_RMT_VNUM(GET_ROOM_TEMPLATE(room)))) {
				continue;
			}
			// hide dailies
			if (QUEST_FLAGGED(ql->quest, QST_DAILY) && !dailies) {
				continue;
			}
			// matching empire
			if (QUEST_FLAGGED(ql->quest, QST_EMPIRE_ONLY) && ROOM_OWNER(room) != GET_LOYALTY(ch)) {
				continue;
			}
			if (!can_use_room(ch, room, QUEST_FLAGGED(ql->quest, QST_NO_GUESTS) ? MEMBERS_ONLY : GUESTS_ALLOWED)) {
				continue;	// room permissions
			}
			// already on quest?
			if (is_on_quest(ch, QUEST_VNUM(ql->quest))) {
				continue;
			}
			
			// success
			inst = (room ? find_instance_by_room(room, FALSE, TRUE) : NULL);
			
			// pre-reqs?
			if (char_meets_prereqs(ch, ql->quest, inst)) {
				if (build_list) {
					any = TRUE;
					add_to_quest_temp_list(build_list, ql->quest, inst);
				}
				else {
					// only need 1
					return TRUE;
				}
			}
		}
	}
	
	// nope
	return any;
}


/**
* @param char_data *ch Any player playing.
* @param vehicle_data *veh Any vehicle.
* @param struct quest_temp_list **build_list Optional: Builds a temp list of quests available.
* @return bool TRUE if the vehicle has a quest the character can get; FALSE otherwise.
*/
bool can_get_quest_from_vehicle(char_data *ch, vehicle_data *veh, struct quest_temp_list **build_list) {
	struct instance_data *inst;
	struct quest_lookup *ql;
	bool any = FALSE;
	bool dailies;
	
	if (IS_NPC(ch) || !VEH_QUEST_LOOKUPS(veh) || !CAN_SEE_VEHICLE(ch, veh)) {
		return FALSE;
	}
	
	dailies = GET_DAILY_QUESTS(ch) < config_get_int("dailies_per_day");
	
	LL_FOREACH(VEH_QUEST_LOOKUPS(veh), ql) {
		// make sure they're a giver
		if (!find_quest_giver_in_list(QUEST_STARTS_AT(ql->quest), QG_VEHICLE, VEH_VNUM(veh))) {
			continue;
		}
		// hide tutorials
		if (QUEST_FLAGGED(ql->quest, QST_TUTORIAL) && PRF_FLAGGED(ch, PRF_NO_TUTORIALS)) {
			continue;
		}
		// hide dailies
		if (QUEST_FLAGGED(ql->quest, QST_DAILY) && !dailies) {
			continue;
		}
		// matching empire
		if (QUEST_FLAGGED(ql->quest, QST_EMPIRE_ONLY) && VEH_OWNER(veh) != GET_LOYALTY(ch)) {
			continue;
		}
		if (!can_use_vehicle(ch, veh, QUEST_FLAGGED(ql->quest, QST_NO_GUESTS) ? MEMBERS_ONLY : GUESTS_ALLOWED)) {
			continue;	// vehicle permissions
		}
		// already on quest?
		if (is_on_quest(ch, QUEST_VNUM(ql->quest))) {
			continue;
		}
		
		// success
		inst = (IN_ROOM(veh) ? find_instance_by_room(IN_ROOM(veh), FALSE, TRUE) : NULL);
		
		// pre-reqs?
		if (char_meets_prereqs(ch, ql->quest, inst)) {
			if (build_list) {
				any = TRUE;
				add_to_quest_temp_list(build_list, ql->quest, inst);
			}
			else {
				// only need 1
				return TRUE;
			}
		}
	}
	
	// nope
	return any;
}


/**
* @param char_data *ch Any player playing.
* @param char_data *mob Any mob.
* @param struct quest_temp_list **build_list Optional: Builds a temp list of quests available.
* @return bool TRUE if the player has finished a quest that the mob accepts; FALSE otherwise.
*/
bool can_turn_quest_in_to_mob(char_data *ch, char_data *mob, struct quest_temp_list **build_list) {
	struct player_quest *pq;
	struct quest_lookup *ql;
	int complete, total;
	bool any = FALSE;
	
	if (IS_NPC(ch) || !MOB_QUEST_LOOKUPS(mob) || !CAN_SEE(ch, mob)) {
		return FALSE;
	}
	
	LL_FOREACH(MOB_QUEST_LOOKUPS(mob), ql) {
		// make sure they're a giver
		if (!find_quest_giver_in_list(QUEST_ENDS_AT(ql->quest), QG_MOBILE, GET_MOB_VNUM(mob))) {
			continue;
		}
		// matching empire
		if (QUEST_FLAGGED(ql->quest, QST_EMPIRE_ONLY) && GET_LOYALTY(mob) != GET_LOYALTY(ch)) {
			continue;
		}
		if (!can_use_room(ch, IN_ROOM(ch), QUEST_FLAGGED(ql->quest, QST_NO_GUESTS) ? MEMBERS_ONLY : GUESTS_ALLOWED)) {
			continue;	// room permissions
		}
		// are they on quest?
		if (!(pq = is_on_quest(ch, QUEST_VNUM(ql->quest)))) {
			continue;
		}
		
		count_quest_tasks(pq->tracker, &complete, &total);
		if (complete < total) {
			continue;
		}
		
		// success!
		if (build_list) {
			any = TRUE;
			add_to_quest_temp_list(build_list, ql->quest, NULL);
		}
		else {
			return TRUE;
		}
	}
	
	// nope
	return any;
}


/**
* @param char_data *ch Any player playing.
* @param obj_data *obj Any obj.
* @param struct quest_temp_list **build_list Optional: Builds a temp list of quests available.
* @return bool TRUE if the player has a quest complete that the obj ends; FALSE otherwise.
*/
bool can_turn_quest_in_to_obj(char_data *ch, obj_data *obj, struct quest_temp_list **build_list) {
	struct player_quest *pq;
	struct quest_lookup *ql;
	int complete, total;
	bool any = FALSE;
	room_data *room;
	
	if (IS_NPC(ch) || !GET_OBJ_QUEST_LOOKUPS(obj) || !CAN_SEE_OBJ(ch, obj) || !bind_ok(obj, ch)) {
		return FALSE;
	}
	
	LL_FOREACH(GET_OBJ_QUEST_LOOKUPS(obj), ql) {
		// make sure they're a giver
		if (!find_quest_giver_in_list(QUEST_ENDS_AT(ql->quest), QG_OBJECT, GET_OBJ_VNUM(obj))) {
			continue;
		}
		// are they on quest?
		if (!(pq = is_on_quest(ch, QUEST_VNUM(ql->quest)))) {
			continue;
		}
		room = obj_room(obj);
		if (QUEST_FLAGGED(ql->quest, QST_EMPIRE_ONLY) && !obj->carried_by && room && ROOM_OWNER(room) != GET_LOYALTY(ch)) {
			continue;	// matching empire
		}
		if (room && !obj->carried_by && !can_use_room(ch, room, QUEST_FLAGGED(ql->quest, QST_NO_GUESTS) ? MEMBERS_ONLY : GUESTS_ALLOWED)) {
			continue;	// room permissions
		}
		
		count_quest_tasks(pq->tracker, &complete, &total);
		if (complete < total) {
			continue;
		}
		
		// success!
		if (build_list) {
			any = TRUE;
			add_to_quest_temp_list(build_list, ql->quest, NULL);
		}
		else {
			return TRUE;
		}
	}
	
	// nope
	return any;
}


/**
* @param char_data *ch Any player playing.
* @param room_data *room Any room.
* @param struct quest_temp_list **build_list Optional: Builds a temp list of quests available.
* @return bool TRUE if the player has finished a quest that the room ends; FALSE otherwise.
*/
bool can_turn_quest_in_to_room(char_data *ch, room_data *room, struct quest_temp_list **build_list) {
	struct quest_lookup *ql, *list[2];
	int iter, complete, total;
	struct player_quest *pq;
	bool any = FALSE;
	
	if (IS_NPC(ch)) {
		return FALSE;
	}
	
	// two places to look
	list[0] = GET_BUILDING(room) ? GET_BLD_QUEST_LOOKUPS(GET_BUILDING(room)) : NULL;
	list[1] = GET_ROOM_TEMPLATE(room) ? GET_RMT_QUEST_LOOKUPS(GET_ROOM_TEMPLATE(room)) : NULL;
	
	for (iter = 0; iter < 2; ++iter) {
		LL_FOREACH(list[iter], ql) {
			// make sure they're a giver
			if (iter == 0 && !find_quest_giver_in_list(QUEST_ENDS_AT(ql->quest), QG_BUILDING, GET_BLD_VNUM(GET_BUILDING(room)))) {
				continue;
			}
			if (iter == 1 && !find_quest_giver_in_list(QUEST_ENDS_AT(ql->quest), QG_ROOM_TEMPLATE, GET_RMT_VNUM(GET_ROOM_TEMPLATE(room)))) {
				continue;
			}
			// matching empire
			if (QUEST_FLAGGED(ql->quest, QST_EMPIRE_ONLY) && ROOM_OWNER(room) != GET_LOYALTY(ch)) {
				continue;
			}
			if (!can_use_room(ch, room, QUEST_FLAGGED(ql->quest, QST_NO_GUESTS) ? MEMBERS_ONLY : GUESTS_ALLOWED)) {
				continue;	// room permissions
			}
			// are they on quest?
			if (!(pq = is_on_quest(ch, QUEST_VNUM(ql->quest)))) {
				continue;
			}
		
			count_quest_tasks(pq->tracker, &complete, &total);
			if (complete < total) {
				continue;
			}
		
			// success!
			if (build_list) {
				any = TRUE;
				add_to_quest_temp_list(build_list, ql->quest, NULL);
			}
			else {
				return TRUE;
			}
		}
	}
	
	// nope
	return any;
}


/**
* @param char_data *ch Any player playing.
* @param vehicle_data *veh Any vehicle.
* @param struct quest_temp_list **build_list Optional: Builds a temp list of quests available.
* @return bool TRUE if the player has finished a quest that the vehicle accepts; FALSE otherwise.
*/
bool can_turn_quest_in_to_vehicle(char_data *ch, vehicle_data *veh, struct quest_temp_list **build_list) {
	struct player_quest *pq;
	struct quest_lookup *ql;
	int complete, total;
	bool any = FALSE;
	
	if (IS_NPC(ch) || !VEH_QUEST_LOOKUPS(veh) || !CAN_SEE_VEHICLE(ch, veh)) {
		return FALSE;
	}
	
	LL_FOREACH(VEH_QUEST_LOOKUPS(veh), ql) {
		// make sure they're a giver
		if (!find_quest_giver_in_list(QUEST_ENDS_AT(ql->quest), QG_VEHICLE, VEH_VNUM(veh))) {
			continue;
		}
		// matching empire
		if (QUEST_FLAGGED(ql->quest, QST_EMPIRE_ONLY) && VEH_OWNER(veh) != GET_LOYALTY(ch)) {
			continue;
		}
		if (!can_use_vehicle(ch, veh, QUEST_FLAGGED(ql->quest, QST_NO_GUESTS) ? MEMBERS_ONLY : GUESTS_ALLOWED)) {
			continue;	// vehicle
		}
		// are they on quest?
		if (!(pq = is_on_quest(ch, QUEST_VNUM(ql->quest)))) {
			continue;
		}
		
		count_quest_tasks(pq->tracker, &complete, &total);
		if (complete < total) {
			continue;
		}
		
		// success!
		if (build_list) {
			any = TRUE;
			add_to_quest_temp_list(build_list, ql->quest, NULL);
		}
		else {
			return TRUE;
		}
	}
	
	// nope
	return any;
}


/**
* @param char_data *ch Any player.
* @param quest_data *quest The quest to check.
* @param struct instance_data *instance Optional; If the quest is offered in/from an instance.
* @return bool TRUE if the player can get the quest.
*/
bool char_meets_prereqs(char_data *ch, quest_data *quest, struct instance_data *instance) {
	extern bool meets_requirements(char_data *ch, struct req_data *list, struct instance_data *instance);
	
	bool daily = QUEST_FLAGGED(quest, QST_DAILY);
	struct player_completed_quest *completed;
	bool ok = TRUE;
	// needs to know instance/adventure
	
	// sanitation
	if (!ch || !quest || IS_NPC(ch)) {
		return FALSE;
	}
	
	// only immortals see in-dev quests
	if (QUEST_FLAGGED(quest, QST_IN_DEVELOPMENT) && !IS_IMMORTAL(ch)) {
		return FALSE;
	}
	
	// some daily quests are off
	if (QUEST_FLAGGED(quest, QST_DAILY) && !QUEST_DAILY_ACTIVE(quest)) {
		return FALSE;
	}
	
	// check repeatability
	if ((completed = has_completed_quest(ch, QUEST_VNUM(quest), instance ? INST_ID(instance) : NOTHING))) {
		if (daily && QUEST_REPEATABLE_AFTER(quest) <= 0) {
			// daily quest allows immediate/never: ok
		}
		else if (QUEST_REPEATABLE_AFTER(quest) >= 0 && completed->last_completed + (QUEST_REPEATABLE_AFTER(quest) * SECS_PER_REAL_MIN) <= time(0)) {
			// repeat time: ok
		}
		else if (QUEST_FLAGGED(quest, QST_REPEAT_PER_INSTANCE) && (completed->last_adventure != (instance ? GET_ADV_VNUM(INST_ADVENTURE(instance)) : NOTHING) || completed->last_instance_id != (instance ? INST_ID(instance) : NOTHING))) {
			// repeat per instance: ok (different instance)
		}
		else {
			// not repeatable
			ok = FALSE;
		}
	}
	
	// make sure daily wasn't already done TODAY
	if (daily && (completed = has_completed_quest_any(ch, QUEST_VNUM(quest))) && completed->last_completed >= data_get_long(DATA_DAILY_CYCLE)) {
		ok = FALSE;
	}
	
	// check prereqs
	if (ok && !meets_requirements(ch, QUEST_PREREQS(quest), instance)) {
		ok = FALSE;
	}
	
	return ok;
}


/**
* Similar to has_completed_quest() except it doesn't care which instance it
* was completed on.
*
* @param char_data *ch Any player.
* @param quest_vnum quest The quest to check.
* @return struct player_completed_quest* Returns completion data (TRUE) if the player has completed the quest; NULL (FALSE) otherwise.
*/
struct player_completed_quest *has_completed_quest_any(char_data *ch, any_vnum quest) {
	struct player_completed_quest *pcq;
	
	if (IS_NPC(ch)) {
		return FALSE;
	}
	
	// look up completion data
	HASH_FIND_INT(GET_COMPLETED_QUESTS(ch), &quest, pcq);
	
	return pcq;
}


/**
* Note: repeats-per-instance quests will only show as has-completed if it was
* the same instance.
*
* @param char_data *ch Any player.
* @param quest_vnum quest The quest to check.
* @param int instance_id Optional: For per-instance quests, the instance id to check. (NOTHING = no check / doesn't matter)
* @return struct player_completed_quest* Returns completion data (TRUE) if the player has completed the quest; NULL (FALSE) otherwise.
*/
struct player_completed_quest *has_completed_quest(char_data *ch, any_vnum quest, int instance_id) {
	struct player_completed_quest *pcq = has_completed_quest_any(ch, quest);
		
	// check per-instance limit (only bother with this part if instructed to check)
	if (instance_id != NOTHING && pcq) {
		quest_data *qst = quest_proto(quest);
		if (qst && QUEST_FLAGGED(qst, QST_REPEAT_PER_INSTANCE) && pcq->last_instance_id != instance_id) {
			// bad find
			pcq = NULL;
		}
	}
	
	return pcq;
}


/**
* @param char_data *ch Any player.
* @param quest_vnum quest The quest to check.
* @return struct player_quest *The player's quest data (TRUE) if on the quest, NULL (FALSE) if not.
*/
struct player_quest *is_on_quest(char_data *ch, any_vnum quest) {
	struct player_quest *pq;
	
	if (IS_NPC(ch)) {
		return NULL;
	}
	
	LL_SEARCH_SCALAR(GET_QUESTS(ch), pq, vnum, quest);
	return pq;
}


/**
* Used to determine if the character is on a quest with the given name, e.g.
* to give a better error message when they try to start/drop a quest. Prefers
* an exact match
*
* @param char_data *ch The person to check.
* @param char *argument What was typed.
* @return struct player_quest* The player's quest data if on the quest, NULL (FALSE) if not.
*/
struct player_quest *is_on_quest_by_name(char_data *ch, char *argument) {
	struct player_quest *pq, *abbrev = NULL;
	quest_data *quest;
	
	if (IS_NPC(ch) || !*argument) {
		return NULL;	// shortcut
	}
	
	LL_FOREACH(GET_QUESTS(ch), pq) {
		if (!(quest = quest_proto(pq->vnum))) {
			continue;	// missing data
		}
		
		if (!str_cmp(argument, QUEST_NAME(quest))) {
			return pq; // exact match
		}
		else if (!abbrev && multi_isname(argument, QUEST_NAME(quest))) {
			abbrev = pq;
		}
	}
	
	return abbrev;	// if any
}


 //////////////////////////////////////////////////////////////////////////////
//// QUEST TRACKERS //////////////////////////////////////////////////////////

/**
* Quest Tracker: ch gains or loses ability
*
* @param char_data *ch The player.
* @param any_vnum abil The ability vnum.
*/
void qt_change_ability(char_data *ch, any_vnum abil) {
	struct player_quest *pq;
	struct req_data *task;
	
	if (IS_NPC(ch)) {
		return;
	}
	
	LL_FOREACH(GET_QUESTS(ch), pq) {
		LL_FOREACH(pq->tracker, task) {
			if (task->type == REQ_HAVE_ABILITY && task->vnum == abil) {
				task->current = (has_ability(ch, abil) ? task->needed : 0);
			}
		}
	}
}


/**
* Quest Tracker: ch gains or loses faction reputation
*
* @param char_data *ch The player.
* @param any_vnum faction Which faction changed.
*/
void qt_change_reputation(char_data *ch, any_vnum faction) {
	struct player_faction_data *pfd = get_reputation(ch, faction, FALSE);
	faction_data *fct = find_faction_by_vnum(faction);
	struct player_quest *pq;
	struct req_data *task;
	
	if (IS_NPC(ch)) {
		return;
	}
	
	LL_FOREACH(GET_QUESTS(ch), pq) {
		LL_FOREACH(pq->tracker, task) {
			if (task->type == REQ_REP_OVER && task->vnum == faction) {
				task->current = (compare_reptuation((pfd ? pfd->rep : (fct ? FCT_STARTING_REP(fct) : REP_NEUTRAL)), task->needed) >= 0) ? task->needed : 0;
			}
			else if (task->type == REQ_REP_UNDER && task->vnum == faction) {
				task->current = (compare_reptuation((pfd ? pfd->rep : (fct ? FCT_STARTING_REP(fct) : REP_NEUTRAL)), task->needed) <= 0) ? task->needed : 0;
			}
		}
	}
}


/**
* Quest Tracker: ch gains or loses skill
*
* @param char_data *ch The player.
* @param any_vnum skl The skill vnum.
*/
void qt_change_skill_level(char_data *ch, any_vnum skl) {
	struct player_quest *pq;
	struct req_data *task;
	
	if (IS_NPC(ch)) {
		return;
	}
	
	LL_FOREACH(GET_QUESTS(ch), pq) {
		LL_FOREACH(pq->tracker, task) {
			if (task->type == REQ_SKILL_LEVEL_OVER && task->vnum == skl) {
				task->current = (get_skill_level(ch, skl) >= task->needed ? task->needed : 0);
			}
			else if (task->type == REQ_SKILL_LEVEL_UNDER && task->vnum == skl) {
				task->current = (get_skill_level(ch, skl) <= task->needed ? task->needed : -1);	// must set below 0 because 0 is a valid needed
			}
			else if (task->type == REQ_CAN_GAIN_SKILL) {
				task->current = check_can_gain_skill(ch, task->vnum) ? task->needed : 0;
			}
		}
	}
}


/**
* Quest Tracker: ch drops (or otherwise loses) an item
*
* Note: Call this AFTER taking the obj away.
*
* @param char_data *ch The player.
* @param obj_data *obj The item.
*/
void qt_drop_obj(char_data *ch, obj_data *obj) {
	struct player_quest *pq;
	struct req_data *task;
	int crop_var = -1;
	
	if (IS_NPC(ch)) {
		return;
	}
	
	LL_FOREACH(GET_QUESTS(ch), pq) {
		LL_FOREACH(pq->tracker, task) {
			if (task->type == REQ_GET_COMPONENT && GET_OBJ_CMP_TYPE(obj) == task->vnum && (GET_OBJ_CMP_FLAGS(obj) & task->misc) == task->misc) {
				--task->current;
			}
			else if (task->type == REQ_GET_OBJECT && GET_OBJ_VNUM(obj) == task->vnum) {
				--task->current;
			}
			else if (task->type == REQ_WEARING_OR_HAS && GET_OBJ_VNUM(obj) == task->vnum) {
				--task->current;
			}
			else if (task->type == REQ_CROP_VARIETY && OBJ_FLAGGED(obj, OBJ_PLANTABLE)) {
				if (crop_var == -1) {
					// update look this up once
					crop_var = count_crop_variety_in_list(ch->carrying);
				}
				task->current = crop_var;
			}
			
			// check min
			task->current = MAX(task->current, 0);
		}
	}
}


/**
* Quest Tracker: empire cities change
*
* @param char_data *ch The player.
* @param any_vnum amount (ignored, required by function pointer call).
*/
void qt_empire_cities(char_data *ch, any_vnum amount) {
	struct player_quest *pq;
	struct req_data *task;
	
	if (IS_NPC(ch)) {
		return;
	}
	
	LL_FOREACH(GET_QUESTS(ch), pq) {
		LL_FOREACH(pq->tracker, task) {
			if (task->type == REQ_DIPLOMACY) {
				task->current = GET_LOYALTY(ch) ? count_cities(GET_LOYALTY(ch)) : 0;
			}
		}
	}
}


/**
* Quest Tracker: empire diplomacy changes
*
* @param char_data *ch The player.
* @param any_vnum amount (ignored, required by function pointer call).
*/
void qt_empire_diplomacy(char_data *ch, any_vnum amount) {
	struct player_quest *pq;
	struct req_data *task;
	
	if (IS_NPC(ch)) {
		return;
	}
	
	LL_FOREACH(GET_QUESTS(ch), pq) {
		LL_FOREACH(pq->tracker, task) {
			if (task->type == REQ_DIPLOMACY) {
				task->current = GET_LOYALTY(ch) ? count_diplomacy(GET_LOYALTY(ch), task->misc) : 0;
			}
		}
	}
}


/**
* Quest Tracker: empire greatness changes
*
* @param char_data *ch The player.
* @param any_vnum amount Change in greatness (usually 0, but this parameter is required -- we rescan the empire instead).
*/
void qt_empire_greatness(char_data *ch, any_vnum amount) {
	struct player_quest *pq;
	struct req_data *task;
	
	if (IS_NPC(ch)) {
		return;
	}
	
	LL_FOREACH(GET_QUESTS(ch), pq) {
		LL_FOREACH(pq->tracker, task) {
			if (task->type == REQ_EMPIRE_GREATNESS) {
				task->current = GET_LOYALTY(ch) ? EMPIRE_GREATNESS(GET_LOYALTY(ch)) : 0;
			}
		}
	}
}


/**
* This can be used to call several different qt functions on all members of
* an empire online.
*
* @param empire_data *emp The empire to call the function on.
* @param void (*func)(char_data *ch, any_vnum vnum) The function to call.
* @param any_vnum vnum The vnum to pass to the function.
*/
void qt_empire_players(empire_data *emp, void (*func)(char_data *ch, any_vnum vnum), any_vnum vnum) {
	descriptor_data *desc;
	char_data *ch;
	
	if (!emp || !func || vnum == NOTHING) {
		return;
	}
	
	LL_FOREACH(descriptor_list, desc) {
		if (STATE(desc) != CON_PLAYING || !(ch = desc->character)) {
			continue;
		}
		if (GET_LOYALTY(ch) != emp) {
			continue;
		}
		
		// call it
		(func)(ch, vnum);
	}
}


/**
* Quest Tracker: ch gains/loses coins
*
* @param char_data *ch The player.
*/
void qt_change_coins(char_data *ch) {
	struct player_quest *pq;
	struct req_data *task;
	
	if (IS_NPC(ch)) {
		return;
	}
	
	LL_FOREACH(GET_QUESTS(ch), pq) {
		LL_FOREACH(pq->tracker, task) {
			if (task->type == REQ_GET_COINS) {
				task->current = count_total_coins_as(ch, REAL_OTHER_COIN);
			}
		}
	}
}


/**
* Quest Tracker: ch gains/loses currency
*
* @param char_data *ch The player.
* @param any_vnum vnum The generic currency vnum.
* @param int total The new value of the currency.
*/
void qt_change_currency(char_data *ch, any_vnum vnum, int total) {
	struct player_quest *pq;
	struct req_data *task;
	
	if (IS_NPC(ch)) {
		return;
	}
	
	LL_FOREACH(GET_QUESTS(ch), pq) {
		LL_FOREACH(pq->tracker, task) {
			if (task->type == REQ_GET_CURRENCY && task->vnum == vnum) {
				task->current = total;
			}
		}
	}
}


/**
* Quest Tracker: empire changes production-total
*
* @param char_data *ch The player.
* @param obj_vnum vnum Which object vnum.
* @param int amount How much was gained (or lost).
*/
void qt_change_production_total(char_data *ch, any_vnum vnum, int amount) {
	obj_data *proto = obj_proto(vnum);
	struct player_quest *pq;
	struct req_data *task;
	
	if (IS_NPC(ch) || !proto) {
		return;
	}
	
	LL_FOREACH(GET_QUESTS(ch), pq) {
		LL_FOREACH(pq->tracker, task) {
			if (task->type == REQ_EMPIRE_PRODUCED_OBJECT && task->vnum == vnum) {
				SAFE_ADD(task->current, amount, 0, INT_MAX, FALSE);
			}
			else if (task->type == REQ_EMPIRE_PRODUCED_COMPONENT && GET_OBJ_CMP_TYPE(proto) == task->vnum && (GET_OBJ_CMP_FLAGS(proto) & task->misc) == task->misc) {
				SAFE_ADD(task->current, amount, 0, INT_MAX, FALSE);
			}
		}
	}
}


/**
* Quest Tracker: empire wealth changes
*
* @param char_data *ch The player.
* @param any_vnum amount Change in wealth (may be 0; reread anyway).
*/
void qt_empire_wealth(char_data *ch, any_vnum amount) {
	struct player_quest *pq;
	struct req_data *task;
	
	if (IS_NPC(ch)) {
		return;
	}
	
	LL_FOREACH(GET_QUESTS(ch), pq) {
		LL_FOREACH(pq->tracker, task) {
			if (task->type == REQ_EMPIRE_WEALTH) {
				task->current = GET_LOYALTY(ch) ? GET_TOTAL_WEALTH(GET_LOYALTY(ch)) : 0;
			}
		}
	}
}


/**
* Quest Tracker: an event starts or stops (updates all players in-game)
*
* @param any_vnum event_vnum Which event has started or stopped.
*/
void qt_event_start_stop(any_vnum event_vnum) {
	struct player_quest *pq;
	struct req_data *task;
	char_data *ch;
	
	LL_FOREACH(character_list, ch) {
		if (IS_NPC(ch)) {
			continue;
		}
		
		LL_FOREACH(GET_QUESTS(ch), pq) {
			LL_FOREACH(pq->tracker, task) {
				if (task->type == REQ_EVENT_RUNNING && task->vnum == event_vnum) {
					task->current = find_running_event_by_vnum(task->vnum) ? task->needed : 0;
				}
				else if (task->type == REQ_EVENT_NOT_RUNNING && task->vnum == event_vnum) {
					task->current = find_running_event_by_vnum(task->vnum) ? 0 : task->needed;
				}
			}
		}
	}
}


/**
* Quest Tracker: ch gets a building
*
* @param char_data *ch The player.
* @param any_vnum vnum The building vnum.
*/
void qt_gain_building(char_data *ch, any_vnum vnum) {
	struct player_quest *pq;
	struct req_data *task;
	bld_data *bld = building_proto(vnum);
	
	if (IS_NPC(ch) || !bld) {
		return;
	}
	
	LL_FOREACH(GET_QUESTS(ch), pq) {
		LL_FOREACH(pq->tracker, task) {
			if (task->type == REQ_OWN_BUILDING && task->vnum == vnum) {
				++task->current;
			}
			else if (task->type == REQ_OWN_BUILDING_FUNCTION && (GET_BLD_FUNCTIONS(bld) & task->misc) == task->misc) {
				++task->current;
			}
			else if (task->type == REQ_OWN_HOMES && !IS_SET(GET_BLD_FLAGS(bld), BLD_ROOM) && GET_BLD_CITIZENS(bld) > 0) {
				++task->current;
			}
			else if (task->type == REQ_EMPIRE_FAME && GET_BLD_FAME(bld) != 0) {
				task->current += GET_BLD_FAME(bld);
			}
			else if (task->type == REQ_EMPIRE_MILITARY && GET_BLD_MILITARY(bld) != 0) {
				task->current += GET_BLD_MILITARY(bld);
			}
		}
	}
}


/**
* Quest Tracker: ch gets a new tile, by sector
*
* @param char_data *ch The player.
* @param sector_vnum vnum The sector vnum.
*/
void qt_gain_tile_sector(char_data *ch, sector_vnum vnum) {
	struct player_quest *pq;
	struct req_data *task;
	
	if (IS_NPC(ch)) {
		return;
	}
	
	LL_FOREACH(GET_QUESTS(ch), pq) {
		LL_FOREACH(pq->tracker, task) {
			if (task->type == REQ_OWN_SECTOR && task->vnum == vnum) {
				++task->current;
			}
		}
	}
}


/**
* Quest Tracker: ch gets a vehicle
*
* @param char_data *ch The player.
* @param any_vnum vnum The vehicle vnum.
*/
void qt_gain_vehicle(char_data *ch, any_vnum vnum) {
	struct player_quest *pq;
	struct req_data *task;
	vehicle_data *veh;
	
	if (IS_NPC(ch)) {
		return;
	}
	
	LL_FOREACH(GET_QUESTS(ch), pq) {
		LL_FOREACH(pq->tracker, task) {
			if (task->type == REQ_OWN_VEHICLE && task->vnum == vnum) {
				++task->current;
			}
			else if (task->type == REQ_OWN_VEHICLE_FLAGGED && (veh = vehicle_proto(vnum)) && (VEH_FLAGS(veh) & task->misc) == task->misc) {
				++task->current;
			}
		}
	}
}


/**
* Quest Tracker: ch obtains an item
*
* @param char_data *ch The player.
* @param obj_data *obj The item.
*/
void qt_get_obj(char_data *ch, obj_data *obj) {
	struct player_quest *pq;
	struct req_data *task;
	int crop_var = -1;
	
	if (IS_NPC(ch)) {
		return;
	}
	
	LL_FOREACH(GET_QUESTS(ch), pq) {
		LL_FOREACH(pq->tracker, task) {
			if (task->type == REQ_GET_COMPONENT && GET_OBJ_CMP_TYPE(obj) == task->vnum && (GET_OBJ_CMP_FLAGS(obj) & task->misc) == task->misc) {
				++task->current;
			}
			else if (task->type == REQ_GET_OBJECT && GET_OBJ_VNUM(obj) == task->vnum) {
				++task->current;
			}
			else if (task->type == REQ_WEARING_OR_HAS && GET_OBJ_VNUM(obj) == task->vnum) {
				++task->current;
			}
			else if (task->type == REQ_CROP_VARIETY && OBJ_FLAGGED(obj, OBJ_PLANTABLE)) {
				if (crop_var == -1) {
					// update look this up once
					crop_var = count_crop_variety_in_list(ch->carrying);
				}
				task->current = crop_var;
			}
		}
	}
}


/**
* Quest Tracker: ch keeps or unkeeps an item (updates quests where it would be
* extracted.
*
* @param char_data *ch The player.
* @param obj_data *obj The item.
* @param bool true_for_keep Keeping if TRUE, un-keeping if FALSE.
*/
void qt_keep_obj(char_data *ch, obj_data *obj, bool true_for_keep) {
	struct player_quest *pq;
	struct req_data *task;
	quest_data *qst;
	
	if (IS_NPC(ch)) {
		return;
	}
	
	LL_FOREACH(GET_QUESTS(ch), pq) {
		if (!(qst = quest_proto(pq->vnum)) || !QUEST_FLAGGED(qst, QST_EXTRACT_TASK_OBJECTS)) {
			continue;	// only interested in quests that extract items
		}
		
		LL_FOREACH(pq->tracker, task) {
			if (task->type == REQ_GET_COMPONENT && GET_OBJ_CMP_TYPE(obj) == task->vnum && (GET_OBJ_CMP_FLAGS(obj) & task->misc) == task->misc) {
				task->current += (true_for_keep ? -1 : 1);
			}
			else if (task->type == REQ_GET_OBJECT && GET_OBJ_VNUM(obj) == task->vnum) {
				task->current += (true_for_keep ? -1 : 1);
			}
			else if (task->type == REQ_WEARING_OR_HAS && GET_OBJ_VNUM(obj) == task->vnum) {
				task->current += (true_for_keep ? -1 : 1);
			}
			
			// check min
			task->current = MAX(task->current, 0);
		}
	}
}


/**
* Quest Tracker: ch kills a mob
*
* @param char_data *ch The player.
* @param char_data *mob The mob killed.
*/
void qt_kill_mob(char_data *ch, char_data *mob) {
	struct player_quest *pq;
	struct req_data *task;
	
	if (IS_NPC(ch) || !IS_NPC(mob)) {
		return;
	}
	
	// player trackers
	LL_FOREACH(GET_QUESTS(ch), pq) {
		LL_FOREACH(pq->tracker, task) {
			if (task->type == REQ_KILL_MOB_FLAGGED && (MOB_FLAGS(mob) & task->misc) == task->misc) {
				++task->current;
			}
			else if (task->type == REQ_KILL_MOB && GET_MOB_VNUM(mob) == task->vnum) {
				++task->current;
			}
		}
	}
	
	// empire trackers
	if (GET_LOYALTY(ch)) {
		struct empire_goal *goal, *next_goal;
		HASH_ITER(hh, EMPIRE_GOALS(GET_LOYALTY(ch)), goal, next_goal) {
			LL_FOREACH(goal->tracker, task) {
				if (task->type == REQ_KILL_MOB_FLAGGED && (MOB_FLAGS(mob) & task->misc) == task->misc) {
					++task->current;
					EMPIRE_NEEDS_SAVE(GET_LOYALTY(ch)) = TRUE;
					TRIGGER_DELAYED_REFRESH(GET_LOYALTY(ch), DELAY_REFRESH_GOAL_COMPLETE);
				}
				else if (task->type == REQ_KILL_MOB && GET_MOB_VNUM(mob) == task->vnum) {
					++task->current;
					EMPIRE_NEEDS_SAVE(GET_LOYALTY(ch)) = TRUE;
					TRIGGER_DELAYED_REFRESH(GET_LOYALTY(ch), DELAY_REFRESH_GOAL_COMPLETE);
				}
			}
		}
	}
}


/**
* Quest Tracker: ch loses/dismantles a building
*
* @param char_data *ch The player.
* @param any_vnum vnum The building vnum.
*/
void qt_lose_building(char_data *ch, any_vnum vnum) {
	struct player_quest *pq;
	struct req_data *task;
	bld_data *bld = building_proto(vnum);
	
	if (IS_NPC(ch) || !bld) {
		return;
	}
	
	LL_FOREACH(GET_QUESTS(ch), pq) {
		LL_FOREACH(pq->tracker, task) {
			if (task->type == REQ_OWN_BUILDING && task->vnum == vnum) {
				--task->current;
			}
			else if (task->type == REQ_OWN_BUILDING_FUNCTION && (GET_BLD_FUNCTIONS(bld) & task->misc) == task->misc) {
				--task->current;
			}
			else if (task->type == REQ_OWN_HOMES && !IS_SET(GET_BLD_FLAGS(bld), BLD_ROOM) && GET_BLD_CITIZENS(bld) > 0) {
				--task->current;
			}
			else if (task->type == REQ_EMPIRE_FAME && GET_BLD_FAME(bld) != 0) {
				task->current -= GET_BLD_FAME(bld);
			}
			else if (task->type == REQ_EMPIRE_MILITARY && GET_BLD_MILITARY(bld) != 0) {
				task->current -= GET_BLD_MILITARY(bld);
			}
			
			// check min
			task->current = MAX(task->current, 0);
		}
	}
}


/**
* Quest Tracker: ch drops/finishes a quest
*
* @param char_data *ch The player.
* @param any_vnum vnum The quest vnum.
*/
void qt_lose_quest(char_data *ch, any_vnum vnum) {
	struct player_quest *pq;
	struct req_data *task;
	
	if (IS_NPC(ch)) {
		return;
	}
	
	LL_FOREACH(GET_QUESTS(ch), pq) {
		LL_FOREACH(pq->tracker, task) {
			if (task->type == REQ_NOT_ON_QUEST && task->vnum == vnum) {
				task->current = task->needed;
			}
		}
	}
}


/**
* Quest Tracker: ch loses a tile, by sector
*
* @param char_data *ch The player.
* @param sector_vnum vnum The sector vnum.
*/
void qt_lose_tile_sector(char_data *ch, sector_vnum vnum) {
	struct player_quest *pq;
	struct req_data *task;
	
	if (IS_NPC(ch)) {
		return;
	}
	
	LL_FOREACH(GET_QUESTS(ch), pq) {
		LL_FOREACH(pq->tracker, task) {
			if (task->type == REQ_OWN_SECTOR && task->vnum == vnum) {
				--task->current;
			}
			
			// check min
			task->current = MAX(task->current, 0);
		}
	}
}


/**
* Quest Tracker: ch loses a vehicle
*
* @param char_data *ch The player.
* @param any_vnum vnum The vehicle vnum.
*/
void qt_lose_vehicle(char_data *ch, any_vnum vnum) {
	struct player_quest *pq;
	struct req_data *task;
	vehicle_data *veh;
	
	if (IS_NPC(ch)) {
		return;
	}
	
	LL_FOREACH(GET_QUESTS(ch), pq) {
		LL_FOREACH(pq->tracker, task) {
			if (task->type == REQ_OWN_VEHICLE && task->vnum == vnum) {
				--task->current;
			}
			else if (task->type == REQ_OWN_VEHICLE_FLAGGED && (veh = vehicle_proto(vnum)) && (VEH_FLAGS(veh) & task->misc) == task->misc) {
				--task->current;
			}
			
			// check min
			task->current = MAX(task->current, 0);
		}
	}
}


/**
* Quest Tracker: ch completes a quest
*
* @param char_data *ch The player.
* @param any_vnum vnum The quest vnum.
*/
void qt_quest_completed(char_data *ch, any_vnum vnum) {
	struct player_quest *pq;
	struct req_data *task;
	
	if (IS_NPC(ch)) {
		return;
	}
	
	LL_FOREACH(GET_QUESTS(ch), pq) {
		LL_FOREACH(pq->tracker, task) {
			if (task->type == REQ_COMPLETED_QUEST && task->vnum == vnum) {
				task->current = task->needed;
			}
			else if (task->type == REQ_NOT_COMPLETED_QUEST && task->vnum == vnum) {
				task->current = 0;
			}
		}
	}
}


/**
* Quest Tracker: ch removes (un-wears) an item
*
* @param char_data *ch The player.
* @param obj_data *obj The item.
*/
void qt_remove_obj(char_data *ch, obj_data *obj) {
	struct player_quest *pq;
	struct req_data *task;
	
	if (IS_NPC(ch)) {
		return;
	}
	
	LL_FOREACH(GET_QUESTS(ch), pq) {
		LL_FOREACH(pq->tracker, task) {
			if (task->type == REQ_WEARING && GET_OBJ_VNUM(obj) == task->vnum) {
				--task->current;
			}
			else if (task->type == REQ_WEARING_OR_HAS && GET_OBJ_VNUM(obj) == task->vnum) {
				--task->current;
			}
			
			// check min
			task->current = MAX(task->current, 0);
		}
	}
}


/**
* Quest Tracker: ch starts a quest
*
* @param char_data *ch The player.
* @param any_vnum vnum The quest vnum.
*/
void qt_start_quest(char_data *ch, any_vnum vnum) {
	struct player_quest *pq;
	struct req_data *task;
	
	if (IS_NPC(ch)) {
		return;
	}
	
	LL_FOREACH(GET_QUESTS(ch), pq) {
		LL_FOREACH(pq->tracker, task) {
			if (task->type == REQ_NOT_ON_QUEST && task->vnum == vnum) {
				task->current = 0;
			}
		}
	}
}


/**
* Quest Tracker: mark a triggered condition for 1 quest
*
* @param char_data *ch The player.
* @param any_vnum vnum The quest to mark.
*/
void qt_triggered_task(char_data *ch, any_vnum vnum) {
	struct player_quest *pq;
	struct req_data *task;
	
	if (IS_NPC(ch)) {
		return;
	}
	
	LL_FOREACH(GET_QUESTS(ch), pq) {
		if (pq->vnum == vnum) {
			LL_FOREACH(pq->tracker, task) {
				if (task->type == REQ_TRIGGERED) {
					task->current = task->needed;
				}
			}
		}
	}
}


/**
* Quest Tracker: cancel a triggered condition for the quest
*
* @param char_data *ch The player.
* @param any_vnum vnum The quest to un-mark.
*/
void qt_untrigger_task(char_data *ch, any_vnum vnum) {
	struct player_quest *pq;
	struct req_data *task;
	
	if (IS_NPC(ch)) {
		return;
	}
	
	LL_FOREACH(GET_QUESTS(ch), pq) {
		if (pq->vnum == vnum) {
			LL_FOREACH(pq->tracker, task) {
				if (task->type == REQ_TRIGGERED) {
					task->current = 0;
				}
			}
		}
	}
}


/**
* Quest Tracker: ch visits a building
*
* @param char_data *ch The player.
* @param any_vnum vnum The building vnum.
*/
void qt_visit_building(char_data *ch, any_vnum vnum) {
	struct player_quest *pq;
	struct req_data *task;
	
	if (IS_NPC(ch)) {
		return;
	}
	
	LL_FOREACH(GET_QUESTS(ch), pq) {
		LL_FOREACH(pq->tracker, task) {
			if (task->type == REQ_VISIT_BUILDING && task->vnum == vnum) {
				task->current = task->needed;
			}
		}
	}
}


/**
* Quest Tracker: ch visits a room template
*
* @param char_data *ch The player.
* @param any_vnum vnum The rmt vnum.
*/
void qt_visit_room_template(char_data *ch, any_vnum vnum) {
	struct player_quest *pq;
	struct req_data *task;
	
	if (IS_NPC(ch)) {
		return;
	}
	
	LL_FOREACH(GET_QUESTS(ch), pq) {
		LL_FOREACH(pq->tracker, task) {
			if (task->type == REQ_VISIT_ROOM_TEMPLATE && task->vnum == vnum) {
				task->current = task->needed;
			}
		}
	}
}


/**
* Quest Tracker: ch visits a sector
*
* @param char_data *ch The player.
* @param any_vnum vnum The sector vnum.
*/
void qt_visit_sector(char_data *ch, any_vnum vnum) {
	struct player_quest *pq;
	struct req_data *task;
	
	if (IS_NPC(ch)) {
		return;
	}
	
	LL_FOREACH(GET_QUESTS(ch), pq) {
		LL_FOREACH(pq->tracker, task) {
			if (task->type == REQ_VISIT_SECTOR && task->vnum == vnum) {
				task->current = task->needed;
			}
		}
	}
}


/**
* Quest Tracker: run all 'visit' types on the room.
*
* @param char_data *ch The player.
* @param any_vnum vnum The sector vnum.
*/
void qt_visit_room(char_data *ch, room_data *room) {
	if (IS_NPC(ch)) {
		return;
	}
	
	qt_visit_sector(ch, GET_SECT_VNUM(SECT(room)));
	if (GET_BUILDING(room)) {
		qt_visit_building(ch, GET_BLD_VNUM(GET_BUILDING(room)));
	}
	if (GET_ROOM_TEMPLATE(room)) {
		qt_visit_room_template(ch, GET_RMT_VNUM(GET_ROOM_TEMPLATE(room)));
	}
}


/**
* Quest Tracker: ch wears an item
*
* @param char_data *ch The player.
* @param obj_data *obj The item.
*/
void qt_wear_obj(char_data *ch, obj_data *obj) {
	struct player_quest *pq;
	struct req_data *task;
	
	if (IS_NPC(ch)) {
		return;
	}
	
	LL_FOREACH(GET_QUESTS(ch), pq) {
		LL_FOREACH(pq->tracker, task) {
			if (task->type == REQ_WEARING && GET_OBJ_VNUM(obj) == task->vnum) {
				++task->current;
			}
			else if (task->type == REQ_WEARING_OR_HAS && GET_OBJ_VNUM(obj) == task->vnum) {
				++task->current;
			}
		}
	}
}


 //////////////////////////////////////////////////////////////////////////////
//// UTILITIES ///////////////////////////////////////////////////////////////

/**
* Checks for common quest problems and reports them to ch.
*
* @param quest_data *quest The item to audit.
* @param char_data *ch The person to report to.
* @return bool TRUE if any problems were reported; FALSE if all good.
*/
bool audit_quest(quest_data *quest, char_data *ch) {
	extern const bool requirement_needs_tracker[];
	
	struct trig_proto_list *tpl;
	struct req_data *task;
	trig_data *trig;
	bool problem = FALSE;
	
	if (QUEST_FLAGGED(quest, QST_IN_DEVELOPMENT)) {
		olc_audit_msg(ch, QUEST_VNUM(quest), "IN-DEVELOPMENT");
		problem = TRUE;
	}
	if (!QUEST_NAME(quest) || !*QUEST_NAME(quest) || !str_cmp(QUEST_NAME(quest), default_quest_name)) {
		olc_audit_msg(ch, QUEST_VNUM(quest), "Name not set");
		problem = TRUE;
	}
	if (!isupper(*QUEST_NAME(quest))) {
		olc_audit_msg(ch, QUEST_VNUM(quest), "Name not capitalized");
		problem = TRUE;
	}
	if (!QUEST_DESCRIPTION(quest) || !*QUEST_DESCRIPTION(quest) || !str_cmp(QUEST_DESCRIPTION(quest), default_quest_description)) {
		olc_audit_msg(ch, QUEST_VNUM(quest), "Description not set");
		problem = TRUE;
	}
	if (!QUEST_COMPLETE_MSG(quest) || !*QUEST_COMPLETE_MSG(quest) || !str_cmp(QUEST_COMPLETE_MSG(quest), default_quest_complete_msg)) {
		olc_audit_msg(ch, QUEST_VNUM(quest), "Complete message not set");
		problem = TRUE;
	}
	
	if (QUEST_MIN_LEVEL(quest) > QUEST_MAX_LEVEL(quest) && QUEST_MAX_LEVEL(quest) != 0) {
		olc_audit_msg(ch, QUEST_VNUM(quest), "Min level higher than max level");
		problem = TRUE;
	}

	// check scripts
	LL_FOREACH(QUEST_SCRIPTS(quest), tpl) {
		if (!(trig = real_trigger(tpl->vnum))) {
			olc_audit_msg(ch, QUEST_VNUM(quest), "Non-existent trigger %d", tpl->vnum);
			problem = TRUE;
			continue;
		}
		if (trig->attach_type != WLD_TRIGGER) {
			olc_audit_msg(ch, QUEST_VNUM(quest), "Incorrect trigger type (trg %d)", tpl->vnum);
			problem = TRUE;
		}
	}
	
	if (!QUEST_STARTS_AT(quest)) {
		olc_audit_msg(ch, QUEST_VNUM(quest), "No start locations");
		problem = TRUE;
	}
	if (!QUEST_ENDS_AT(quest)) {
		olc_audit_msg(ch, QUEST_VNUM(quest), "No end locations");
		problem = TRUE;
	}
	
	LL_FOREACH(QUEST_PREREQS(quest), task) {
		if (requirement_needs_tracker[task->type]) {
			olc_audit_msg(ch, QUEST_VNUM(quest), "Invalid prereq type %s", requirement_types[task->type]);
			problem = TRUE;
			break;
		}
	}
	
	return problem;
}


/**
* Deletes entries by type+vnum.
*
* @param struct quest_giver **list A pointer to the list to delete from.
* @param int type QG_ type.
* @param any_vnum vnum The vnum to remove.
* @return bool TRUE if the type+vnum was removed from the list. FALSE if not.
*/
bool delete_quest_giver_from_list(struct quest_giver **list, int type, any_vnum vnum) {
	struct quest_giver *iter, *next_iter;
	bool any = FALSE;
	
	LL_FOREACH_SAFE(*list, iter, next_iter) {
		if (iter->type == type && iter->vnum == vnum) {
			any = TRUE;
			LL_DELETE(*list, iter);
			free(iter);
		}
	}
	
	return any;
}


/**
* Deletes entries by type+vnum.
*
* @param struct quest_reward **list A pointer to the list to delete from.
* @param int type QG_ type.
* @param any_vnum vnum The vnum to remove.
* @return bool TRUE if the type+vnum was removed from the list. FALSE if not.
*/
bool delete_quest_reward_from_list(struct quest_reward **list, int type, any_vnum vnum) {
	struct quest_reward *iter, *next_iter;
	bool any = FALSE;
	
	LL_FOREACH_SAFE(*list, iter, next_iter) {
		if (iter->type == type && iter->vnum == vnum) {
			any = TRUE;
			LL_DELETE(*list, iter);
			free(iter);
		}
	}
	
	return any;
}


/**
* @param struct quest_giver *list A list to search.
* @param int type QG_ type.
* @param any_vnum vnum The vnum to look for.
* @return bool TRUE if the type+vnum is in the list. FALSE if not.
*/
bool find_quest_giver_in_list(struct quest_giver *list, int type, any_vnum vnum) {
	struct quest_giver *iter;
	LL_FOREACH(list, iter) {
		if (iter->type == type && iter->vnum == vnum) {
			return TRUE;
		}
	}
	return FALSE;
}


/**
* @param struct quest_reward *list A list to search.
* @param int type REQ_ type.
* @param any_vnum vnum The vnum to look for.
* @return bool TRUE if the type+vnum is in the list. FALSE if not.
*/
bool find_quest_reward_in_list(struct quest_reward *list, int type, any_vnum vnum) {
	struct quest_reward *iter;
	LL_FOREACH(list, iter) {
		if (iter->type == type && iter->vnum == vnum) {
			return TRUE;
		}
	}
	return FALSE;
}


/**
* For the .list command.
*
* @param quest_data *quest The thing to list.
* @param bool detail If TRUE, provide additional details
* @return char* The line to show (without a CRLF).
*/
char *list_one_quest(quest_data *quest, bool detail) {
	static char output[MAX_STRING_LENGTH];
	
	if (detail) {
		snprintf(output, sizeof(output), "[%5d] %s%s", QUEST_VNUM(quest), QUEST_NAME(quest), (QUEST_FLAGGED(quest, QST_DAILY) ? " (daily)" : ""));
	}
	else {
		snprintf(output, sizeof(output), "[%5d] %s%s", QUEST_VNUM(quest), QUEST_NAME(quest), (QUEST_FLAGGED(quest, QST_DAILY) ? " (daily)" : ""));
	}
		
	return output;
}


/**
* Searches for all uses of a quest and displays them.
*
* @param char_data *ch The player.
* @param any_vnum vnum The quest vnum.
*/
void olc_search_quest(char_data *ch, any_vnum vnum) {
	char buf[MAX_STRING_LENGTH];
	quest_data *quest = quest_proto(vnum);
	event_data *event, *next_event;
	quest_data *qiter, *next_qiter;
	progress_data *prg, *next_prg;
	social_data *soc, *next_soc;
	shop_data *shop, *next_shop;
	int size, found;
	bool any;
	
	if (!quest) {
		msg_to_char(ch, "There is no quest %d.\r\n", vnum);
		return;
	}
	
	found = 0;
	size = snprintf(buf, sizeof(buf), "Occurrences of quest %d (%s):\r\n", vnum, QUEST_NAME(quest));
	
	// events
	HASH_ITER(hh, event_table, event, next_event) {
		if (size >= sizeof(buf)) {
			break;
		}
		// QR_x: event rewards
		any = find_event_reward_in_list(EVT_RANK_REWARDS(event), QR_QUEST_CHAIN, vnum);
		any |= find_event_reward_in_list(EVT_THRESHOLD_REWARDS(event), QR_QUEST_CHAIN, vnum);
		
		if (any) {
			++found;
			size += snprintf(buf + size, sizeof(buf) - size, "EVT [%5d] %s\r\n", EVT_VNUM(event), EVT_NAME(event));
		}
	}
	
	// progress
	HASH_ITER(hh, progress_table, prg, next_prg) {
		if (size >= sizeof(buf)) {
			break;
		}
		// REQ_x: requirement search
		any = find_requirement_in_list(PRG_TASKS(prg), REQ_COMPLETED_QUEST, vnum);
		any |= find_requirement_in_list(PRG_TASKS(prg), REQ_NOT_COMPLETED_QUEST, vnum);
		any |= find_requirement_in_list(PRG_TASKS(prg), REQ_NOT_ON_QUEST, vnum);
		
		if (any) {
			++found;
			size += snprintf(buf + size, sizeof(buf) - size, "PRG [%5d] %s\r\n", PRG_VNUM(prg), PRG_NAME(prg));
		}
	}
	
	// on other quests
	HASH_ITER(hh, quest_table, qiter, next_qiter) {
		if (size >= sizeof(buf)) {
			break;
		}
		// QR_x, REQ_x: quest types
		any = find_requirement_in_list(QUEST_TASKS(qiter), REQ_COMPLETED_QUEST, vnum);
		any |= find_requirement_in_list(QUEST_PREREQS(qiter), REQ_COMPLETED_QUEST, vnum);
		any |= find_requirement_in_list(QUEST_TASKS(qiter), REQ_NOT_COMPLETED_QUEST, vnum);
		any |= find_requirement_in_list(QUEST_PREREQS(qiter), REQ_NOT_COMPLETED_QUEST, vnum);
		any |= find_requirement_in_list(QUEST_TASKS(qiter), REQ_NOT_ON_QUEST, vnum);
		any |= find_requirement_in_list(QUEST_PREREQS(qiter), REQ_NOT_ON_QUEST, vnum);
		any |= find_quest_reward_in_list(QUEST_REWARDS(qiter), QR_QUEST_CHAIN, vnum);
		any |= find_quest_giver_in_list(QUEST_STARTS_AT(qiter), QG_QUEST, vnum);
		any |= find_quest_giver_in_list(QUEST_ENDS_AT(qiter), QG_QUEST, vnum);
		
		if (any) {
			++found;
			size += snprintf(buf + size, sizeof(buf) - size, "QST [%5d] %s\r\n", QUEST_VNUM(qiter), QUEST_NAME(qiter));
		}
	}
	
	// on shops
	HASH_ITER(hh, shop_table, shop, next_shop) {
		if (size >= sizeof(buf)) {
			break;
		}
		// QG_x: shop types
		any = find_quest_giver_in_list(SHOP_LOCATIONS(shop), QG_QUEST, vnum);
		
		if (any) {
			++found;
			size += snprintf(buf + size, sizeof(buf) - size, "SHOP [%5d] %s\r\n", SHOP_VNUM(shop), SHOP_NAME(shop));
		}
	}
	
	// on socials
	HASH_ITER(hh, social_table, soc, next_soc) {
		if (size >= sizeof(buf)) {
			break;
		}
		// REQ_x: quest types
		any = find_requirement_in_list(SOC_REQUIREMENTS(soc), REQ_COMPLETED_QUEST, vnum);
		any |= find_requirement_in_list(SOC_REQUIREMENTS(soc), REQ_NOT_COMPLETED_QUEST, vnum);
		any |= find_requirement_in_list(SOC_REQUIREMENTS(soc), REQ_NOT_ON_QUEST, vnum);
		
		if (any) {
			++found;
			size += snprintf(buf + size, sizeof(buf) - size, "SOC [%5d] %s\r\n", SOC_VNUM(soc), SOC_NAME(soc));
		}
	}
	
	if (found > 0) {
		size += snprintf(buf + size, sizeof(buf) - size, "%d location%s shown\r\n", found, PLURAL(found));
	}
	else {
		size += snprintf(buf + size, sizeof(buf) - size, " none\r\n");
	}
	
	page_string(ch->desc, buf, TRUE);
}


/**
* Parses the <vnum> arg for a quest/event reward add/change.
*
* @param char_data *ch The person editing.
* @param int type The QR_ reward type.
* @param char *vnum_arg The argument which the player supplied.
* @param char *prev_arg The argument right BEFORE that one (if it's the quantity arg, because types that don't use quantity will want that instead).
* @return any_vnum The 'vnum' field for the reward, or NOTHING if it failed.
*/
any_vnum parse_quest_reward_vnum(char_data *ch, int type, char *vnum_arg, char *prev_arg) {
	faction_data *fct;
	event_data *event;
	any_vnum vnum = 0;
	bool ok = FALSE;
	
	// QR_x: validate vnum
	switch (type) {
		case QR_BONUS_EXP: {
			// vnum not required
			ok = TRUE;
			break;
		}
		case QR_COINS: {
			if (is_abbrev(vnum_arg, "miscellaneous") || is_abbrev(vnum_arg, "simple") || is_abbrev(vnum_arg, "other")) {
				vnum = OTHER_COIN;
				ok = TRUE;
			}
			else if (is_abbrev(vnum_arg, "empire")) {
				vnum = REWARD_EMPIRE_COIN;
				ok = TRUE;
			}
			else {
				msg_to_char(ch, "You must choose misc or empire coins.\r\n");
				return NOTHING;
			}
			break;	
		}
		case QR_CURRENCY: {
			if (!*vnum_arg) {
				msg_to_char(ch, "You must specify a currency (generic) vnum.\r\n");
				return NOTHING;
			}
			if (!isdigit(*vnum_arg) || (vnum = atoi(vnum_arg)) < 0) {
				msg_to_char(ch, "Invalid generic vnum '%s'.\r\n", vnum_arg);
				return NOTHING;
			}
			if (find_generic(vnum, GENERIC_CURRENCY)) {
				ok = TRUE;
			}
			break;
		}
		case QR_OBJECT: {
			if (!*vnum_arg) {
				msg_to_char(ch, "You must specify an object vnum.\r\n");
				return NOTHING;
			}
			if (!isdigit(*vnum_arg) || (vnum = atoi(vnum_arg)) < 0) {
				msg_to_char(ch, "Invalid obj vnum '%s'.\r\n", vnum_arg);
				return NOTHING;
			}
			if (obj_proto(vnum)) {
				ok = TRUE;
			}
			break;
		}
		case QR_SET_SKILL:
		case QR_SKILL_EXP:
		case QR_SKILL_LEVELS: {
			if (!*vnum_arg) {
				msg_to_char(ch, "You must specify a skill vnum.\r\n");
				return NOTHING;
			}
			if (!isdigit(*vnum_arg) || (vnum = atoi(vnum_arg)) < 0) {
				msg_to_char(ch, "Invalid skill vnum '%s'.\r\n", vnum_arg);
				return NOTHING;
			}
			if (find_skill_by_vnum(vnum)) {
				ok = TRUE;
			}
			break;
		}
		case QR_QUEST_CHAIN: {
			if (!*vnum_arg) {
				strcpy(vnum_arg, prev_arg);	// does not generally need 2 args
			}
			if (!*vnum_arg || !isdigit(*vnum_arg) || (vnum = atoi(vnum_arg)) < 0) {
				msg_to_char(ch, "Invalid quest vnum '%s'.\r\n", vnum_arg);
				return NOTHING;
			}
			if (quest_proto(vnum)) {
				ok = TRUE;
			}
			break;
		}
		case QR_REPUTATION: {
			if (!*vnum_arg) {
				msg_to_char(ch, "You must specify a faction name or vnum.\r\n");
				return NOTHING;
			}
			if (!(fct = find_faction(vnum_arg))) {
				msg_to_char(ch, "Invalid faction '%s'.\r\n", vnum_arg);
				return NOTHING;
			}
			vnum = FCT_VNUM(fct);
			ok = TRUE;
			break;
		}
		case QR_EVENT_POINTS: {
			if (!*vnum_arg || !isdigit(*vnum_arg)) {
				msg_to_char(ch, "You must specify an event vnum.\r\n");
				return NOTHING;
			}
			if (!(event = find_event_by_vnum(atoi(vnum_arg)))) {
				msg_to_char(ch, "Invalid event vnum '%s'.\r\n", vnum_arg);
				return NOTHING;
			}
			vnum = EVT_VNUM(event);
			ok = TRUE;
			break;
		}
	}
	
	// did we find one?
	if (!ok) {
		msg_to_char(ch, "Unable to find %s %d.\r\n", quest_reward_types[type], vnum);
		return NOTHING;
	}
	
	return vnum;
}


/**
* Processing for quest-givers (starts at, ends at).
*
* @param char_data *ch The player using OLC.
* @param char *argument The full argument after the command.
* @param struct quest_giver **list A pointer to the list we're adding/changing.
* @param char *command The command used by the player (starts, ends).
*/
void qedit_process_quest_givers(char_data *ch, char *argument, struct quest_giver **list, char *command) {
	char cmd_arg[MAX_INPUT_LENGTH], field_arg[MAX_INPUT_LENGTH];
	char num_arg[MAX_INPUT_LENGTH], type_arg[MAX_INPUT_LENGTH];
	char vnum_arg[MAX_INPUT_LENGTH], buf[MAX_STRING_LENGTH];
	struct quest_giver *giver, *iter, *change, *copyfrom;
	int findtype, num, type;
	bool found, ok;
	any_vnum vnum;
	
	argument = any_one_arg(argument, cmd_arg);	// add/remove/change/copy
	
	if (is_abbrev(cmd_arg, "copy")) {
		// usage: qedit starts/ends copy <from type> <from vnum> <starts/ends>
		argument = any_one_arg(argument, type_arg);	// just "quest" for now
		argument = any_one_arg(argument, vnum_arg);	// any vnum for that type
		argument = any_one_arg(argument, field_arg);	// starts/ends
		
		if (!*type_arg || !*vnum_arg) {
			msg_to_char(ch, "Usage: %s copy <from type> <from vnum> [starts | ends]\r\n", command);
		}
		else if ((findtype = find_olc_type(type_arg)) == 0) {
			msg_to_char(ch, "Unknown olc type '%s'.\r\n", type_arg);
		}
		else if (!isdigit(*vnum_arg)) {
			sprintbit(findtype, olc_type_bits, buf, FALSE);
			msg_to_char(ch, "Copy from which %s?\r\n", buf);
		}
		else if ((vnum = atoi(vnum_arg)) < 0) {
			msg_to_char(ch, "Invalid vnum.\r\n");
		}
		else {
			sprintbit(findtype, olc_type_bits, buf, FALSE);
			copyfrom = NULL;
			
			switch (findtype) {
				case OLC_QUEST: {
					// requires starts/ends
					if (!*field_arg || (!is_abbrev(field_arg, "starts") && !is_abbrev(field_arg, "ends"))) {
						msg_to_char(ch, "Copy from the 'starts' or 'ends' list?\r\n");
						return;
					}
					quest_data *from_qst = quest_proto(vnum);
					if (from_qst) {
						copyfrom = (is_abbrev(field_arg, "starts") ? QUEST_STARTS_AT(from_qst) : QUEST_ENDS_AT(from_qst));
					}
					break;
				}
				case OLC_SHOP: {
					shop_data *from_shp = real_shop(vnum);
					if (from_shp) {
						copyfrom = SHOP_LOCATIONS(from_shp);
					}
					break;
				}
				default: {
					msg_to_char(ch, "You can't copy '%s' from %ss.\r\n", command, buf);
					return;
				}
			}
			
			if (!copyfrom) {
				msg_to_char(ch, "Invalid %s vnum '%s'.\r\n", buf, vnum_arg);
			}
			else {
				smart_copy_quest_givers(list, copyfrom);
				msg_to_char(ch, "Copied '%s' from %s %d.\r\n", command, buf, vnum);
			}
		}
	}	// end 'copy'
	else if (is_abbrev(cmd_arg, "remove")) {
		// usage: qedit starts|ends remove <number | all>
		skip_spaces(&argument);	// only arg is number
		
		if (!*argument) {
			msg_to_char(ch, "Remove which '%s' (number)?\r\n", command);
		}
		else if (!str_cmp(argument, "all")) {
			free_quest_givers(*list);
			*list = NULL;
			msg_to_char(ch, "You remove all the '%s'.\r\n", command);
		}
		else if (!isdigit(*argument) || (num = atoi(argument)) < 1) {
			msg_to_char(ch, "Invalid '%s' number.\r\n", command);
		}
		else {
			found = FALSE;
			LL_FOREACH(*list, iter) {
				if (--num == 0) {
					found = TRUE;
					
					msg_to_char(ch, "You remove the '%s' info for %s %d.\r\n", command, quest_giver_types[iter->type], iter->vnum);
					LL_DELETE(*list, iter);
					free(iter);
					break;
				}
			}
			
			if (!found) {
				msg_to_char(ch, "Invalid '%s' number.\r\n", command);
			}
		}
	}	// end 'remove'
	else if (is_abbrev(cmd_arg, "add")) {
		// usage: qedit starts|ends add <type> <vnum>
		argument = any_one_arg(argument, type_arg);
		argument = any_one_arg(argument, vnum_arg);
		
		if (!*type_arg || !*vnum_arg) {
			msg_to_char(ch, "Usage: %s add <type> <vnum>\r\n", command);
		}
		else if ((type = search_block(type_arg, quest_giver_types, FALSE)) == NOTHING) {
			msg_to_char(ch, "Invalid type '%s'.\r\n", type_arg);
		}
		else if (!isdigit(*vnum_arg) || (vnum = atoi(vnum_arg)) < 0) {
			msg_to_char(ch, "Invalid vnum '%s'.\r\n", vnum_arg);
		}
		else {
			// QG_x: validate vnum
			ok = FALSE;
			switch (type) {
				case QG_BUILDING: {
					if (building_proto(vnum)) {
						ok = TRUE;
					}
					break;
				}
				case QG_MOBILE: {
					if (mob_proto(vnum)) {
						ok = TRUE;
					}
					break;
				}
				case QG_OBJECT: {
					if (obj_proto(vnum)) {
						ok = TRUE;
					}
					break;
				}
				case QG_ROOM_TEMPLATE: {
					if (room_template_proto(vnum)) {
						ok = TRUE;
					}
					break;
				}
				case QG_TRIGGER: {
					if (real_trigger(vnum)) {
						ok = TRUE;
					}
					break;
				}
				case QG_QUEST: {
					if (quest_proto(vnum)) {
						ok = TRUE;
					}
					break;
				}
				case QG_VEHICLE: {
					if (vehicle_proto(vnum)) {
						ok = TRUE;
					}
					break;
				}
			}
			
			// did we find one? if so, buf is set
			if (!ok) {
				msg_to_char(ch, "Unable to find %s %d.\r\n", quest_giver_types[type], vnum);
				return;
			}
			
			// success
			CREATE(giver, struct quest_giver, 1);
			giver->type = type;
			giver->vnum = vnum;
			
			LL_APPEND(*list, giver);
			msg_to_char(ch, "You add '%s': %s\r\n", command, quest_giver_string(giver, TRUE));
		}
	}	// end 'add'
	else if (is_abbrev(cmd_arg, "change")) {
		// usage: qedit starts|ends change <number> vnum <number>
		argument = any_one_arg(argument, num_arg);
		argument = any_one_arg(argument, field_arg);
		argument = any_one_arg(argument, vnum_arg);
		
		if (!*num_arg || !isdigit(*num_arg) || !*field_arg || !*vnum_arg) {
			msg_to_char(ch, "Usage: %s change <number> vnum <value>\r\n", command);
			return;
		}
		
		// find which one to change
		num = atoi(num_arg);
		change = NULL;
		LL_FOREACH(*list, iter) {
			if (--num == 0) {
				change = iter;
				break;
			}
		}
		
		if (!change) {
			msg_to_char(ch, "Invalid '%s' number.\r\n", command);
		}
		else if (is_abbrev(field_arg, "vnum")) {
			if (!isdigit(*vnum_arg) || (vnum = atoi(vnum_arg)) < 0) {
				msg_to_char(ch, "Invalid vnum '%s'.\r\n", vnum_arg);
				return;
			}
			
			// QG_x: validate vnum
			ok = FALSE;
			switch (change->type) {
				case QG_BUILDING: {
					if (building_proto(vnum)) {
						ok = TRUE;
					}
					break;
				}
				case QG_MOBILE: {
					if (mob_proto(vnum)) {
						ok = TRUE;
					}
					break;
				}
				case QG_OBJECT: {
					if (obj_proto(vnum)) {
						ok = TRUE;
					}
					break;
				}
				case QG_ROOM_TEMPLATE: {
					if (room_template_proto(vnum)) {
						ok = TRUE;
					}
					break;
				}
				case QG_TRIGGER: {
					if (real_trigger(vnum)) {
						ok = TRUE;
					}
					break;
				}
				case QG_QUEST: {
					if (quest_proto(vnum)) {
						ok = TRUE;
					}
					break;
				}
				case QG_VEHICLE: {
					if (vehicle_proto(vnum)) {
						ok = TRUE;
					}
					break;
				}
			}
			
			// did we find one? if so, buf is set
			if (!ok) {
				msg_to_char(ch, "Unable to find %s %d.\r\n", quest_giver_types[change->type], vnum);
				return;
			}
			
			change->vnum = vnum;
			msg_to_char(ch, "Changed '%s' %d to: %s\r\n", command, atoi(num_arg), quest_giver_string(change, TRUE));
		}
		else {
			msg_to_char(ch, "You can only change the vnum.\r\n");
		}
	}	// end 'change'
	else {
		msg_to_char(ch, "Usage: %s add <type> <vnum>\r\n", command);
		msg_to_char(ch, "Usage: %s change <number> vnum <value>\r\n", command);
		msg_to_char(ch, "Usage: %s copy <from type> <from vnum> [starts/ends]\r\n", command);
		msg_to_char(ch, "Usage: %s remove <number | all>\r\n", command);
	}
}


// Simple vnum sorter for the quest hash
int sort_quests(quest_data *a, quest_data *b) {
	return QUEST_VNUM(a) - QUEST_VNUM(b);
}


 //////////////////////////////////////////////////////////////////////////////
//// DATABASE ////////////////////////////////////////////////////////////////

/**
* @param any_vnum vnum Any quest vnum
* @return quest_data* The quest, or NULL if it doesn't exist
*/
quest_data *quest_proto(any_vnum vnum) {
	quest_data *quest;
	
	if (vnum < 0 || vnum == NOTHING) {
		return NULL;
	}
	
	HASH_FIND_INT(quest_table, &vnum, quest);
	return quest;
}


/**
* Puts a quest into the hash table.
*
* @param quest_data *quest The quest data to add to the table.
*/
void add_quest_to_table(quest_data *quest) {
	quest_data *find;
	any_vnum vnum;
	
	if (quest) {
		vnum = QUEST_VNUM(quest);
		HASH_FIND_INT(quest_table, &vnum, find);
		if (!find) {
			HASH_ADD_INT(quest_table, vnum, quest);
			HASH_SORT(quest_table, sort_quests);
		}
	}
}


/**
* Removes a quest from the hash table.
*
* @param quest_data *quest The quest data to remove from the table.
*/
void remove_quest_from_table(quest_data *quest) {
	HASH_DEL(quest_table, quest);
}


/**
* Adds a quest to a temporary list, if it's not already there.
*
* @param struct quest_temp_list **list A pointer to the temporary list to add to.
* @param quest_data *quest The quest to add.
* @param struct instance_data *inst The associated instance for the quest (may be NULL).
*/
void add_to_quest_temp_list(struct quest_temp_list **list, quest_data *quest, struct instance_data *inst) {
	struct quest_temp_list *qtl;
	bool found = FALSE;
	
	LL_FOREACH(*list, qtl) {
		if (qtl->quest == quest) {
			found = TRUE;
			break;
		}
	}
	
	if (!found) {
		CREATE(qtl, struct quest_temp_list, 1);
		qtl->quest = quest;
		qtl->instance = inst;
		LL_PREPEND(*list, qtl);
	}
}


/**
* Initializes a new quest. This clears all memory for it, so set the vnum
* AFTER.
*
* @param quest_data *quest The quest to initialize.
*/
void clear_quest(quest_data *quest) {
	memset((char *) quest, 0, sizeof(quest_data));
	
	QUEST_VNUM(quest) = NOTHING;
	QUEST_REPEATABLE_AFTER(quest) = NOT_REPEATABLE;
	QUEST_DAILY_CYCLE(quest) = NOTHING;
}


/**
* @param struct quest_giver *from The list to copy.
* @return struct quest_giver* The copy of the list.
*/
struct quest_giver *copy_quest_givers(struct quest_giver *from) {
	struct quest_giver *el, *iter, *list = NULL, *end = NULL;
	
	LL_FOREACH(from, iter) {
		CREATE(el, struct quest_giver, 1);
		*el = *iter;
		el->next = NULL;
		
		if (end) {
			end->next = el;
		}
		else {
			list = el;
		}
		end = el;
	}
	
	return list;
}


/**
* @param struct quest_reward *from The list to copy.
* @return struct quest_reward* The copy of the list.
*/
struct quest_reward *copy_quest_rewards(struct quest_reward *from) {
	struct quest_reward *el, *iter, *list = NULL, *end = NULL;
	
	LL_FOREACH(from, iter) {
		CREATE(el, struct quest_reward, 1);
		*el = *iter;
		el->next = NULL;
		
		if (end) {
			end->next = el;
		}
		else {
			list = el;
		}
		end = el;
	}
	
	return list;
}


/**
* Frees a player completed-quests hash.
*
* @param struct player_completed_quest **hash A pointer to the hash to free.
*/
void free_player_completed_quests(struct player_completed_quest **hash) {
	struct player_completed_quest *pcq, *next_pcq;
	HASH_ITER(hh, *hash, pcq, next_pcq) {
		HASH_DEL(*hash, pcq);
		free(pcq);
	}
	*hash = NULL;
}


/**
* Frees a player quest list.
*
* @param struct player_quest *list The list of player quests to free.
*/
void free_player_quests(struct player_quest *list) {
	struct player_quest *pq;
	while ((pq = list)) {
		list = pq->next;
		free_requirements(pq->tracker);
		free(pq);
	}
}


/**
* @param struct quest_giver *list The list to free.
*/
void free_quest_givers(struct quest_giver *list) {
	struct quest_giver *iter, *next_iter;
	LL_FOREACH_SAFE(list, iter, next_iter) {
		free(iter);
	}
}


/**
* @param struct quest_reward *list The list to free.
*/
void free_quest_rewards(struct quest_reward *list) {
	struct quest_reward *iter, *next_iter;
	LL_FOREACH_SAFE(list, iter, next_iter) {
		free(iter);
	}
}


/**
* Frees a temporary quest list.
*
* @param struct quest_temp_list *list The list to free.
*/
void free_quest_temp_list(struct quest_temp_list *list) {
	struct quest_temp_list *qtl, *next_qtl;
	LL_FOREACH_SAFE(list, qtl, next_qtl) {
		free(qtl);
	}
}


/**
* frees up memory for a quest data item.
*
* See also: olc_delete_quest
*
* @param quest_data *quest The quest data to free.
*/
void free_quest(quest_data *quest) {
	quest_data *proto = quest_proto(QUEST_VNUM(quest));
	
	// strings
	if (QUEST_NAME(quest) && (!proto || QUEST_NAME(quest) != QUEST_NAME(proto))) {
		free(QUEST_NAME(quest));
	}
	if (QUEST_DESCRIPTION(quest) && (!proto || QUEST_DESCRIPTION(quest) != QUEST_DESCRIPTION(proto))) {
		free(QUEST_DESCRIPTION(quest));
	}
	if (QUEST_COMPLETE_MSG(quest) && (!proto || QUEST_COMPLETE_MSG(quest) != QUEST_COMPLETE_MSG(proto))) {
		free(QUEST_COMPLETE_MSG(quest));
	}
	
	// pointers
	if (QUEST_STARTS_AT(quest) && (!proto || QUEST_STARTS_AT(quest) != QUEST_STARTS_AT(proto))) {
		free_quest_givers(QUEST_STARTS_AT(quest));
	}
	if (QUEST_ENDS_AT(quest) && (!proto || QUEST_ENDS_AT(quest) != QUEST_ENDS_AT(proto))) {
		free_quest_givers(QUEST_ENDS_AT(quest));
	}
	if (QUEST_TASKS(quest) && (!proto || QUEST_TASKS(quest) != QUEST_TASKS(proto))) {
		free_requirements(QUEST_TASKS(quest));
	}
	if (QUEST_REWARDS(quest) && (!proto || QUEST_REWARDS(quest) != QUEST_REWARDS(proto))) {
		free_quest_rewards(QUEST_REWARDS(quest));
	}
	if (QUEST_PREREQS(quest) && (!proto || QUEST_PREREQS(quest) != QUEST_PREREQS(proto))) {
		free_requirements(QUEST_PREREQS(quest));
	}
	if (QUEST_SCRIPTS(quest) && (!proto || QUEST_SCRIPTS(quest) != QUEST_SCRIPTS(proto))) {
		free_proto_scripts(&QUEST_SCRIPTS(quest));
	}
	
	free(quest);
}


/**
* Loads data from the daily quest file and activates the correct daily quests.
*/
void load_daily_quest_file(void) {
	char line[MAX_INPUT_LENGTH];
	quest_data *qst, *next_qst;
	any_vnum vnum;
	FILE *fl;
	
	if (!(fl = fopen(DAILY_QUEST_FILE, "r"))) {
		// there just isn't a file -- that's ok, we'll generate it
		setup_daily_quest_cycles(NOTHING);
		return;
	}
	
	// turn off all dailies
	HASH_ITER(hh, quest_table, qst, next_qst) {
		if (QUEST_FLAGGED(qst, QST_DAILY) && QUEST_DAILY_CYCLE(qst) != NOTHING) {
			QUEST_DAILY_ACTIVE(qst) = FALSE;
		}
		else {
			QUEST_DAILY_ACTIVE(qst) = TRUE;
		}
	}
	
	while (get_line(fl, line)) {
		if (*line == 'S') {
			break;	// end
		}
		else if (isdigit(*line) && (vnum = atoi(line)) != NOTHING) {
			if ((qst = quest_proto(vnum))) {
				// activate it
				QUEST_DAILY_ACTIVE(qst) = TRUE;
			}
		}
	}
	
	fclose(fl);
}


/**
* Parses a quest giver, saved as:
*
* A
* 1 123
*
* @param FILE *fl The file, having just read the letter tag.
* @param struct quest_giver **list The list to append to.
* @param char *error_str How to report if there is an error.
*/
void parse_quest_giver(FILE *fl, struct quest_giver **list, char *error_str) {
	struct quest_giver *giver;
	char line[256];
	any_vnum vnum;
	int type;
	
	if (!fl || !list || !get_line(fl, line)) {
		log("SYSERR: data error in quest giver line of %s", error_str ? error_str : "UNKNOWN");
		exit(1);
	}
	if (sscanf(line, "%d %d", &type, &vnum) != 2) {
		log("SYSERR: format error in quest giver line of %s", error_str ? error_str : "UNKNOWN");
		exit(1);
	}
	
	CREATE(giver, struct quest_giver, 1);
	giver->type = type;
	giver->vnum = vnum;
	
	LL_APPEND(*list, giver);
}


/**
* Parses a quest reward, saved as:
*
* A
* 1 123 2
*
* @param FILE *fl The file, having just read the letter tag.
* @param struct quest_reward **list The list to append to.
* @param char *error_str How to report if there is an error.
*/
void parse_quest_reward(FILE *fl, struct quest_reward **list, char *error_str) {
	struct quest_reward *reward;
	int type, amount;
	char line[256];
	any_vnum vnum;
	
	if (!fl || !list || !get_line(fl, line)) {
		log("SYSERR: data error in quest reward line of %s", error_str ? error_str : "UNKNOWN");
		exit(1);
	}
	if (sscanf(line, "%d %d %d", &type, &vnum, &amount) != 3) {
		log("SYSERR: format error in quest reward line of %s", error_str ? error_str : "UNKNOWN");
		exit(1);
	}
	
	CREATE(reward, struct quest_reward, 1);
	reward->type = type;
	reward->vnum = vnum;
	reward->amount = amount;
	
	LL_APPEND(*list, reward);
}


/**
* Read one quest from file.
*
* @param FILE *fl The open .qst file
* @param any_vnum vnum The quest vnum
*/
void parse_quest(FILE *fl, any_vnum vnum) {
	void parse_requirement(FILE *fl, struct req_data **list, char *error_str);
	
	char line[256], error[256], str_in[256];
	quest_data *quest, *find;
	int int_in[4];
	
	CREATE(quest, quest_data, 1);
	clear_quest(quest);
	QUEST_VNUM(quest) = vnum;
	
	HASH_FIND_INT(quest_table, &vnum, find);
	if (find) {
		log("WARNING: Duplicate quest vnum #%d", vnum);
		// but have to load it anyway to advance the file
	}
	add_quest_to_table(quest);
		
	// for error messages
	sprintf(error, "quest vnum %d", vnum);
	
	// lines 1-3: strings
	QUEST_NAME(quest) = fread_string(fl, error);
	QUEST_DESCRIPTION(quest) = fread_string(fl, error);
	QUEST_COMPLETE_MSG(quest) = fread_string(fl, error);
	
	// 4. version flags min max repeatable-after
	if (!get_line(fl, line) || sscanf(line, "%d %s %d %d %d", &int_in[0], str_in, &int_in[1], &int_in[2], &int_in[3]) != 5) {
		log("SYSERR: Format error in line 4 of %s", error);
		exit(1);
	}
	
	QUEST_VERSION(quest) = int_in[0];
	QUEST_FLAGS(quest) = asciiflag_conv(str_in);
	QUEST_MIN_LEVEL(quest) = int_in[1];
	QUEST_MAX_LEVEL(quest) = int_in[2];
	QUEST_REPEATABLE_AFTER(quest) = int_in[3];
	
	// optionals
	for (;;) {
		if (!get_line(fl, line)) {
			log("SYSERR: Format error in %s, expecting alphabetic flags", error);
			exit(1);
		}
		switch (*line) {
			case 'A': {	// starts at
				parse_quest_giver(fl, &QUEST_STARTS_AT(quest), error);
				break;
			}
			case 'D': {	// daily cycle
				if (sscanf(line, "D %d", &int_in[0]) != 1) {
					log("SYSERR: Format error in D flag of %s", error);
					exit(1);
				}
				
				QUEST_DAILY_CYCLE(quest) = int_in[0];
				break;
			}
			case 'P': {	// preq-requisites
				parse_requirement(fl, &QUEST_PREREQS(quest), error);
				break;
			}
			case 'R': {	// rewards
				parse_quest_reward(fl, &QUEST_REWARDS(quest), error);
				break;
			}
			case 'T': {	// triggers
				parse_trig_proto(line, &QUEST_SCRIPTS(quest), error);
				break;
			}
			case 'W': {	// tasks / work
				parse_requirement(fl, &QUEST_TASKS(quest), error);
				break;
			}
			case 'Z': {	// ends at
				parse_quest_giver(fl, &QUEST_ENDS_AT(quest), error);
				break;
			}
			
			// end
			case 'S': {
				return;
			}
			
			default: {
				log("SYSERR: Format error in %s, expecting alphabetic flags", error);
				exit(1);
			}
		}
	}
}


/**
* Saves a list of which "cycled" daily quests are on.
*/
void write_daily_quest_file(void) {
	quest_data *qst, *next_qst;
	FILE *fl;
	
	if (!(fl = fopen(DAILY_QUEST_FILE TEMP_SUFFIX, "w"))) {
		log("SYSERR: Unable to write %s", DAILY_QUEST_FILE TEMP_SUFFIX);
		return;
	}
	
	HASH_ITER(hh, quest_table, qst, next_qst) {
		if (!QUEST_FLAGGED(qst, QST_DAILY) || QUEST_DAILY_CYCLE(qst) == NOTHING) {
			continue; // not a cycled daily
		}
		
		// write if on
		if (QUEST_DAILY_ACTIVE(qst)) {
			fprintf(fl, "%d\n", QUEST_VNUM(qst));
		}
	}
	
	// end
	fprintf(fl, "S\n");
	fclose(fl);
	
	rename(DAILY_QUEST_FILE TEMP_SUFFIX, DAILY_QUEST_FILE);
}


// writes entries in the quest index
void write_quest_index(FILE *fl) {
	quest_data *quest, *next_quest;
	int this, last;
	
	last = -1;
	HASH_ITER(hh, quest_table, quest, next_quest) {
		// determine "zone number" by vnum
		this = (int)(QUEST_VNUM(quest) / 100);
	
		if (this != last) {
			fprintf(fl, "%d%s\n", this, QST_SUFFIX);
			last = this;
		}
	}
}


/**
* Writes a list of 'quest_giver' to a data file.
*
* @param FILE *fl The file, open for writing.
* @param char letter The tag letter.
* @param struct quest_giver *list The list to write.
*/
void write_quest_givers_to_file(FILE *fl, char letter, struct quest_giver *list) {
	struct quest_giver *iter;
	LL_FOREACH(list, iter) {
		fprintf(fl, "%c\n%d %d\n", letter, iter->type, iter->vnum);
	}
}


/**
* Writes a list of 'quest_reward' to a data file.
*
* @param FILE *fl The file, open for writing.
* @param char letter The tag letter.
* @param struct quest_reward *list The list to write.
*/
void write_quest_rewards_to_file(FILE *fl, char letter, struct quest_reward *list) {
	struct quest_reward *iter;
	LL_FOREACH(list, iter) {
		fprintf(fl, "%c\n%d %d %d\n", letter, iter->type, iter->vnum, iter->amount);
	}
}


/**
* Outputs one quest item in the db file format, starting with a #VNUM and
* ending with an S.
*
* @param FILE *fl The file to write it to.
* @param quest_data *quest The thing to save.
*/
void write_quest_to_file(FILE *fl, quest_data *quest) {
	void write_requirements_to_file(FILE *fl, char letter, struct req_data *list);
	void write_trig_protos_to_file(FILE *fl, char letter, struct trig_proto_list *list);
	
	char temp[MAX_STRING_LENGTH];
	
	if (!fl || !quest) {
		syslog(SYS_ERROR, LVL_START_IMM, TRUE, "SYSERR: write_quest_to_file called without %s", !fl ? "file" : "quest");
		return;
	}
	
	fprintf(fl, "#%d\n", QUEST_VNUM(quest));
	
	// 1. name
	fprintf(fl, "%s~\n", NULLSAFE(QUEST_NAME(quest)));
	
	// 2. desc
	strcpy(temp, NULLSAFE(QUEST_DESCRIPTION(quest)));
	strip_crlf(temp);
	fprintf(fl, "%s~\n", temp);
	
	// 3. complete msg
	strcpy(temp, NULLSAFE(QUEST_COMPLETE_MSG(quest)));
	strip_crlf(temp);
	fprintf(fl, "%s~\n", temp);
	
	// 4. version flags min max repeatable-after
	fprintf(fl, "%d %s %d %d %d\n", QUEST_VERSION(quest), bitv_to_alpha(QUEST_FLAGS(quest)), QUEST_MIN_LEVEL(quest), QUEST_MAX_LEVEL(quest), QUEST_REPEATABLE_AFTER(quest));
		
	// A. starts at
	write_quest_givers_to_file(fl, 'A', QUEST_STARTS_AT(quest));
	
	// D. daily cycle
	if (QUEST_FLAGGED(quest, QST_DAILY) && QUEST_DAILY_CYCLE(quest) != NOTHING) {
		fprintf(fl, "D %d\n", QUEST_DAILY_CYCLE(quest));
	}
	
	// P. pre-requisites
	write_requirements_to_file(fl, 'P', QUEST_PREREQS(quest));
	
	// R. rewards
	write_quest_rewards_to_file(fl, 'R', QUEST_REWARDS(quest));
	
	// T. triggers
	write_trig_protos_to_file(fl, 'T', QUEST_SCRIPTS(quest));
	
	// W. tasks (work)
	write_requirements_to_file(fl, 'W', QUEST_TASKS(quest));
	
	// Z. ends at
	write_quest_givers_to_file(fl, 'Z', QUEST_ENDS_AT(quest));
	
	// end
	fprintf(fl, "S\n");
}


 //////////////////////////////////////////////////////////////////////////////
//// OLC HANDLERS ////////////////////////////////////////////////////////////


/**
* Creates a new quest entry.
* 
* @param any_vnum vnum The number to create.
* @return quest_data* The new quest's prototype.
*/
quest_data *create_quest_table_entry(any_vnum vnum) {
	quest_data *quest;
	
	// sanity
	if (quest_proto(vnum)) {
		log("SYSERR: Attempting to insert quest at existing vnum %d", vnum);
		return quest_proto(vnum);
	}
	
	CREATE(quest, quest_data, 1);
	clear_quest(quest);
	QUEST_VNUM(quest) = vnum;
	QUEST_NAME(quest) = str_dup(default_quest_name);
	QUEST_DESCRIPTION(quest) = str_dup(default_quest_description);
	QUEST_COMPLETE_MSG(quest) = str_dup(default_quest_complete_msg);
	QUEST_FLAGS(quest) = QST_IN_DEVELOPMENT;
	add_quest_to_table(quest);

	// save index and quest file now
	save_index(DB_BOOT_QST);
	save_library_file_for_vnum(DB_BOOT_QST, vnum);

	return quest;
}


/**
* WARNING: This function actually deletes a quest.
*
* @param char_data *ch The person doing the deleting.
* @param any_vnum vnum The vnum to delete.
*/
void olc_delete_quest(char_data *ch, any_vnum vnum) {
	quest_data *quest, *qiter, *next_qiter;
	event_data *event, *next_event;
	progress_data *prg, *next_prg;
	social_data *soc, *next_soc;
	shop_data *shop, *next_shop;
	descriptor_data *desc;
	char_data *chiter;
	bool found;
	
	if (!(quest = quest_proto(vnum))) {
		msg_to_char(ch, "There is no such quest %d.\r\n", vnum);
		return;
	}
	
	// remove it from the hash table first
	remove_quest_from_table(quest);
	
	// look for people on the quest and force a refresh
	LL_FOREACH(character_list, chiter) {
		if (IS_NPC(chiter)) {
			continue;
		}
		if (!is_on_quest(chiter, vnum)) {
			continue;
		}
		refresh_all_quests(chiter);
	}
	
	// save index and quest file now
	save_index(DB_BOOT_QST);
	save_library_file_for_vnum(DB_BOOT_QST, vnum);
	
	// delete from lookups
	add_or_remove_all_quest_lookups_for(quest, FALSE);
	
	// update events
	HASH_ITER(hh, event_table, event, next_event) {
		// QR_x: event reward types
		found = delete_event_reward_from_list(&EVT_RANK_REWARDS(event), QR_QUEST_CHAIN, vnum);
		found |= delete_event_reward_from_list(&EVT_THRESHOLD_REWARDS(event), QR_QUEST_CHAIN, vnum);
		
		if (found) {
			// SET_BIT(EVT_FLAGS(event), EVTF_IN_DEVELOPMENT);
			save_library_file_for_vnum(DB_BOOT_EVT, EVT_VNUM(event));
		}
	}
	
	// update progress
	HASH_ITER(hh, progress_table, prg, next_prg) {
		found = delete_requirement_from_list(&PRG_TASKS(prg), REQ_COMPLETED_QUEST, vnum);
		found |= delete_requirement_from_list(&PRG_TASKS(prg), REQ_NOT_COMPLETED_QUEST, vnum);
		found |= delete_requirement_from_list(&PRG_TASKS(prg), REQ_NOT_ON_QUEST, vnum);
		
		if (found) {
			SET_BIT(PRG_FLAGS(prg), PRG_IN_DEVELOPMENT);
			save_library_file_for_vnum(DB_BOOT_PRG, PRG_VNUM(prg));
			need_progress_refresh = TRUE;
		}
	}
	
	// update other quests
	HASH_ITER(hh, quest_table, qiter, next_qiter) {
		// REQ_x, QR_x: quest types
		found = delete_requirement_from_list(&QUEST_TASKS(qiter), REQ_COMPLETED_QUEST, vnum);
		found |= delete_requirement_from_list(&QUEST_PREREQS(qiter), REQ_COMPLETED_QUEST, vnum);
		found |= delete_requirement_from_list(&QUEST_TASKS(qiter), REQ_NOT_COMPLETED_QUEST, vnum);
		found |= delete_requirement_from_list(&QUEST_PREREQS(qiter), REQ_NOT_COMPLETED_QUEST, vnum);
		found |= delete_requirement_from_list(&QUEST_TASKS(qiter), REQ_NOT_ON_QUEST, vnum);
		found |= delete_requirement_from_list(&QUEST_PREREQS(qiter), REQ_NOT_ON_QUEST, vnum);
		found |= delete_quest_reward_from_list(&QUEST_REWARDS(qiter), QR_QUEST_CHAIN, vnum);
		found |= delete_quest_giver_from_list(&QUEST_STARTS_AT(qiter), QG_QUEST, vnum);
		found |= delete_quest_giver_from_list(&QUEST_ENDS_AT(qiter), QG_QUEST, vnum);
		
		if (found) {
			SET_BIT(QUEST_FLAGS(qiter), QST_IN_DEVELOPMENT);
			save_library_file_for_vnum(DB_BOOT_QST, QUEST_VNUM(qiter));
		}
	}
	
	// update shops
	HASH_ITER(hh, shop_table, shop, next_shop) {
		// QG_x: quest types
		found = delete_quest_giver_from_list(&SHOP_LOCATIONS(shop), QG_QUEST, vnum);
		
		if (found) {
			SET_BIT(SHOP_FLAGS(shop), SHOP_IN_DEVELOPMENT);
			save_library_file_for_vnum(DB_BOOT_SHOP, SHOP_VNUM(shop));
		}
	}
	
	// update socials
	HASH_ITER(hh, social_table, soc, next_soc) {
		// REQ_x: quest types
		found = delete_requirement_from_list(&SOC_REQUIREMENTS(soc), REQ_COMPLETED_QUEST, vnum);
		found |= delete_requirement_from_list(&SOC_REQUIREMENTS(soc), REQ_NOT_COMPLETED_QUEST, vnum);
		found |= delete_requirement_from_list(&SOC_REQUIREMENTS(soc), REQ_NOT_ON_QUEST, vnum);
		
		if (found) {
			SET_BIT(SOC_FLAGS(soc), SOC_IN_DEVELOPMENT);
			save_library_file_for_vnum(DB_BOOT_SOC, SOC_VNUM(soc));
		}
	}
	
	// remove from from active editors
	for (desc = descriptor_list; desc; desc = desc->next) {
		if (GET_OLC_EVENT(desc)) {
			// QR_x: event reward types
			found = delete_event_reward_from_list(&EVT_RANK_REWARDS(GET_OLC_EVENT(desc)), QR_QUEST_CHAIN, vnum);
			found |= delete_event_reward_from_list(&EVT_THRESHOLD_REWARDS(GET_OLC_EVENT(desc)), QR_QUEST_CHAIN, vnum);
		
			if (found) {
				// SET_BIT(EVT_FLAGS(GET_OLC_EVENT(desc)), EVTF_IN_DEVELOPMENT);
				msg_to_desc(desc, "A quest used as a reward by the event you are editing was deleted.\r\n");
			}
		}
		if (GET_OLC_PROGRESS(desc)) {
			found = delete_requirement_from_list(&PRG_TASKS(GET_OLC_PROGRESS(desc)), REQ_COMPLETED_QUEST, vnum);
			found |= delete_requirement_from_list(&PRG_TASKS(GET_OLC_PROGRESS(desc)), REQ_NOT_COMPLETED_QUEST, vnum);
			found |= delete_requirement_from_list(&PRG_TASKS(GET_OLC_PROGRESS(desc)), REQ_NOT_ON_QUEST, vnum);
		
			if (found) {
				SET_BIT(QUEST_FLAGS(GET_OLC_PROGRESS(desc)), PRG_IN_DEVELOPMENT);
				msg_to_desc(desc, "A quest used by the progression goal you're editing has been deleted.\r\n");
			}
		}
		if (GET_OLC_QUEST(desc)) {
			// REQ_x, QR_x: quest types
			found = delete_requirement_from_list(&QUEST_TASKS(GET_OLC_QUEST(desc)), REQ_COMPLETED_QUEST, vnum);
			found |= delete_requirement_from_list(&QUEST_PREREQS(GET_OLC_QUEST(desc)), REQ_COMPLETED_QUEST, vnum);
			found |= delete_requirement_from_list(&QUEST_TASKS(GET_OLC_QUEST(desc)), REQ_NOT_COMPLETED_QUEST, vnum);
			found |= delete_requirement_from_list(&QUEST_PREREQS(GET_OLC_QUEST(desc)), REQ_NOT_COMPLETED_QUEST, vnum);
			found |= delete_requirement_from_list(&QUEST_TASKS(GET_OLC_QUEST(desc)), REQ_NOT_ON_QUEST, vnum);
			found |= delete_requirement_from_list(&QUEST_PREREQS(GET_OLC_QUEST(desc)), REQ_NOT_ON_QUEST, vnum);
			found |= delete_quest_reward_from_list(&QUEST_REWARDS(GET_OLC_QUEST(desc)), QR_QUEST_CHAIN, vnum);
			found |= delete_quest_giver_from_list(&QUEST_STARTS_AT(GET_OLC_QUEST(desc)), QG_QUEST, vnum);
			found |= delete_quest_giver_from_list(&QUEST_ENDS_AT(GET_OLC_QUEST(desc)), QG_QUEST, vnum);
		
			if (found) {
				SET_BIT(QUEST_FLAGS(GET_OLC_QUEST(desc)), QST_IN_DEVELOPMENT);
				msg_to_desc(desc, "Another quest used by the quest you are editing was deleted.\r\n");
			}
		}
		if (GET_OLC_SHOP(desc)) {
			// QG_x: quest types
			found = delete_quest_giver_from_list(&SHOP_LOCATIONS(GET_OLC_SHOP(desc)), QG_QUEST, vnum);
		
			if (found) {
				SET_BIT(SHOP_FLAGS(GET_OLC_SHOP(desc)), SHOP_IN_DEVELOPMENT);
				msg_to_desc(desc, "A quest used by the shop you are editing was deleted.\r\n");
			}
		}
		if (GET_OLC_SOCIAL(desc)) {
			// REQ_x: quest types
			found = delete_requirement_from_list(&SOC_REQUIREMENTS(GET_OLC_SOCIAL(desc)), REQ_COMPLETED_QUEST, vnum);
			found |= delete_requirement_from_list(&SOC_REQUIREMENTS(GET_OLC_SOCIAL(desc)), REQ_NOT_COMPLETED_QUEST, vnum);
			found |= delete_requirement_from_list(&SOC_REQUIREMENTS(GET_OLC_SOCIAL(desc)), REQ_NOT_ON_QUEST, vnum);
			
			if (found) {
				SET_BIT(SOC_FLAGS(GET_OLC_SOCIAL(desc)), SOC_IN_DEVELOPMENT);
				msg_to_desc(desc, "A quest required by the social you are editing was deleted.\r\n");
			}
		}
	}
	
	syslog(SYS_OLC, GET_INVIS_LEV(ch), TRUE, "OLC: %s has deleted quest %d", GET_NAME(ch), vnum);
	msg_to_char(ch, "Quest %d deleted.\r\n", vnum);
	
	free_quest(quest);
}


/**
* Function to save a player's changes to a quest (or a new one).
*
* @param descriptor_data *desc The descriptor who is saving.
*/
void save_olc_quest(descriptor_data *desc) {	
	quest_data *proto, *quest = GET_OLC_QUEST(desc);
	any_vnum vnum = GET_OLC_VNUM(desc);
	int old_cycle = NOTHING;
	descriptor_data *iter;
	UT_hash_handle hh;
	
	// have a place to save it?
	if (!(proto = quest_proto(vnum))) {
		proto = create_quest_table_entry(vnum);
	}
	
	// store for later
	old_cycle = QUEST_DAILY_CYCLE(proto);
	
	// delete from lookups FIRST
	add_or_remove_all_quest_lookups_for(proto, FALSE);
	
	// free prototype strings and pointers
	if (QUEST_NAME(proto)) {
		free(QUEST_NAME(proto));
	}
	if (QUEST_DESCRIPTION(proto)) {
		free(QUEST_DESCRIPTION(proto));
	}
	if (QUEST_COMPLETE_MSG(proto)) {
		free(QUEST_COMPLETE_MSG(proto));
	}
	free_quest_givers(QUEST_STARTS_AT(proto));
	free_quest_givers(QUEST_ENDS_AT(proto));
	free_requirements(QUEST_TASKS(proto));
	free_quest_rewards(QUEST_REWARDS(proto));
	free_requirements(QUEST_PREREQS(proto));
	free_proto_scripts(&QUEST_SCRIPTS(proto));
	
	// sanity
	if (!QUEST_NAME(quest) || !*QUEST_NAME(quest)) {
		if (QUEST_NAME(quest)) {
			free(QUEST_NAME(quest));
		}
		QUEST_NAME(quest) = str_dup(default_quest_name);
	}
	if (!QUEST_DESCRIPTION(quest) || !*QUEST_DESCRIPTION(quest)) {
		if (QUEST_DESCRIPTION(quest)) {
			free(QUEST_DESCRIPTION(quest));
		}
		QUEST_DESCRIPTION(quest) = str_dup(default_quest_description);
	}
	if (!QUEST_COMPLETE_MSG(quest) || !*QUEST_COMPLETE_MSG(quest)) {
		if (QUEST_COMPLETE_MSG(quest)) {
			free(QUEST_COMPLETE_MSG(quest));
		}
		QUEST_COMPLETE_MSG(quest) = str_dup(default_quest_complete_msg);
	}
	
	// save data back over the proto-type
	hh = proto->hh;	// save old hash handle
	*proto = *quest;	// copy over all data
	proto->vnum = vnum;	// ensure correct vnum
	proto->hh = hh;	// restore old hash handle
	
	// re-add lookups
	add_or_remove_all_quest_lookups_for(proto, TRUE);
	
	// and save to file
	save_library_file_for_vnum(DB_BOOT_QST, vnum);
	
	// look for players on the quest and update them
	LL_FOREACH(descriptor_list, iter) {
		if (STATE(iter) != CON_PLAYING || !iter->character) {
			continue;
		}
		if (!is_on_quest(iter->character, vnum)) {
			continue;
		}
		refresh_all_quests(iter->character);
	}
	
	// update daily cycles
	if (old_cycle != NOTHING) {
		setup_daily_quest_cycles(old_cycle);
	}
	if (QUEST_DAILY_CYCLE(proto) != NOTHING && QUEST_DAILY_CYCLE(proto) != old_cycle) {
		setup_daily_quest_cycles(QUEST_DAILY_CYCLE(proto));
	}
}


/**
* Creates a copy of a quest, or clears a new one, for editing.
* 
* @param quest_data *input The quest to copy, or NULL to make a new one.
* @return quest_data* The copied quest.
*/
quest_data *setup_olc_quest(quest_data *input) {
	extern struct apply_data *copy_apply_list(struct apply_data *input);
	
	quest_data *new;
	
	CREATE(new, quest_data, 1);
	clear_quest(new);
	
	if (input) {
		// copy normal data
		*new = *input;

		// copy things that are pointers
		QUEST_NAME(new) = QUEST_NAME(input) ? str_dup(QUEST_NAME(input)) : NULL;
		QUEST_DESCRIPTION(new) = QUEST_DESCRIPTION(input) ? str_dup(QUEST_DESCRIPTION(input)) : NULL;
		QUEST_COMPLETE_MSG(new) = QUEST_COMPLETE_MSG(input) ? str_dup(QUEST_COMPLETE_MSG(input)) : NULL;
		
		QUEST_STARTS_AT(new) = copy_quest_givers(QUEST_STARTS_AT(input));
		QUEST_ENDS_AT(new) = copy_quest_givers(QUEST_ENDS_AT(input));
		QUEST_TASKS(new) = copy_requirements(QUEST_TASKS(input));
		QUEST_REWARDS(new) = copy_quest_rewards(QUEST_REWARDS(input));
		QUEST_PREREQS(new) = copy_requirements(QUEST_PREREQS(input));
		QUEST_SCRIPTS(new) = copy_trig_protos(QUEST_SCRIPTS(input));
		
		// update version number
		QUEST_VERSION(new) += 1;
	}
	else {
		// brand new: some defaults
		QUEST_NAME(new) = str_dup(default_quest_name);
		QUEST_DESCRIPTION(new) = str_dup(default_quest_description);
		QUEST_COMPLETE_MSG(new) = str_dup(default_quest_complete_msg);
		QUEST_FLAGS(new) = QST_IN_DEVELOPMENT;
		QUEST_VERSION(new) = 1;
	}
	
	// done
	return new;	
}


 //////////////////////////////////////////////////////////////////////////////
//// DISPLAYS ////////////////////////////////////////////////////////////////

/**
* Gets the display for a set of quest givers.
*
* @param struct quest_giver *list Pointer to the start of a list of quest givers.
* @param char *save_buffer A buffer to store the result to.
*/
void get_quest_giver_display(struct quest_giver *list, char *save_buffer) {
	struct quest_giver *giver;
	int count = 0;
	
	*save_buffer = '\0';
	LL_FOREACH(list, giver) {		
		sprintf(save_buffer + strlen(save_buffer), "%2d. %s\r\n", ++count, quest_giver_string(giver, TRUE));
	}
	
	// empty list not shown
}


/**
* Gets the display for a set of quest rewards.
*
* @param struct quest_reward *list Pointer to the start of a list of quest rewards.
* @param char *save_buffer A buffer to store the result to.
*/
void get_quest_reward_display(struct quest_reward *list, char *save_buffer) {
	struct quest_reward *reward;
	int count = 0;
	
	*save_buffer = '\0';
	LL_FOREACH(list, reward) {		
		sprintf(save_buffer + strlen(save_buffer), "%2d. %s: %s\r\n", ++count, quest_reward_types[reward->type], quest_reward_string(reward, TRUE));
	}
	
	// empty list not shown
}


/**
* For vstat.
*
* @param char_data *ch The player requesting stats.
* @param quest_data *quest The quest to display.
*/
void do_stat_quest(char_data *ch, quest_data *quest) {
	char buf[MAX_STRING_LENGTH], part[MAX_STRING_LENGTH];
	size_t size;
	
	if (!quest) {
		return;
	}
	
	// first line
	size = snprintf(buf, sizeof(buf), "VNum: [\tc%d\t0], Name: \tc%s\t0\r\n", QUEST_VNUM(quest), QUEST_NAME(quest));
	size += snprintf(buf + size, sizeof(buf) - size, "%s", QUEST_DESCRIPTION(quest));
	size += snprintf(buf + size, sizeof(buf) - size, "-------------------------------------------------\r\n");
	size += snprintf(buf + size, sizeof(buf) - size, "%s", QUEST_COMPLETE_MSG(quest));
	
	sprintbit(QUEST_FLAGS(quest), quest_flags, part, TRUE);
	size += snprintf(buf + size, sizeof(buf) - size, "Flags: \tg%s\t0\r\n", part);
	
	if (QUEST_FLAGGED(quest, QST_DAILY)) {
		if (QUEST_DAILY_CYCLE(quest) != NOTHING) {
			size += snprintf(buf + size, sizeof(buf) - size, "Daily cycle id: \tc%d\t0\r\n", QUEST_DAILY_CYCLE(quest));
		}
		else {
			size += snprintf(buf + size, sizeof(buf) - size, "Daily cycle id: \tcnone\t0\r\n");
		}
	}
	
	if (QUEST_REPEATABLE_AFTER(quest) == NOT_REPEATABLE) {
		strcpy(part, "never");
	}
	else if (QUEST_REPEATABLE_AFTER(quest) == 0) {
		strcpy(part, "immediate");
	}
	else {
		sprintf(part, "%d minutes (%d:%02d:%02d)", QUEST_REPEATABLE_AFTER(quest), (QUEST_REPEATABLE_AFTER(quest) / (60 * 24)), ((QUEST_REPEATABLE_AFTER(quest) % (60 * 24)) / 60), ((QUEST_REPEATABLE_AFTER(quest) % (60 * 24)) % 60));
	}
	size += snprintf(buf + size, sizeof(buf) - size, "Level limits: [\tc%s\t0], Repeatable: [\tc%s\t0]\r\n", level_range_string(QUEST_MIN_LEVEL(quest), QUEST_MAX_LEVEL(quest), 0), part);
		
	get_requirement_display(QUEST_PREREQS(quest), part);
	size += snprintf(buf + size, sizeof(buf) - size, "Pre-requisites:\r\n%s", *part ? part : " none\r\n");
	
	get_quest_giver_display(QUEST_STARTS_AT(quest), part);
	size += snprintf(buf + size, sizeof(buf) - size, "Starts at:\r\n%s", *part ? part : " nowhere\r\n");
	
	get_quest_giver_display(QUEST_ENDS_AT(quest), part);
	size += snprintf(buf + size, sizeof(buf) - size, "Ends at:\r\n%s", *part ? part : " nowhere\r\n");
	
	get_requirement_display(QUEST_TASKS(quest), part);
	size += snprintf(buf + size, sizeof(buf) - size, "Tasks:\r\n%s", *part ? part : " none\r\n");
	
	get_quest_reward_display(QUEST_REWARDS(quest), part);
	size += snprintf(buf + size, sizeof(buf) - size, "Rewards:\r\n%s", *part ? part : " none\r\n");
	
	// scripts
	get_script_display(QUEST_SCRIPTS(quest), part);
	size += snprintf(buf + size, sizeof(buf) - size, "Scripts:\r\n%s", QUEST_SCRIPTS(quest) ? part : " none\r\n");
	
	page_string(ch->desc, buf, TRUE);
}


/**
* This is the main recipe display for quest OLC. It displays the user's
* currently-edited quest.
*
* @param char_data *ch The person who is editing a quest and will see its display.
*/
void olc_show_quest(char_data *ch) {
	quest_data *quest = GET_OLC_QUEST(ch->desc);
	char buf[MAX_STRING_LENGTH], lbuf[MAX_STRING_LENGTH];
	
	if (!quest) {
		return;
	}
	
	*buf = '\0';
	
	sprintf(buf + strlen(buf), "[%s%d\t0] %s%s\t0\r\n", OLC_LABEL_CHANGED, GET_OLC_VNUM(ch->desc), OLC_LABEL_UNCHANGED, !quest_proto(QUEST_VNUM(quest)) ? "new quest" : get_quest_name_by_proto(QUEST_VNUM(quest)));
	sprintf(buf + strlen(buf), "<%sname\t0> %s\r\n", OLC_LABEL_STR(QUEST_NAME(quest), default_quest_name), NULLSAFE(QUEST_NAME(quest)));
	sprintf(buf + strlen(buf), "<%sdescription\t0>\r\n%s", OLC_LABEL_STR(QUEST_DESCRIPTION(quest), default_quest_description), NULLSAFE(QUEST_DESCRIPTION(quest)));
	sprintf(buf + strlen(buf), "<%scompletemessage\t0>\r\n%s", OLC_LABEL_STR(QUEST_COMPLETE_MSG(quest), default_quest_complete_msg), NULLSAFE(QUEST_COMPLETE_MSG(quest)));
	
	sprintbit(QUEST_FLAGS(quest), quest_flags, lbuf, TRUE);
	sprintf(buf + strlen(buf), "<%sflags\t0> %s\r\n", OLC_LABEL_VAL(QUEST_FLAGS(quest), QST_IN_DEVELOPMENT), lbuf);
	
	if (QUEST_MIN_LEVEL(quest) > 0) {
		sprintf(buf + strlen(buf), "<%sminlevel\t0> %d\r\n", OLC_LABEL_CHANGED, QUEST_MIN_LEVEL(quest));
	}
	else {
		sprintf(buf + strlen(buf), "<%sminlevel\t0> none\r\n", OLC_LABEL_UNCHANGED);
	}
	if (QUEST_MAX_LEVEL(quest) > 0) {
		sprintf(buf + strlen(buf), "<%smaxlevel\t0> %d\r\n", OLC_LABEL_CHANGED, QUEST_MAX_LEVEL(quest));
	}
	else {
		sprintf(buf + strlen(buf), "<%smaxlevel\t0> none\r\n", OLC_LABEL_UNCHANGED);
	}
	
	get_requirement_display(QUEST_PREREQS(quest), lbuf);
	sprintf(buf + strlen(buf), "Pre-requisites: <%sprereqs\t0>\r\n%s", OLC_LABEL_PTR(QUEST_PREREQS(quest)), lbuf);
	
	if (QUEST_REPEATABLE_AFTER(quest) == NOT_REPEATABLE) {
		sprintf(buf + strlen(buf), "<%srepeat\t0> never\r\n", OLC_LABEL_VAL(QUEST_REPEATABLE_AFTER(quest), 0));
	}
	else if (QUEST_REPEATABLE_AFTER(quest) > 0) {
		sprintf(buf + strlen(buf), "<%srepeat\t0> %d minutes (%d:%02d:%02d)\r\n", OLC_LABEL_VAL(QUEST_REPEATABLE_AFTER(quest), 0), QUEST_REPEATABLE_AFTER(quest), (QUEST_REPEATABLE_AFTER(quest) / (60 * 24)), ((QUEST_REPEATABLE_AFTER(quest) % (60 * 24)) / 60), ((QUEST_REPEATABLE_AFTER(quest) % (60 * 24)) % 60));
	}
	else if (QUEST_REPEATABLE_AFTER(quest) == 0) {
		sprintf(buf + strlen(buf), "<%srepeat\t0> immediately\r\n", OLC_LABEL_VAL(QUEST_REPEATABLE_AFTER(quest), 0));
	}
	
	if (QUEST_FLAGGED(quest, QST_DAILY)) {
		if (QUEST_DAILY_CYCLE(quest) != NOTHING) {
			sprintf(buf + strlen(buf), "<%sdailycycle\t0> %d\r\n", OLC_LABEL_CHANGED, QUEST_DAILY_CYCLE(quest));
		}
		else {
			sprintf(buf + strlen(buf), "<%sdailycycle\t0> none\r\n", OLC_LABEL_UNCHANGED);
		}
	}
	
	get_quest_giver_display(QUEST_STARTS_AT(quest), lbuf);
	sprintf(buf + strlen(buf), "Starts at: <%sstarts\t0>\r\n%s", OLC_LABEL_PTR(QUEST_STARTS_AT(quest)), lbuf);
	
	get_quest_giver_display(QUEST_ENDS_AT(quest), lbuf);
	sprintf(buf + strlen(buf), "Ends at: <%sends\t0>\r\n%s", OLC_LABEL_PTR(QUEST_ENDS_AT(quest)), lbuf);
	
	get_requirement_display(QUEST_TASKS(quest), lbuf);
	sprintf(buf + strlen(buf), "Tasks: <%stasks\t0>\r\n%s", OLC_LABEL_PTR(QUEST_TASKS(quest)), lbuf);
	
	get_quest_reward_display(QUEST_REWARDS(quest), lbuf);
	sprintf(buf + strlen(buf), "Rewards: <%srewards\t0>\r\n%s", OLC_LABEL_PTR(QUEST_REWARDS(quest)), lbuf);
	
	// scripts
	sprintf(buf + strlen(buf), "Scripts: <%sscript\t0>\r\n", OLC_LABEL_PTR(QUEST_SCRIPTS(quest)));
	if (QUEST_SCRIPTS(quest)) {
		get_script_display(QUEST_SCRIPTS(quest), lbuf);
		strcat(buf, lbuf);
	}
	
	page_string(ch->desc, buf, TRUE);
}


/**
* Searches the quest db for a match, and prints it to the character.
*
* @param char *searchname The search string.
* @param char_data *ch The player who is searching.
* @return int The number of matches shown.
*/
int vnum_quest(char *searchname, char_data *ch) {
	quest_data *iter, *next_iter;
	int found = 0;
	
	HASH_ITER(hh, quest_table, iter, next_iter) {
		if (multi_isname(searchname, QUEST_NAME(iter))) {
			msg_to_char(ch, "%3d. [%5d] %s\r\n", ++found, QUEST_VNUM(iter), QUEST_NAME(iter));
		}
	}
	
	return found;
}


 //////////////////////////////////////////////////////////////////////////////
//// OLC MODULES /////////////////////////////////////////////////////////////

OLC_MODULE(qedit_completemessage) {
	quest_data *quest = GET_OLC_QUEST(ch->desc);
	
	if (ch->desc->str) {
		msg_to_char(ch, "You are already editing a string.\r\n");
	}
	else {
		sprintf(buf, "completion message for %s", QUEST_NAME(quest));
		start_string_editor(ch->desc, buf, &QUEST_COMPLETE_MSG(quest), MAX_ITEM_DESCRIPTION, FALSE);
	}
}


OLC_MODULE(qedit_dailycycle) {
	quest_data *quest = GET_OLC_QUEST(ch->desc);
	
	if (!QUEST_FLAGGED(quest, QST_DAILY)) {
		msg_to_char(ch, "This is not a daily quest.\r\n");
	}
	else if (!str_cmp(argument, "none")) {
		QUEST_DAILY_CYCLE(quest) = NOTHING;
		msg_to_char(ch, "It is no longer part of a daily cycle.\r\n");
	}
	else {
		QUEST_DAILY_CYCLE(quest) = olc_process_number(ch, argument, "daily cycle", "dailycycle", 0, MAX_INT, QUEST_DAILY_CYCLE(quest));
	}
}


OLC_MODULE(qedit_description) {
	quest_data *quest = GET_OLC_QUEST(ch->desc);
	
	if (ch->desc->str) {
		msg_to_char(ch, "You are already editing a string.\r\n");
	}
	else {
		sprintf(buf, "description for %s", QUEST_NAME(quest));
		start_string_editor(ch->desc, buf, &QUEST_DESCRIPTION(quest), MAX_ITEM_DESCRIPTION, FALSE);
	}
}


OLC_MODULE(qedit_ends) {
	quest_data *quest = GET_OLC_QUEST(ch->desc);
	qedit_process_quest_givers(ch, argument, &QUEST_ENDS_AT(quest), "ends");
}


OLC_MODULE(qedit_flags) {
	quest_data *quest = GET_OLC_QUEST(ch->desc);
	bool had_indev = IS_SET(QUEST_FLAGS(quest), QST_IN_DEVELOPMENT) ? TRUE : FALSE;
	
	QUEST_FLAGS(quest) = olc_process_flag(ch, argument, "quest", "flags", quest_flags, QUEST_FLAGS(quest));
	
	// validate removal of IN-DEVELOPMENT
	if (had_indev && !IS_SET(QUEST_FLAGS(quest), QST_IN_DEVELOPMENT) && GET_ACCESS_LEVEL(ch) < LVL_UNRESTRICTED_BUILDER && !OLC_FLAGGED(ch, OLC_FLAG_CLEAR_IN_DEV)) {
		msg_to_char(ch, "You don't have permission to remove the IN-DEVELOPMENT flag.\r\n");
		SET_BIT(QUEST_FLAGS(quest), QST_IN_DEVELOPMENT);
	}
	
	// check for no daily flag
	if (!QUEST_FLAGGED(quest, QST_DAILY)) {
		QUEST_DAILY_CYCLE(quest) = NOTHING;
	}
}


OLC_MODULE(qedit_name) {
	quest_data *quest = GET_OLC_QUEST(ch->desc);
	olc_process_string(ch, argument, "name", &QUEST_NAME(quest));
}


OLC_MODULE(qedit_maxlevel) {
	quest_data *quest = GET_OLC_QUEST(ch->desc);
	QUEST_MAX_LEVEL(quest) = olc_process_number(ch, argument, "maximum level", "maxlevel", 0, MAX_INT, QUEST_MAX_LEVEL(quest));
}


OLC_MODULE(qedit_minlevel) {
	quest_data *quest = GET_OLC_QUEST(ch->desc);
	QUEST_MIN_LEVEL(quest) = olc_process_number(ch, argument, "minimum level", "minlevel", 0, MAX_INT, QUEST_MIN_LEVEL(quest));
}


OLC_MODULE(qedit_prereqs) {
	quest_data *quest = GET_OLC_QUEST(ch->desc);
	olc_process_requirements(ch, argument, &QUEST_PREREQS(quest), "prereq", FALSE);
}


OLC_MODULE(qedit_repeat) {
	quest_data *quest = GET_OLC_QUEST(ch->desc);
	
	if (!*argument) {
		msg_to_char(ch, "Set the repeat interval to how many minutes (or immediately/never)?\r\n");
	}
	else if (is_abbrev(argument, "never") || is_abbrev(argument, "none")) {
		QUEST_REPEATABLE_AFTER(quest) = NOT_REPEATABLE;
		msg_to_char(ch, "It is now non-repeatable.\r\n");
	}
	else if (is_abbrev(argument, "immediately")) {
		QUEST_REPEATABLE_AFTER(quest) = 0;
		msg_to_char(ch, "It is now immediately repeatable.\r\n");
	}
	else if (isdigit(*argument)) {
		QUEST_REPEATABLE_AFTER(quest) = olc_process_number(ch, argument, "repeatable after", "repeat", 0, MAX_INT, QUEST_REPEATABLE_AFTER(quest));
		msg_to_char(ch, "It now repeats after %d minutes (%d:%02d:%02d).\r\n", QUEST_REPEATABLE_AFTER(quest), (QUEST_REPEATABLE_AFTER(quest) / (60 * 24)), ((QUEST_REPEATABLE_AFTER(quest) % (60 * 24)) / 60), ((QUEST_REPEATABLE_AFTER(quest) % (60 * 24)) % 60));
	}
	else {
		msg_to_char(ch, "Invalid repeat interval.\r\n");
	}
}


OLC_MODULE(qedit_rewards) {
	quest_data *quest = GET_OLC_QUEST(ch->desc);
	char cmd_arg[MAX_INPUT_LENGTH], field_arg[MAX_INPUT_LENGTH];
	char num_arg[MAX_INPUT_LENGTH], type_arg[MAX_INPUT_LENGTH];
	char vnum_arg[MAX_INPUT_LENGTH], buf[MAX_STRING_LENGTH];
	struct quest_reward *reward, *iter, *change, *copyfrom;
	struct quest_reward **list = &QUEST_REWARDS(quest);
	int findtype, num, stype;
	bool found;
	any_vnum vnum;
	
	argument = any_one_arg(argument, cmd_arg);	// add/remove/change/copy
	
	if (is_abbrev(cmd_arg, "copy")) {
		// usage: rewards copy <from type> <from vnum>
		argument = any_one_arg(argument, type_arg);	// just "quest" for now
		argument = any_one_arg(argument, vnum_arg);	// any vnum for that type
		
		if (!*type_arg || !*vnum_arg) {
			msg_to_char(ch, "Usage: rewards copy <from type> <from vnum>\r\n");
		}
		else if ((findtype = find_olc_type(type_arg)) == 0) {
			msg_to_char(ch, "Unknown olc type '%s'.\r\n", type_arg);
		}
		else if (!isdigit(*vnum_arg)) {
			sprintbit(findtype, olc_type_bits, buf, FALSE);
			msg_to_char(ch, "Copy from which %s?\r\n", buf);
		}
		else if ((vnum = atoi(vnum_arg)) < 0) {
			msg_to_char(ch, "Invalid vnum.\r\n");
		}
		else {
			sprintbit(findtype, olc_type_bits, buf, FALSE);
			copyfrom = NULL;
			
			switch (findtype) {
				case OLC_QUEST: {
					quest_data *from_qst = quest_proto(vnum);
					if (from_qst) {
						copyfrom = QUEST_REWARDS(from_qst);
					}
					break;
				}
				default: {
					msg_to_char(ch, "You can't copy rewards from %ss.\r\n", buf);
					return;
				}
			}
			
			if (!copyfrom) {
				msg_to_char(ch, "Invalid %s vnum '%s'.\r\n", buf, vnum_arg);
			}
			else {
				smart_copy_quest_rewards(list, copyfrom);
				msg_to_char(ch, "Copied rewards from %s %d.\r\n", buf, vnum);
			}
		}
	}	// end 'copy'
	else if (is_abbrev(cmd_arg, "remove")) {
		// usage: rewards remove <number | all>
		skip_spaces(&argument);	// only arg is number
		
		if (!*argument) {
			msg_to_char(ch, "Remove which reward (number)?\r\n");
		}
		else if (!str_cmp(argument, "all")) {
			free_quest_rewards(*list);
			*list = NULL;
			msg_to_char(ch, "You remove all the rewards.\r\n");
		}
		else if (!isdigit(*argument) || (num = atoi(argument)) < 1) {
			msg_to_char(ch, "Invalid reward number.\r\n");
		}
		else {
			found = FALSE;
			LL_FOREACH(*list, iter) {
				if (--num == 0) {
					found = TRUE;
					
					if (iter->vnum > 0) {
						msg_to_char(ch, "You remove the reward for %dx %s %d.\r\n", iter->amount, quest_reward_types[iter->type], iter->vnum);
					}
					else {
						msg_to_char(ch, "You remove the reward for %dx %s.\r\n", iter->amount, quest_reward_types[iter->type]);
					}
					LL_DELETE(*list, iter);
					free(iter);
					break;
				}
			}
			
			if (!found) {
				msg_to_char(ch, "Invalid reward number.\r\n");
			}
		}
	}	// end 'remove'
	else if (is_abbrev(cmd_arg, "add")) {
		// usage: rewards add <type> <amount> <vnum/type>
		argument = any_one_arg(argument, type_arg);
		argument = any_one_arg(argument, num_arg);
		argument = any_one_word(argument, vnum_arg);
		
		if (!*type_arg || !*num_arg || !isdigit(*num_arg)) {
			msg_to_char(ch, "Usage: rewards add <type> <amount> <vnum/type>\r\n");
		}
		else if ((stype = search_block(type_arg, quest_reward_types, FALSE)) == NOTHING) {
			msg_to_char(ch, "Invalid type '%s'.\r\n", type_arg);
		}
		else if ((num = atoi(num_arg)) < 1) {
			msg_to_char(ch, "Invalid amount '%s'.\r\n", num_arg);
		}
		else if ((vnum = parse_quest_reward_vnum(ch, stype, vnum_arg, num_arg)) == NOTHING) {
			// sends own error
		}
		else {	// success
			CREATE(reward, struct quest_reward, 1);
			reward->type = stype;
			reward->amount = num;
			reward->vnum = vnum;
			
			LL_APPEND(*list, reward);
			msg_to_char(ch, "You add %s %s reward: %s\r\n", AN(quest_reward_types[stype]), quest_reward_types[stype], quest_reward_string(reward, TRUE));
		}
	}	// end 'add'
	else if (is_abbrev(cmd_arg, "change")) {
		// usage: rewards change <number> <amount | vnum> <value>
		argument = any_one_arg(argument, num_arg);
		argument = any_one_arg(argument, field_arg);
		argument = any_one_word(argument, vnum_arg);
		
		if (!*num_arg || !isdigit(*num_arg) || !*field_arg || !*vnum_arg) {
			msg_to_char(ch, "Usage: rewards change <number> <amount | vnum> <value>\r\n");
			return;
		}
		
		// find which one to change
		num = atoi(num_arg);
		change = NULL;
		LL_FOREACH(*list, iter) {
			if (--num == 0) {
				change = iter;
				break;
			}
		}
		
		if (!change) {
			msg_to_char(ch, "Invalid reward number.\r\n");
		}
		else if (is_abbrev(field_arg, "amount") || is_abbrev(field_arg, "quantity")) {
			if (!isdigit(*vnum_arg) || (num = atoi(vnum_arg)) < 0) {
				msg_to_char(ch, "Invalid amount '%s'.\r\n", vnum_arg);
				return;
			}
			else {
				change->amount = num;
				msg_to_char(ch, "You change reward %d to: %s\r\n", atoi(num_arg), quest_reward_string(change, TRUE));
			}
		}
		else if (is_abbrev(field_arg, "vnum")) {
			if ((vnum = parse_quest_reward_vnum(ch, change->type, vnum_arg, NULL)) == NOTHING) {
				// sends own error
			}
			else {
				change->vnum = vnum;
				msg_to_char(ch, "Changed reward %d to: %s\r\n", atoi(num_arg), quest_reward_string(change, TRUE));
			}
		}
		else {
			msg_to_char(ch, "You can only change the amount or vnum.\r\n");
		}
	}	// end 'change'
	else if (is_abbrev(cmd_arg, "move")) {
		struct quest_reward *to_move, *prev, *a, *b, *a_next, *b_next, iitem;
		bool up;
		
		// usage: rewards move <number> <up | down>
		argument = any_one_arg(argument, num_arg);
		argument = any_one_arg(argument, field_arg);
		up = is_abbrev(field_arg, "up");
		
		if (!*num_arg || !*field_arg) {
			msg_to_char(ch, "Usage: rewards move <number> <up | down>\r\n");
		}
		else if (!isdigit(*num_arg) || (num = atoi(num_arg)) < 1) {
			msg_to_char(ch, "Invalid reward number.\r\n");
		}
		else if (!is_abbrev(field_arg, "up") && !is_abbrev(field_arg, "down")) {
			msg_to_char(ch, "You must specify whether you're moving it up or down in the list.\r\n");
		}
		else if (up && num == 1) {
			msg_to_char(ch, "You can't move it up; it's already at the top of the list.\r\n");
		}
		else {
			// find the one to move
			to_move = prev = NULL;
			for (reward = *list; reward && !to_move; reward = reward->next) {
				if (--num == 0) {
					to_move = reward;
				}
				else {
					// store for next iteration
					prev = reward;
				}
			}
			
			if (!to_move) {
				msg_to_char(ch, "Invalid reward number.\r\n");
			}
			else if (!up && !to_move->next) {
				msg_to_char(ch, "You can't move it down; it's already at the bottom of the list.\r\n");
			}
			else {
				// SUCCESS: "move" them by swapping data
				if (up) {
					a = prev;
					b = to_move;
				}
				else {
					a = to_move;
					b = to_move->next;
				}
				
				// store next pointers
				a_next = a->next;
				b_next = b->next;
				
				// swap data
				iitem = *a;
				*a = *b;
				*b = iitem;
				
				// restore next pointers
				a->next = a_next;
				b->next = b_next;
				
				// message: re-atoi(num_arg) because we destroyed num finding our target
				msg_to_char(ch, "You move reward %d %s.\r\n", atoi(num_arg), (up ? "up" : "down"));
			}
		}
	}	// end 'move'
	else {
		msg_to_char(ch, "Usage: rewards add <type> <amount> <vnum/type>\r\n");
		msg_to_char(ch, "Usage: rewards change <number> <amount | vnum> <value>\r\n");
		msg_to_char(ch, "Usage: rewards copy <from type> <from vnum>\r\n");
		msg_to_char(ch, "Usage: rewards remove <number | all>\r\n");
		msg_to_char(ch, "Usage: rewards move <number> <up | down>\r\n");
	}
}


OLC_MODULE(qedit_script) {
	quest_data *quest = GET_OLC_QUEST(ch->desc);
	olc_process_script(ch, argument, &QUEST_SCRIPTS(quest), WLD_TRIGGER);
}


OLC_MODULE(qedit_starts) {
	quest_data *quest = GET_OLC_QUEST(ch->desc);
	qedit_process_quest_givers(ch, argument, &QUEST_STARTS_AT(quest), "starts");
}


OLC_MODULE(qedit_tasks) {
	quest_data *quest = GET_OLC_QUEST(ch->desc);
	olc_process_requirements(ch, argument, &QUEST_TASKS(quest), "task", TRUE);
}
