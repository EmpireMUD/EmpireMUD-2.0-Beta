/* ************************************************************************
*   File: skilldata.c                                     EmpireMUD 2.0b2 *
*  Usage: skill definitions                                               *
*                                                                         *
*  EmpireMUD code base by Paul Clarke, (C) 2000-2015                      *
*  All rights reserved.  See license.doc for complete information.        *
*                                                                         *
*  EmpireMUD based upon CircleMUD 3.0, bpl 17, by Jeremy Elson.           *
*  CircleMUD (C) 1993, 94 by the Trustees of the Johns Hopkins University *
*  CircleMUD is based on DikuMUD, Copyright (C) 1990, 1991.               *
************************************************************************ */

#include "conf.h"
#include "sysdep.h"

#include "structs.h"
#include "skills.h"
#include "utils.h"


// global skills data
struct skill_data_type skill_data[NUM_SKILLS];
struct ability_data_type ability_data[NUM_ABILITIES];
int skill_sort[NUM_SKILLS];
int ability_sort[NUM_ABILITIES];


// skill levels at which you gain an ability -- hoping to make this per-skill soon 4/23/13
int master_ability_levels[] = { 1, 5, 10, 15, 20, 25, 30, 40, 50, 60, 70, 75, 80, 90, 100, -1 };


