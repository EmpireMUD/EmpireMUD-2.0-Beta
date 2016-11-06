//////////  New website

- What makes EmpireMUD different

- VIP Mud client


Resources Section
- online helps
- world map


History
- Backstory


Fun Links
- mapshow




//// buildings on map like vehicles

- buildings on map like vehicles
- world map as X,Y grid stored as structs for fast read/write
- extended data stored in hash table rather than for whole world
- linked lists of terrain, like { list of all plains } for rapid searches like instancing
- instanced zones using dynamic rooms similar to how it's currently done
- interiors also using dynamic rooms added as-needed
- walls, roads, gates present the biggest issues -> need custom movement AND display



///// code cleanups

targetting chars
-- need a macro or something:
- isname(tmp, i->player.name) || isname(tmp, PERS(i, i, 0)) || isname(tmp, PERS(i, i, 1)) || (GET_LASTNAME(i) && isname(name, GET_LASTNAME(i)))
-- account for disguise/morph and only check real name if the person can know it (maybe a macro for this)



///// storage


-- need to store variables on objs, rooms, mobs, vehicles

// later:
// TODO apply the normal "zone" concept to adventures? building-echo still works because it applies to everything that shares a homeroom
// TODO check trigger list in dg_scripts.h for more places to apply those triggers
// TODO gonna need a empire-can-target thing for empire mobs + something to set a spawned mob to the same empire as the room
// TODO permission-to-enter in movement should check npc loyalty correctly, but non-charmed npcs should be allowed to enter anywhere via script?
- dg_affect_room
- possibly a way to "remove any effect that causes AFF_FLY regardless of origin"?
- sends, echoes, etc should ignore earthmeld or !combat !target chars as they wouldn't see what's happening in the room
-- actually, .next_in_room should ignore earthmeld (or !target) as it's never relevant


///
can_use_ability(ch, ability, cost_pool, cost_amount, cooldown_type);
charge_ability_cost(ch, cost_pool, cost_amount, cooldown_type, cooldown_time);
get_approximate_level(ch);


- maybe a GET_DAMAGE_ADD(ch) -> is npc ? MOB_DAMAGE : get-wield-weapon ? weapon damage
-> look for places that use either mob damage or weapon damage

////

struct trig_var_data {
	char *name;				/* name of variable  */
	char *value;				/* value of variable */
	long context;				/* 0: global context */

	struct trig_var_data *next;
};

/* structure for triggers */
struct trig_data {
	trig_rnum nr; 	                /* trigger's rnum                  */
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


- need something that outputs script in the form of trig/var/trig/var plus metadata like context
  - look up context, find how it's stored and how they pick a new context (is it uid?)
- triggers will probably lose current state (waits, etc)