// set up all skills
void init_skills() {
	void sort_skills_and_abilities();
	void init_skill_data();
	void init_ability_data();
	void setup_skill(int number, char *name, int flags, char *description, char *creation_description);
	void setup_ability(int number, int parent_skill, int parent_skill_required, int parent_ability, char *name);
	
	#define setup_class_ability(abil, name)	setup_ability((abil), NO_SKILL, 100, NO_PREREQ, name)
	
	init_skill_data();
	init_ability_data();

	// Battle
	setup_skill(SKILL_BATTLE, "Battle", 0, "Charge confidently into combat", "Battle is important for combat, especially tanks and melee damage characters");
		setup_ability(ABIL_ENDURANCE, SKILL_BATTLE, 75, NO_PREREQ, "Endurance");
		setup_ability(ABIL_REFLEXES, SKILL_BATTLE, 15, NO_PREREQ, "Reflexes");
			setup_ability(ABIL_SHIELD_BLOCK, SKILL_BATTLE, 30, ABIL_REFLEXES, "Shield Block");
				setup_ability(ABIL_BLOCK_ARROWS, SKILL_BATTLE, 50, ABIL_SHIELD_BLOCK, "Block Arrows");
				setup_ability(ABIL_QUICK_BLOCK, SKILL_BATTLE, 60, ABIL_SHIELD_BLOCK, "Quick Block");
			setup_ability(ABIL_DISARM, SKILL_BATTLE, 75, ABIL_REFLEXES, "Disarm");
			setup_ability(ABIL_OUTRAGE, SKILL_BATTLE, 75, ABIL_REFLEXES, "Outrage");
		setup_ability(ABIL_RESCUE, SKILL_BATTLE, 50, NO_PREREQ, "Rescue");
		setup_ability(ABIL_SPARRING, SKILL_BATTLE, 15, NO_PREREQ, "Sparring");
			setup_ability(ABIL_KICK, SKILL_BATTLE, 30, ABIL_SPARRING, "Kick");
				setup_ability(ABIL_BASH, SKILL_BATTLE, 40, ABIL_KICK, "Bash");				
			setup_ability(ABIL_CUT_DEEP, SKILL_BATTLE, 65, ABIL_SPARRING, "Cut Deep");
			setup_ability(ABIL_STUNNING_BLOW, SKILL_BATTLE, 75, ABIL_SPARRING, "Stunning Blow");
			setup_ability(ABIL_FINESSE, SKILL_BATTLE, 50, ABIL_SPARRING, "Finesse");
		setup_ability(ABIL_ARCHERY, SKILL_BATTLE, 20, NO_PREREQ, "Archery");
			setup_ability(ABIL_QUICK_DRAW, SKILL_BATTLE, 60, ABIL_ARCHERY, "Quick Draw");
		setup_ability(ABIL_BIG_GAME_HUNTER, SKILL_BATTLE, 75, NO_PREREQ, "Big Game Hunter");
			setup_ability(ABIL_NULL_MANA, SKILL_BATTLE, 90, ABIL_BIG_GAME_HUNTER, "Null Mana");
			setup_ability(ABIL_HEARTSTOP, SKILL_BATTLE, 95, ABIL_BIG_GAME_HUNTER, "Heartstop");			
		setup_ability(ABIL_FIRSTAID, SKILL_BATTLE, 15, NO_PREREQ, "Firstaid");
		setup_ability(ABIL_FLEET, SKILL_BATTLE, 15, NO_PREREQ, "Fleet");
		setup_ability(ABIL_MAGE_ARMOR, SKILL_BATTLE, 35, NO_PREREQ, "Mage Armor");
		setup_ability(ABIL_LIGHT_ARMOR, SKILL_BATTLE, 55, NO_PREREQ, "Light Armor");
		setup_ability(ABIL_MEDIUM_ARMOR, SKILL_BATTLE, 50, NO_PREREQ, "Medium Armor");
		setup_ability(ABIL_HEAVY_ARMOR, SKILL_BATTLE, 75, NO_PREREQ, "Heavy Armor");
	// end Battle
	

	// Empire: gains up to 15 just from simple empire tasks
	setup_skill(SKILL_EMPIRE, "Empire", 0, "Lead your empire to greatness", "Empire allows you to construct buildings and cities");
		setup_ability(ABIL_BARDE, SKILL_EMPIRE, 20, NO_PREREQ, "Barde");
		setup_ability(ABIL_LOCKS, SKILL_EMPIRE, 30, NO_PREREQ, "Locksmithing");
			setup_ability(ABIL_RAISE_ARMIES, SKILL_EMPIRE, 50, ABIL_LOCKS, "Raise Armies");
				setup_ability(ABIL_ADVANCED_TACTICS, SKILL_EMPIRE, 80, ABIL_RAISE_ARMIES, "Advanced Tactics");
				setup_ability(ABIL_GREAT_WALLS, SKILL_EMPIRE, 60, ABIL_RAISE_ARMIES, "Great Walls");
			setup_ability(ABIL_SUMMON_GUARDS, SKILL_EMPIRE, 60, ABIL_LOCKS, "Summon Guards");
				setup_ability(ABIL_SUMMON_BODYGUARD, SKILL_EMPIRE, 90, ABIL_SUMMON_GUARDS, "Summon Bodyguard");
		setup_ability(ABIL_HOUSING, SKILL_EMPIRE, 15, NO_PREREQ, "Housing");
			setup_ability(ABIL_WORKFORCE, SKILL_EMPIRE, 55, ABIL_HOUSING, "Workforce");
				setup_ability(ABIL_SKILLED_LABOR, SKILL_EMPIRE, 75, ABIL_WORKFORCE, "Skilled Labor");
			setup_ability(ABIL_SWAMP_ENGINEERING, SKILL_EMPIRE, 55, ABIL_HOUSING, "Swamp Engineering");
		setup_ability(ABIL_CUSTOMIZE_BUILDING, SKILL_EMPIRE, 50, NO_PREREQ, "Customize Building");
		setup_ability(ABIL_ARTISANS, SKILL_EMPIRE, 25, NO_PREREQ, "Artisans");
			setup_ability(ABIL_TRADE_ROUTES, SKILL_EMPIRE, 50, ABIL_ARTISANS, "Trade Routes");
			setup_ability(ABIL_COMMERCE, SKILL_EMPIRE, 65, ABIL_TRADE_ROUTES, "Commerce");
			setup_ability(ABIL_LUXURY, SKILL_EMPIRE, 75, ABIL_ARTISANS, "Luxury");
			setup_ability(ABIL_MONUMENTS, SKILL_EMPIRE, 45, ABIL_ARTISANS, "Monuments");
				setup_ability(ABIL_GRAND_MONUMENTS, SKILL_EMPIRE, 65, ABIL_MONUMENTS, "Grand Monuments");
			setup_ability(ABIL_SEAFARING, SKILL_EMPIRE, 35, ABIL_ARTISANS, "Seafaring");
		setup_ability(ABIL_ROADS, SKILL_EMPIRE, 15, NO_PREREQ, "Roads");
			setup_ability(ABIL_CITY_LIGHTS, SKILL_EMPIRE, 35, ABIL_ROADS, "City Lights");
			setup_ability(ABIL_TUNNEL, SKILL_EMPIRE, 80, ABIL_ROADS, "Tunnel");
		setup_ability(ABIL_RADIANCE, SKILL_EMPIRE, 75, NO_PREREQ, "Radiance");
			setup_ability(ABIL_PROMINENCE, SKILL_EMPIRE, 95, ABIL_RADIANCE, "Prominence");
			setup_ability(ABIL_REWARD, SKILL_EMPIRE, 90, ABIL_RADIANCE, "Reward");
			setup_ability(ABIL_INSPIRE, SKILL_EMPIRE, 80, ABIL_RADIANCE, "Inspire");
		setup_ability(ABIL_PROSPECT, SKILL_EMPIRE, 25, NO_PREREQ, "Prospect");
			setup_ability(ABIL_DEEP_MINES, SKILL_EMPIRE, 55, ABIL_PROSPECT, "Deep Mines");
			setup_ability(ABIL_RARE_METALS, SKILL_EMPIRE, 70, ABIL_PROSPECT, "Rare Metals");
	// end Empire
	
	setup_skill(SKILL_HIGH_SORCERY, "High Sorcery", 0, "Learn the secrets of the imperial sorcerers", "High Sorcery lets you cast powerful spells, rituals, and enchantments");
		setup_ability(ABIL_RITUAL_OF_BURDENS, SKILL_HIGH_SORCERY, 1, NO_PREREQ, "Ritual of Burdens");
			setup_ability(ABIL_VIGOR, SKILL_HIGH_SORCERY, 40, ABIL_RITUAL_OF_BURDENS, "Vigor");
			setup_ability(ABIL_DEVASTATION_RITUAL, SKILL_HIGH_SORCERY, 55, ABIL_RITUAL_OF_BURDENS, "Devastation Ritual");
			setup_ability(ABIL_SUMMON_MATERIALS, SKILL_HIGH_SORCERY, 75, ABIL_RITUAL_OF_BURDENS, "Summon Materials");
		setup_ability(ABIL_STAFFMAKING, SKILL_HIGH_SORCERY, 10, NO_PREREQ, "Staffmaking");
			setup_ability(ABIL_POWERFUL_STAVES, SKILL_HIGH_SORCERY, 75, ABIL_STAFFMAKING, "Powerful Staves");
				setup_ability(ABIL_STAFF_MASTERY, SKILL_HIGH_SORCERY, 85, ABIL_POWERFUL_STAVES, "Staff Mastery");
					setup_ability(ABIL_SIEGE_RITUAL, SKILL_HIGH_SORCERY, 95, ABIL_STAFF_MASTERY, "Siege Ritual");
		setup_ability(ABIL_MANASHIELD, SKILL_HIGH_SORCERY, 1, NO_PREREQ, "Manashield");
			setup_ability(ABIL_FORESIGHT, SKILL_HIGH_SORCERY, 15, ABIL_MANASHIELD, "Foresight");
			setup_ability(ABIL_COLORBURST, SKILL_HIGH_SORCERY, 25, ABIL_MANASHIELD, "Colorburst");
				setup_ability(ABIL_ENERVATE, SKILL_HIGH_SORCERY, 55, ABIL_COLORBURST, "Enervate");
					setup_ability(ABIL_SLOW, SKILL_HIGH_SORCERY, 75, ABIL_ENERVATE, "Slow");
				setup_ability(ABIL_SIPHON, SKILL_HIGH_SORCERY, 50, ABIL_COLORBURST, "Siphon");
					setup_ability(ABIL_MIRRORIMAGE, SKILL_HIGH_SORCERY, 65, ABIL_SIPHON, "Mirrorimage");
					setup_ability(ABIL_SUNSHOCK, SKILL_HIGH_SORCERY, 80, ABIL_COLORBURST, "Sunshock");
						setup_ability(ABIL_PHOENIX_RITE, SKILL_HIGH_SORCERY, 90, ABIL_SUNSHOCK, "Phoenix Rite");
		setup_ability(ABIL_DISENCHANT, SKILL_HIGH_SORCERY, 20, NO_PREREQ, "Disenchant");
			setup_ability(ABIL_DISPEL, SKILL_HIGH_SORCERY, 80, ABIL_DISENCHANT, "Dispel");
			setup_ability(ABIL_ENCHANT_WEAPONS, SKILL_HIGH_SORCERY, 30, ABIL_DISENCHANT, "Enchant Weapons");
				setup_ability(ABIL_ENCHANT_ARMOR, SKILL_HIGH_SORCERY, 50, ABIL_ENCHANT_WEAPONS, "Enchant Armor");
					setup_ability(ABIL_ENCHANT_TOOLS, SKILL_HIGH_SORCERY, 60, ABIL_ENCHANT_ARMOR, "Enchant Tools");
				setup_ability(ABIL_GREATER_ENCHANTMENTS, SKILL_HIGH_SORCERY, 90, ABIL_ENCHANT_WEAPONS, "Greater Enchantments");
			setup_ability(ABIL_RITUAL_OF_DEFENSE, SKILL_HIGH_SORCERY, 75, ABIL_DISENCHANT, "Ritual of Defense");
		setup_ability(ABIL_RITUAL_OF_TELEPORTATION, SKILL_HIGH_SORCERY, 55, NO_PREREQ, "Ritual of Teleportation");
			setup_ability(ABIL_CITY_TELEPORTATION, SKILL_HIGH_SORCERY, 65, ABIL_RITUAL_OF_TELEPORTATION, "City Teleportation");
				setup_ability(ABIL_PORTAL_MAGIC, SKILL_HIGH_SORCERY, 75, ABIL_CITY_TELEPORTATION, "Portal Magic");
					setup_ability(ABIL_PORTAL_MASTER, SKILL_HIGH_SORCERY, 85, ABIL_PORTAL_MAGIC, "Portal Master");
		setup_ability(ABIL_SUMMON_SWIFT, SKILL_HIGH_SORCERY, 15, NO_PREREQ, "Summon Swift");
		setup_ability(ABIL_SENSE_LIFE_RITUAL, SKILL_HIGH_SORCERY, 50, NO_PREREQ, "Sense Life Ritual");
			setup_ability(ABIL_RITUAL_OF_DETECTION, SKILL_HIGH_SORCERY, 75, ABIL_SENSE_LIFE_RITUAL, "Ritual of Detection");
		setup_ability(ABIL_ARCANE_POWER, SKILL_HIGH_SORCERY, 75, NO_PREREQ, "Arcane Power");
	// end High Sorcery
	
	setup_skill(SKILL_NATURAL_MAGIC, "Natural Magic", 0, "Attain mastery over the natural world", "Natural Magic lets you use mana to heal, fight, and make potions");
		setup_ability(ABIL_HEAL, SKILL_NATURAL_MAGIC, 1, NO_PREREQ, "Heal");
			setup_ability(ABIL_HEAL_FRIEND, SKILL_NATURAL_MAGIC, 10, ABIL_HEAL, "Heal Friend");
				setup_ability(ABIL_REJUVENATE, SKILL_NATURAL_MAGIC, 50, ABIL_HEAL_FRIEND, "Rejuvenate");
				setup_ability(ABIL_HEAL_PARTY, SKILL_NATURAL_MAGIC, 75, ABIL_HEAL_FRIEND, "Heal Party");
				setup_ability(ABIL_EARTHARMOR, SKILL_NATURAL_MAGIC, 25, ABIL_HEAL_FRIEND, "Eartharmor");
			setup_ability(ABIL_CLEANSE, SKILL_NATURAL_MAGIC, 55, ABIL_HEAL, "Cleanse");
				setup_ability(ABIL_PURIFY, SKILL_NATURAL_MAGIC, 90, ABIL_CLEANSE, "Purify");
		setup_ability(ABIL_READY_FIREBALL, SKILL_NATURAL_MAGIC, 1, NO_PREREQ, "Ready Fireball");
			setup_ability(ABIL_FAMILIAR, SKILL_NATURAL_MAGIC, 10, ABIL_READY_FIREBALL, "Familiar");
				setup_ability(ABIL_HASTEN, SKILL_NATURAL_MAGIC, 80, ABIL_FAMILIAR, "Hasten");
				setup_ability(ABIL_COUNTERSPELL, SKILL_NATURAL_MAGIC, 60, ABIL_FAMILIAR, "Counterspell");
			setup_ability(ABIL_LIGHTNINGBOLT, SKILL_NATURAL_MAGIC, 30, ABIL_READY_FIREBALL, "Lightningbolt");
				setup_ability(ABIL_SKYBRAND, SKILL_NATURAL_MAGIC, 50, ABIL_LIGHTNINGBOLT, "Skybrand");
			setup_ability(ABIL_ENTANGLE, SKILL_NATURAL_MAGIC, 65, ABIL_READY_FIREBALL, "Entangle");
		setup_ability(ABIL_CHANT_OF_NATURE, SKILL_NATURAL_MAGIC, 50, NO_PREREQ, "Chant of Nature");
		setup_ability(ABIL_SOLAR_POWER, SKILL_NATURAL_MAGIC, 10, NO_PREREQ, "Solar Power");
			setup_ability(ABIL_GIFT_OF_NATURE, SKILL_NATURAL_MAGIC, 75, ABIL_SOLAR_POWER, "Gift of Nature");
		setup_ability(ABIL_TOUCH_OF_FLAME, SKILL_NATURAL_MAGIC, 15, NO_PREREQ, "Touch of Flame");
		setup_ability(ABIL_EARTHMELD, SKILL_NATURAL_MAGIC, 20, NO_PREREQ, "Earthmeld");
			setup_ability(ABIL_WORM, SKILL_NATURAL_MAGIC, 80, ABIL_EARTHMELD, "Worm");
		setup_ability(ABIL_SOULSIGHT, SKILL_NATURAL_MAGIC, 35, NO_PREREQ, "Soulsight");
		setup_ability(ABIL_SUMMON_ANIMALS, SKILL_NATURAL_MAGIC, 15, NO_PREREQ, "Summon Animals");
			setup_ability(ABIL_ANIMAL_FORMS, SKILL_NATURAL_MAGIC, 50, ABIL_SUMMON_ANIMALS, "Animal Forms");
		setup_ability(ABIL_FLY, SKILL_NATURAL_MAGIC, 85, NO_PREREQ, "Fly");
		setup_ability(ABIL_HEALING_POTIONS, SKILL_NATURAL_MAGIC, 55, NO_PREREQ, "Healing Potions");
			setup_ability(ABIL_NATURE_POTIONS, SKILL_NATURAL_MAGIC, 70, ABIL_HEALING_POTIONS, "Nature Potions");
				setup_ability(ABIL_HEALING_ELIXIRS, SKILL_NATURAL_MAGIC, 85, ABIL_NATURE_POTIONS, "Healing Elixirs");
					setup_ability(ABIL_WRATH_OF_NATURE_POTIONS, SKILL_NATURAL_MAGIC, 95, ABIL_HEALING_ELIXIRS, "Wrath of Nature Potions");
	// end Natural Magic
	
	setup_skill(SKILL_STEALTH, "Stealth", 0, "Become one with the shadows and learn their secrets", "Stealth lets you hide, sneak, infiltrate, steal, and create poisons");
		setup_ability(ABIL_SNEAK, SKILL_STEALTH, 1, NO_PREREQ, "Sneak");
			setup_ability(ABIL_PICKPOCKET, SKILL_STEALTH, 10, ABIL_SNEAK, "Pickpocket");
				setup_ability(ABIL_STEAL, SKILL_STEALTH, 50, ABIL_PICKPOCKET, "Steal");
					setup_ability(ABIL_VAULTCRACKING, SKILL_STEALTH, 95, ABIL_STEAL, "Vaultcracking");
					setup_ability(ABIL_INFILTRATE, SKILL_STEALTH, 55, ABIL_STEAL, "Infiltrate");
						setup_ability(ABIL_IMPROVED_INFILTRATE, SKILL_STEALTH, 80, ABIL_INFILTRATE, "Improved Infiltrate");
							setup_ability(ABIL_ESCAPE, SKILL_STEALTH, 90, ABIL_IMPROVED_INFILTRATE, "Escape");
				setup_ability(ABIL_APPRAISAL, SKILL_STEALTH, 60, ABIL_PICKPOCKET, "Appraisal");
				setup_ability(ABIL_CONCEALMENT, SKILL_STEALTH, 85, ABIL_PICKPOCKET, "Concealment");
			setup_ability(ABIL_SECRET_CACHE, SKILL_STEALTH, 75, ABIL_SNEAK, "Secret Cache");
			setup_ability(ABIL_SEARCH, SKILL_STEALTH, 30, ABIL_SNEAK, "Search");
		setup_ability(ABIL_HIDE, SKILL_STEALTH, 1, NO_PREREQ, "Hide");
			setup_ability(ABIL_DISGUISE, SKILL_STEALTH, 20, ABIL_HIDE, "Disguise");
			setup_ability(ABIL_TERRIFY, SKILL_STEALTH, 30, ABIL_HIDE, "Terrify");
				setup_ability(ABIL_DARKNESS, SKILL_STEALTH, 55, ABIL_TERRIFY, "Darkness");
			setup_ability(ABIL_CLING_TO_SHADOW, SKILL_STEALTH, 50, ABIL_HIDE, "Cling to Shadow");
				setup_ability(ABIL_UNSEEN_PASSING, SKILL_STEALTH, 75, ABIL_CLING_TO_SHADOW, "Unseen Passing");
				setup_ability(ABIL_CLOAK_OF_DARKNESS, SKILL_STEALTH, 80, ABIL_CLING_TO_SHADOW, "Cloak of Darkness");
					setup_ability(ABIL_SHADOWSTEP, SKILL_STEALTH, 90, ABIL_CLOAK_OF_DARKNESS, "Shadowstep");
		setup_ability(ABIL_BACKSTAB, SKILL_STEALTH, 10, NO_PREREQ, "Backstab");
			setup_ability(ABIL_JAB, SKILL_STEALTH, 25, ABIL_BACKSTAB, "Jab");
				setup_ability(ABIL_BLIND, SKILL_STEALTH, 50, ABIL_JAB, "Blind");
				setup_ability(ABIL_SAP, SKILL_STEALTH, 80, ABIL_JAB, "Sap");
				setup_ability(ABIL_DAGGER_MASTERY, SKILL_STEALTH, 75, ABIL_JAB, "Dagger Mastery");
			setup_ability(ABIL_SUMMON_THUGS, SKILL_STEALTH, 60, ABIL_BACKSTAB, "Summon Thugs");
			setup_ability(ABIL_POISONS, SKILL_STEALTH, 65, ABIL_BACKSTAB, "Poisons");
				setup_ability(ABIL_DEADLY_POISONS, SKILL_STEALTH, 90, ABIL_POISONS, "Deadly Poisons");
				setup_ability(ABIL_PRICK, SKILL_STEALTH, 75, ABIL_POISONS, "Prick");
	// end Stealth
	
	setup_skill(SKILL_SURVIVAL, "Survival", 0, "Make your way in the harsh wilderness", "Survival helps you navigate, find food, drink less, and ride animals");
		setup_ability(ABIL_SWIMMING, SKILL_SURVIVAL, 15, NO_PREREQ, "Swimming");
		setup_ability(ABIL_FORAGE, SKILL_SURVIVAL, 15, NO_PREREQ, "Forage");
			setup_ability(ABIL_SATED_THIRST, SKILL_SURVIVAL, 75, ABIL_FORAGE, "Sated Thirst");
		setup_ability(ABIL_RIDE, SKILL_SURVIVAL, 25, NO_PREREQ, "Ride");
			setup_ability(ABIL_ALL_TERRAIN_RIDING, SKILL_SURVIVAL, 85, ABIL_RIDE, "All-Terrain Riding");
		setup_ability(ABIL_MOUNTAIN_CLIMBING, SKILL_SURVIVAL, 35, NO_PREREQ, "Mountain Climbing");
			setup_ability(ABIL_STAMINA, SKILL_SURVIVAL, 65, ABIL_MOUNTAIN_CLIMBING, "Stamina");
			setup_ability(ABIL_PATHFINDING, SKILL_SURVIVAL, 90, ABIL_MOUNTAIN_CLIMBING, "Pathfinding");
		setup_ability(ABIL_BY_MOONLIGHT, SKILL_SURVIVAL, 50, NO_PREREQ, "By Moonlight");
			setup_ability(ABIL_NIGHTSIGHT, SKILL_SURVIVAL, 75, ABIL_BY_MOONLIGHT, "Nightsight");
		setup_ability(ABIL_FINDER, SKILL_SURVIVAL, 40, NO_PREREQ, "Finder");
			setup_ability(ABIL_FIND_HERBS, SKILL_SURVIVAL, 50, ABIL_FINDER, "Find Herbs");
				setup_ability(ABIL_HERB_GARDENS, SKILL_SURVIVAL, 60, ABIL_FIND_HERBS, "Herb Gardens");
				setup_ability(ABIL_RESIST_POISON, SKILL_SURVIVAL, 75, ABIL_FIND_HERBS, "Resist Poison");
					setup_ability(ABIL_POISON_IMMUNITY, SKILL_SURVIVAL, 90, ABIL_RESIST_POISON, "Poison Immunity");
		setup_ability(ABIL_FISH, SKILL_SURVIVAL, 10, NO_PREREQ, "Fish");
		setup_ability(ABIL_NAVIGATION, SKILL_SURVIVAL, 5, NO_PREREQ, "Navigation");
		setup_ability(ABIL_TRACK, SKILL_SURVIVAL, 50, NO_PREREQ, "Track");
			setup_ability(ABIL_MASTER_TRACKER, SKILL_SURVIVAL, 80, ABIL_TRACK, "Master Tracker");
				setup_ability(ABIL_NO_TRACE, SKILL_SURVIVAL, 95, ABIL_MASTER_TRACKER, "No Trace");
		setup_ability(ABIL_FIND_SHELTER, SKILL_SURVIVAL, 20, NO_PREREQ, "Find Shelter");
		setup_ability(ABIL_BUTCHER, SKILL_SURVIVAL, 1, NO_PREREQ, "Butcher");
			setup_ability(ABIL_HUNT, SKILL_SURVIVAL, 10, ABIL_BUTCHER, "Hunt");
		setup_ability(ABIL_MASTER_SURVIVALIST, SKILL_SURVIVAL, 95, NO_PREREQ, "Master Survivalist");
	
	setup_skill(SKILL_TRADE, "Trade", 0, "Earn your fortune on the roads and seas", "Trade allows you to craft tools, weapons, ships, and trinkets");
		setup_ability(ABIL_FORGE, SKILL_TRADE, 15, NO_PREREQ, "Forge");
			setup_ability(ABIL_WEAPONCRAFTING, SKILL_TRADE, 30, ABIL_FORGE, "Weaponcrafting");
				setup_ability(ABIL_IRON_BLADES, SKILL_TRADE, 55, ABIL_WEAPONCRAFTING, "Iron Blades");
				setup_ability(ABIL_DEADLY_WEAPONS, SKILL_TRADE, 80, ABIL_IRON_BLADES, "Deadly Weapons");
			setup_ability(ABIL_ARMORSMITHING, SKILL_TRADE, 25, ABIL_FORGE, "Armorsmithing");
				setup_ability(ABIL_STURDY_ARMORS, SKILL_TRADE, 50, ABIL_ARMORSMITHING, "Sturdy Armors");
				setup_ability(ABIL_IMPERIAL_ARMORS, SKILL_TRADE, 85, ABIL_STURDY_ARMORS, "Imperial Armors");
			setup_ability(ABIL_JEWELRY, SKILL_TRADE, 60, ABIL_FORGE, "Jewelry");
			setup_ability(ABIL_REFORGE, SKILL_TRADE, 80, ABIL_FORGE, "Reforge");
				setup_ability(ABIL_MASTER_BLACKSMITH, SKILL_TRADE, 90, ABIL_REFORGE, "Master Blacksmith");
		setup_ability(ABIL_SEWING, SKILL_TRADE, 5, NO_PREREQ, "Sew");
			setup_ability(ABIL_MAGIC_ATTIRE, SKILL_TRADE, 50, ABIL_SEWING, "Magic Attire");
				setup_ability(ABIL_MAGICAL_VESTMENTS, SKILL_TRADE, 75, ABIL_MAGIC_ATTIRE, "Magical Vestments");
					setup_ability(ABIL_REFASHION, SKILL_TRADE, 80, ABIL_MAGICAL_VESTMENTS, "Refashion");
						setup_ability(ABIL_MASTER_TAILOR, SKILL_TRADE, 90, ABIL_REFASHION, "Master Tailor");
			setup_ability(ABIL_RAWHIDE_STITCHING, SKILL_TRADE, 35, ABIL_SEWING, "Rawhide Stitching");
				setup_ability(ABIL_LEATHERWORKING, SKILL_TRADE, 50, ABIL_RAWHIDE_STITCHING, "Leatherworking");
					setup_ability(ABIL_DANGEROUS_LEATHERS, SKILL_TRADE, 75, ABIL_LEATHERWORKING, "Dangerous Leathers");
		setup_ability(ABIL_BASIC_CRAFTS, SKILL_TRADE, 1, NO_PREREQ, "Basic Crafts");
			setup_ability(ABIL_POTTERY, SKILL_TRADE, 10, ABIL_BASIC_CRAFTS, "Pottery");
				setup_ability(ABIL_FINE_POTTERY, SKILL_TRADE, 50, ABIL_POTTERY, "Fine Pottery");
					setup_ability(ABIL_MASTER_CRAFTSMAN, SKILL_TRADE, 85, ABIL_FINE_POTTERY, "Master Craftsman");
			setup_ability(ABIL_WOODWORKING, SKILL_TRADE, 15, ABIL_BASIC_CRAFTS, "Woodworking");
				setup_ability(ABIL_ADVANCED_WOODWORKING, SKILL_TRADE, 40, ABIL_WOODWORKING, "Advanced Woodworking");
					setup_ability(ABIL_SIEGEWORKS, SKILL_TRADE, 55, ABIL_ADVANCED_WOODWORKING, "Siegeworks");
				setup_ability(ABIL_SHIPBUILDING, SKILL_TRADE, 65, ABIL_WOODWORKING, "Shipbuilding");
					setup_ability(ABIL_ADVANCED_SHIPS, SKILL_TRADE, 80, ABIL_SHIPBUILDING, "Advanced Ships");
		setup_ability(ABIL_MASTER_FARMER, SKILL_TRADE, 75, NO_PREREQ, "Master Farmer");
	// end Trade
	
	setup_skill(SKILL_VAMPIRE, "Vampire", SKLF_VAMPIRE, "Feed the hunger, control the blood", "Vampire allows you to drink only blood, to shapeshift, and see in the dark");
		setup_ability(ABIL_REGENERATE, SKILL_VAMPIRE, 15, NO_PREREQ, "Regenerate");
			setup_ability(ABIL_ALACRITY, SKILL_VAMPIRE, 60, ABIL_REGENERATE, "Alacrity");
		setup_ability(ABIL_TASTE_BLOOD, SKILL_VAMPIRE, 15, NO_PREREQ, "Taste Blood");
			setup_ability(ABIL_BLOODSWORD, SKILL_VAMPIRE, 30, ABIL_TASTE_BLOOD, "Bloodsword");
			setup_ability(ABIL_VEINTAP, SKILL_VAMPIRE, 50, ABIL_BLOODSWORD, "Veintap");
				setup_ability(ABIL_ANCIENT_BLOOD, SKILL_VAMPIRE, 75, ABIL_VEINTAP, "Ancient Blood");
				setup_ability(ABIL_SANGUINE_RESTORATION, SKILL_VAMPIRE, 95, ABIL_VEINTAP, "Sanguine Restoration");
		setup_ability(ABIL_BOOST, SKILL_VAMPIRE, 75, NO_PREREQ, "Boost");
		setup_ability(ABIL_CLAWS, SKILL_VAMPIRE, 25, NO_PREREQ, "Claws");
			setup_ability(ABIL_PREDATOR_VISION, SKILL_VAMPIRE, 45, ABIL_CLAWS, "Predator Vision");
			setup_ability(ABIL_WOLF_FORM, SKILL_VAMPIRE, 50, ABIL_CLAWS, "Wolf Form");
				setup_ability(ABIL_MIST_FORM, SKILL_VAMPIRE, 75, ABIL_WOLF_FORM, "Mist Form");
				setup_ability(ABIL_BAT_FORM, SKILL_VAMPIRE, 85, ABIL_WOLF_FORM, "Bat Form");
		setup_ability(ABIL_DEATHSHROUD, SKILL_VAMPIRE, 20, NO_PREREQ, "Deathshroud");
			setup_ability(ABIL_MUMMIFY, SKILL_VAMPIRE, 50, ABIL_DEATHSHROUD, "Mummify");
			setup_ability(ABIL_SOULMASK, SKILL_VAMPIRE, 65, ABIL_DEATHSHROUD, "Soulmask");
			setup_ability(ABIL_UNNATURAL_THIRST, SKILL_VAMPIRE, 55, ABIL_DEATHSHROUD, "Unnatural Thirst");
			setup_ability(ABIL_WEAKEN, SKILL_VAMPIRE, 80, ABIL_DEATHSHROUD, "Weaken");
		setup_ability(ABIL_COMMAND, SKILL_VAMPIRE, 45, NO_PREREQ, "Command");
			setup_ability(ABIL_SUMMON_HUMANS, SKILL_VAMPIRE, 55, ABIL_COMMAND, "Summon Humans");
			setup_ability(ABIL_MAJESTY, SKILL_VAMPIRE, 90, ABIL_COMMAND, "Majesty");
		setup_ability(ABIL_HAVENS, SKILL_VAMPIRE, 35, NO_PREREQ, "Build Haven");
		setup_ability(ABIL_DAYWALKING, SKILL_VAMPIRE, 75, NO_PREREQ, "Daywalking");
	// end Vampire
	
	
	// class-only abilities (require less data; see class.c for more)
	
	setup_class_ability(ABIL_ALCHEMIST_CRAFTS, "Alchemist Crafts");
	setup_class_ability(ABIL_ANCESTRAL_HEALING, "Ancestral Healing");
	setup_class_ability(ABIL_ANTIQUARIAN_CRAFTS, "Antiquarian Crafts");
	setup_class_ability(ABIL_ARTIFICER_CRAFTS, "Artificer Crafts");
	setup_class_ability(ABIL_BANSHEE, "Banshee");
	setup_class_ability(ABIL_BASILISK, "Basilisk");
	setup_class_ability(ABIL_BLOOD_FORTITUDE, "Blood Fortitude");
	setup_class_ability(ABIL_BLOODSWEAT, "Bloodsweat");
	setup_class_ability(ABIL_CONFER, "Confer");
	setup_class_ability(ABIL_CRUCIAL_JAB, "Crucial Jab");
	setup_class_ability(ABIL_DRAGONRIDING, "Dragonriding");
	setup_class_ability(ABIL_DREAD_BLOOD_FORM, "Dread Blood Form");
	setup_class_ability(ABIL_DIRE_WOLF, "Dire Wolf");
	setup_class_ability(ABIL_DIVERSION, "Diversion");
	setup_class_ability(ABIL_DUAL_WIELD, "Dual-Wield");
	setup_class_ability(ABIL_EXARCH_CRAFTS, "Exarch Crafts");
	setup_class_ability(ABIL_FASTCASTING, "Fastcasting");
	setup_class_ability(ABIL_GRIFFIN, "Griffin");
	setup_class_ability(ABIL_GUILDSMAN_CRAFTS, "Guildsman Crafts");
	setup_class_ability(ABIL_HORRID_FORM, "Horrid Form");
	setup_class_ability(ABIL_HOWL, "Howl");
	setup_class_ability(ABIL_MANTICORE, "Manticore");
	setup_class_ability(ABIL_MOON_RABBIT, "Moon Rabbit");
	setup_class_ability(ABIL_MOONRISE, "Moonrise");
	setup_class_ability(ABIL_NOBLE_BEARING, "Noble Bearing");
	setup_class_ability(ABIL_OWL_SHADOW, "Owl Shadow");
	setup_class_ability(ABIL_PHOENIX, "Phoenix");
	setup_class_ability(ABIL_RESURRECT, "Resurrect");
	setup_class_ability(ABIL_SAGE_WEREWOLF_FORM, "Sage Werewolf Form");
	setup_class_ability(ABIL_SALAMANDER, "Salamander");
	setup_class_ability(ABIL_SAVAGE_WEREWOLF_FORM, "Savage Werewolf Form");
	setup_class_ability(ABIL_SCORPION_SHADOW, "Scorpion Shadow");
	setup_class_ability(ABIL_SHADOW_JAB, "Shadow Jab");
	setup_class_ability(ABIL_SHADOW_KICK, "Shadow Kick");
	setup_class_ability(ABIL_SHADOWCAGE, "Shadowcage");
	setup_class_ability(ABIL_SKELETAL_HULK, "Skeletal Hulk");
	setup_class_ability(ABIL_SMUGGLER_CRAFTS, "Smuggler Crafts");
	setup_class_ability(ABIL_SPIRIT_WOLF, "Spirit Wolf");
	setup_class_ability(ABIL_STAGGER_JAB, "Stagger Jab");
	setup_class_ability(ABIL_STEELSMITH_CRAFTS, "Steelsmith Crafts");
	setup_class_ability(ABIL_TINKER_CRAFTS, "Tinker Crafts");
	setup_class_ability(ABIL_TOWERING_WEREWOLF_FORM, "Towering Werewolf Form");
	setup_class_ability(ABIL_TWO_HANDED_WEAPONS, "Two-Handed Weapons");
	setup_class_ability(ABIL_WARD_AGAINST_MAGIC, "Ward Against Magic");
	
	// Workforce abilities (ties into do_chore_gen_craft()), they are essentially class abilities with no class
	setup_class_ability(ABIL_WORKFORCE_SAWING, "Workforce Sawing");
	
	sort_skills_and_abilities();
}


// modifiers to your skill level before a skill check
double skill_check_difficulty_modifier[NUM_DIFF_TYPES] = {
	1.5,  // easy
	1,  // medium
	0.66,  // hard
	0.1  // rarely
};


// clear the skill_data structure
void init_skill_data() {
	int iter;
	
	for (iter = 0; iter < NUM_SKILLS; ++iter) {
		skill_data[iter].number = iter;
		skill_data[iter].name = NULL;
		skill_data[iter].flags = 0;
		skill_data[iter].description = NULL;
	}
}


/**
* int number The skill's SKILL_x define
* char *name The skill's display name
* int flags Any skills flags to set
* char *description The skill's one-line description.
*/
void setup_skill(int number, char *name, int flags, char *description, char *creation_description) {
	skill_data[number].number = number;	// yo dawg, I heard you liked numbers
	skill_data[number].name = strdup(name);
	skill_data[number].flags = flags;
	skill_data[number].description = strdup(description);
	skill_data[number].creation_description = strdup(creation_description);
}


// sets up the ability_data array
void init_ability_data() {
	int iter;
	
	for (iter = 0; iter < NUM_ABILITIES; ++iter) {
		ability_data[iter].number = iter;
		ability_data[iter].parent_skill = NO_SKILL;
		ability_data[iter].parent_skill_required = 101;
		ability_data[iter].parent_ability = NO_ABIL;
		ability_data[iter].name = NULL;
	}
}