 //////////////////////////////////////////////////////////////////////////////
//// TODO UNSORTED ///////////////////////////////////////////////////////////


-> must import changes to world files
-> put island sizes on the website

- check world craftables for scaling
- spear, spearhead need scaling limits -> anything that requires an obj




Battle:

Duke (Empire)

Battlemage (High Sorcery)
!Battle

Werewolf (Natural Magic)

Assassin (Stealth)

Barbarian (Survival)
!High Sorc

Steelsmith (Trade)

Reaper (Vampire)


-> a generic system for storing dates, numbers, etc (similar to configs) would be useful here


skycleaver armor and dragonwing armor are both wrong armor type



- raise mob scaling
- lower item scaling over 100
- if a player manually abandons a city, in-city-only buildings inside should ruin rapidly
  - perhaps this is also true at end-of-year

- potions for hit/dodge

- healing tweaks


base = level
heals += (intelligence * (1 + (level/100.0)))


need:
1. big heal on a cooldown
2. show new health value/prc after a heal


Natural Magic:
Werewolf (Battle)
- Resurrect, Sage Werewolf Form
+ Overgrow: BIG high-cost heal on a moderate cooldown
+ all heals are bigger but cost more?

Luminary (Empire)
- Resurrect
- Dragonriding
+ Greatness (scaled) added to basic 'heal', eartharmor, etc?

Archmage (High Sorcery)
- Resurrect
+ Big cheap heal on a long cooldown
+ Group magic shield

Shadow Wolf (Stealth)
- Moonrise, Sage Werewolf Form
+ moderate cooldown ability that uses mv to boost the size of the next heal cast
+ better HoT

Feral (Survival)
- Resurrect, Sage Werewolf Form
+ Ability to dump all your available mv for mana
+ AoE HoT heal

Alchemist (Trade)
- Resurrect
+ Bigger healing potions (check that class abilities don't trigger skill scaling)
+ Big heal on a long cooldown

Necromancer (Vampire)
- Resurrect
- Dread Blood Form
+ Heartvenom - powerful HoT using blood instead of mana


++ Rejuv needs a cost/level update; should scale with level.
++ Basic heal needs a cost/level update and scaling.


- change to-hit, dodge, and block to scale % from 0 to the cap?
-- add diminishing returns to these again?



HEALER TRAITS
?. Last one based on class:
   X Werewolf/Battle - Confer: Lose Strength to get buffs
   X Luminary/Empire - Greatness adds to healing
   - Archmage/HighSorc - Mana pool? Things would cost more but be more effective, creating a larger mana burden.
   - Shadow Wolf/Stealth - Charisma somehow
   - Feral/Survival - Spend mv to get mana (stack mv too); abilities to spend mv for healing buffs
   - Alchemist/Trade - BoP self-buff potions? [crafts must always go to inv if takeable] Requires forethought instead of gear...
   X Necromancer/Vampire - can already spend blood for mana



- new abilities for regen in combat

- wits add could scale based on proportion to your witscap, rather than flat mod by wits



- hitsume would like to see what your level would be after equipping an item


-> scan command to find all tiles matching NAME within visual range, sorted by distance, collapsed similar to territory
-> screenreader should get a "players you can see" for ALL players in range, not just ones on the lines


changing mine data to interactions:
- add a spare int to interactions
- use spare for required-ability
- use min-max = quantity-(2xquantity)
- when setting up mine data, instead of mine_type, just store ore vnum
- can determine if it's deep either by looking up the matching interaction, or storing extra data for it
- to adapt from old code:
  - could write an updater command using util...
  - could write an auto-updater that saves last-good-boot version and, if the mud boots with a newer update, runs some automagic fixes (configurable)


-> script command to bind an item to a player
-> bind boe patterns on-use? and on-refund, if applicable

-> portal -a should say "Available" instead of "known"

-> add craft small skins; craft small leather


- snoop not showing political map due to size -- also possible to lose all output due to size

"rgbymcwajloptvnRGBYMCWAJLOPTV0" +u, maybe -wW


- could store only the F###/B### strings and then parse them into one big color code when flushed
- this would reduce the work on each color discovery
- would need special handling for people who get ansi color
- could detect that the user gets no colors at the flush level, and save work

-- map generator will miss the irrigated fields on its autoconvert
-- irrigated plains need own trench type




&([rgbcmyRGBCMYwWu0%])
- need f() to convert & to \t in a string, and back
- replace count_colorchars with something like real_strlen()
- strip_color and the f() that shows color codes may need updates
- banner does not support extended color codes
- count color things don't either


- could add sound triggers even without providing a sound pack
- chore action
- start chore
- finish chore ?
- hit in combat
- a way to trigger sounds from scripts


- generic function for "string ends with &"



- some way to list and unset homes in eedit

- disarm -> need to adjust for mob's normal speed

- badpw = echo loop!


Natural Magic:
Werewolf (Battle) -
 melee: dire wolf/melee, ABIL_DIRE_WOLF
Luminary (Empire) -
 caster: manticore/dps, griffin/tank, ABIL_GRIFFIN, ABIL_MANTICORE
 healer: manticore/dps, griffin/tank, ABIL_GRIFFIN, ABIL_MANTICORE
Archmage (High Sorcery) -
 caster: mirror image
 healer: phoenix/buff, ABIL_PHOENIX
  1. +bonus-heal on master
  2. heal tank
  3. heal party (less)
  4. bonus-phys/magical on party
Shadow Wolf (Stealth) -
 melee: scorpion shadow/debuff, ABIL_SCORPION_SHADOW
  1. slow on enemy
  2. -bonusphy/bonusmag on enemy
  3. -wits on enemy
  4. poison dot on enemy
 healer: owl shadow/buff, giant tortoise/tank, ABIL_OWL_SHADOW
  1. wits buff on group
  2. HoT on party
  3. restore move on party
  4. dodge on tank
Feral (Survival) -
 tank: basilisk/debuff, ABIL_BASILISK
 healer: basilisk/debuff, ABIL_BASILISK
  1. partial stone form +block on tank
  2. partial stone form reduces bonus-phys/mag on enemy
  3. partial stone form reduces to-hit on enemy
  4. partial stone form reduces wits on enemy
Alchemist (Trade) -
 healer: salamander/buff, ABIL_SALAMANDER
  1. big bonus-heal spike on master, briefly
  2. big, short HoT on tank
  3. dodge bonus on tank
  4. bonus phys/mag on party
Necromancer (Vampire) -
 caster: skeleton/dps, ABIL_SKELETAL_HULK
 healer: banshee/debuff, ABIL_BANSHEE
  1. aoe resist debuff
  2. aoe to-hit debuff
  3. aoe -bonus phys/mag
  4. small aoe dot

claw 2.6 1.6  5%=32 4%=40 3%=53
bite 2.4 1.5  5%=30 4%=38 3%=50



- check if negative bonus-phys and bonus-mag work
- negative resist should cause increased damage


- could copy variables on warehouse copy
- save variables to obj file, write_room_to_file ?


Auditors:
- spawns, interactions: report vnums that are from outside the adventure
- Khufu    (May 12) [BLD1000] olc auditors: auto-audit an item when you save it, or add a .audit for current item
- Khufu    (May 13) [BLD1000] description auditors could look for double-crlf at the end
- check correct trigger types on any trigger list

ADV:
- detailed list shows count of obj/mob/etc

CFT:
- could check for requiring scalable?

CRP:
- full auditor

GLB:
- full auditor
- check valid interacts

MOB:

OBJ:

RMT:
- new list command with exit count, spawns, and ?

SCT:
- full auditor

TRG:
- Khufu    (Jul  6) [BLD1000] trigger auditor: ensure attached to something; correct thing
- global without random
- empty args
- odd number of % on a line?
- empty type


-> basic can-go that checks entrances, permissions, etc. optionally sends error message. use for movement, flee pre-checks, etc.



--- greatness issue: could index greatness and be able to recompute it without loading players
  - main problem is when equipment is not loaded



///// MAP

- should the natural sector be reduced to mountain/river/plains/desert/ocean ?

generator:
- move map generator location
- generate base_map instead of .wld
- advise people to delete their .wld files?




///// new abilities

Trade: Steelsmith (Battle), Guildsman (Empire), Artificer (High Sorcery),
        Alchemist (Natural Magic), Smuggler (Stealth), Tinker (Survival),
        Antiquarian (Vampire)

Trade 76-100:
[75] Hone (self-only item buff)
     - Modify a weapon/OH/shield with a level-scaled apply (similar to enchant)
     - +bonus-damage (magical or physical based on weapon type)
     - +bonus-healing on offhand healer item somehow?
     - +resist-* on shield
     - only if no "honed" bonuses on it
     - causes it to bind to you (add BoP if it doesn't have bind flags)

Sharpen (Steelsmith): +X damage to sharp weapon
Weight (Guildsman): +X damage to blunt weapon
Darken (Artificer): +X damage to magic weapon
??? (Alchemist): +X healing to weapon
(Smuggler)
(Tinker)
(Antiquarian)


battle axe 2204 -> scales to +0 str




- add AUGMENT item type, values:
  - 0: augment vnum
  - 1: scale level



// vnums
100-199: weapon enchants
200-299: offhand enchants
300-399: armor enchants
400-499: accessory enchants
500-599: shield enchants
600-699: tool enchants
1000-1099: hones

globals:
160-199: mines
200-300: newbie gear


//// archetypes

// notes:
 

- aug(ment)?s?



Battle: Boorish Warrior
  Survival: * Promising Squire (Str)
  Stealth: Petty Thug (Str)

Empire:
  Battle: * Noble Birth (Grt)
  Survival: * Tribal Leader (Wits)

High Sorcery:
  Battle: * Novice Sorcerer (Int)
  NatMag: Heretic (Int)

Natural Magic:
 Survival: * Druidic Commune (Int)
 HighSorc: X
 Trade: X
 Stealth: Druid of Urbs (?) <--

Stealth:
  Battle: * Street Urchin (Cha)
  High Sorc: Disgraced Apprentice (?) <--

Survival:
  Trade: * Rugged Woodsman (Dex)

Trade: * Skilled Apprentice (Dex)
 NatMag: Herbalist (Int)

Vampire:
  Battle: * Lone Vampire (Wits)
  Empire: Reclusive Baron (Wits)
  High Sorc: Tainted Wizard (?)

3X:
  Battle/Survival/Trade: Adventurer
  Vampire/Battle/Stealth



- the &? probably needs a real color replacement of base color in the .icon add output

- check builder's ideas file for jeff's suggestions esp custom messages

- player message queue for things like perform-get/retrieve/etc -> combine duplicate messages with a (3x)
  - normally flush messages manually at end of func.
  - when to flush messages if not? right before send?
  - ACT_QUEUE flag
  - queue_msg(ch, ...)
  - all logs should do this


JEFF'S CROPS:
=============

- must remove !WILD flags, and IN-DEV crafts
- audit objs 3000-3999, craft 3000-3999, crop 3000, 3099
- must write converter to introduce them

[30000] tomatoes: 3000
 30000: 3026
 30001: 3027
[30001] bananas: 3001
 30013: 3028
 30014: 3029
 30017/c: 3330
 30024/c: 3331
[30002] coconuts: 3002
 30015: 3030
 30016: 3031
[30003] grapes: 3003
 30019: 3032
 30020: 3033
[30004] peppers: 3004
 30008: 3034
 30009: 3035
 30021/c: 3332
 30022/c: 3333
[30005] potatoes: 3005
 30010: 3036
 30078/c: 3334
[30006] plums: 3006
 30002: 3037
 30003: 3038
 30004/c: 3335
 30012/c: 3336
[30007] strawberries: 3007
 30026: 3039
 30027: 3040
 30028/c: 3337
 30064/c: 3338
[30008] mangos: 3008
 30029: 3041
 30051: 3042
 30052/c: 3339
[30009] blueberries: 3009
 30046: 3043
 30047: 3044
 30048/c: 3340
 30050/c: 3341
[30011] coffee: 3011
 30056: 3045
 30057c: 3342 (not obj, just soup recipe)
[30015] oats: 3015
 30074: 3046
 30075: 3343
 30076/c: 3344
[30016] watermelons: 3016
 30083: 3047

[30010] apricots
[30012] sugarcane
[30013] garlic
[30014] lemons
[30017] limes
[30018] grapefruits -> wrong era
[30019] cucumbers

.icon add any &g ^^^&vo
.icon add any &g ^&vo&?^^
.icon add any &g ^^&vo&?^
.icon add any &g &vo&?^^^



|            |            |            |            |
|            |            |            |            | Temperate
|            |            |            |            |  y=51-100
|  tomatoes  | blueberry  |   grapes   |   plums    |  x=0-24, 25-49, 50-74, 75-100
+------------+------------+------------+------------+
|       <plantains>       |         bananas         | Tropics, x=0-49, 50-100
+------------+------------+------------+------------+
|                      coconuts                     | Desert
|                      peppers                      | y=0-55, 45-100
+------------+------------+------------+------------+
|       watermelon        |         mangoes         | Tropics
+------------+------------+------------+------------+
|  potatoes  |    oats    |  strawbry  |   coffee   |
|            |            |            |            | Temperate
|            |            |            |            | y=0-50
|            |            |            |            |


River crop ideas:
- cranberry
- flax?
- lettuce
- cucumber



- additional requires-water crops?
- UPDATE MAP GENERATOR: generate base_map, maybe add variant landforms and easier configs


[IMMORTAL Yvain]: Bustling page shoes scale higher at 50/normal than 50/hard-drop



- update sanguine restoration to also boost regen a lot?


possible reason for crash: two things iterating over the world_table, nested -- or, deleting more than just the current entry during iteration
- evolutions more likely to add wantonly to the world table though


// skills/abilities

- TODO sort
 - could sort the new skill hash and then iterate over it to apply numeric sort order ids for use in sorter player skill list
 - may NOT actually need to sort



- on class or skill save, should check all players in-game and update them
 - need a func to make sure a player qualifies for all their skill abilities (not just class abilities)


ABIL_VNUM:


SKILL_VNUM:
get_skill_level(vict, SKILL_VNUM(skill));
set_skill(vict, SKILL_VNUM(skill), level);
clear_char_abilities(vict, SKILL_VNUM(skill));
get_skill_exp(vict, SKILL_VNUM(skill))
get_ability_points_available_for_char(vict, SKILL_VNUM(skill))
set_skill(vict, SKILL_VNUM(skill), 100);
get_skill_level(ch, SKILL_VNUM(ABIL_ASSIGNED_SKILL(abil)))); -> could have an abil version of this
noskill_ok(c, SKILL_VNUM(sk))
skdata = get_skill_data(c, SKILL_VNUM(sk), TRUE)
IS_ANY_SKILL_CAP(ch, SKILL_VNUM(ABIL_ASSIGNED_SKILL(abil))) -> could make this a function with a level tier input
--> could create a "level tier" struct that has the 50/75/100, number of skills that can go that high, etc -> specialize would use it
--> need a %ch.can_specialize(skill)% and %ch.specialize_skill(skill)% function
levels_gained_from_ability(ch, abil)
bool can_gain_skill_from(char_data *ch, ability_data *abil) -> this has a TON of skill vnum calls


New Finder:
scan_abilities(ch, vnum, run_func) -> each type would have a scanner; also scans olc

// run_func either reports, or fixes, etc
run_func(ch, abil, vnum, is_olc) {
	if (foo == vnum) {
		do ...
	}
}



New Saver:
- mark vnum/type for saving -> actually marks vnum block only
- write all these every few seconds as-needed
- as many as wanted can be queued without re-writes
- deleters could warn vnums changed



//// socials

- requirements list: { type, vnum, value } -> { skill, Vampire, 1 }, { wearing, Mask, unused }
- sort by name then by has-requirements (to hit ones with reqs first)
- lookup function to find best match

- do_stat_social: requirements
- olc_show_social: requirements
- olc module for requirements

- mention which messages can be omitted
- need auditor

- count reqs for social priority
- color stat social

-> bool validate_social_requirements(char_data *ch, social_data *soc)



/**
* Used for choosing a social that's valid for the player.
*
* @param char_data *ch The person trying to social.
* @param char *name The argument.
* @return social_data* The matching social, or NULL if none.
*/
social_data *find_social_by_name(char_data *ch, char *name) {
	social_data *soc, *next_soc, *partial = NULL;
	
	HASH_ITER(sorted_hh, sorted_socials, soc, next_soc) {
		if (SOCIAL_FLAGGED(soc, SOC_IN_DEVELOPMENT) && !IS_IMMORTAL(ch)) {
			continue;
		}
		
		// matches:
		if (!str_cmp(name, SOC_NAME(soc))) {
			// perfect match
			return soc;
		}
		if (!partial && is_multiword_abbrev(name, SOC_NAME(soc))) {
			// probable match
			partial = soc;
		}
	}
	
	// no exact match...
	return partial;
}



/// mainteance
- todo: add resource actions to the craft for some buildings (platform, drainage, sorcery, portal, etc)
- survey should show real hitpoints
- delaying empire saving just for elogs could greatly reduce elog-caused lag
- maybe need to be able to auto-maintain buildings with instances
- findmaint could also do vehicles?

claim/abandon: trigger emp save?


more resources:
- dig
- clear terrain
- tidy up
- repair -> default (also make default for vehicles)
- scout the area
- block off water
- engrave
- magic words?
- organize


test:
- build
- dismantle
- upgrade
- tunnel
- maintain (while needing resources or while damaged but needing none)
- workforce build/maintain


Vehicles:
[  900] a rickety cart  10
[  901] a carriage  14
[  902] a covered wagon  20
[  903] the catapult  20
[  904] a chair  1
[  905] a wooden bench  1
[  906] a long table  1
[  907] a stool  1
[  917] the throne  1
[  920] a wooden canoe  10  --> change desc
[  921] the caravan  25
[  952] the caravel  32
[  953] a cog  32
[  954] the longship  64
[  955] the brigantine  128
[  956] the carrack  96
[  957] a hulk  64
[10165] a clockwork mule  10
[10715] the sleigh  14
[11034] the clockwork roc  96
[11101] a rib-bone boat  32

alias aa .b edit $1;.hitpoints $2;.save

Buildings:

simple maint:
.res add action 1 dig
.res add action 1 tidy
.res add action 1 repair
.res add action 1 scout
	[ 5000] Grave  4  !RUINS
	[ 5001] Tent  2  !RUINS
	[ 5002] Rock Shelter  2  !RUINS
	[ 5003] Tree Shelter  2  !RUINS
	[ 5004] Cave Shelter  2  !RUINS
	[ 5005] Jungle Shelter  2  !RUINS
	[ 5106] Well  4
	[ 5107] Mine  128  !RUINS
	[ 5122] Quarry  4
	[ 5123] Clay Pit  4
	[ 5140] Steps  4  !RUINS
	[ 5144] Cemetery  16
	[ 5145] Park  10  !RUINS
	[ 5146] Tomb  48


housing:
.res add comp 1 lumber
.res add comp 1 nail
.res add act 1 repair
	[ 5100] Hut  4  tidy, 2x stick
	[ 5101] Cabin  8  tidy, 2x stick
	[ 5102] Small House  8  nail, repair
	[ 5103] Mud Hut  4  dig, 2x stick
	[ 5104] Pueblo  8  tidy, pillar
	[ 5105] Cliff Dwelling  8  nail, repair
	[ 5124] Stone House  16  repair, nail
	[ 5125] Large House  24  lumber, nail
	[ 5126] Tree House  16  repair, nail
	[ 5127] Tree Complex  24  lumber, nail
	[ 5128] Underground Complex  48  pillar, nail
	[ 5129] Mine Complex  128  pillar, nail
	[ 5157] Swamp Hut  4  tidy, 2x stick
	[ 5158] Swamp Manor  16  lumber, nail
	[ 5159] Swamp Complex  24  lumber, nail
	[ 5186] Rickety House  32  scout, lumber, nail
	[ 5187] Strange Burrow  32  scout, lumber, nail
	[ 5188] Old Pueblo  32  scout, lumber, nail
	[ 5189] Obscure Cave  48  scout, lumber, nail


misc:
	[ 5008] Tunnel  128  !RUINS  dig, pillar
	[ 5112] Fence  4  !RUINS  repair, 2x stick
	[ 5113] Gate  4  !RUINS  repair, 2x stick
	[ 5133] Bridge  16  pillar, lumber, nail
	[ 5155] Swamp Walk  10  block water
	[ 5156] Swamp Platform  4  block water, stick
	[ 5191] Draining Oasis  4  block water, stick
	[ 5185] Portal  24  large block, magic words
	[ 5190] Astral Nexus  24  large block, magic words
	[ 5141] Statue  18  clear terrain, large block
	[ 5142] Henge  128  clear terrain, large block
	[ 5143] Megalith  48  clear terrain, large block
	[ 5147] Shrine  16  tidy, nails
	[ 5148] Temple  24  lumber, nail
	[ 5149] Library  24  lumber, nail
	[ 5150] High Temple  48  lumber, nail, block
	[ 5151] Pyramid  128  lumber, nail, block


Artisans:  lumber, nail
	[ 5108] Forge  10
	[ 5109] Mill  10
	[ 5110] Potter  10
	[ 5111] Stable  10
	[ 5130] Alchemist  10
	[ 5131] Apiary  10
	[ 5132] Bakery  10
	[ 5134] Carpenter  10
	[ 5135] Glass Blower  10
	[ 5136] Pigeon Post  10
	[ 5137] Tailor  10
	[ 5138] Tavern  10
	[ 5139] Warehouse  10
	[ 5161] Trapper  10
	[ 5162] Baths  16
	[ 5183] Trading Post  10
	[ 5192] Oilmaker  10
	[ 5152] Docks  10
	[ 5153] Shipyard  16
	[ 5154] Great Shipyard  48  +pillar
	[ 5177] Terraced Garden  32  !RUINS
	[ 5178] Raft Garden  10  !RUINS
	[ 5179] Oasis Garden  10  !RUINS
	[ 5180] Estuary Garden  10  !RUINS
	[ 5181] Shady Garden  10  !RUINS


Storage:  tidy, stick
.res add act 1 tidy
.res add comp 1 stick
	[ 5114] Cannery  8
	[ 5115] Foundry  8
	[ 5116] Granary  8
	[ 5117] Gravel Pit  8
	[ 5118] Lumber Yard  8
	[ 5119] Smokehouse  8
	[ 5120] Tannery  10
	[ 5121] Utility Shed  10
	[ 5193] Cabinet of Curiosities  10  tidy, lumber, nail


Defense:
.res add comp 1 lumber
.res add comp 1 nail
.res add act 1 rep
	[ 5167] Guard Tower  16  lumber, nail
	[ 5168] Advanced Guard Tower  24  lumber, nail
	[ 5169] Superior Guard Tower  64  lumber, nail, iron
	[ 5170] Outpost  64  2x large block
	[ 5171] Barracks  128  2x large block
	[ 5172] Training Yard  128  2x large block
	[ 5173] Gatehouse  32  large block, repair
	[ 5174] Wall  32  large block, repair
	[ 5176] River Gate  32  large block, repair


Castle:
	[ 5160] Courtyard  10  !RUINS  tidy
	[ 5163] Fountain  10  large block
	[ 5164] Tower of Sorcery  128  lumber, nail, large block
	[ 5165] Portcullis  64  2x lumber, 2x nail
	[ 5166] Great Hall  64  2x lumber, 2x nail
	[ 5182] Mint  64  lumber, nail, large block
	[ 5184] Secret Cache  128  lumber, nail, large block


Adventure:
	[ 9180] Cabintent  4  !RUINS  repair
	[10479] Goblin War Tent  64  !RUINS  repair
	[10480] Goblin Message Post  24  !RUINS  repair
	[10481] Goblin Lab Tent  24  !RUINS  repair
	[10482] Goblin Dolmen  48  clear terrain
	[10923] Butcher Tent  48  repair
	[10924] Dragon Cave  128  tidy
	[10925] Dragonslayer Statue  48  clear terrain
	[10926] Dragon Library  48  tidy
	[11051] Dome  48  tidy


//////// bugs in skills



- show stats: abils, etc
- setting skill DOWN too low for class cap resulting in GETTING the "all" class abils
- setting skill UP to the class cap resulted in NOT getting the dragonriding abil




-> island lookups in territory read could be causing lag




//// mounts
- barde must be able to target current mount


//// mobs
- consider de-scaling a mob when its health resets, unless its adventure is locked


/// dps tracker

- prompt key to show dps (current during fight, last-fight when out of combat)

-- credit for DoTs


SCRIPT ERR: Trigger: Dragon loot load boe/bop, VNum 10904, type: 1. unknown object field: 'is_pc' 
Haruka bug: dragon wristblade only needed 1 bloodstone to reforge with renew even though it has 19.5 gear rating


// colossal red dragon adventure:

- don't show "has a quest" if way too low
- don't allow starting quests way below level (configurable under-limit)
- need a level restriction on the pickpocky?

10902 hide 1-4x per kill
10903 bone 1-4x per kill
10927 dragon cloth
10928 dragon leather
10929 dragonsteel
10944 gem

[10900] Mythic Forge: Steelsmith, Guildsman (6 crafts)
[10901] Legendary Stitching: Artificer, Smuggler (2 crafts)
[10902] Lore Crafts: Artificer, Tinker (4 crafts)
[10903] Fabled Hides: Antiquarian, Alchemist (2 crafts)


// new damage spells

-- BADLY need a quick generic way to scale effects to level w/flags


/// ranged combat

- ability to move back to ranged if in melee and not the tank
- inability to use bash, etc at range
- ranged could have resistance to AoE -- Quick-Footed: 50% reduction to AoE damage at range


// multi-spec

- actor.has_component(type, flags, amt)


///// item scaling tweaks

- the +bonus in item scaling happens BEFORE multipliers but it should strongly be AFTER
- hit/dodge scaled too high? or else too many points after 100

- bonus-phys/mag could be up-scaled because basic_speed changed how it applies
- weapon damage needs to cost scaling the same as str/bonus-phys in terms of damage it adds, so it doesn't matter which you get

- hard/group flags possibly giving too big a boost -- it's more than 75 levels better in the 200s


//// meters

-- config to log all meters
- end-meters to log a timestamp and various numbers to file


/// new res

abilities:
602 Prismatic Crystals
609 Moonstone Smelting
612 Herbcloth Weaving
614 Verdant Leathers

sectors:
600 enchanted saplings (planted)
601 nothing-left-to-chop
602 enchanted copse
603 enchanted woods
604 enchanted forest
605 dead forest

objs:
600 crystal seed
601 prismatic seed
602 prismatic crystal
603 enchanted tree
604 enchanted lumber (needs new name)
605 dead wood
606 elderberries
607 shimmering hide
608 shimmering leather
609 moonstone ore
610 argentine ingot
611 eventide ingot
612 sparkfiber cloth
613 manasilk cloth
614 briar hide
615 absinthian leather
616 elegant handle


- look for switch->end in ALL scripts
#

//// combat spam toggles

- dots = hits against?  ==> remove the "not hitting self" restriction on against-*, and count it as a normal against-*


//// roc egg adventure

[IMMORTAL Khufu]: ok I'm on board with making the adventure 100-125 and crafted gear 150
- audit craft for 'requiresobj' same as 'creates'

--> pursue mobs should be better about finding someone in the room with them
--> or we need a super-pursue for adventures
--> need stats on gear

- can see invisible spawner moving
- valid_dg_target -> could take a "direct target" param that allows invisible imms


//// GUARD TOWERS:
- raise damage
- consider regen penalty since target isn't in combat
- mark hostile on first guard tower hit; prevent fillin while hostile
- increased move delay?
- guard towers should shoot enemy vehicles
- shoot enemy vehicles if upgraded?


Removed from shoot_at_char():
	if (GET_PULLING(ch) && CART_CAN_FIRE(GET_PULLING(ch))) {
		log_to_empire(emp, ELOG_HOSTILITY, "A catapult has been spotted at (%d, %d)!", X_COORD(to_room), Y_COORD(to_room));
	}

Removed from tower_would_shoot():
	// try for master of other-pulling
	if (m == vict && pulling && GET_PULLED_BY(pulling, 0) && (GET_PULLED_BY(pulling, 0) != vict)) {
		m = GET_PULLED_BY(pulling, 0)->master ? GET_PULLED_BY(pulling, 0)->master : vict;
	}
	// try for master of other-other-pulling
	if (m == vict && pulling && GET_PULLED_BY(pulling, 1) && (GET_PULLED_BY(pulling, 1) != vict)) {
		m = GET_PULLED_BY(pulling, 1)->master ? GET_PULLED_BY(pulling, 1)->master : vict;
	}


//// vehicles


spawning:
- need a spawn check on char_to_room if inside a vehicle
- ensure no other type of spawn is being called


stealing:
- perform_get_from_container includes some examples on steal


BUILDING SCRIPTING:
- a way to complete a building
  - %building% command
  - add, remove resources
  - way to get the first needed resource
- need a way to get opposite dir
- need a way to check bld flags


EMPIRES:
- way to check diplomacy
 %emp.diplomacy(type,targ)%
 targ is person OR empire
 returns true/false


FUTURE SCRIPT UPDATE:
- %bind% command to bind an item to a person? or build this into %load%
- mhunt may not actually engage combat at the end?
- entry_mtrigger might need to fire on portal movements too and script gotos, too
- could add handy examples to many triggers (like checking vnum on receive)
- need oasound, vasound (and add to helps)
- room vars could use building_flagged, template_flagged, sector_flagged, crop_flagged, indoors, is_on_map
- something to force a repair
- portal entry for vehicles
- filter % out of player's input for speech/command triggers
- ability for %load%s to load an obj into a vehicle


FUTURE VEHICLE DEV:
- need an imm way to unclaim a vehicle
- show a list of vehicles and portals in the room with the vehicle you're in (maybe on "look out" in general)
- come up with a way to upgrade the level on vehicles


ABANDON:
- abandon <target vehicle> -> imm eedit grant
- abandon [no-arg] -> imm eedit grant
- abandon <coords> -> cooldown


-> help mob-var is hitting an exact length scrolling bug?


Newbie zone ideas:

1. resource challenge
- Quest-givers ask for resources in several tiers:
-- trees, rocks, clay
-- lumber, blocks, bricks
-- nails, copper, cloth
- Rewards are skillups and newbie gear
- Sell starter empire skill

2. dragon rookery
- Nest of dragon eggs, tended by sorcerers
- quest timer triggers momma dragon (time limit after beginning quest)
- rewards some low-level gear

4. goblin expedition
- spawns map-roaming goblins
- rewards low-level gear for low-level fights
- possibly also allow bribing the goblins or buying basic resources using coins

5. goblin treasure ship
- spawns on the coast
- goblin sailors on board?
- puzzles rather than combat?

6. goldilocks puzzle
- sleep in which bed, eat which porridge
- rewards include experience and a bear companion?



-> need to check levels on all Whirlwind gear



Seasonal terrain idea:
- sectors need seasonal titles, commands, and interactions (in addition to ones they have all the time)
- build flags would be things that apply all the time only (or, if buildings become like vehicles, could have seasonal build flags but buildings get wiped out if they don't apply in all seasons)
- Flood plain within 2 tiles of river: grasslands most of the year, wider river part time; build flags "river"



//// WRITE CHANTS


--> should add noskill to stealth triggers
--> create high sorc book using triggers
    --> how are book vnums determined? will it ignore system vnums or create numbers after them? Or did I give player books their own block?

bug: .book olc masks obj .book field -> add help on .o book ?


- simple editor for maintaining vnum->name types (things that have no other data)
  - could load into a hash table and use a get-name func that returns a string no matter what: get_typelist_name(typelist *list, int vnum)
  - Stored to file as: ???
  - Interface: typelist <listname>, typelist <listname> add <vnum> <string>, typelist <listname> remove <vnum>



/// SEEDS + COOKING

-> seed?:
  - change to interaction
  - store vnum and level for refunding
  - workforce
-> seed: destroy an object (like butchering) in exchange for seed interaction objs

-> script errors from nohassle: eyeguard + walk


//// IN-GAME MAP GENERATOR

- open with .mapgen -> send the player a verbose warning
- struct map_generator_data -> descriptor (open editor)
- lock world updates (no spawns, no evos, no annuals)
- funcs to:
  - purge all mobs
  - purge all objs
  - destroy all interior rooms
  - empty einvs
  - remove empire territory / npcs
  - remove all complex data
- ability to re-detect and re-number islands, also expand current islands
- re-detect starting locations and set a temporary startloc if needed
- function to force mapout (not all funcs require open editor)
- suggest recompiling to change map size or wrapping rules
- determine land target manually by down-scaling for map size

Dials:
- ability to set desert min/max
- ability to set jungle min/max
- ability to set arctic levels

Terrainers:
- modifier funcs to apply to "this island" or "all islands"
- funcs to add rivers, mountains, etc either from a starting place+dir, or randomly on the island
  - control width and bendiness of each
- various blob patterns
- .sprinkle crop
- fill tool
- tool to delete an entire island

Island tools:
- tag islands with things like "desert", etc for quick lookups or patterns, like applying something to all desert regions

Presets:
- for quick maps, rapid presets

Finishers:
- add arctic
- run all evolutions several times? (but what would this do to desert crops)


///// Quests:

- has own interaction lists, plural, like: mob <vnum> gains interactions: loot xxx, pickpocket xxx
- has own spawn list like sector/building/crop/rmt spawns xxx ??


- deleting things used by quests should add in-dev, log, and also remove from live players?
-> function to cause a refresh on players by quest vnum

-> drop instance-only quests on idle-out/quit but NOT reboot (or alt?)


// MISC


Khyldes says, 'SCRIPT ERR: Trigger: Lumberjack chop, VNum 18100, type: 0. unknown room field: 'buildi??~*g''


+ empire progression web with unlocks and milestones like band -> clan -> ? -> kingdom -> empire


- building maintenance update
- empire maintenance + feeding workforce

- are the sector spawns for Crop sectors used correctly?

- Scripting: way to check hit vs dodge

- map generator -> create base_map, eliminate .wld files, new instructions, new compile location
 - map generator should only poot out a base_map file and a data txt
 - instructions to remove *.wld and replace the wld index with $
 - does .wld tolerate zero rooms?
 - bash script to remove .wld files and 


void save_world_map_to_file(void) {	
	struct map_data *iter;
	FILE *fl;
	
	// shortcut
	if (!world_map_needs_save) {
		return;
	}
	
	if (!(fl = fopen(WORLD_MAP_FILE TEMP_SUFFIX, "w"))) {
		log("Unable to open %s for writing", WORLD_MAP_FILE TEMP_SUFFIX);
		return;
	}
	
	// only bother with ones that aren't base ocean
	for (iter = land_map; iter; iter = iter->next) {
		// x y island sect base natural crop
		fprintf(fl, "%d %d %d %d %d %d %d\n", MAP_X_COORD(iter->vnum), MAP_Y_COORD(iter->vnum), iter->island, (iter->sector_type ? GET_SECT_VNUM(iter->sector_type) : -1), (iter->base_sector ? GET_SECT_VNUM(iter->base_sector) : -1), (iter->natural_sector ? GET_SECT_VNUM(iter->natural_sector) : -1), (iter->crop_type ? GET_CROP_VNUM(iter->crop_type) : -1));
	}
	
	fclose(fl);
	rename(WORLD_MAP_FILE TEMP_SUFFIX, WORLD_MAP_FILE);
	world_map_needs_save = FALSE;
}



-> quest rewards should verify you're on whatever quest they need
 -> may need to rearrange spirit token quests


////// APPROVAL

later:
- public clients like mudconnect.com ?
- add long-form strings to in-game configs
- tips for using approval, doc
- the following things:
	---> plus anything stealthable?	
	STANDARD_CMD( "fire", POS_SITTING, do_fire, LVL_APPROVED, NO_GRANTS, NO_SCMD, CTYPE_COMBAT, CMD_NO_ANIMALS, NO_ABIL ),
	SIMPLE_CMD( "reclaim", POS_STANDING, do_reclaim, LVL_APPROVED, CTYPE_EMPIRE ),
	STANDARD_CMD( "throw", POS_FIGHTING, do_throw, LVL_APPROVED, NO_GRANTS, NO_SCMD, CTYPE_COMBAT, CMD_NO_ANIMALS, NO_ABIL ),


///// XOGIUM BUG

--> access level being set 1 above lvl_top but only on Xogium's copy -> for imm and mortal?
- unsigned char default



///// desert monsoon

slay enough attackers (battle)
1. quest starts by killing any attacker (is this possible?)
2. after killing enough of them, spawn a boss fight
3. killing that boss finishes the quest

seal the monsoon rift (sorc)
1. receive item and instructions to get info at sorcery tower
2. learn spell (timed lessons with key information)
3. perform spell in adventure: must type specific commands from the lesson, when prompted
4. return to sorcery tower for reward

appease a druid (natmag)
1. talk to the druids
2. receive a monsoon totem
3. perform a special chant at locations in the desert outside (mark locations with an item or something to require the player to move around)

blackmail the druid (stealth)
1. thiefy guy in adventure offers quest/instructions
2. must infiltrate the druid's home
3. must steal diary or letter?
4. type 'blackmail druid' to win

craft herbicide (surv/trade)
1. wandering mob wants help
2. craft enough herbicide for him
3. let herbicide age
4. turn in quest

eclipse ritual (vamp)
1. hidden vampire mob on the rift tile, appears if a vampire arrives (also could be found via search)
2. infuse the blood item with blood at an oasis after dark
X. perform the ritual, causing an eclipse



ideas:
- rift closing echoes after adventure complete?
- blackmail quest might be more interesting if it has part 2 with command trigger 'blackmail druid' (thief holds onto the laundry tho)

- find out if triggers queue if one tries to run while another is running






--- could lower skill gain per manufacture/build cycle



terraform command for triggering evos like trench and magic growth; must also be able to check for them

- when I type out "infiltrate" with trig 10161, it goes off twice, but abbreving it only triggers once


-> combat and mobact could be shut off during catchup cycles


-- can-assist(ch, act_, abil_) -> only allow if you have abil or someone doing action in room has it


- combat speed: store last-swing-time for both hands; deal a hit if last+speed<=now
- allows skipping combat rounds without losing hits
- prevents mobs that never hit



/// message queue:

-- send before prompt?


// for stack_msg_to_char()
struct stack_msg {
	char *string;	// text
	int count;	// (x2)
	struct stack_msg *next;
};


struct stack_msg *stack_msg_list;
desc->stack_msg_list;



/**
* Similar to msg_to_desc, but the message is put in a queue for stacking and
* then sent on a very short delay. If more than one identical message is sent
* in this time, it stacks with (x2).
*
* @param descriptor_data *desc The player.
* @param const char *messg... va_arg format.
*/
void stack_msg_to_desc(descriptor_data *desc, const char *messg, ...) {
	char output[MAX_STRING_LENGTH];
	struct stack_msg *iter, *stm;
	bool found = FALSE;
	va_list tArgList;
	
	if (!messg || !desc) {
		return;
	}
	
	va_start(tArgList, messg);
	vsprintf(output, messg, tArgList);
	va_end(tArgList);
	
	// look in queue
	LL_FOREACH(desc->stack_msg_list, iter) {
		if (!strcmp(iter->string, output)) {
			++iter->count;
			found = TRUE;
			break;
		}
	}
	
	// add
	if (!found) {
		CREATE(stm, struct stack_msg, 1);
		stm->string = str_dup(output);
		stm->count = 1;
		LL_APPEND(desc->stack_msg_list, stm);
	}
}


/**
* Flushes a descriptor's stacked messages, adding (x2) where needed.
*
* @param descriptor_data *desc The descriptor to send the messages to.
*/
void send_stacked_msgs(descriptor_data *desc) {
	struct stack_msg *iter, *next_iter;
	int len, rem;
	
	LL_FOREACH_SAFE(desc->stack_msg_list, iter, next_iter) {
		if (iter->count > 1) {
			// deconstruct to add the (x2)
			len = strlen(iter->string);
			rem = (len > 1 && ISNEWL(iter->string[len-1])) ? 1 : 0;
			rem += (len > 2 && ISNEWL(iter->string[len-2])) ? 1 : 0;
			// rebuild
			msg_to_desc(desc, "%*.*s (x%d)%s", (len-rem), (len-rem), iter->string, iter->count, (rem > 0 ? "\r\n" : ""));
		}
		else {
			SEND_TO_Q(NULLSAFE(iter->string), desc);
		}
		
		// free it up
		if (iter->string) {
			free(iter->string);
		}
		free(iter);
	}
	
	desc->stack_msg_list = NULL;
}





/////////// instancer

// utils.h:
#define IS_ANY_BUILDING(room)  ANY_BUILDING_SECT(SECT(room))
#define ANY_BUILDING_SECT(sct)  SECT_FLAGGED((sct), SECTF_MAP_BUILDING | SECTF_INSIDE)

// instance.c
/**
* This function finds a location that matches a linking rule.
*
* @param adv_data *adv The adventure we are linking.
* @param struct adventure_link_rule *rule The linking rule we're trying.
* @param int *which_dir The direction, for linking rules that require one.
* @return room_data* A valid location, or else NULL if we couldn't find one.
*/
room_data *find_location_for_rule(adv_data *adv, struct adventure_link_rule *rule, int *which_dir) {
	extern bool can_build_on(room_data *room, bitvector_t flags);
	
	room_template *start_room = room_template_proto(GET_ADV_START_VNUM(adv));
	sector_data *sect, *next_sect, *findsect = NULL;
	int dir, num_found, found_dir, x_coord, y_coord;
	room_data *room, *shift, *found = NULL;
	struct map_data *map, *map_shift;
	struct sector_index_type *idx;
	bool match_buildon = FALSE;
	bld_data *findbdg = NULL;
	
	*which_dir = found_dir = NO_DIR;
	
	// cannot find a location without a start room
	if (!start_room) {
		return NULL;
	}
	
	// ADV_LINK_x: determine what we're looking for
	switch (rule->type) {
		case ADV_LINK_BUILDING_EXISTING: {
			findbdg = building_proto(rule->value);
			break;
		}
		case ADV_LINK_BUILDING_NEW: {
			match_buildon = TRUE;
			break;
		}
		case ADV_LINK_PORTAL_WORLD: {
			findsect = sector_proto(rule->value);
			break;
		}
		case ADV_LINK_PORTAL_BUILDING_EXISTING: {
			findbdg = building_proto(rule->value);
			break;
		}
		case ADV_LINK_PORTAL_BUILDING_NEW: {
			match_buildon = TRUE;
			break;
		}
		default: {
			// type not implemented or cannot be used to generate a link
			return NULL;
		}
	}
	
	// several ways of doing this:
	if (findsect) {	// only check rooms that have this sector
		num_found = 0;
		idx = find_sector_index(GET_SECT_VNUM(findsect));
		LL_FOREACH2(idx->sect_rooms, map, next_in_sect) {
			// attributes/limits checks
			if (!validate_one_loc(adv, rule, NULL, map) || !validate_linking_limits(adv, NULL, map)) {
				continue;
			}
			
			// SUCCESS: mark it ok
			if (!number(0, num_found++) || !found) {
				// may already have looked up room
				found = real_room(map->vnum);
			}
		}
	}
	else {	// will check multiple sectors (findbdg or match_buildon)
		num_found = 0;
		HASH_ITER(hh, sector_table, sect, next_sect) {
			// shortcut: skip BASIC_OCEAN
			if (GET_SECT_VNUM(sect) == BASIC_OCEAN) {
				continue;
			}
			// are we looking for a building?
			if (findbdg && !ANY_BUILDING_SECT(sect)) {
				continue;
			}
			// are we looking for something we can build on? this is a preliminary match of the build rule (not guaranteed)
			if (match_buildon && !IS_SET(GET_SECT_BUILD_FLAGS(sect), rule->bld_on)) {
				continue;
			}
			
			// ok at this point we are reasonably sure this sector type is valid
			idx = find_sector_index(GET_SECT_VNUM(sect));
			
			// check its map locations
			LL_FOREACH2(idx->sect_rooms, map, next_in_sect) {
				map_shift = NULL;
				dir = NO_DIR;	// we may set this if needed
				
				// if it's instantiated already...
				room = real_real_room(map->vnum);
				
				// actual building type
				if (findbdg && (!room || BUILDING_VNUM(room) != GET_BLD_VNUM(findbdg) || !IS_COMPLETE(room))) {
					continue;
				}
				// check buildon reqs
				if (match_buildon) {
					// must find a facing direction (we'll only try 1 per tile)
					if (rule->bld_facing != NOBITS) {
						// pick a dir (we will ONLY try one per room)
						dir = number(0, NUM_2D_DIRS-1);
						
						// matches the dir we need inside
						if (dir == rule->dir) {
							continue;
						}
						
						// find an adjacent room in that dir
						if (!get_coord_shift(MAP_X_COORD(map->vnum), MAP_Y_COORD(map->vnum), shift_dir[dir][0], shift_dir[dir][1], &x_coord, &y_coord)) {
							continue;
						}
						map_shift = &(world_map[x_coord][y_coord]);
						
						// check bld_facing flags
						if (!IS_SET(GET_SECT_BUILD_FLAGS(map_shift->sector_type), rule->bld_facing)) {
							continue;
						}
					}
				}
				
				// basic validation
				if (!validate_one_loc(adv, rule, NULL, map)) {
					continue;
				}
				// check secondary limits
				if (!validate_linking_limits(adv, NULL, map)) {
					continue;
				}
				
				// need the room by this point
				if (!room) {
					room = real_room(map->vnum);
				}
				
				// checking for buildon that required a room
				if (match_buildon) {
					// can we build here? (never build on a closed location)
					if (ROOM_IS_CLOSED(room) || !can_build_on(room, rule->bld_on)) {
						continue;
					}
					// more validation on the facing tile
					if (rule->bld_facing != NOBITS) {
						// did we somehow get here without these set?
						if (dir == NO_DIR || !map_shift) {
							continue;
						}
						
						// need a valid map tile to face
						if (!(shift = real_room(map_shift->vnum)) || !can_build_on(shift, rule->bld_facing)) {
							continue;
						}
					}
				}
				
				// SUCCESS: mark it ok
				if (!number(0, num_found++) || !found) {
					found = room;
					found_dir = dir;
				}
			}
		}
	}
	
	// and return what we did or did not find
	*which_dir = found_dir;
	return found;
}






/////// LAG WORK

1. queue of things that need global scans/action:
- empire territory (one, all)
- world save
- map save
- mapout writes
- evolutions
- instance spawns
++ create a world update that runs every hour, and schedule ALL world updates during that
  - rooms with random triggers need to fire slightly more often -- call them during this and call them on interior rooms separately if this cycle isn't running

X 2. all territory changes need to twiddle the territory counts of the empire directly INSTEAD of rereading

3. evos and instance linking can run on sector lists
- for evos, iterate over sectors looking for any type of evo that could run, and only iterate over its list if it has an evo

4. this is a good time to fix the new year bug (mud idle)

5. random triggers are probably a huge culprit



bugs:
- util-rescan-all has surprisingly large lag (from members?) --> could possibly skip idle empires on full rescans (but not targeted rescans)


concerns:
- when to add/remove population? especially on abandon -- search for EMPIRE_POPULATION
- does the abandon process delete npcs at some point?


- also look into rereading members

- create homeless population list per island, rapidly re-populate
  - despawn the npc's mob and move the npc to the island's homeless list
  - every tick, attempt to populate using a homeless npc (and reset the populate timer)
  - delete_territory_npc -> 2nd function to just remove them
- make interior rooms share the population timer of the home room



--> random triggers: new type that only fires with a player in the SAME room, for environmentals
-> obj needs to respect global/local
- help file for new type
- apply type to more things


--> could split up Overgrown Forest into multiple types to avoid hitting the same evo cycle
    - could also add a utility for showing which things fall on the same cycle


--> how to speed up distance (maybe track a char's x/y as they move?)
--> room_data could have pointers to its map loc to avoid repeated lookups
    - if room is in a ship, it does NOT have this (so allow for empty room ptr)
GET_LAST_X_COORD(ch) -> -1 or a valid coord


void random_mtrigger(char_data *ch) {
	bool near = FALSE, local = FALSE, checked_near = FALSE, checked_local = FALSE;
	trig_data *t;
	
	if (!SCRIPT_CHECK(ch, MTRIG_RANDOM)) {
		return;
	}
	
	for (t = TRIGGERS(SCRIPT(ch)); t; t = t->next) {
		// validation:
		if (GET_TRIG_DEPTH(t)) {
			continue;	// trigger already running
		}
		if (AFF_FLAGGED(ch, AFF_CHARM) && !TRIGGER_CHECK(t, MTRIG_CHARMED)) {
			continue;	// can't do while charmed
		}
		if (IS_SET(GET_TRIG_TYPE(t), MTRIG_PLAYER_IN_ROOM)) {
			if (!checked_local) {	// prevent repeat checks
				local = any_players_in_room(IN_ROOM(ch));
				checked_local = TRUE;
			}
			if (!local) {
				continue;	// needs players in room
			}
		}
		if (!IS_SET(GET_TRIG_TYPE(t), MTRIG_GLOBAL | MTRIG_PLAYER_IN_ROOM)) {
			if (!checked_near) {	// prevent repeat checks
				near = players_nearby_script(IN_ROOM(ch));
				checked_near = TRUE;
			}
			if (!near) {
				continue;	// needs players nearby
			}
		}
		
		// okay: run it
		if (TRIGGER_CHECK(t, MTRIG_RANDOM) && (number(1, 100) <= GET_TRIG_NARG(t))) {
			union script_driver_data_u sdd;
			sdd.c = ch;
			if (script_driver(&sdd, t, MOB_TRIGGER, TRIG_NEW)) {
				break;
			}
		}
	}
}
			if (IS_SET(SCRIPT_TYPES(sc), WTRIG_PLAYER_IN_ROOM) && !any_players_in_room(room)) {
				continue;	// needs players in room
			}
			if (!IS_SET(SCRIPT_TYPES(sc), WTRIG_GLOBAL | WTRIG_PLAYER_IN_ROOM) && !players_nearby_script(room)) {
				continue;	// needs players nearby
			}


///// DAILIES

- not-completed needs to check only within last day

Test:
- start all, with a daily present, when over the daily limit

--> need to add a requiresquest to crafts
 + craft start
 + recipes
 + vehicle crafting
 + craft info?
 + building buildings?


Khufu    (Jun  1) [BLD1000] quest completed could take an arg to filter
Yvain    (Jun 22) [BLD1000] cancel-quest trigger type


Yvain    (Jul 11) [BLD1000] has-coins / charge-coins quest task/prerequisite (auto-remove coins on hand-in if task?
Yvain    (Jul 11) [BLD1000] ^ just call it 'coins'?


//// Roc updates:

- change salve to uncured salve with timer; timer decays to actual salve with half-hour timer
  ++ could require a specific item in inventory for salve to age correctly
- remove requires-quest on trade items but put timers on those, too
- OR: 24-hour minimum
- change harmful store to 1:1 exchange on talons


	else if (GET_CRAFT_REQUIRES_OBJ(type) != NOTHING && !get_obj_in_list_vnum(GET_CRAFT_REQUIRES_OBJ(type), ch->carrying)) {
		msg_to_char(ch, "You need %s to make that.\r\n", get_obj_name_by_proto(GET_CRAFT_REQUIRES_OBJ(type)));
	}
	else if (!check_can_craft(ch, type)) {
		// sends its own messages
	}
	// TODO: move this above requires_obj and make sure it only requires the obj to START
	else if (CRAFT_FLAGGED(type, CRAFT_VEHICLE)) {
		// vehicles pass off at this point
		do_gen_craft_vehicle(ch, type);
	}


//// new way to track mob counts in instance
- hash table by vnum, attached to instance
- func to attach mob to instance (set instance id, add to hash) and detach
- same for obs

- also possibly a %instance.purge_mobs(VNUM)% and objs


---> dailies not working with repeats-per-instance
- should also show remaining daily count on "quest list" even if not on any quests


//// inv
i -c [no-arg]: could list component types in parens; same for -w, -t
bound specifically, or that it has bind flags like boe/bop? possibly could have -i show wear flags and item type, too: "1. a short sword (25) WEAPON/WIELD"
+ show if bound to the player?


/// offenses against *
- offense data { from player, from empire, type, timestamp }
- offenses against players (started combat, killed)
- offenses against empires (stole, infiltrated, 
- possibly empire vs empire offenses?



//// new home system

- storage like warehouse but for bound items



[    1] wheat
[   10] olives
[   11] peas
[   12] gourds
[   13] apples
[   14] plantains
[ 3000] tomatoes
[ 3002] coconuts
[ 3003] grapes
[ 3005] potatoes
[ 3006] plums
[ 3007] strawberries
[ 3009] blueberries
[ 3011] coffee
[ 3016] watermelons
[10146] saguaro cacti
[10150] mesquite trees
[10153] alligator pears
[10155] limes
[10157] firemelons
[10225] pineapples
[10226] agave
[10227] prickly pears
[10228] aloe
[10338] dragonfruit



/// skill leveling tweak
perhaps require "within 50 levels of player" OR ">= level of the skill" so a level 1 mob would work at Battle 1, but not 2


/// level-0 abilities
Battle:
- Dodge
- Weapons Proficiency

Trade:
- Primitive Crafts

Empire:
- Basic Buildings
- Chores

Survival:
- Scavenging


x	#define ABIL_EVASION  286
x	#define ABIL_WEAPON_PROFICIENCY  287
x	#define ABIL_PRIMITIVE_CRAFTS  288
x	#define ABIL_BASIC_BUILDINGS  289
x	#define ABIL_CHORES  290
x	#define ABIL_SCAVENGING  291
x	#define ABIL_BITE  292	-> update tree
x	#define ABIL_COOK  293



	#define SKILL_BATTLE  0
#define SKILL_EMPIRE  1	// -> roadsign requirement?
#define SKILL_HIGH_SORCERY  2
#define SKILL_NATURAL_MAGIC  3
	#define SKILL_STEALTH  4
	#define SKILL_SURVIVAL  5
	#define SKILL_TRADE  6
#define SKILL_VAMPIRE  7


al aa .o edit $1;.flag add scala;.apply add 1 $2;.min 1;.max 1;.full 24;.save


	[ 3300] fried meat pie	/ str
	[ 3301] apple pie	/ cha
	[ 3302] peach pie	/ cha
	[ 3303] cherry pie	/ cha
	[ 3305] casserole	/ inven
	[ 3306] jerky	/ bonus-phys
	[ 3309] burrito	/ resist-phys
	[ 3313] bread	/ move-regen
	[ 3314] fried rice	/ dex
	[ 3316] chicken and vegetables	/ max-health
	[ 3317] ham and cheese sandwich	/ move-regen
	[ 3320] sausage link	/ to-hit
	[ 3321] road biscuit	/ block
	[ 3322] mushroom plate	/ resist-mag
	[ 3323] pumpkin pie	/ cha
	[ 3325] nutty squash flatbread	/ dodge
	[ 3326] roast plate	/ resist-phys
	[ 3327] fish plate	/ wits
	[ 3328] catfish plate	/ intel
	[ 3330] banana pie	/ cha
	[ 3331] banana bread	/ max-mov
	[ 3333] spicy jerky	/ bonus-mag
	[ 3335] prunes	/ age
	[ 3336] plum pie	/ cha
	[ 3337] strawberry pie	/ cha
	[ 3338] strawberry jam	/ mana-regen
	[ 3340] blueberry pie	/ cha
	[ 3341] blueberry jam	/ max-mana
	[ 3344] oat cake	/ bonus-heal

[10176] herbicide	/ 
	[10340] dragonfruit jam	/ 
	[10503] seafood glowkra gumbo	/ 
	[10512] ironbread	/ 
	[10520] figgy pudding	/ 
	[10522] dried figs	/ 
	[10703] honey-cured ham	/ 
	[10705] fruitcake	/ 
	[10707] roast goose	/ 
	[10716] Christmas cookies	/ 
	[10752] spider meat	/ 
	[10908] dragon roast	/ 


////
- break-tell command removes reply to you on all live players
- on login, check reply-id to see if it's an invisible/incog imm and break it




/// MOB CUSTOM STRINGS
ECHO - string to send to the room
SAY - string to "say"-echo

X OLC editors (delete doubledollar)
X OLC helps
X olc copy on copy, free on free

X structs
X write to file
X read from file
X free char


- periodic function to run it --> run on players with active links?
	- while mob is awake/not fighting


/**
* This is called every few seconds to animate mobs in the room with players.
*/
void run_mob_echoes(void) {
	ACMD(do_say);
	
	struct mob_custom_message *mcm, *found_mcm;
	char_data *ch, *mob, *found_mob;
	descriptor_data *desc;
	int count;
	
	LL_FOREACH(descriptor_list, desc) {
		// validate ch
		if (STATE(desc) != CON_PLAYING || !(ch = desc->character) || !AWAKE(ch)) {
			continue;
		}
		
		// 25% random chance per player to happen at all...
		if (number(0, 3)) {
			continue;
		}
		
		// initialize vars
		found_mob = NULL;
		found_mcm = NULL;
		count = 0;
		
		// now find a mob with a valid message
		LL_FOREACH(ROOM_PEOPLE(IN_ROOM(ch)), mob) {
			// things that disqualify the mob
			if (mob->desc || !IS_NPC(mob) || FIGHTING(mob) || !AWAKE(mob) || MOB_FLAGGED(mob, MOB_TIED) || IS_INJURED(mob, INJ_TIED) || GET_LED_BY(mob)) {
				continue;
			}
			
			// ok now find a random message to show?
			LL_FOREACH(MOB_CUSTOM_MSGS(mob), mcm) {
				// MOB_CUSTOM_x: types we use here
				if (mcm->type != MOB_CUSTOM_ECHO && mcm->type != MOB_CUSTOM_SAY) {
					continue;
				}
				
				// looks good! pick one at random
				if (!number(0, count++) || !found_mcm) {
					found_mob = mob;
					found_mcm = mcm;
				}
			}
		}
		
		// did we find something to show?
		if (found_mcm) {
			// MOB_CUSTOM_x
			switch (found_mcm->type) {
				case MOB_CUSTOM_ECHO: {
					act(found_mcm->msg, FALSE, found_mob, NULL, NULL, TO_ROOM);
					break;
				}
				case MOB_CUSTOM_SAY: {
					do_say(found_mob, found_mcm->msg, 0, 0);
					break;
				}
			}
		}
	}
}


- convert any mob that has an env trigger


struct mob_custom_message
MOB_CUSTOM_MSGS(ch)

#define MOB_CUSTOM_EMOTE  0
#define MOB_CUSTOM_SAY  1