/**
* int number The ability's ABIL_x define
* int parent_skill The SKILL_x that this falls under, or NO_SKILL
* int parent_skill_required The amount of that skill needed to purchase
* char *name The display name
* int parent_ability If this ability has a prereq, its ABIL_x define, otherwise NO_ABIL
*/
void setup_ability(int number, int parent_skill, int parent_skill_required, int parent_ability, char *name) {
	ability_data[number].number = number;
	ability_data[number].parent_skill = parent_skill;
	ability_data[number].parent_skill_required = parent_skill_required;
	ability_data[number].parent_ability = parent_ability;
	ability_data[number].name = strdup(name);
}


void sort_skills_and_abilities() {
	int iter, a, b, temp;
	
	// init skills
	for (iter = 0; iter < NUM_SKILLS; ++iter) {
		skill_sort[iter] = iter;
	}
	
	// init abils
	for (iter = 0; iter < NUM_ABILITIES; ++iter) {
		ability_sort[iter] = iter;
	}
	
	// sort skills
	for (a = 0; a < NUM_SKILLS - 1; ++a) {
		for (b = a + 1; b < NUM_SKILLS; ++b) {
			if (str_cmp(skill_data[skill_sort[a]].name, skill_data[skill_sort[b]].name) > 0) {
				temp = skill_sort[a];
				skill_sort[a] = skill_sort[b];
				skill_sort[b] = temp;
			}
		}
	}
	
	// sort abilities
	for (a = 0; a < NUM_ABILITIES - 1; ++a) {
		for (b = a + 1; b < NUM_ABILITIES; ++b) {
			if (ability_data[a].name && ability_data[b].name && str_cmp(ability_data[ability_sort[a]].name, ability_data[ability_sort[b]].name) > 0) {
				temp = ability_sort[a];
				ability_sort[a] = ability_sort[b];
				ability_sort[b] = temp;
			}
		}
	}
}
