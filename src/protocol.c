/* ************************************************************************
*   File: protocol.c                                      EmpireMUD 2.0b5 *
*  Usage: KaVir's protocol snippet                                        *
*                                                                         *
*  EmpireMUD code base by Paul Clarke, (C) 2000-2015                      *
*  All rights reserved.  See license.doc for complete information.        *
*                                                                         *
*  EmpireMUD based upon CircleMUD 3.0, bpl 17, by Jeremy Elson.           *
*  CircleMUD (C) 1993, 94 by the Trustees of the Johns Hopkins University *
*  CircleMUD is based on DikuMUD, Copyright (C) 1990, 1991.               *
************************************************************************ */

/******************************************************************************
 Protocol snippet by KaVir.  Released into the public domain in February 2011.

 In case this is not legally possible:
 The copyright holder grants any entity the right to use this work for any 
 purpose, without any conditions, unless such conditions are required by law.
 
 Updates and fixes to this snippet by Paul Clarke, including the repeat color
 code reduction.
 *****************************************************************************/

#include <alloca.h>

#include "conf.h"
#include "sysdep.h"

#include "structs.h"
#include "utils.h"
#include "comm.h"
#include "handler.h"
#include "db.h"
#include "skills.h"
#include "dg_scripts.h"

#ifdef HAVE_ARPA_TELNET_H
#include <arpa/telnet.h>
#else
#include "telnet.h"
#endif


static void Write(descriptor_t *apDescriptor, const char *apData) {
	if (apDescriptor != NULL && apDescriptor->has_prompt) {
		if (apDescriptor->pProtocol->WriteOOB > 0 || *(apDescriptor->output) == '\0') {
			apDescriptor->pProtocol->WriteOOB = 2;
		}
	}

	write_to_output(apData, apDescriptor);
}

static void ReportBug(const char *apText) {
	log("%s", apText);
}

static void InfoMessage(descriptor_t *apDescriptor, const char *apData) {
	Write(apDescriptor, "\t[F210][\toINFO\t[F210]]\tn ");
	Write(apDescriptor, apData);
	apDescriptor->pProtocol->WriteOOB = 0;
}

static void CompressStart(descriptor_t *apDescriptor) {
	/* If your mud uses MCCP (Mud Client Compression Protocol), you need to 
	* call whatever function normally starts compression from here - the 
	* ReportBug() call should then be deleted.
	* 
	* Otherwise you can just ignore this function.
	*/
	ReportBug("CompressStart() in protocol.c is being called, but it doesn't do anything!\n");
}

static void CompressEnd(descriptor_t *apDescriptor) {
	/* If your mud uses MCCP (Mud Client Compression Protocol), you need to 
	* call whatever function normally starts compression from here - the 
	* ReportBug() call should then be deleted.
	* 
	* Otherwise you can just ignore this function.
	*/
	ReportBug("CompressEnd() in protocol.c is being called, but it doesn't do anything!\n");
}

/******************************************************************************
 MSDP file-scope variables.
 *****************************************************************************/

/**
* These are for the GUI_VARIABLES, my unofficial extension of MSDP.  They're 
* intended for clients that wish to offer a generic GUI - not as nice as a 
* custom GUI, admittedly, but still better than a plain terminal window.
*
* These are per-player so that they can be customised for different characters 
* (eg changing 'mana' to 'blood' for vampires).  You could even allow players 
* to customise the buttons and gauges themselves if you wish.
*/
static const char s_Button1[] = "\005\002Help\002help\006";
static const char s_Button2[] = "\005\002Look\002look\006";
static const char s_Button3[] = "\005\002Score\002score\006";
static const char s_Button4[] = "\005\002Equipment\002equipment\006";
static const char s_Button5[] = "\005\002Inventory\002inventory\006";

static const char s_Gauge1[] = "\005\002Health\002red\002HEALTH\002HEALTH_MAX\006";
static const char s_Gauge2[] = "\005\002Mana\002blue\002MANA\002MANA_MAX\006";
static const char s_Gauge3[] = "\005\002Movement\002green\002MOVEMENT\002MOVEMENT_MAX\006";
static const char s_Gauge4[] = "\005\002Blood\002red\002BLOOD\002BLOOD_MAX\006";
static const char s_Gauge5[] = "\005\002Opponent\002darkred\002OPPONENT_HEALTH\002OPPONENT_HEALTH_MAX\006";


/******************************************************************************
 MSDP variable table.
 *****************************************************************************/

/* Macros for readability, but you can remove them if you don't like them */
#define NUMBER_READ_ONLY           false, false, false, false, -1, -1,  0, NULL
#define NUMBER_READ_ONLY_SET_TO(x) false, false, false, false, -1, -1,  x, NULL
#define STRING_READ_ONLY           true,  false, false, false, -1, -1,  0, NULL
#define NUMBER_IN_THE_RANGE(x,y)   false, true,  false, false,  x,  y,  0, NULL
#define BOOLEAN_SET_TO(x)          false, true,  false, false,  0,  1,  x, NULL
#define STRING_WITH_LENGTH_OF(x,y) true,  true,  false, false,  x,  y,  0, NULL
#define STRING_WRITE_ONCE(x,y)     true,  true,  true,  false,  x,  y,  0, NULL
#define STRING_GUI(x)              true,  false, false, true,  -1, -1,  0, x

static variable_name_t VariableNameTable[eMSDP_MAX+1] = {
	/* General */
	{ eMSDP_ACCOUNT_NAME, "ACCOUNT_NAME", STRING_READ_ONLY },
	{ eMSDP_CHARACTER_NAME, "CHARACTER_NAME", STRING_READ_ONLY },
	{ eMSDP_SERVER_ID, "SERVER_ID", STRING_READ_ONLY },
	{ eMSDP_SNIPPET_VERSION, "SNIPPET_VERSION", NUMBER_READ_ONLY_SET_TO(SNIPPET_VERSION) },
	
	/* Character */
	{ eMSDP_GENDER, "GENDER", STRING_READ_ONLY },
	{ eMSDP_HEALTH, "HEALTH", NUMBER_READ_ONLY },
	{ eMSDP_HEALTH_MAX, "HEALTH_MAX", NUMBER_READ_ONLY },
	{ eMSDP_HEALTH_REGEN, "HEALTH_REGEN", NUMBER_READ_ONLY },
	{ eMSDP_MANA, "MANA", NUMBER_READ_ONLY },
	{ eMSDP_MANA_MAX, "MANA_MAX", NUMBER_READ_ONLY },
	{ eMSDP_MANA_REGEN, "MANA_REGEN", NUMBER_READ_ONLY },
	{ eMSDP_MOVEMENT, "MOVEMENT", NUMBER_READ_ONLY },
	{ eMSDP_MOVEMENT_MAX, "MOVEMENT_MAX", NUMBER_READ_ONLY },
	{ eMSDP_MOVEMENT_REGEN, "MOVEMENT_REGEN", NUMBER_READ_ONLY },
	{ eMSDP_BLOOD, "BLOOD", NUMBER_READ_ONLY },
	{ eMSDP_BLOOD_MAX, "BLOOD_MAX", NUMBER_READ_ONLY },
	{ eMSDP_BLOOD_UPKEEP, "BLOOD_UPKEEP", NUMBER_READ_ONLY },

	{ eMSDP_AFFECTS, "AFFECTS", STRING_READ_ONLY },
	{ eMSDP_DOTS, "DOTS", STRING_READ_ONLY },
	{ eMSDP_COOLDOWNS, "COOLDOWNS", STRING_READ_ONLY },
	{ eMSDP_LEVEL, "LEVEL", NUMBER_READ_ONLY },
	{ eMSDP_SKILL_LEVEL, "SKILL_LEVEL", NUMBER_READ_ONLY },
	{ eMSDP_GEAR_LEVEL, "GEAR_LEVEL", NUMBER_READ_ONLY },
	{ eMSDP_CRAFTING_LEVEL, "CRAFTING_LEVEL", NUMBER_READ_ONLY },
	{ eMSDP_CLASS, "CLASS", STRING_READ_ONLY },
	{ eMSDP_SKILLS, "SKILLS", STRING_READ_ONLY },
	{ eMSDP_MONEY, "MONEY", NUMBER_READ_ONLY },
	{ eMSDP_BONUS_EXP, "BONUS_EXP", NUMBER_READ_ONLY },
	{ eMSDP_INVENTORY, "INVENTORY", NUMBER_READ_ONLY },
	{ eMSDP_INVENTORY_MAX, "INVENTORY_MAX", NUMBER_READ_ONLY },
	
	{ eMSDP_STR, "STR", NUMBER_READ_ONLY },
	{ eMSDP_DEX, "DEX", NUMBER_READ_ONLY },
	{ eMSDP_CHA, "CHA", NUMBER_READ_ONLY },
	{ eMSDP_GRT, "GRT", NUMBER_READ_ONLY },
	{ eMSDP_INT, "INT", NUMBER_READ_ONLY },
	{ eMSDP_WIT, "WIT", NUMBER_READ_ONLY },
	{ eMSDP_STR_PERM, "STR_PERM", NUMBER_READ_ONLY },
	{ eMSDP_DEX_PERM, "DEX_PERM", NUMBER_READ_ONLY },
	{ eMSDP_CHA_PERM, "CHA_PERM", NUMBER_READ_ONLY },
	{ eMSDP_GRT_PERM, "GRT_PERM", NUMBER_READ_ONLY },
	{ eMSDP_INT_PERM, "INT_PERM", NUMBER_READ_ONLY },
	{ eMSDP_WIT_PERM, "WIT_PERM", NUMBER_READ_ONLY },
	
	{ eMSDP_BLOCK, "BLOCK", NUMBER_READ_ONLY },
	{ eMSDP_DODGE, "DODGE", NUMBER_READ_ONLY },
	{ eMSDP_TO_HIT, "TO_HIT", NUMBER_READ_ONLY },
	{ eMSDP_SPEED, "SPEED", STRING_READ_ONLY },
	{ eMSDP_RESIST_PHYSICAL, "RESIST_PHYSICAL", NUMBER_READ_ONLY },
	{ eMSDP_RESIST_MAGICAL, "RESIST_MAGICAL", NUMBER_READ_ONLY },
	{ eMSDP_BONUS_PHYSICAL, "BONUS_PHYSICAL", NUMBER_READ_ONLY },
	{ eMSDP_BONUS_MAGICAL, "BONUS_MAGICAL", NUMBER_READ_ONLY },
	{ eMSDP_BONUS_HEALING, "BONUS_HEALING", NUMBER_READ_ONLY },
	
	// Empire
	{ eMSDP_EMPIRE_NAME, "EMPIRE_NAME", STRING_READ_ONLY },
	{ eMSDP_EMPIRE_ADJECTIVE, "EMPIRE_ADJECTIVE", STRING_READ_ONLY },
	{ eMSDP_EMPIRE_RANK, "EMPIRE_RANK", STRING_READ_ONLY },
	{ eMSDP_EMPIRE_TERRITORY, "EMPIRE_TERRITORY", NUMBER_READ_ONLY },
	{ eMSDP_EMPIRE_TERRITORY_MAX, "EMPIRE_TERRITORY_MAX", NUMBER_READ_ONLY },
	{ eMSDP_EMPIRE_TERRITORY_OUTSIDE, "EMPIRE_TERRITORY_OUTSIDE", NUMBER_READ_ONLY },
	{ eMSDP_EMPIRE_TERRITORY_OUTSIDE_MAX, "EMPIRE_TERRITORY_OUTSIDE_MAX", NUMBER_READ_ONLY },
	{ eMSDP_EMPIRE_TERRITORY_FRONTIER, "EMPIRE_TERRITORY_FRONTIER", NUMBER_READ_ONLY },
	{ eMSDP_EMPIRE_TERRITORY_FRONTIER_MAX, "EMPIRE_TERRITORY_FRONTIER_MAX", NUMBER_READ_ONLY },
	{ eMSDP_EMPIRE_WEALTH, "EMPIRE_WEALTH", NUMBER_READ_ONLY },
	{ eMSDP_EMPIRE_SCORE, "EMPIRE_SCORE", NUMBER_READ_ONLY },
	
	/* Combat */
	{ eMSDP_OPPONENT_HEALTH, "OPPONENT_HEALTH", NUMBER_READ_ONLY },
	{ eMSDP_OPPONENT_HEALTH_MAX, "OPPONENT_HEALTH_MAX", NUMBER_READ_ONLY },
	{ eMSDP_OPPONENT_LEVEL, "OPPONENT_LEVEL", NUMBER_READ_ONLY },
	{ eMSDP_OPPONENT_NAME, "OPPONENT_NAME", STRING_READ_ONLY },
	{ eMSDP_OPPONENT_FOCUS_HEALTH, "OPPONENT_FOCUS_HEALTH", NUMBER_READ_ONLY },
	{ eMSDP_OPPONENT_FOCUS_HEALTH_MAX, "OPPONENT_FOCUS_HEALTH_MAX", NUMBER_READ_ONLY },
	{ eMSDP_OPPONENT_FOCUS_NAME, "OPPONENT_FOCUS_NAME", STRING_READ_ONLY },
	
	/* World */
	{ eMSDP_AREA_NAME, "AREA_NAME", STRING_READ_ONLY },
	{ eMSDP_ROOM, "ROOM", STRING_READ_ONLY },
	{ eMSDP_ROOM_EXITS, "ROOM_EXITS", STRING_READ_ONLY },
	{ eMSDP_ROOM_NAME, "ROOM_NAME", STRING_READ_ONLY },
	{ eMSDP_ROOM_VNUM, "ROOM_VNUM", NUMBER_READ_ONLY },
	{ eMSDP_WORLD_TIME, "WORLD_TIME", NUMBER_READ_ONLY },
	{ eMSDP_WORLD_SEASON, "WORLD_SEASON", STRING_READ_ONLY },
	
	/* Configurable variables */
	{ eMSDP_CLIENT_ID, "CLIENT_ID", STRING_WRITE_ONCE(1,40) },
	{ eMSDP_CLIENT_VERSION, "CLIENT_VERSION", STRING_WRITE_ONCE(1,40) },
	{ eMSDP_PLUGIN_ID, "PLUGIN_ID", STRING_WITH_LENGTH_OF(1,40) },
	{ eMSDP_ANSI_COLORS, "ANSI_COLORS", BOOLEAN_SET_TO(1) },
	{ eMSDP_XTERM_256_COLORS, "XTERM_256_COLORS", BOOLEAN_SET_TO(0) },
	{ eMSDP_UTF_8, "UTF_8", BOOLEAN_SET_TO(0) },
	{ eMSDP_SOUND, "SOUND", BOOLEAN_SET_TO(0) },
	{ eMSDP_MXP, "MXP", BOOLEAN_SET_TO(0) },
	
	/* GUI variables */
	{ eMSDP_BUTTON_1, "BUTTON_1", STRING_GUI(s_Button1) },
	{ eMSDP_BUTTON_2, "BUTTON_2", STRING_GUI(s_Button2) },
	{ eMSDP_BUTTON_3, "BUTTON_3", STRING_GUI(s_Button3) },
	{ eMSDP_BUTTON_4, "BUTTON_4", STRING_GUI(s_Button4) },
	{ eMSDP_BUTTON_5, "BUTTON_5", STRING_GUI(s_Button5) },
	{ eMSDP_GAUGE_1, "GAUGE_1", STRING_GUI(s_Gauge1) },
	{ eMSDP_GAUGE_2, "GAUGE_2", STRING_GUI(s_Gauge2) },
	{ eMSDP_GAUGE_3, "GAUGE_3", STRING_GUI(s_Gauge3) },
	{ eMSDP_GAUGE_4, "GAUGE_4", STRING_GUI(s_Gauge4) },
	{ eMSDP_GAUGE_5, "GAUGE_5", STRING_GUI(s_Gauge5) },
	
	{ eMSDP_MAX, "", 0 } /* This must always be last. */
};


/******************************************************************************
 MSSP file-scope variables.
 *****************************************************************************/

static int s_Players = 0;
static time_t s_Uptime = 0;


/******************************************************************************
 Local function prototypes.
 *****************************************************************************/

static void Negotiate(descriptor_t *apDescriptor);
static void PerformHandshake(descriptor_t *apDescriptor, char aCmd, char aProtocol);
static void PerformSubnegotiation(descriptor_t *apDescriptor, char aCmd, char *apData, int aSize);
static void SendNegotiationSequence(descriptor_t *apDescriptor, char aCmd, char aProtocol);
static bool_t ConfirmNegotiation(descriptor_t *apDescriptor, negotiated_t aProtocol, bool_t abWillDo, bool_t abSendReply);

static void ParseMSDP(descriptor_t *apDescriptor, const char *apData);
static void ExecuteMSDPPair(descriptor_t *apDescriptor, const char *apVariable, const char *apValue);

static void ParseATCP(descriptor_t *apDescriptor, const char *apData);
#ifdef MUDLET_PACKAGE
	static void SendATCP(descriptor_t *apDescriptor, const char *apVariable, const char *apValue);
#endif /* MUDLET_PACKAGE */

static void SendMSSP(descriptor_t *apDescriptor);

static char *GetMxpTag(const char *apTag, const char *apText);

static const char *GetAnsiColour(bool_t abBackground, int aRed, int aGreen, int aBlue);
static const char *GetRGBColour(bool_t abBackground, int aRed, int aGreen, int aBlue);
static bool_t IsValidColour(const char *apArgument);

static bool_t MatchString(const char *apFirst, const char *apSecond);
static bool_t PrefixString(const char *apPart, const char *apWhole);
static bool_t IsNumber(const char *apString);
static char *AllocString(const char *apString);


/******************************************************************************
 ANSI colour codes.
 *****************************************************************************/

static const char s_Clean[] = "\033[0m";	// Remove colour

static const char s_DarkBlack[] = "\033[0;30m";	// Black foreground
static const char s_DarkRed[] = "\033[0;31m";	// Red foreground
static const char s_DarkGreen[] = "\033[0;32m";	// Green foreground
static const char s_DarkYellow[] = "\033[0;33m";	// Yellow foreground
static const char s_DarkBlue[] = "\033[0;34m";	// Blue foreground
static const char s_DarkMagenta[] = "\033[0;35m";	// Magenta foreground
static const char s_DarkCyan[] = "\033[0;36m";	// Cyan foreground
static const char s_DarkWhite[] = "\033[0;37m";	// White foreground

static const char s_BoldBlack[] = "\033[1;30m";	// Grey foreground
static const char s_BoldRed[] = "\033[1;31m";	// Bright red foreground
static const char s_BoldGreen[] = "\033[1;32m";	// Bright green foreground
static const char s_BoldYellow[] = "\033[1;33m";	// Bright yellow foreground
static const char s_BoldBlue[] = "\033[1;34m";	// Bright blue foreground
static const char s_BoldMagenta[] = "\033[1;35m";	// Bright magenta foreground
static const char s_BoldCyan[] = "\033[1;36m";	// Bright cyan foreground
static const char s_BoldWhite[] = "\033[1;37m";	// Bright white foreground

static const char s_BackBlack[] = "\033[1;40m";	// Black background
static const char s_BackRed[] = "\033[1;41m";	// Red background
static const char s_BackGreen[] = "\033[1;42m";	// Green background
static const char s_BackYellow[] = "\033[1;43m";	// Yellow background
static const char s_BackBlue[] = "\033[1;44m";	// Blue background
static const char s_BackMagenta[] = "\033[1;45m";	// Magenta background
static const char s_BackCyan[] = "\033[1;46m";	// Cyan background
static const char s_BackWhite[] = "\033[1;47m";	// White background

static const char s_Underline[] = "\033[4m";	// underline


/******************************************************************************
 Color reduction system: reduce number of color codes sent.
 *****************************************************************************/

/**
* Signals the system that the output is requesting a color code. You can call
* this for foreground, background, or both. The color strings passed here
* should be like "\033[0;31m", not internal "\tr" or "&r" codes.
*
* Use want_reduced_color_clean() and want_reduced_color_underline() instead
* for &0, &n, &u.
*
* @param descriptor_data *desc The connection getting the color code.
* @param const char *fg The foreground color string requested (NULL if only doing bg).
* @param const char *bg The background color string requested (NULL if only doing fg).
*/
static void want_reduced_color_codes(descriptor_data *desc, const char *fg, const char *bg) {
	if (!desc) {
		return;
	}
	
	if (fg && *fg) {
		// check if they asked for &0 but are repeating the same colors: requesting the same fg as before, (there wasn't a bg or they're requesting the same bg), (there wasn't an underline or they are requesting the same underline)
		if (desc->color.want_clean && !strcmp(fg, desc->color.last_fg) && (!*desc->color.last_bg || !strcmp(desc->color.want_bg, desc->color.last_bg)) && (!desc->color.is_underline || desc->color.want_underline == desc->color.is_underline)) {
			// cancel the clean; don't need a new want
			desc->color.want_clean = FALSE;
		}
		// this happens even if the last one triggered
		if (strcmp(fg, desc->color.want_fg) && (*desc->color.want_fg || strcmp(fg, desc->color.last_fg) || desc->color.want_clean)) {
			snprintf(desc->color.want_fg, COLREDUC_SIZE, "%s", fg);
		}
	}
	if (bg && *bg) {
		// check if they asked for &0 but are repeating the same colors: requesting the same bg as before, (there wasn't a fg or they're requesting the same fg), (there wasn't an underline or they are requesting the same underline)
		if (desc->color.want_clean && !strcmp(bg, desc->color.last_bg) && (!*desc->color.last_fg || !strcmp(desc->color.want_fg, desc->color.last_fg)) && (!desc->color.is_underline || desc->color.want_underline == desc->color.is_underline)) {
			// cancel the clean; don't need a new want
			desc->color.want_clean = FALSE;
		}
		// this happens even if the last one triggered
		if (strcmp(bg, desc->color.want_bg) && (*desc->color.want_bg || strcmp(bg, desc->color.last_bg) || desc->color.want_clean || (*desc->color.want_fg && strstr(desc->color.want_fg, "[0;")))) {
			snprintf(desc->color.want_bg, COLREDUC_SIZE, "%s", bg);
		}
	}
}


/**
* Signals that the output is requesting a &0 or &n color terminator.
*
* @param descriptor_data *desc The connection getting the clean-color.
*/
static void want_reduced_color_clean(descriptor_data *desc) {
	if (!desc) {
		return;
	}
	desc->color.want_clean = TRUE;
	desc->color.want_underline = FALSE;
	*desc->color.want_fg = '\0';
	*desc->color.want_bg = '\0';
}


/**
* Signals that the output is requesting an underline.
*
* @param descriptor_data *desc The connection getting the underline.
*/
static void want_reduced_color_underline(descriptor_data *desc) {
	if (!desc) {
		return;
	}
	desc->color.want_underline = TRUE;
}


/**
* This indicates that we're ready to send all requested color codes, and mark
* them as sent.
*
* @param descriptor_data *desc The person who is getting color codes.
* @return char* The string of rendered color codes to send.
*/
char *flush_reduced_color_codes(descriptor_data *desc) {
	static char output[COLREDUC_SIZE * 4 + 1];	// guarantee enough room
	*output = '\0';
	
	if (!desc) {
		return output;
	}
	
	if (desc->color.want_clean) {
		if (!desc->color.is_clean) {
			strcat(output, s_Clean);
			desc->color.is_clean = TRUE;
		}
		desc->color.want_clean = FALSE;
		desc->color.is_underline = FALSE;
		*desc->color.last_fg = '\0';
		*desc->color.last_bg = '\0';
	}
	
	if (*desc->color.want_fg) {
		strcat(output, desc->color.want_fg);
		strcpy(desc->color.last_fg, desc->color.want_fg);
		*desc->color.want_fg = '\0';
		desc->color.is_clean = FALSE;
	}
	if (*desc->color.want_bg) {
		strcat(output, desc->color.want_bg);
		strcpy(desc->color.last_bg, desc->color.want_bg);
		*desc->color.want_bg = '\0';
		desc->color.is_clean = FALSE;
	}
	if (desc->color.want_underline) {
		if (!desc->color.is_underline) {
			strcat(output, s_Underline);
			desc->color.is_underline = TRUE;
		}
		desc->color.want_underline = FALSE;
		desc->color.is_clean = FALSE;
	}
	
	return output;
}


/******************************************************************************
 Protocol global functions.
 *****************************************************************************/

protocol_t *ProtocolCreate(void) {
	int i; /* Loop counter */
	protocol_t *pProtocol;

	/* Called the first time we enter - make sure the table is correct */
	static bool_t bInit = false;
	if (!bInit) {
		bInit = true;
		for (i = eMSDP_NONE+1; i < eMSDP_MAX; ++i) {
			if (VariableNameTable[i].Variable != i) {
				ReportBug("MSDP: Variable table does not match the enums in the header.\n");
				break;
			}
		}
	}

	pProtocol = malloc(sizeof(protocol_t));
	pProtocol->WriteOOB = 0;
	for (i = eNEGOTIATED_TTYPE; i < eNEGOTIATED_MAX; ++i)
		pProtocol->Negotiated[i] = false;
	pProtocol->bIACMode = false;
	pProtocol->bNegotiated = false;
	pProtocol->bRenegotiate = false;
	pProtocol->bNeedMXPVersion = false;
	pProtocol->bBlockMXP = false;
	pProtocol->bTTYPE = false;
	pProtocol->bECHO = false;
	pProtocol->bNAWS = false;
	pProtocol->bCHARSET = false;
	pProtocol->bMSDP = false;
	pProtocol->bMSSP = false;
	pProtocol->bATCP = false;
	pProtocol->bMSP = false;
	pProtocol->bMXP = false;
	pProtocol->bMCCP = false;
	pProtocol->b256Support = eUNKNOWN;
	pProtocol->ScreenWidth = 0;
	pProtocol->ScreenHeight = 0;
	pProtocol->pMXPVersion = AllocString("Unknown");
	pProtocol->pLastTTYPE = NULL;
	pProtocol->pVariables = malloc(sizeof(MSDP_t*)*eMSDP_MAX);

	for (i = eMSDP_NONE+1; i < eMSDP_MAX; ++i) {
		pProtocol->pVariables[i] = malloc(sizeof(MSDP_t));
		pProtocol->pVariables[i]->bReport = false;
		pProtocol->pVariables[i]->bDirty = false;
		pProtocol->pVariables[i]->ValueInt = 0;
		pProtocol->pVariables[i]->pValueString = NULL;

		if (VariableNameTable[i].bString) {
			if (VariableNameTable[i].pDefault != NULL)
				pProtocol->pVariables[i]->pValueString = AllocString(VariableNameTable[i].pDefault);
			else if (VariableNameTable[i].bConfigurable)
				pProtocol->pVariables[i]->pValueString = AllocString("Unknown");
			else /* Use an empty string */
				pProtocol->pVariables[i]->pValueString = AllocString("");
		}
		else if (VariableNameTable[i].Default != 0) {
			pProtocol->pVariables[i]->ValueInt = VariableNameTable[i].Default;
		}
	}

	return pProtocol;
}

void ProtocolDestroy(protocol_t *apProtocol) {
	int i; /* Loop counter */

	for (i = eMSDP_NONE+1; i < eMSDP_MAX; ++i) {
		free(apProtocol->pVariables[i]->pValueString);
		free(apProtocol->pVariables[i]);
	}

	free(apProtocol->pVariables);
	free(apProtocol->pLastTTYPE);
	free(apProtocol->pMXPVersion);
	free(apProtocol);
}

void ProtocolInput(descriptor_t *apDescriptor, char *apData, int aSize, char *apOut, int maxSize) {
	static char CmdBuf[MAX_PROTOCOL_BUFFER+1];
	static char IacBuf[MAX_PROTOCOL_BUFFER+1];
	int CmdIndex = 0;
	int IacIndex = 0;
	int Index;

	protocol_t *pProtocol = apDescriptor ? apDescriptor->pProtocol : NULL;

	for (Index = 0; Index < aSize; ++Index) {
		/* If we'd overflow the buffer, we just ignore the input */
		if (CmdIndex >= MAX_PROTOCOL_BUFFER || IacIndex >= MAX_PROTOCOL_BUFFER) {
			ReportBug("ProtocolInput: Too much incoming data to store in the buffer.\n");
			return;
		}

		/* IAC IAC is treated as a single value of 255 */
		if (apData[Index] == (char)IAC && apData[Index+1] == (char)IAC) {
			if (pProtocol->bIACMode)
				IacBuf[IacIndex++] = (char)IAC;
			else /* In-band command */
				CmdBuf[CmdIndex++] = (char)IAC;
			Index++;
		}
		else if (pProtocol->bIACMode) {
			/* End subnegotiation. */
			if (apData[Index] == (char)IAC && apData[Index+1] == (char)SE) {
				Index++;
				pProtocol->bIACMode = false;
				IacBuf[IacIndex] = '\0';
				if (IacIndex >= 2)
					PerformSubnegotiation(apDescriptor, IacBuf[0], &IacBuf[1], IacIndex-1);
				IacIndex = 0;
			}
			else
				IacBuf[IacIndex++] = apData[Index];
		}
		else if (apData[Index] == (char)27 && apData[Index+1] == '[' && isdigit(apData[Index+2]) && apData[Index+3] == 'z') {
			char MXPBuffer [1024];
			char *pMXPTag = NULL;
			int i = 0; /* Loop counter */

			Index += 4; /* Skip to the start of the MXP sequence. */

			while (Index < aSize && apData[Index] != '>' && i < 1000) {
				MXPBuffer[i++] = apData[Index++];
			}
			MXPBuffer[i++] = '>';
			MXPBuffer[i] = '\0';

			if ((pMXPTag = GetMxpTag("CLIENT=", MXPBuffer)) != NULL) {
				/* Overwrite the previous client name - this is harder to fake */
				free(pProtocol->pVariables[eMSDP_CLIENT_ID]->pValueString);
				pProtocol->pVariables[eMSDP_CLIENT_ID]->pValueString = AllocString(pMXPTag);
			}

			if ((pMXPTag = GetMxpTag("VERSION=", MXPBuffer)) != NULL) {
				const char *pClientName = pProtocol->pVariables[eMSDP_CLIENT_ID]->pValueString;

				free(pProtocol->pVariables[eMSDP_CLIENT_VERSION]->pValueString);
				pProtocol->pVariables[eMSDP_CLIENT_VERSION]->pValueString = AllocString(pMXPTag);

				if (MatchString("MUSHCLIENT", pClientName)) {
					/* MUSHclient 4.02 and later supports 256 colours. */
					if (strcmp(pMXPTag, "4.02") >= 0) {
						pProtocol->pVariables[eMSDP_XTERM_256_COLORS]->ValueInt = 1;
						pProtocol->b256Support = eYES;
					}
					else { /* We know for sure that 256 colours are not supported */
						pProtocol->b256Support = eNO;
					}
				}
				else if (MatchString("CMUD", pClientName)) {
					/* CMUD 3.04 and later supports 256 colours. */
					if (strcmp(pMXPTag, "3.04") >= 0) {
						pProtocol->pVariables[eMSDP_XTERM_256_COLORS]->ValueInt = 1;
						pProtocol->b256Support = eYES;
					}
					else { /* We know for sure that 256 colours are not supported */
						pProtocol->b256Support = eNO;
					}
				}
				else if (MatchString("ATLANTIS", pClientName)) {
					/* Atlantis 0.9.9.0 supports XTerm 256 colours, but it doesn't 
					* yet have MXP.  However MXP is planned, so once it responds 
					* to a <VERSION> tag we'll know we can use 256 colours.
					*/
					pProtocol->pVariables[eMSDP_XTERM_256_COLORS]->ValueInt = 1;
					pProtocol->b256Support = eYES;
				}
			}

			if ((pMXPTag = GetMxpTag("MXP=", MXPBuffer)) != NULL) {
				free(pProtocol->pMXPVersion);
				pProtocol->pMXPVersion = AllocString(pMXPTag);
			}

			if (strcmp(pProtocol->pMXPVersion, "Unknown")) {
				Write(apDescriptor, "\n");
				sprintf(MXPBuffer, "MXP version %s detected and enabled.\r\n", pProtocol->pMXPVersion);
				InfoMessage(apDescriptor, MXPBuffer);
			}
			
			// prevent treating this like a blank "enter to continue"
			apDescriptor->no_nanny = TRUE;
		}
		else {	// In-band command
			if (apData[Index] == (char)IAC) {
				switch (apData[Index+1]) {
					case (char)SB: /* Begin subnegotiation. */
						Index++;
						pProtocol->bIACMode = true;
						break;

					case (char)DO: /* Handshake. */
					case (char)DONT:
					case (char)WILL:
					case (char)WONT: 
						PerformHandshake(apDescriptor, apData[Index+1], apData[Index+2]);
						Index += 2;
						break;

					case (char)IAC: /* Two IACs count as one. */
						CmdBuf[CmdIndex++] = (char)IAC;
						Index++;
						break;

					default: /* Skip it. */
						Index++;
						break;
				}
			}
			else {
				CmdBuf[CmdIndex++] = apData[Index];
			}
		}
	}

	/* Terminate the two buffers */
	IacBuf[IacIndex] = '\0';
	CmdBuf[CmdIndex] = '\0';

	/* Copy the input buffer back to the player. */
	strncat(apOut, CmdBuf, maxSize);
	apOut[maxSize-1] = '\0';
}

const char *ProtocolOutput(descriptor_t *apDescriptor, const char *apData, int *apLength) {
	static char Result[MAX_OUTPUT_BUFFER+1];
	const char Tab[] = "\t";
	const char MSP[] = "!!";
	const char MXPStart[] = "\033[1z<";
	const char MXPStop[] = ">\033[7z";
	const char LinkStart[] = "\033[1z<send>\033[7z";
	const char LinkStop[] = "\033[1z</send>\033[7z";	
	#ifdef COLOUR_CHAR
		const char ColourChar[] = { COLOUR_CHAR, '\0' };
		bool_t bColourOn = true;	// always default to true, or many parts of the code won't work correctly -- TODO change all instances of & in the code to \t so this can be a config
	#endif /* COLOUR_CHAR */
	bool_t bTerminate = false, bUseMXP = false, bUseMSP = false;
	int i = 0, j = 0; /* Index values */

	protocol_t *pProtocol = apDescriptor ? apDescriptor->pProtocol : NULL;
	if (pProtocol == NULL || apData == NULL)
		return apData;

	/* Strip !!SOUND() triggers if they support MSP or are using sound */
	if (pProtocol->bMSP || pProtocol->pVariables[eMSDP_SOUND]->ValueInt)
		bUseMSP = true;

	for (; i < MAX_OUTPUT_BUFFER && apData[j] != '\0' && !bTerminate && (*apLength <= 0 || j < *apLength); ++j) {
		if (apData[j] == '\t') {
			const char *pCopyFrom = NULL;

			switch (apData[++j]) {
				case '\t': /* Two tabs in a row will display an actual tab */
					pCopyFrom = Tab;
					break;
				case 'n':
				case '0':
					want_reduced_color_clean(apDescriptor);
					// ideally we'll change to this, but need a terminator for the underline
					// want_reduced_color_codes(apDescriptor, ColourRGB(apDescriptor, "F555", s_Clean), NULL);
					break;
				case 'u': // underline
					want_reduced_color_underline(apDescriptor);
					break;
				case 'r': /* dark red */
					want_reduced_color_codes(apDescriptor, s_DarkRed, NULL);
					break;
				case 'R': /* light red */
					want_reduced_color_codes(apDescriptor, s_BoldRed, NULL);
					break;
				case 'g': /* dark green */
					want_reduced_color_codes(apDescriptor, s_DarkGreen, NULL);
					break;
				case 'G': /* light green */
					want_reduced_color_codes(apDescriptor, s_BoldGreen, NULL);
					break;
				case 'y': /* dark yellow */
					want_reduced_color_codes(apDescriptor, s_DarkYellow, NULL);
					break;
				case 'Y': /* light yellow */
					want_reduced_color_codes(apDescriptor, s_BoldYellow, NULL);
					break;
				case 'b': /* dark blue */
					want_reduced_color_codes(apDescriptor, s_DarkBlue, NULL);
					break;
				case 'B': /* light blue */
					want_reduced_color_codes(apDescriptor, s_BoldBlue, NULL);
					break;
				case 'm': /* dark magenta */
					want_reduced_color_codes(apDescriptor, s_DarkMagenta, NULL);
					break;
				case 'M': /* light magenta */
					want_reduced_color_codes(apDescriptor, s_BoldMagenta, NULL);
					break;
				case 'c': /* dark cyan */
					want_reduced_color_codes(apDescriptor, s_DarkCyan, NULL);
					break;
				case 'C': /* light cyan */
					want_reduced_color_codes(apDescriptor, s_BoldCyan, NULL);
					break;
				case 'w': /* dark white */
					want_reduced_color_codes(apDescriptor, ColourRGB(apDescriptor, "F222", s_DarkWhite), NULL);
					break;
				case 'W': /* light white */
					want_reduced_color_codes(apDescriptor, ColourRGB(apDescriptor, "F555", s_BoldWhite), NULL);
					break;
				case 'a': /* dark azure */
					want_reduced_color_codes(apDescriptor, ColourRGB(apDescriptor, "F014", s_DarkBlue), NULL);
					break;
				case 'A': /* light azure */
					want_reduced_color_codes(apDescriptor, ColourRGB(apDescriptor, "F025", s_BoldBlue), NULL);
					break;
				case 'j': /* dark jade */
					want_reduced_color_codes(apDescriptor, ColourRGB(apDescriptor, "F031", s_DarkGreen), NULL);
					break;
				case 'J': /* light jade */
					want_reduced_color_codes(apDescriptor, ColourRGB(apDescriptor, "F052", s_BoldGreen), NULL);
					break;
				case 'l': /* dark lime */
					want_reduced_color_codes(apDescriptor, ColourRGB(apDescriptor, "F140", s_BoldGreen), NULL);
					break;
				case 'L': /* light lime */
					want_reduced_color_codes(apDescriptor, ColourRGB(apDescriptor, "F250", s_BoldGreen), NULL);
					break;
				case 'o': /* dark orange */
					want_reduced_color_codes(apDescriptor, ColourRGB(apDescriptor, "F520", s_DarkYellow), NULL);
					break;
				case 'O': /* light orange */
					want_reduced_color_codes(apDescriptor, ColourRGB(apDescriptor, "F530", s_DarkYellow), NULL);
					break;
				case 'p': /* dark pink */
					want_reduced_color_codes(apDescriptor, ColourRGB(apDescriptor, "F301", s_DarkMagenta), NULL);
					break;
				case 'P': /* light pink */
					want_reduced_color_codes(apDescriptor, ColourRGB(apDescriptor, "F502", s_BoldMagenta), NULL);
					break;
				case 't': /* dark tan */
					want_reduced_color_codes(apDescriptor, ColourRGB(apDescriptor, "F210", s_DarkYellow), NULL);
					break;
				case 'T': /* light tan */
					want_reduced_color_codes(apDescriptor, ColourRGB(apDescriptor, "F321", s_DarkYellow), NULL);
					break;
				case 'v': /* dark violet */
					want_reduced_color_codes(apDescriptor, ColourRGB(apDescriptor, "F104", s_DarkBlue), NULL);
					break;
				case 'V': /* light violet */
					want_reduced_color_codes(apDescriptor, ColourRGB(apDescriptor, "F205", s_DarkMagenta), NULL);
					break;
				case '(': /* MXP link */
					if (!pProtocol->bBlockMXP && pProtocol->pVariables[eMSDP_MXP]->ValueInt)
						pCopyFrom = LinkStart;
					break;
				case ')': /* MXP link */
					if (!pProtocol->bBlockMXP && pProtocol->pVariables[eMSDP_MXP]->ValueInt)
						pCopyFrom = LinkStop;
					pProtocol->bBlockMXP = false;
					break;
				case '<':
					if (!pProtocol->bBlockMXP && pProtocol->pVariables[eMSDP_MXP]->ValueInt) {
						pCopyFrom = MXPStart;
						bUseMXP = true;
					}
					else {	// No MXP support, so just strip it out
						while (apData[j] != '\0' && apData[j] != '>') {
							++j;
						}
					}
					pProtocol->bBlockMXP = false;
					break;
				case '[':
					if (tolower(apData[++j]) == 'u') {
						char Buffer[8] = {'\0'}, BugString[256];
						int Index = 0;
						int Number = 0;
						bool_t bDone = false, bValid = true;

						while (isdigit(apData[++j])) {
							Number *= 10;
							Number += (apData[j])-'0';
						}

						if (apData[j] == '/')
							++j;

						while (apData[j] != '\0' && !bDone) {
							if (apData[j] == ']')
								bDone = true;
							else if (Index < 7)
								Buffer[Index++] = apData[j++];
							else {	// It's too long, so ignore the rest and note the problem
								j++;
								bValid = false;
							}
						}

						if (!bDone) {
							sprintf(BugString, "BUG: Unicode substitute '%s' wasn't terminated with ']'.\n", Buffer);
							ReportBug(BugString);
						}
						else if (!bValid) {
							sprintf(BugString, "BUG: Unicode substitute '%s' truncated.  Missing ']'?\n", Buffer);
							ReportBug(BugString);
						}
						else if (pProtocol->pVariables[eMSDP_UTF_8]->ValueInt) {
							pCopyFrom = UnicodeGet(Number);
						}
						else {	// Display the substitute string
							pCopyFrom = Buffer;
						}

						/* Terminate if we've reached the end of the string */
						bTerminate = !bDone;
					}
					else if (tolower(apData[j]) == 'f' || tolower(apData[j]) == 'b') {
						char Buffer[8] = {'\0'}, BugString[256];
						int Index = 0;
						bool_t bDone = false, bValid = true;

						/* Copy the 'f' (foreground) or 'b' (background) */
						Buffer[Index++] = apData[j++];

						while (apData[j] != '\0' && !bDone && bValid) {
							if (apData[j] == ']')
								bDone = true;
							else if (Index < 4)
								Buffer[Index++] = apData[j++];
							else /* It's too long, so drop out - the colour code may still be valid */
								bValid = false;
						}

						if (!bDone || !bValid) {
							sprintf(BugString, "BUG: RGB %sground colour '%s' wasn't terminated with ']'.\n", (tolower(Buffer[0]) == 'f') ? "fore" : "back", &Buffer[1]);
							ReportBug(BugString);
						}
						else if (!IsValidColour(Buffer)) {
							sprintf(BugString, "BUG: RGB %sground colour '%s' invalid (each digit must be in the range 0-5).\n", (tolower(Buffer[0]) == 'f') ? "fore" : "back", &Buffer[1]);
							ReportBug(BugString);
						}
						else {	// Success
							if (tolower(Buffer[0]) == 'b') {	// background
								want_reduced_color_codes(apDescriptor, NULL, ColourRGB(apDescriptor, Buffer, NULL));
							}
							else {	// foreground
								want_reduced_color_codes(apDescriptor, ColourRGB(apDescriptor, Buffer, NULL), NULL);
							}
						}
					}
					else if (tolower(apData[j]) == 'x') {
						char Buffer[8] = {'\0'}, BugString[256];
						int Index = 0;
						bool_t bDone = false, bValid = true;

						++j; /* Skip the 'x' */

						while (apData[j] != '\0' && !bDone) {
							if (apData[j] == ']')
								bDone = true;
							else if (Index < 7)
								Buffer[Index++] = apData[j++];
							else {	// It's too long, so ignore the rest and note the problem
								j++;
								bValid = false;
							}
						}

						if (!bDone) {
							sprintf(BugString, "BUG: Required MXP version '%s' wasn't terminated with ']'.\n", Buffer);
							ReportBug(BugString);
						}
						else if (!bValid) {
							sprintf(BugString, "BUG: Required MXP version '%s' too long.  Missing ']'?\n", Buffer);
							ReportBug(BugString);
						}
						else if (!strcmp(pProtocol->pMXPVersion, "Unknown") || strcmp(pProtocol->pMXPVersion, Buffer) < 0) {
							/* Their version of MXP isn't high enough */
							pProtocol->bBlockMXP = true;
						}
						else {	// MXP is sufficient for this tag
							pProtocol->bBlockMXP = false;
						}

						/* Terminate if we've reached the end of the string */
						bTerminate = !bDone;
					}
					break;
				case '!': /* Used for in-band MSP sound triggers */
					pCopyFrom = MSP;
					break;

				#ifdef COLOUR_CHAR
				case '+':
					bColourOn = true;
					break;
				case '-':
					bColourOn = false;
					break;
				case COLOUR_CHAR: {	// allows \t& to guarantee a printed & whether COLOUR_CHAR is on or off, unlike && which displays both & if COLOUR_CHAR is undefined
					pCopyFrom = ColourChar;
					break;
				}
				#endif /* COLOUR_CHAR */

				case '\0':
					bTerminate = true;
					break;
				default:
					break;
			}

			// Copy the output, if any
			if (pCopyFrom != NULL) {
				// display and flush color codes
				const char *temp = flush_reduced_color_codes(apDescriptor);
				while (*temp != '\0' && i < MAX_OUTPUT_BUFFER) {
					Result[i++] = *temp++;
				}
				
				// requested output
				while (*pCopyFrom != '\0' && i < MAX_OUTPUT_BUFFER) {
					Result[i++] = *pCopyFrom++;
				}
			}
		}

		#ifdef COLOUR_CHAR
		else if (bColourOn && apData[j] == COLOUR_CHAR) {
			const char *pCopyFrom = NULL;
			char storage[8];

			switch (apData[++j]) {
				case COLOUR_CHAR: /* Two in a row display the actual character */
					pCopyFrom = ColourChar;
					break;
				case '\t': {	// &\t can be triggered by player input and eat a color code, so:
					pCopyFrom = ColourChar;	// display the &
					--j;	// skip back one so the \t proceeds as normal
					break;
				}
				case 'n':
				case '0':
					want_reduced_color_clean(apDescriptor);
					// ideally we'll change to this, but need a terminator for the underline
					// want_reduced_color_codes(apDescriptor, ColourRGB(apDescriptor, "F555", s_Clean), NULL);
					break;
				case 'u': // underline
					want_reduced_color_underline(apDescriptor);
					break;
				case 'r': /* dark red */
					want_reduced_color_codes(apDescriptor, s_DarkRed, NULL);
					break;
				case 'R': /* light red */
					want_reduced_color_codes(apDescriptor, s_BoldRed, NULL);
					break;
				case 'g': /* dark green */
					want_reduced_color_codes(apDescriptor, s_DarkGreen, NULL);
					break;
				case 'G': /* light green */
					want_reduced_color_codes(apDescriptor, s_BoldGreen, NULL);
					break;
				case 'y': /* dark yellow */
					want_reduced_color_codes(apDescriptor, s_DarkYellow, NULL);
					break;
				case 'Y': /* light yellow */
					want_reduced_color_codes(apDescriptor, s_BoldYellow, NULL);
					break;
				case 'b': /* dark blue */
					want_reduced_color_codes(apDescriptor, s_DarkBlue, NULL);
					break;
				case 'B': /* light blue */
					want_reduced_color_codes(apDescriptor, s_BoldBlue, NULL);
					break;
				case 'm': /* dark magenta */
					want_reduced_color_codes(apDescriptor, s_DarkMagenta, NULL);
					break;
				case 'M': /* light magenta */
					want_reduced_color_codes(apDescriptor, s_BoldMagenta, NULL);
					break;
				case 'c': /* dark cyan */
					want_reduced_color_codes(apDescriptor, s_DarkCyan, NULL);
					break;
				case 'C': /* light cyan */
					want_reduced_color_codes(apDescriptor, s_BoldCyan, NULL);
					break;
				case 'w': /* dark white */
					want_reduced_color_codes(apDescriptor, ColourRGB(apDescriptor, "F222", s_DarkWhite), NULL);
					break;
				case 'W': /* light white */
					want_reduced_color_codes(apDescriptor, ColourRGB(apDescriptor, "F555", s_BoldWhite), NULL);
					break;
				case 'a': /* dark azure */
					want_reduced_color_codes(apDescriptor, ColourRGB(apDescriptor, "F014", s_DarkBlue), NULL);
					break;
				case 'A': /* light azure */
					want_reduced_color_codes(apDescriptor, ColourRGB(apDescriptor, "F025", s_BoldBlue), NULL);
					break;
				case 'j': /* dark jade */
					want_reduced_color_codes(apDescriptor, ColourRGB(apDescriptor, "F031", s_DarkGreen), NULL);
					break;
				case 'J': /* light jade */
					want_reduced_color_codes(apDescriptor, ColourRGB(apDescriptor, "F052", s_BoldGreen), NULL);
					break;
				case 'l': /* dark lime */
					want_reduced_color_codes(apDescriptor, ColourRGB(apDescriptor, "F140", s_BoldGreen), NULL);
					break;
				case 'L': /* light lime */
					want_reduced_color_codes(apDescriptor, ColourRGB(apDescriptor, "F250", s_BoldGreen), NULL);
					break;
				case 'o': /* dark orange */
					want_reduced_color_codes(apDescriptor, ColourRGB(apDescriptor, "F520", s_DarkYellow), NULL);
					break;
				case 'O': /* light orange */
					want_reduced_color_codes(apDescriptor, ColourRGB(apDescriptor, "F530", s_DarkYellow), NULL);
					break;
				case 'p': /* dark pink */
					want_reduced_color_codes(apDescriptor, ColourRGB(apDescriptor, "F301", s_DarkMagenta), NULL);
					break;
				case 'P': /* light pink */
					want_reduced_color_codes(apDescriptor, ColourRGB(apDescriptor, "F502", s_BoldMagenta), NULL);
					break;
				case 't': /* dark tan */
					want_reduced_color_codes(apDescriptor, ColourRGB(apDescriptor, "F210", s_DarkYellow), NULL);
					break;
				case 'T': /* light tan */
					want_reduced_color_codes(apDescriptor, ColourRGB(apDescriptor, "F321", s_DarkYellow), NULL);
					break;
				case 'v': /* dark violet */
					want_reduced_color_codes(apDescriptor, ColourRGB(apDescriptor, "F104", s_DarkBlue), NULL);
					break;
				case 'V': /* light violet */
					want_reduced_color_codes(apDescriptor, ColourRGB(apDescriptor, "F205", s_DarkMagenta), NULL);
					break;
				case '\0':
					bTerminate = true;
					break;

				case '[': {	// extended codes
					if (config_get_bool("allow_extended_color_codes")) {
						if (tolower(apData[++j]) == 'f' || tolower(apData[j]) == 'b') {
							char Buffer[8] = {'\0'};
							int Index = 0;
							bool_t bDone = false, bValid = true;

							/* Copy the 'f' (foreground) or 'b' (background) */
							Buffer[Index++] = apData[j++];

							while (apData[j] != '\0' && !bDone && bValid) {
								if (apData[j] == ']')
									bDone = true;
								else if (Index < 4)
									Buffer[Index++] = apData[j++];
								else /* It's too long, so drop out - the colour code may still be valid */
									bValid = false;
							}

							if (bDone && bValid && IsValidColour(Buffer)) {
								if (tolower(Buffer[0]) == 'b') {	// background
									want_reduced_color_codes(apDescriptor, NULL, ColourRGB(apDescriptor, Buffer, NULL));
								}
								else {	// foreground
									want_reduced_color_codes(apDescriptor, ColourRGB(apDescriptor, Buffer, NULL), NULL);
								}
							}
						}
					}
					else {	// not allowed
						sprintf(storage, "%c%c", COLOUR_CHAR, apData[j]);
						pCopyFrom = storage;
					}
					break;
				}

				default:
					#ifdef DISPLAY_INVALID_COLOUR_CODES
						sprintf(storage, "%c%c", COLOUR_CHAR, apData[j]);
						pCopyFrom = storage;
					#endif /* DISPLAY_INVALID_COLOUR_CODES */
					break;
				}

			// Copy the output, if any
			if (pCopyFrom != NULL) {
				// display and flush color codes
				const char *temp = flush_reduced_color_codes(apDescriptor);
				while (*temp != '\0' && i < MAX_OUTPUT_BUFFER) {
					Result[i++] = *temp++;
				}
				
				// show requested output
				while (*pCopyFrom != '\0' && i < MAX_OUTPUT_BUFFER) {
					Result[i++] = *pCopyFrom++;
				}
			}
		}
		#endif /* COLOUR_CHAR */

		else if (bUseMXP && apData[j] == '>') {
			const char *pCopyFrom = MXPStop;
			while (*pCopyFrom != '\0' && i < MAX_OUTPUT_BUFFER)
				Result[i++] = *pCopyFrom++;
			bUseMXP = false;
		}
		else if (bUseMSP && j > 0 && apData[j-1] == '!' && apData[j] == '!' && PrefixString("SOUND(", &apData[j+1])) {
			/* Avoid accidental triggering of old-style MSP triggers */
			Result[i++] = '?';
		}
		else {	// Just copy the character normally
			// display and flush color codes
			const char *temp = flush_reduced_color_codes(apDescriptor);
			while (*temp != '\0' && i < MAX_OUTPUT_BUFFER-1) {
				Result[i++] = *temp++;
			}
			
			// copy the character
			Result[i++] = apData[j];
		}
	}

	// truncate and overflow
	if (i >= MAX_OUTPUT_BUFFER) {
		const char *overflow = "**OVERFLOW**\r\n";
		strcpy(Result + (MAX_OUTPUT_BUFFER - (strlen(overflow) + 1)), overflow);
		i = MAX_OUTPUT_BUFFER - 1;
		ReportBug("ProtocolOutput: Too much outgoing data to store in the buffer.\n");
	}

	/* Terminate the string */
	Result[i] = '\0';

	/* Store the length */
	if (apLength)
		*apLength = i;

	/* Return the string */
	return Result;
}

/* Some clients (such as GMud) don't properly handle negotiation, and simply 
* display every printable character to the screen.  However TTYPE isn't a 
* printable character, so we negotiate for it first, and only negotiate for 
* other protocols if the client responds with IAC WILL TTYPE or IAC WONT 
* TTYPE.  Thanks go to Donky on MudBytes for the suggestion.
*/
void ProtocolNegotiate(descriptor_t *apDescriptor) {
	ConfirmNegotiation(apDescriptor, eNEGOTIATED_TTYPE, true, true);
}

/* Tells the client to switch echo on or off. */
void ProtocolNoEcho(descriptor_t *apDescriptor, bool_t abOn) {
	ConfirmNegotiation(apDescriptor, eNEGOTIATED_ECHO, abOn, true);
}


/******************************************************************************
 Copyover save/load functions.
 *****************************************************************************/

const char *CopyoverGet(descriptor_t *apDescriptor) {
	static char Buffer[64];
	char *pBuffer = Buffer;
	protocol_t *pProtocol = apDescriptor ? apDescriptor->pProtocol : NULL;

	if (pProtocol != NULL) {
		sprintf(Buffer, "%d/%d", pProtocol->ScreenWidth, pProtocol->ScreenHeight);

		/* Skip to the end */
		while (*pBuffer != '\0')
			++pBuffer;

		if (pProtocol->bTTYPE)
			*pBuffer++ = 'T';
		if (pProtocol->bNAWS)
			*pBuffer++ = 'N';
		if (pProtocol->bMSDP)
			*pBuffer++ = 'M';
		if (pProtocol->bATCP)
			*pBuffer++ = 'A';
		if (pProtocol->bMSP)
			*pBuffer++ = 'S';
		if (pProtocol->pVariables[eMSDP_MXP]->ValueInt)
			*pBuffer++ = 'X';
		if (pProtocol->bMCCP) {
			*pBuffer++ = 'c';
			CompressEnd(apDescriptor);
		}
		if (pProtocol->pVariables[eMSDP_XTERM_256_COLORS]->ValueInt)
			*pBuffer++ = 'C';
		if (pProtocol->bCHARSET)
			*pBuffer++ = 'H';
		if (pProtocol->pVariables[eMSDP_UTF_8]->ValueInt)
			*pBuffer++ = 'U';
	}

	/* Terminate the string */
	*pBuffer = '\0';

	return Buffer;
}

void CopyoverSet(descriptor_t *apDescriptor, const char *apData) {
	protocol_t *pProtocol = apDescriptor ? apDescriptor->pProtocol : NULL;

	if (pProtocol != NULL && apData != NULL) {
		int Width = 0, Height = 0;
		bool_t bDoneWidth = false;
		int i; /* Loop counter */

		for (i = 0; apData[i] != '\0'; ++i) {
			switch (apData[i]) {
				case 'T':
					pProtocol->bTTYPE = true;
					break;
				case 'N':
					pProtocol->bNAWS = true;
					break;
				case 'M':
					pProtocol->bMSDP = true;
					break;
				case 'A':
					pProtocol->bATCP = true;
					break;
				case 'S':
					pProtocol->bMSP = true;
					break;
				case 'X':
					pProtocol->bMXP = true;
					pProtocol->pVariables[eMSDP_MXP]->ValueInt = 1;
					break;
				case 'c':
					pProtocol->bMCCP = true;
					CompressStart(apDescriptor);
					break;
				case 'C':
					pProtocol->pVariables[eMSDP_XTERM_256_COLORS]->ValueInt = 1;
					break;
				case 'H':
					pProtocol->bCHARSET = true;
					break;
				case 'U':
					pProtocol->pVariables[eMSDP_UTF_8]->ValueInt = 1;
					break;
				default:
					if (apData[i] == '/')
						bDoneWidth = true;
					else if (isdigit(apData[i])) {
						if (bDoneWidth) {
							Height *= 10;
							Height += (apData[i] - '0');
						}
						else {	// We're still calculating height
							Width *= 10;
							Width += (apData[i] - '0');
						}
					}
					break;
			}
		}

		/* Restore the width and height */
		pProtocol->ScreenWidth = Width;
		pProtocol->ScreenHeight = Height;

		/* If we're using MSDP or ATCP, we need to renegotiate it so that the 
		* client can resend the list of variables it wants us to REPORT.
		*
		* Note that we only use ATCP if MSDP is not supported.
		*/
		if (pProtocol->bMSDP) {
			ConfirmNegotiation(apDescriptor, eNEGOTIATED_MSDP, true, true);
		}
		else if (pProtocol->bATCP) {
			ConfirmNegotiation(apDescriptor, eNEGOTIATED_ATCP, true, true);
		}

		/* Ask the client to send its MXP version again */
		if (pProtocol->bMXP) {
			MXPSendTag(apDescriptor, "<VERSION>");
		}
	}
}


/******************************************************************************
MSDP global functions.
******************************************************************************/

void MSDPUpdate(descriptor_t *apDescriptor) {
	int i; /* Loop counter */

	protocol_t *pProtocol = apDescriptor ? apDescriptor->pProtocol : NULL;

	for (i = eMSDP_NONE+1; i < eMSDP_MAX; ++i) {
		if (pProtocol->pVariables[i]->bReport) {
			if (pProtocol->pVariables[i]->bDirty) {
				MSDPSend(apDescriptor, (variable_t)i);
				pProtocol->pVariables[i]->bDirty = false;
			}
		}
	}
}

void MSDPFlush(descriptor_t *apDescriptor, variable_t aMSDP) {
	if (aMSDP > eMSDP_NONE && aMSDP < eMSDP_MAX) {
		protocol_t *pProtocol = apDescriptor ? apDescriptor->pProtocol : NULL;

		if (pProtocol->pVariables[aMSDP]->bReport) {
			if (pProtocol->pVariables[aMSDP]->bDirty) {
				MSDPSend(apDescriptor, aMSDP);
				pProtocol->pVariables[aMSDP]->bDirty = false;
			}
		}
	}
}

void MSDPSend(descriptor_t *apDescriptor, variable_t aMSDP) {
	char MSDPBuffer[MAX_VARIABLE_LENGTH+1] = { '\0' };

	if (aMSDP > eMSDP_NONE && aMSDP < eMSDP_MAX) {
		protocol_t *pProtocol = apDescriptor ? apDescriptor->pProtocol : NULL;

		if (VariableNameTable[aMSDP].bString) {
			/* Should really be replaced with a dynamic buffer */
			int RequiredBuffer = strlen(VariableNameTable[aMSDP].pName) + strlen(pProtocol->pVariables[aMSDP]->pValueString) + 12;

			if (RequiredBuffer >= MAX_VARIABLE_LENGTH) {
				sprintf(MSDPBuffer, "MSDPSend: %s %d bytes (exceeds MAX_VARIABLE_LENGTH of %d).\n", VariableNameTable[aMSDP].pName, RequiredBuffer, MAX_VARIABLE_LENGTH);
				ReportBug(MSDPBuffer);
				MSDPBuffer[0] = '\0';
			}
			else if (pProtocol->bMSDP) {
				sprintf(MSDPBuffer, "%c%c%c%c%s%c%s%c%c", IAC, SB, TELOPT_MSDP, MSDP_VAR, VariableNameTable[aMSDP].pName, MSDP_VAL, pProtocol->pVariables[aMSDP]->pValueString, IAC, SE);
			}
			else if (pProtocol->bATCP) {
				sprintf(MSDPBuffer, "%c%c%cMSDP.%s %s%c%c", IAC, SB, TELOPT_ATCP, VariableNameTable[aMSDP].pName, pProtocol->pVariables[aMSDP]->pValueString, IAC, SE);
			}
		}
		else {	// It's an integer, not a string
			if (pProtocol->bMSDP) {
				sprintf(MSDPBuffer, "%c%c%c%c%s%c%d%c%c", IAC, SB, TELOPT_MSDP, MSDP_VAR, VariableNameTable[aMSDP].pName, MSDP_VAL, pProtocol->pVariables[aMSDP]->ValueInt, IAC, SE);
			}
			else if (pProtocol->bATCP) {
				sprintf(MSDPBuffer, "%c%c%cMSDP.%s %d%c%c", IAC, SB, TELOPT_ATCP, VariableNameTable[aMSDP].pName, pProtocol->pVariables[aMSDP]->ValueInt, IAC, SE);
			}
		}

		/* Just in case someone calls this function without checking MSDP/ATCP */
		if (MSDPBuffer[0] != '\0') {
			Write(apDescriptor, MSDPBuffer);
		}
	}
}

void MSDPSendPair(descriptor_t *apDescriptor, const char *apVariable, const char *apValue) {
	char MSDPBuffer[MAX_VARIABLE_LENGTH+1] = { '\0' };

	if (apVariable != NULL && apValue != NULL) {
		protocol_t *pProtocol = apDescriptor ? apDescriptor->pProtocol : NULL;

		/* Should really be replaced with a dynamic buffer */
		int RequiredBuffer = strlen(apVariable) + strlen(apValue) + 12;

		if (RequiredBuffer >= MAX_VARIABLE_LENGTH) {
			if (RequiredBuffer - strlen(apValue) < MAX_VARIABLE_LENGTH) {
				sprintf(MSDPBuffer, "MSDPSendPair: %s %d bytes (exceeds MAX_VARIABLE_LENGTH of %d).\n", apVariable, RequiredBuffer, MAX_VARIABLE_LENGTH);
			}
			else {	// The variable name itself is too long
				sprintf(MSDPBuffer, "MSDPSendPair: Variable name has a length of %d bytes (exceeds MAX_VARIABLE_LENGTH of %d).\n", RequiredBuffer, MAX_VARIABLE_LENGTH);
			}

			ReportBug(MSDPBuffer);
			MSDPBuffer[0] = '\0';
		}
		else if (pProtocol->bMSDP) {
			sprintf(MSDPBuffer, "%c%c%c%c%s%c%s%c%c", IAC, SB, TELOPT_MSDP, MSDP_VAR, apVariable, MSDP_VAL, apValue, IAC, SE);
		}
		else if (pProtocol->bATCP) {
			sprintf(MSDPBuffer, "%c%c%cMSDP.%s %s%c%c", IAC, SB, TELOPT_ATCP, apVariable, apValue, IAC, SE);
		}

		/* Just in case someone calls this function without checking MSDP/ATCP */
		if (MSDPBuffer[0] != '\0') {
			Write(apDescriptor, MSDPBuffer);
		}
	}
}

void MSDPSendList(descriptor_t *apDescriptor, const char *apVariable, const char *apValue) {
	char MSDPBuffer[MAX_VARIABLE_LENGTH+1] = { '\0' };

	if (apVariable != NULL && apValue != NULL) {
		protocol_t *pProtocol = apDescriptor ? apDescriptor->pProtocol : NULL;

		/* Should really be replaced with a dynamic buffer */
		int RequiredBuffer = strlen(apVariable) + strlen(apValue) + 12;

		if (RequiredBuffer >= MAX_VARIABLE_LENGTH) {
			if (RequiredBuffer - strlen(apValue) < MAX_VARIABLE_LENGTH) {
				sprintf(MSDPBuffer, "MSDPSendList: %s %d bytes (exceeds MAX_VARIABLE_LENGTH of %d).\n", apVariable, RequiredBuffer, MAX_VARIABLE_LENGTH);
			}
			else {	// The variable name itself is too long
				sprintf(MSDPBuffer, "MSDPSendList: Variable name has a length of %d bytes (exceeds MAX_VARIABLE_LENGTH of %d).\n", RequiredBuffer, MAX_VARIABLE_LENGTH);
			}

			ReportBug(MSDPBuffer);
			MSDPBuffer[0] = '\0';
		}
		else if (pProtocol->bMSDP) {
			int i; /* Loop counter */
			sprintf(MSDPBuffer, "%c%c%c%c%s%c%c%c%s%c%c%c", IAC, SB, TELOPT_MSDP, MSDP_VAR, apVariable, MSDP_VAL, MSDP_ARRAY_OPEN, MSDP_VAL, apValue, MSDP_ARRAY_CLOSE, IAC, SE);

			/* Convert the spaces to MSDP_VAL */
			for (i = 0; MSDPBuffer[i] != '\0'; ++i) {
				if (MSDPBuffer[i] == ' ') {
					MSDPBuffer[i] = MSDP_VAL;
				}
			}
		}
		else if (pProtocol->bATCP) {
			sprintf(MSDPBuffer, "%c%c%cMSDP.%s %s%c%c", IAC, SB, TELOPT_ATCP, apVariable, apValue, IAC, SE);
		}

		/* Just in case someone calls this function without checking MSDP/ATCP */
		if (MSDPBuffer[0] != '\0') {
			Write(apDescriptor, MSDPBuffer);
		}
	}
}

void MSDPSetNumber(descriptor_t *apDescriptor, variable_t aMSDP, int aValue) {
	protocol_t *pProtocol = apDescriptor ? apDescriptor->pProtocol : NULL;

	if (pProtocol != NULL && aMSDP > eMSDP_NONE && aMSDP < eMSDP_MAX) {
		if (!VariableNameTable[aMSDP].bString) {
			if (pProtocol->pVariables[aMSDP]->ValueInt != aValue) {
				pProtocol->pVariables[aMSDP]->ValueInt = aValue;
				pProtocol->pVariables[aMSDP]->bDirty = true;
			}
		}
	}
}

void MSDPSetString(descriptor_t *apDescriptor, variable_t aMSDP, const char *apValue) {
	protocol_t *pProtocol = apDescriptor ? apDescriptor->pProtocol : NULL;

	if (pProtocol != NULL && apValue != NULL) {
		if (VariableNameTable[aMSDP].bString) {
			if (strcmp(pProtocol->pVariables[aMSDP]->pValueString, apValue)) {
				free(pProtocol->pVariables[aMSDP]->pValueString);
				pProtocol->pVariables[aMSDP]->pValueString = AllocString(apValue);
				pProtocol->pVariables[aMSDP]->bDirty = true;
			}
		}
	}
}

void MSDPSetTable(descriptor_t *apDescriptor, variable_t aMSDP, const char *apValue) {
	protocol_t *pProtocol = apDescriptor ? apDescriptor->pProtocol : NULL;

	if (pProtocol != NULL && apValue != NULL) {
		if (*apValue == '\0') {
			/* It's easier to call MSDPSetString if the value is empty */
			MSDPSetString(apDescriptor, aMSDP, apValue);
		}
		else if (VariableNameTable[aMSDP].bString) {
			const char MsdpTableStart[] = { (char)MSDP_TABLE_OPEN, '\0' };
			const char MsdpTableStop[] = { (char)MSDP_TABLE_CLOSE, '\0' };

			char *pTable = malloc(strlen(apValue) + 3); /* 3: START, STOP, NUL */

			strcpy(pTable, MsdpTableStart);
			strcat(pTable, apValue);
			strcat(pTable, MsdpTableStop);

			if (strcmp(pProtocol->pVariables[aMSDP]->pValueString, pTable)) {
				free(pProtocol->pVariables[aMSDP]->pValueString);
				pProtocol->pVariables[aMSDP]->pValueString = pTable;
				pProtocol->pVariables[aMSDP]->bDirty = true;
			}
			else {	// Just discard the table, we've already got one
				free(pTable);
			}
		}
	}
}

void MSDPSetArray(descriptor_t *apDescriptor, variable_t aMSDP, const char *apValue) {
	protocol_t *pProtocol = apDescriptor ? apDescriptor->pProtocol : NULL;

	if (pProtocol != NULL && apValue != NULL) {
		if (*apValue == '\0') {
			/* It's easier to call MSDPSetString if the value is empty */
			MSDPSetString(apDescriptor, aMSDP, apValue);
		}
		else if (VariableNameTable[aMSDP].bString) {
			const char MsdpArrayStart[] = { (char)MSDP_ARRAY_OPEN, '\0' };
			const char MsdpArrayStop[] = { (char)MSDP_ARRAY_CLOSE, '\0' };

			char *pArray = malloc(strlen(apValue) + 3); /* 3: START, STOP, NUL */

			strcpy(pArray, MsdpArrayStart);
			strcat(pArray, apValue);
			strcat(pArray, MsdpArrayStop);

			if (strcmp(pProtocol->pVariables[aMSDP]->pValueString, pArray)) {
				free(pProtocol->pVariables[aMSDP]->pValueString);
				pProtocol->pVariables[aMSDP]->pValueString = pArray;
				pProtocol->pVariables[aMSDP]->bDirty = true;
			}
			else {	// Just discard the array, we've already got one
				free(pArray);
			}
		}
	}
}


/******************************************************************************
 MSSP global functions.
 *****************************************************************************/

void MSSPSetPlayers(int aPlayers) {
	s_Players = aPlayers;

	if (s_Uptime == 0) {
		s_Uptime = time(0);
	}
}


/******************************************************************************
 MXP global functions.
 *****************************************************************************/

const char *MXPCreateTag(descriptor_t *apDescriptor, const char *apTag) {
	protocol_t *pProtocol = apDescriptor ? apDescriptor->pProtocol : NULL;

	if (pProtocol != NULL && pProtocol->pVariables[eMSDP_MXP]->ValueInt && strlen(apTag) < 1000) {
		static char MXPBuffer [1024];
		sprintf(MXPBuffer, "\033[1z%s\033[7z", apTag);
		return MXPBuffer;
	}
	else {	// Leave the tag as-is, don't try to MXPify it
		return apTag;
	}
}

void MXPSendTag(descriptor_t *apDescriptor, const char *apTag) {
	protocol_t *pProtocol = apDescriptor ? apDescriptor->pProtocol : NULL;

	if (pProtocol != NULL && apTag != NULL && strlen(apTag) < 1000) {
		if (pProtocol->pVariables[eMSDP_MXP]->ValueInt) {
			char MXPBuffer [1024];
			sprintf(MXPBuffer, "\033[1z%s\033[7z\r\n", apTag);
			Write(apDescriptor, MXPBuffer);
		}
		else if (pProtocol->bRenegotiate) {
			/* Tijer pointed out that when MUSHclient autoconnects, it fails 
			* to complete the negotiation.  This workaround will attempt to 
			* renegotiate after the character has connected.
			*/

			int i; /* Renegotiate everything except TTYPE */
			for (i = eNEGOTIATED_TTYPE+1; i < eNEGOTIATED_MAX; ++i) {
				pProtocol->Negotiated[i] = false;
				ConfirmNegotiation(apDescriptor, (negotiated_t)i, true, true);
			}

			pProtocol->bRenegotiate = false;
			pProtocol->bNeedMXPVersion = true;
			Negotiate(apDescriptor);
		}
	}
}


/******************************************************************************
 Sound global functions.
 *****************************************************************************/

void SoundSend(descriptor_t *apDescriptor, const char *apTrigger) {
	const int MaxTriggerLength = 128; /* Used for the buffer size */

	if (apDescriptor != NULL && apTrigger != NULL) {
		protocol_t *pProtocol = apDescriptor ? apDescriptor->pProtocol : NULL;

		if (pProtocol != NULL && pProtocol->pVariables[eMSDP_SOUND]->ValueInt) {
			if (pProtocol->bMSDP || pProtocol->bATCP) {
				/* Send the sound trigger through MSDP or ATCP */
				MSDPSendPair(apDescriptor, "PLAY_SOUND", apTrigger);
			}
			else if (strlen(apTrigger) <= MaxTriggerLength) {
				/* Use an old MSP-style trigger */
				char *pBuffer = alloca(MaxTriggerLength+10);
				sprintf(pBuffer, "\t!SOUND(%s)", apTrigger);
				Write(apDescriptor, pBuffer);
			}
		}
	}
}


/******************************************************************************
 Colour global functions.
 *****************************************************************************/

const char *ColourRGB(descriptor_t *apDescriptor, const char *apRGB, const char *ansiBackup) {
	protocol_t *pProtocol = apDescriptor ? apDescriptor->pProtocol : NULL;
	
	if (ansiBackup && !pProtocol->pVariables[eMSDP_XTERM_256_COLORS]->ValueInt) {
		return ansiBackup;
	}

	if (pProtocol && pProtocol->pVariables[eMSDP_ANSI_COLORS]->ValueInt) {
		if (IsValidColour(apRGB)) {
			bool_t bBackground = (tolower(apRGB[0]) == 'b');
			int Red = apRGB[1] - '0';
			int Green = apRGB[2] - '0';
			int Blue = apRGB[3] - '0';

			if (pProtocol->pVariables[eMSDP_XTERM_256_COLORS]->ValueInt) {
				return GetRGBColour(bBackground, Red, Green, Blue);
			}
			else { /* Use regular ANSI colour */
				return ansiBackup ? ansiBackup : GetAnsiColour(bBackground, Red, Green, Blue);
			}
		}
		else {	// Invalid colour - use this to clear any existing colour.
			return s_Clean;
		}
	}
	else {	// Don't send any colour, not even clear
		return "";
	}
}


/******************************************************************************
 UTF-8 global functions.
 *****************************************************************************/

char *UnicodeGet(int aValue) {
	static char Buffer[8];
	char *pString = Buffer;

	UnicodeAdd(&pString, aValue);
	*pString = '\0';

	return Buffer;
}

void UnicodeAdd(char **apString, int aValue) {
	if (aValue < 0x80) {
		*(*apString)++ = (char)aValue;
	}
	else if (aValue < 0x800) {
		*(*apString)++ = (char)(0xC0 | (aValue>>6));
		*(*apString)++ = (char)(0x80 | (aValue & 0x3F));
	}
	else if (aValue < 0x10000) {
		*(*apString)++ = (char)(0xE0 | (aValue>>12));
		*(*apString)++ = (char)(0x80 | (aValue>>6 & 0x3F));
		*(*apString)++ = (char)(0x80 | (aValue & 0x3F));
	}
	else if (aValue < 0x200000) {
		*(*apString)++ = (char)(0xF0 | (aValue>>18));
		*(*apString)++ = (char)(0x80 | (aValue>>12 & 0x3F));
		*(*apString)++ = (char)(0x80 | (aValue>>6 & 0x3F));
		*(*apString)++ = (char)(0x80 | (aValue & 0x3F));
	}
}


/******************************************************************************
 Local negotiation functions.
 *****************************************************************************/

static void Negotiate(descriptor_t *apDescriptor) {
	protocol_t *pProtocol = apDescriptor->pProtocol;

	if (pProtocol->bNegotiated) {
		const char RequestTTYPE[] = { (char)IAC, (char)SB, TELOPT_TTYPE, SEND, (char)IAC, (char)SE, '\0' };

		/* Request the client type if TTYPE is supported. */
		if (pProtocol->bTTYPE)
			Write(apDescriptor, RequestTTYPE);

		/* Check for other protocols. */
		ConfirmNegotiation(apDescriptor, eNEGOTIATED_NAWS, true, true);
		ConfirmNegotiation(apDescriptor, eNEGOTIATED_CHARSET, true, true);
		ConfirmNegotiation(apDescriptor, eNEGOTIATED_MSDP, true, true);
		ConfirmNegotiation(apDescriptor, eNEGOTIATED_MSSP, true, true);
		ConfirmNegotiation(apDescriptor, eNEGOTIATED_ATCP, true, true);
		ConfirmNegotiation(apDescriptor, eNEGOTIATED_MSP, true, true);
		ConfirmNegotiation(apDescriptor, eNEGOTIATED_MXP, true, true);
		ConfirmNegotiation(apDescriptor, eNEGOTIATED_MCCP, true, true);
	}
}

static void PerformHandshake(descriptor_t *apDescriptor, char aCmd, char aProtocol) {
	protocol_t *pProtocol = apDescriptor->pProtocol;

	switch (aProtocol) {
		case (char)TELOPT_TTYPE: {
			if (aCmd == (char)WILL) {
				ConfirmNegotiation(apDescriptor, eNEGOTIATED_TTYPE, true, true);
				pProtocol->bTTYPE = true;

				if (!pProtocol->bNegotiated) {
					/* Negotiate for the remaining protocols. */
					pProtocol->bNegotiated = true;
					Negotiate(apDescriptor);

					/* We may need to renegotiate if they don't reply */
					pProtocol->bRenegotiate = true;
				}
			}
			else if (aCmd == (char)WONT) {
				ConfirmNegotiation(apDescriptor, eNEGOTIATED_TTYPE, false, pProtocol->bTTYPE);
				pProtocol->bTTYPE = false;

				if (!pProtocol->bNegotiated) {
					/* Still negotiate, as this client obviously knows how to 
					* correctly respond to negotiation attempts - but we don't 
					* ask for TTYPE, as it doesn't support it.
					*/
					pProtocol->bNegotiated = true;
					Negotiate(apDescriptor);

					/* We may need to renegotiate if they don't reply */
					pProtocol->bRenegotiate = true;
				}
			}
			else if (aCmd == (char)DO) {
				/* Invalid negotiation, send a rejection */
				SendNegotiationSequence(apDescriptor, (char)WONT, (char)aProtocol);
			}
			break;
		}

		case (char)TELOPT_ECHO: {
			if (aCmd == (char)DO) {
				ConfirmNegotiation(apDescriptor, eNEGOTIATED_ECHO, true, true);
				pProtocol->bECHO = true;
			}
			else if (aCmd == (char)DONT) {
				ConfirmNegotiation(apDescriptor, eNEGOTIATED_ECHO, false, pProtocol->bECHO);
				pProtocol->bECHO = false;
			}
			else if (aCmd == (char)WILL) {
				/* Invalid negotiation, send a rejection */
				SendNegotiationSequence(apDescriptor, (char)DONT, (char)aProtocol);
			}
			break;
		}

		case (char)TELOPT_NAWS: {
			if (aCmd == (char)WILL) {
				ConfirmNegotiation(apDescriptor, eNEGOTIATED_NAWS, true, true);
				pProtocol->bNAWS = true;

				/* Renegotiation workaround won't be necessary. */
				pProtocol->bRenegotiate = false;
			}
			else if (aCmd == (char)WONT) {
				ConfirmNegotiation(apDescriptor, eNEGOTIATED_NAWS, false, pProtocol->bNAWS);
				pProtocol->bNAWS = false;

				/* Renegotiation workaround won't be necessary. */
				pProtocol->bRenegotiate = false;
			}
			else if (aCmd == (char)DO) {
				/* Invalid negotiation, send a rejection */
				SendNegotiationSequence(apDescriptor, (char)WONT, (char)aProtocol);
			}
			break;
		}

		case (char)TELOPT_CHARSET: {
			if (aCmd == (char)WILL) {
				ConfirmNegotiation(apDescriptor, eNEGOTIATED_CHARSET, true, true);
				if (!pProtocol->bCHARSET) {
					const char charset_utf8 [] = { (char)IAC, (char)SB, TELOPT_CHARSET, 1, ' ', 'U', 'T', 'F', '-', '8', (char)IAC, (char)SE, '\0' };
					Write(apDescriptor, charset_utf8);
					pProtocol->bCHARSET = true;
				}
			}
			else if (aCmd == (char)WONT) {
				ConfirmNegotiation(apDescriptor, eNEGOTIATED_CHARSET, false, pProtocol->bCHARSET);
				pProtocol->bCHARSET = false;
			}
			else if (aCmd == (char)DO) {
				/* Invalid negotiation, send a rejection */
				SendNegotiationSequence(apDescriptor, (char)WONT, (char)aProtocol);
			}
			break;
		}

		case (char)TELOPT_MSDP: {
			if (aCmd == (char)DO) {
				ConfirmNegotiation(apDescriptor, eNEGOTIATED_MSDP, true, true);

				if (!pProtocol->bMSDP) {
					pProtocol->bMSDP = true;

					/* Identify the mud to the client. */
					MSDPSendPair(apDescriptor, "SERVER_ID", config_get_string("mud_name"));
				}
			}
			else if (aCmd == (char)DONT) {
				ConfirmNegotiation(apDescriptor, eNEGOTIATED_MSDP, false, pProtocol->bMSDP);
				pProtocol->bMSDP = false;
			}
			else if (aCmd == (char)WILL) {
				/* Invalid negotiation, send a rejection */
				SendNegotiationSequence(apDescriptor, (char)DONT, (char)aProtocol);
			}
			break;
		}

		case (char)TELOPT_MSSP: {
			if (aCmd == (char)DO) {
				ConfirmNegotiation(apDescriptor, eNEGOTIATED_MSSP, true, true);

				if (!pProtocol->bMSSP) {
					SendMSSP(apDescriptor);
					pProtocol->bMSSP = true;
				}
			}
			else if (aCmd == (char)DONT) {
				ConfirmNegotiation(apDescriptor, eNEGOTIATED_MSSP, false, pProtocol->bMSSP);
				pProtocol->bMSSP = false;
			}
			else if (aCmd == (char)WILL) {
				/* Invalid negotiation, send a rejection */
				SendNegotiationSequence(apDescriptor, (char)DONT, (char)aProtocol);
			}
			break;
		}

		case (char)TELOPT_MCCP: {
			if (aCmd == (char)DO) {
				ConfirmNegotiation(apDescriptor, eNEGOTIATED_MCCP, true, true);

				if (!pProtocol->bMCCP) {
					pProtocol->bMCCP = true;
					CompressStart(apDescriptor);
				}
			}
			else if (aCmd == (char)DONT) {
				ConfirmNegotiation(apDescriptor, eNEGOTIATED_MCCP, false, pProtocol->bMCCP);

				if (pProtocol->bMCCP) {
					pProtocol->bMCCP = false;
					CompressEnd(apDescriptor);
				}
			}
			else if (aCmd == (char)WILL) {
				/* Invalid negotiation, send a rejection */
				SendNegotiationSequence(apDescriptor, (char)DONT, (char)aProtocol);
			}
			break;
		}

		case (char)TELOPT_MSP: {
			if (aCmd == (char)DO) {
				ConfirmNegotiation(apDescriptor, eNEGOTIATED_MSP, true, true);
				pProtocol->bMSP = true;
			}
			else if (aCmd == (char)DONT) {
				ConfirmNegotiation(apDescriptor, eNEGOTIATED_MSP, false, pProtocol->bMSP);
				pProtocol->bMSP = false;
			}
			else if (aCmd == (char)WILL) {
				/* Invalid negotiation, send a rejection */
				SendNegotiationSequence(apDescriptor, (char)DONT, (char)aProtocol);
			}
			break;
		}

		case (char)TELOPT_MXP: {
			if (aCmd == (char)WILL || aCmd == (char)DO) {
				if (aCmd == (char)WILL)
					ConfirmNegotiation(apDescriptor, eNEGOTIATED_MXP, true, true);
				else /* aCmd == (char)DO */
					ConfirmNegotiation(apDescriptor, eNEGOTIATED_MXP2, true, true);

				if (!pProtocol->bMXP) {
					/* Enable MXP. */
					const char EnableMXP[] = { (char)IAC, (char)SB, TELOPT_MXP, (char)IAC, (char)SE, '\0' };
					Write(apDescriptor, EnableMXP);

					/* Create a secure channel, and note that MXP is active. */
					Write(apDescriptor, "\033[7z");
					pProtocol->bMXP = true;
					pProtocol->pVariables[eMSDP_MXP]->ValueInt = 1;

					if (pProtocol->bNeedMXPVersion) {
						MXPSendTag(apDescriptor, "<VERSION>");
					}
				}
			}
			else if (aCmd == (char)WONT) {
				ConfirmNegotiation(apDescriptor, eNEGOTIATED_MXP, false, pProtocol->bMXP);

				if (!pProtocol->bMXP) {
					/* The MXP standard doesn't actually specify whether you should 
					* negotiate with IAC DO MXP or IAC WILL MXP.  As a result, some 
					* clients support one, some the other, and some support both.
					* 
					* Therefore we first try IAC DO MXP, and if the client replies 
					* with WONT, we try again (here) with IAC WILL MXP.
					*/
					ConfirmNegotiation(apDescriptor, eNEGOTIATED_MXP2, true, true);
				}
				else {	// The client is actually asking us to switch MXP off.
					pProtocol->bMXP = false;
				}
			}
			else if (aCmd == (char)DONT) {
				ConfirmNegotiation(apDescriptor, eNEGOTIATED_MXP2, false, pProtocol->bMXP);
				pProtocol->bMXP = false;
			}
			break;
		}

		case (char)TELOPT_ATCP: {
			if (aCmd == (char)WILL) {
				ConfirmNegotiation(apDescriptor, eNEGOTIATED_ATCP, true, true);

				/* If we don't support MSDP, fake it with ATCP */
				if (!pProtocol->bMSDP && !pProtocol->bATCP) {
					pProtocol->bATCP = true;

					#ifdef MUDLET_PACKAGE
						/* Send the Mudlet GUI package to the user. */
						if (MatchString("Mudlet", pProtocol->pVariables[eMSDP_CLIENT_ID]->pValueString)) {
							SendATCP(apDescriptor, "Client.GUI", MUDLET_PACKAGE);
						}
					#endif /* MUDLET_PACKAGE */

					/* Identify the mud to the client. */
					MSDPSendPair(apDescriptor, "SERVER_ID", config_get_string("mud_name"));
				}
			}
			else if (aCmd == (char)WONT) {
				ConfirmNegotiation(apDescriptor, eNEGOTIATED_ATCP, false, pProtocol->bATCP);
				pProtocol->bATCP = false;
			}
			else if (aCmd == (char)DO) {
				/* Invalid negotiation, send a rejection */
				SendNegotiationSequence(apDescriptor, (char)WONT, (char)aProtocol);
			}
			break;
		}

		default: {
			if (aCmd == (char)WILL) {
				/* Invalid negotiation, send a rejection */
				SendNegotiationSequence(apDescriptor, (char)DONT, (char)aProtocol);
			}
			else if (aCmd == (char)DO) {
				/* Invalid negotiation, send a rejection */
				SendNegotiationSequence(apDescriptor, (char)WONT, (char)aProtocol);
			}
			break;
		}
	}
}

static void PerformSubnegotiation(descriptor_t *apDescriptor, char aCmd, char *apData, int aSize) {
	protocol_t *pProtocol = apDescriptor->pProtocol;

	switch (aCmd) {
		case (char)TELOPT_TTYPE: {
			if (pProtocol->bTTYPE) {
				/* Store the client name. */
				const int MaxClientLength = 64;
				char *pClientName = alloca(MaxClientLength+1);
				int i = 0, j = 1;
				bool_t bStopCyclicTTYPE = false;

				for (; apData[j] != '\0' && i < MaxClientLength; ++j) {
					if (isprint(apData[j])) {
						pClientName[i++] = apData[j];
					}
				}
				pClientName[i] = '\0';

				/* Store the first TTYPE as the client name */
				if (!strcmp(pProtocol->pVariables[eMSDP_CLIENT_ID]->pValueString, "Unknown")) {
					free(pProtocol->pVariables[eMSDP_CLIENT_ID]->pValueString);
					pProtocol->pVariables[eMSDP_CLIENT_ID]->pValueString = AllocString(pClientName);

					/* This is a bit nasty, but using cyclic TTYPE on windows telnet 
					* causes it to lock up.  None of the clients we need to cycle 
					* with send ANSI to start with anyway, so we shouldn't have any 
					* conflicts.
					* 
					* An alternative solution is to use escape sequences to check 
					* for windows telnet prior to negotiation, and this also avoids 
					* the player losing echo, but it has other issues.  Because the 
					* escape codes are technically in-band, even though they'll be 
					* stripped from the display, the newlines will still cause some 
					* scrolling.  Therefore you need to either pause the session 
					* for a few seconds before displaying the login screen, or wait 
					* until the player has entered their name before negotiating.
					*/
					if (!strcmp(pClientName,"ANSI")) {
						bStopCyclicTTYPE = true;
					}
				}

				/* Cycle through the TTYPEs until we get the same result twice, or 
				* find ourselves back at the start.
				* 
				* If the client follows RFC1091 properly then it will indicate the 
				* end of the list by repeating the last response, and then return 
				* to the top of the list.  If you're the trusting type, then feel 
				* free to remove the second strcmp ;)
				*/
				if (pProtocol->pLastTTYPE == NULL || (strcmp(pProtocol->pLastTTYPE, pClientName) && strcmp(pProtocol->pVariables[eMSDP_CLIENT_ID]->pValueString, pClientName))) {
					char RequestTTYPE [] = { (char)IAC, (char)SB, TELOPT_TTYPE, SEND, (char)IAC, (char)SE, '\0' };
					const char *pStartPos = strstr(pClientName, "-");

					/* Store the TTYPE */
					free(pProtocol->pLastTTYPE);
					pProtocol->pLastTTYPE = AllocString(pClientName);

					/* Look for 256 colour support */
					if (pStartPos != NULL && MatchString(pStartPos, "-256color")) {
						/* This is currently the only way to detect support for 256 
						* colours in TinTin++, WinTin++ and BlowTorch.
						*/
						pProtocol->pVariables[eMSDP_XTERM_256_COLORS]->ValueInt = 1;
						pProtocol->b256Support = eYES;
					}

					/* Request another TTYPE */
					if (!bStopCyclicTTYPE) {
						Write(apDescriptor, RequestTTYPE);
					}
				}

				if (PrefixString("Mudlet", pClientName)) {
					/* Mudlet beta 15 and later supports 256 colours, but we can't 
					* identify it from the mud - everything prior to 1.1 claims 
					* to be version 1.0, so we just don't know.
					*/ 
					pProtocol->b256Support = eSOMETIMES;

					if (strlen(pClientName) > 7) {
						pClientName[6] = '\0';
						free(pProtocol->pVariables[eMSDP_CLIENT_ID]->pValueString);
						pProtocol->pVariables[eMSDP_CLIENT_ID]->pValueString = AllocString(pClientName);
						free(pProtocol->pVariables[eMSDP_CLIENT_VERSION]->pValueString);
						pProtocol->pVariables[eMSDP_CLIENT_VERSION]->pValueString = AllocString(pClientName+7);

						/* Mudlet 1.1 and later supports 256 colours. */
						if (strcmp(pProtocol->pVariables[eMSDP_CLIENT_VERSION]->pValueString, "1.1") >= 0) {
							pProtocol->pVariables[eMSDP_XTERM_256_COLORS]->ValueInt = 1;
							pProtocol->b256Support = eYES;
						}
					}
				}
				else if (MatchString(pClientName, "EMACS-RINZAI")) {
					/* We know for certain that this client has support */
					pProtocol->pVariables[eMSDP_XTERM_256_COLORS]->ValueInt = 1;
					pProtocol->b256Support = eYES;
				}
				else if (PrefixString("DecafMUD", pClientName)) {
					/* We know for certain that this client has support */
					pProtocol->pVariables[eMSDP_XTERM_256_COLORS]->ValueInt = 1;
					pProtocol->b256Support = eYES;

					if (strlen(pClientName) > 9) {
						pClientName[8] = '\0';
						free(pProtocol->pVariables[eMSDP_CLIENT_ID]->pValueString);
						pProtocol->pVariables[eMSDP_CLIENT_ID]->pValueString = AllocString(pClientName);
						free(pProtocol->pVariables[eMSDP_CLIENT_VERSION]->pValueString);
						pProtocol->pVariables[eMSDP_CLIENT_VERSION]->pValueString = AllocString(pClientName+9);
					}
				}
				else if (MatchString(pClientName, "MUSHCLIENT") || MatchString(pClientName, "CMUD") || MatchString(pClientName, "ATLANTIS") || MatchString(pClientName, "KILDCLIENT") || MatchString(pClientName, "TINTIN++") || MatchString(pClientName, "TINYFUGUE")) {
					/* We know that some versions of this client have support */
					pProtocol->b256Support = eSOMETIMES;
				}
				else if (MatchString(pClientName, "ZMUD")) {
					/* We know for certain that this client does not have support */
					pProtocol->b256Support = eNO;
				}
			}
			break;
		}

		case (char)TELOPT_NAWS: {
			if (pProtocol->bNAWS) {
				/* Store the new width. */
				pProtocol->ScreenWidth = (unsigned char)apData[0];
				pProtocol->ScreenWidth <<= 8;
				pProtocol->ScreenWidth += (unsigned char)apData[1];

				/* Store the new height. */
				pProtocol->ScreenHeight = (unsigned char)apData[2];
				pProtocol->ScreenHeight <<= 8;
				pProtocol->ScreenHeight += (unsigned char)apData[3];
			}
			break;
		}

		case (char)TELOPT_CHARSET: {
			if (pProtocol->bCHARSET) {
				/* Because we're only asking about UTF-8, we can just check the 
				* first character.  If you ask for more than one CHARSET you'll 
				* need to read through the results to see which are accepted.
				*
				* Note that the user must also use a unicode font!
				*/
				if (apData[0] == ACCEPTED) {
					pProtocol->pVariables[eMSDP_UTF_8]->ValueInt = 1;
				}
			}
			break;
		}

		case (char)TELOPT_MSDP: {
			if (pProtocol->bMSDP) {
				ParseMSDP(apDescriptor, apData);
			}
			break;
		}

		case (char)TELOPT_ATCP: {
			if (pProtocol->bATCP) {
				ParseATCP(apDescriptor, apData);
			}
			break;
		}

		default: { // Unknown subnegotiation, so we simply ignore it.
			break;
		}
	}
}

static void SendNegotiationSequence(descriptor_t *apDescriptor, char aCmd, char aProtocol) {
	char NegotiateSequence[4];

	NegotiateSequence[0] = (char)IAC;
	NegotiateSequence[1] = aCmd;
	NegotiateSequence[2] = aProtocol;
	NegotiateSequence[3] = '\0';

	Write(apDescriptor, NegotiateSequence);
}

static bool_t ConfirmNegotiation(descriptor_t *apDescriptor, negotiated_t aProtocol, bool_t abWillDo, bool_t abSendReply) {
	bool_t bResult = false;

	if (aProtocol >= eNEGOTIATED_TTYPE && aProtocol < eNEGOTIATED_MAX) {
		/* Only negotiate if the state has changed. */
		if (apDescriptor->pProtocol->Negotiated[aProtocol] != abWillDo) {
			/* Store the new state. */
			apDescriptor->pProtocol->Negotiated[aProtocol] = abWillDo;

			bResult = true;

			if (abSendReply) {
				switch (aProtocol) {
					case eNEGOTIATED_TTYPE:
						SendNegotiationSequence(apDescriptor, (char) (abWillDo ? DO : DONT), TELOPT_TTYPE);
						break;
					case eNEGOTIATED_ECHO:
						SendNegotiationSequence(apDescriptor, (char) (abWillDo ? WILL : WONT), TELOPT_ECHO);
						break;
					case eNEGOTIATED_NAWS:
						SendNegotiationSequence(apDescriptor, (char) (abWillDo ? DO : DONT), TELOPT_NAWS);
						break;
					case eNEGOTIATED_CHARSET:
						SendNegotiationSequence(apDescriptor, (char) (abWillDo ? DO : DONT), TELOPT_CHARSET);
						break;
					case eNEGOTIATED_MSDP:
						SendNegotiationSequence(apDescriptor, (char) (abWillDo ? WILL : WONT), TELOPT_MSDP);
						break;
					case eNEGOTIATED_MSSP:
						SendNegotiationSequence(apDescriptor, (char) (abWillDo ? WILL : WONT), TELOPT_MSSP);
						break;
					case eNEGOTIATED_ATCP:
						SendNegotiationSequence(apDescriptor, (char) (abWillDo ? DO : DONT), (char)TELOPT_ATCP);
						break;
					case eNEGOTIATED_MSP:
						SendNegotiationSequence(apDescriptor, (char) (abWillDo ? WILL : WONT), TELOPT_MSP);
						break;
					case eNEGOTIATED_MXP:
						SendNegotiationSequence(apDescriptor, (char) (abWillDo ? DO : DONT), TELOPT_MXP);
						break;
					case eNEGOTIATED_MXP2:
						SendNegotiationSequence(apDescriptor, (char) (abWillDo ? WILL : WONT), TELOPT_MXP);
						break;
					case eNEGOTIATED_MCCP:
						#ifdef USING_MCCP
							SendNegotiationSequence(apDescriptor, (char) (abWillDo ? WILL : WONT), TELOPT_MCCP);
						#endif /* USING_MCCP */
						break;
					default: {
						bResult = false;
						break;
					}
				}
			}
		}
	}

	return bResult;
}


/******************************************************************************
 Local MSDP functions.
 *****************************************************************************/

static void ParseMSDP(descriptor_t *apDescriptor, const char *apData) {
	char Variable[MSDP_VAL][MAX_MSDP_SIZE+1] = { {'\0'}, {'\0'} };
	char *pPos = NULL, *pStart = NULL;

	while (*apData) {
		switch (*apData) {
			case MSDP_VAR:
			case MSDP_VAL:
				pPos = pStart = Variable[*apData++-1];
				break;
			default: {	/* Anything else */
				if (pPos && pPos-pStart < MAX_MSDP_SIZE) {
					*pPos++ = *apData;
					*pPos = '\0';
				}

				if (*++apData) {
					continue;
				}
				break;
			}
		}

		ExecuteMSDPPair(apDescriptor, Variable[MSDP_VAR-1], Variable[MSDP_VAL-1]);
		Variable[MSDP_VAL-1][0] = '\0';
	}
}

static void ExecuteMSDPPair(descriptor_t *apDescriptor, const char *apVariable, const char *apValue) {
	if (apVariable[0] != '\0' && apValue[0] != '\0') {
		if (MatchString(apVariable, "SEND")) {
			bool_t bDone = false;
			int i; /* Loop counter */
			for (i = eMSDP_NONE+1; i < eMSDP_MAX && !bDone; ++i) {
				if (MatchString(apValue, VariableNameTable[i].pName)) {
					MSDPSend(apDescriptor, (variable_t)i);
					bDone = true;
				}
			}
		}
		else if (MatchString(apVariable, "REPORT")) {
			bool_t bDone = false;
			int i; /* Loop counter */
			for (i = eMSDP_NONE+1; i < eMSDP_MAX && !bDone; ++i) {
				if (MatchString(apValue, VariableNameTable[i].pName)) {
					apDescriptor->pProtocol->pVariables[i]->bReport = true;
					apDescriptor->pProtocol->pVariables[i]->bDirty = true;
					bDone = true;
				}
			}
		}
		else if (MatchString(apVariable, "RESET")) {
			if (MatchString(apValue, "REPORTABLE_VARIABLES") || MatchString(apValue, "REPORTED_VARIABLES")) {
				int i; /* Loop counter */
				for (i = eMSDP_NONE+1; i < eMSDP_MAX; ++i) {
					if (apDescriptor->pProtocol->pVariables[i]->bReport) {
						apDescriptor->pProtocol->pVariables[i]->bReport = false;
						apDescriptor->pProtocol->pVariables[i]->bDirty = false;
					}
				}
			}
		}
		else if (MatchString(apVariable, "UNREPORT")) {
			bool_t bDone = false;
			int i; /* Loop counter */
			for (i = eMSDP_NONE+1; i < eMSDP_MAX && !bDone; ++i) {
				if (MatchString(apValue, VariableNameTable[i].pName)) {
					apDescriptor->pProtocol->pVariables[i]->bReport = false;
					apDescriptor->pProtocol->pVariables[i]->bDirty = false;
					bDone = true;
				}
			}
		}
		else if (MatchString(apVariable, "LIST")) {
			if (MatchString(apValue, "COMMANDS")) {
				const char MSDPCommands[] = "LIST REPORT RESET SEND UNREPORT";
				MSDPSendList(apDescriptor, "COMMANDS", MSDPCommands);
			}
			else if (MatchString(apValue, "LISTS")) {
				const char MSDPCommands[] = "COMMANDS LISTS CONFIGURABLE_VARIABLES REPORTABLE_VARIABLES REPORTED_VARIABLES SENDABLE_VARIABLES GUI_VARIABLES";
				MSDPSendList(apDescriptor, "LISTS", MSDPCommands);
			}
			/* Split this into two if some variables aren't REPORTABLE */
			else if (MatchString(apValue, "SENDABLE_VARIABLES") || MatchString(apValue, "REPORTABLE_VARIABLES")) {
				char MSDPCommands[MAX_OUTPUT_BUFFER] = { '\0' };
				int i; /* Loop counter */

				for (i = eMSDP_NONE+1; i < eMSDP_MAX; ++i) {
					if (!VariableNameTable[i].bGUI) {
						/* Add the separator between variables */
						strcat(MSDPCommands, " ");

						/* Add the variable to the list */
						strcat(MSDPCommands, VariableNameTable[i].pName);
					}
				}

				MSDPSendList(apDescriptor, apValue, MSDPCommands);
			}
			else if (MatchString(apValue, "REPORTED_VARIABLES")) {
				char MSDPCommands[MAX_OUTPUT_BUFFER] = { '\0' };
				int i; /* Loop counter */

				for (i = eMSDP_NONE+1; i < eMSDP_MAX; ++i) {
					if (apDescriptor->pProtocol->pVariables[i]->bReport) {
						/* Add the separator between variables */
						if (MSDPCommands[0] != '\0')
							strcat(MSDPCommands, " ");

						/* Add the variable to the list */
						strcat(MSDPCommands, VariableNameTable[i].pName);
					}
				}

				MSDPSendList(apDescriptor, apValue, MSDPCommands);
			}
			else if (MatchString(apValue, "CONFIGURABLE_VARIABLES")) {
				char MSDPCommands[MAX_OUTPUT_BUFFER] = { '\0' };
				int i; /* Loop counter */

				for (i = eMSDP_NONE+1; i < eMSDP_MAX; ++i) {
					if (VariableNameTable[i].bConfigurable) {
						/* Add the separator between variables */
						if (MSDPCommands[0] != '\0')
							strcat(MSDPCommands, " ");

						/* Add the variable to the list */
						strcat(MSDPCommands, VariableNameTable[i].pName);
					}
				}

				MSDPSendList(apDescriptor, "CONFIGURABLE_VARIABLES", MSDPCommands);
			}
			else if (MatchString(apValue, "GUI_VARIABLES")) {
				char MSDPCommands[MAX_OUTPUT_BUFFER] = { '\0' };
				int i; /* Loop counter */

				for (i = eMSDP_NONE+1; i < eMSDP_MAX; ++i) {
					if (VariableNameTable[i].bGUI) {
						/* Add the separator between variables */
						if (MSDPCommands[0] != '\0')
							strcat(MSDPCommands, " ");

						/* Add the variable to the list */
						strcat(MSDPCommands, VariableNameTable[i].pName);
					}
				}

				MSDPSendList(apDescriptor, apValue, MSDPCommands);
			}
		}
		else {	// Set any configurable variables
			int i; /* Loop counter */

			for (i = eMSDP_NONE+1; i < eMSDP_MAX; ++i) {
				if (VariableNameTable[i].bConfigurable) {
					if (MatchString(apVariable, VariableNameTable[i].pName)) {
						if (VariableNameTable[i].bString) {
							/* A write-once variable can only be set if the value 
							* is "Unknown".  This is for things like client name, 
							* where we don't really want the player overwriting a 
							* proper client name with junk - but on the other hand, 
							* its possible a client may choose to use MSDP to 
							* identify itself.
							*/
							if (!VariableNameTable[i].bWriteOnce || !strcmp(apDescriptor->pProtocol->pVariables[i]->pValueString, "Unknown")) {
								/* Store the new value if it's valid */
								char *pBuffer = alloca(VariableNameTable[i].Max+1);
								int j; /* Loop counter */

								for (j = 0; j < VariableNameTable[i].Max && *apValue != '\0'; ++apValue) {
									if (isprint(*apValue)) {
										pBuffer[j++] = *apValue;
									}
								}
								pBuffer[j++] = '\0';

								if (j >= VariableNameTable[i].Min) {
									free(apDescriptor->pProtocol->pVariables[i]->pValueString);
									apDescriptor->pProtocol->pVariables[i]->pValueString = AllocString(pBuffer);
								}
							}
						}
						else {	// This variable only accepts numeric values
							/* Strip any leading spaces */
							while (*apValue == ' ')
								++apValue;

							if (*apValue != '\0' && IsNumber(apValue)) {
								int Value = atoi(apValue);
								if (Value >= VariableNameTable[i].Min && Value <= VariableNameTable[i].Max) {
									apDescriptor->pProtocol->pVariables[i]->ValueInt = Value;
								}
							}
						}
					}
				}
			}
		}
	}
}


/******************************************************************************
 Local ATCP functions.
 *****************************************************************************/

static void ParseATCP(descriptor_t *apDescriptor, const char *apData) {
	char Variable[MSDP_VAL][MAX_MSDP_SIZE+1] = { {'\0'}, {'\0'} };
	char *pPos = NULL, *pStart = NULL;

	while (*apData) {
		switch (*apData) {
			case '@': 
				pPos = pStart = Variable[0];
				apData++;
				break;
			case ' ': 
				pPos = pStart = Variable[1];
				apData++;
				break;
			default: {	// Anything else
				if (pPos && pPos-pStart < MAX_MSDP_SIZE) {
					*pPos++ = *apData;
					*pPos = '\0';
				}

				if (*++apData) {
					continue;
				}
				break;
			}
		}

		ExecuteMSDPPair(apDescriptor, Variable[MSDP_VAR-1], Variable[MSDP_VAL-1]);
		Variable[MSDP_VAL-1][0] = '\0';
	}
}

#ifdef MUDLET_PACKAGE
static void SendATCP(descriptor_t *apDescriptor, const char *apVariable, const char *apValue) {
	char ATCPBuffer[MAX_VARIABLE_LENGTH+1] = { '\0' };

	if (apVariable != NULL && apValue != NULL) {
		protocol_t *pProtocol = apDescriptor ? apDescriptor->pProtocol : NULL;

		/* Should really be replaced with a dynamic buffer */
		int RequiredBuffer = strlen(apVariable) + strlen(apValue) + 12;

		if (RequiredBuffer >= MAX_VARIABLE_LENGTH) {
			if (RequiredBuffer - strlen(apValue) < MAX_VARIABLE_LENGTH) {
				sprintf(ATCPBuffer, "SendATCP: %s %d bytes (exceeds MAX_VARIABLE_LENGTH of %d).\n", apVariable, RequiredBuffer, MAX_VARIABLE_LENGTH);
			}
			else {	// The variable name itself is too long
				sprintf(ATCPBuffer, "SendATCP: Variable name has a length of %d bytes (exceeds MAX_VARIABLE_LENGTH of %d).\n", RequiredBuffer, MAX_VARIABLE_LENGTH);
			}

			ReportBug(ATCPBuffer);
			ATCPBuffer[0] = '\0';
		}
		else if (pProtocol->bATCP) {
			sprintf(ATCPBuffer, "%c%c%c%s %s%c%c", IAC, SB, TELOPT_ATCP, apVariable, apValue, IAC, SE);
		}

		/* Just in case someone calls this function without checking ATCP */
		if (ATCPBuffer[0] != '\0') {
			Write(apDescriptor, ATCPBuffer);
		}
	}
}
#endif /* MUDLET_PACKAGE */


/******************************************************************************
 Local MSSP functions.
 *****************************************************************************/

static const char *GetMSSP_Areas() {
	adv_data *iter, *next_iter;
	static char buf[256];
	int count = 0;
	
	// open adventures:
	HASH_ITER(hh, adventure_table, iter, next_iter) {
		if (!ADVENTURE_FLAGGED(iter, ADV_IN_DEVELOPMENT)) {
			++count;
		}
	}
	
	// map areas:
	count += HASH_COUNT(island_table);
	
	snprintf(buf, sizeof(buf), "%d", count);
	return buf;
}

static const char *GetMSSP_Classes() {
	static char buf[256];
	strcpy(buf, "1");
	return buf;
}

static const char *GetMSSP_Codebase() {
	extern const char *version;	// constants.c
	return version;
}

static const char *GetMSSP_Contact() {
	return config_get_string("mud_contact");
}

static const char *GetMSSP_Created() {
	return config_get_string("mud_created");
}

static const char *GetMSSP_DB_Size() {
	static char buf[256];
	int size = 0;

	// we're not 100% sure what counts toward DB Size, but here is a rough guess
	size += HASH_CNT(idnum_hh, player_table_by_idnum);
	size += HASH_COUNT(empire_table);
	size += HASH_COUNT(mobile_table);
	size += HASH_COUNT(object_table);
	size += HASH_COUNT(adventure_table);
	size += HASH_COUNT(world_table);
	size += HASH_COUNT(building_table);
	size += HASH_COUNT(room_template_table);
	size += HASH_COUNT(sector_table);
	size += HASH_COUNT(crop_table);
	size += HASH_COUNT(trigger_table);
	size += HASH_COUNT(craft_table);
	
	snprintf(buf, sizeof(buf), "%d", size);
	return buf;
}

static const char *GetMSSP_Extra_Descs() {
	static char buf[256];
	room_template *rmt, *next_rmt;
	struct extra_descr_data *ex;
	obj_data *obj, *next_obj;
	bld_data *bld, *next_bld;
	int count = 0;
	
	HASH_ITER(hh, object_table, obj, next_obj) {
		if (GET_OBJ_ACTION_DESC(obj) && *GET_OBJ_ACTION_DESC(obj)) {
			++count;
		}
		for (ex = obj->ex_description; ex; ex = ex->next) {
			++count;
		}
	}
	HASH_ITER(hh, room_template_table, rmt, next_rmt) {
		for (ex = GET_RMT_EX_DESCS(rmt); ex; ex = ex->next) {
			++count;
		}
	}
	HASH_ITER(hh, building_table, bld, next_bld) {
		for (ex = GET_BLD_EX_DESCS(bld); ex; ex = ex->next) {
			++count;
		}
	}
	
	snprintf(buf, sizeof(buf), "%d", count);
	return buf;
}

static const char *GetMSSP_Helpfiles() {
	extern int top_of_helpt;	// db.c
	static char buf[256];	
	snprintf(buf, sizeof(buf), "%d", top_of_helpt + 1);
	return buf;
}

static const char *GetMSSP_Hiring_Builders() {
	static char buf[256];	
	snprintf(buf, sizeof(buf), "%d", config_get_bool("hiring_builders") ? 1 : 0);
	return buf;
}

static const char *GetMSSP_Hiring_Coders() {
	static char buf[256];	
	snprintf(buf, sizeof(buf), "%d", config_get_bool("hiring_coders") ? 1 : 0);
	return buf;
}

static const char *GetMSSP_Hostname() {
	return config_get_string("mud_hostname");
}

static const char *GetMSSP_Icon() {
	return config_get_string("mud_icon");
}

static const char *GetMSSP_IP() {
	return config_get_string("mud_ip");
}

static const char *GetMSSP_Levels() {
	adv_data *iter, *next_iter;
	static char buf[256];
	int max = CLASS_SKILL_CAP;	// to start
	
	// find highest level adventure zone:
	HASH_ITER(hh, adventure_table, iter, next_iter) {
		if (!ADVENTURE_FLAGGED(iter, ADV_IN_DEVELOPMENT)) {
			max = MAX(max, GET_ADV_MAX_LEVEL(iter));
		}
	}
	
	snprintf(buf, sizeof(buf), "%d", max);
	return buf;
}

static const char *GetMSSP_Location() {
	return config_get_string("mud_location");
}

static const char *GetMSSP_Minimum_Age() {
	return config_get_string("mud_minimum_age");
}

static const char *GetMSSP_Mobiles() {
	static char buf[256];
	snprintf(buf, sizeof(buf), "%d", HASH_COUNT(mobile_table));
	return buf;
}

static const char *GetMSSP_MudName() {
	return config_get_string("mud_name");
}

static const char *GetMSSP_Objects() {
	static char buf[256];
	snprintf(buf, sizeof(buf), "%d", HASH_COUNT(object_table));
	return buf;
}

static const char *GetMSSP_Players() {
	static char Buffer[32];
	sprintf(Buffer, "%d", s_Players);
	return Buffer;
}

static const char *GetMSSP_Port() {
	extern ush_int port;	// comm.c
	static char buf[32];
	snprintf(buf, sizeof(buf), "%d", port);
	return buf;
}

static const char *GetMSSP_Rooms() {
	static char buf[256];
	snprintf(buf, sizeof(buf), "%d", HASH_COUNT(world_table));
	return buf;
}

static const char *GetMSSP_Skills() {
	static char buf[256];
	
	// we think this refers more to our concept of "abilities" than "skills"
	snprintf(buf, sizeof(buf), "%d", HASH_COUNT(ability_table));
	return buf;
}

static const char *GetMSSP_Status() {
	return config_get_string("mud_status");
}

static const char *GetMSSP_Triggers() {
	static char buf[256];
	snprintf(buf, sizeof(buf), "%d", HASH_COUNT(trigger_table));
	return buf;
}

static const char *GetMSSP_Uptime() {
	static char Buffer[32];
	sprintf(Buffer, "%d", (int)s_Uptime);
	return Buffer;
}

static const char *GetMSSP_Website() {
	return config_get_string("mud_website");
}


/* Macro for readability, but you can remove it if you don't like it */
#define FUNCTION_CALL(f)  "", f

static void SendMSSP(descriptor_t *apDescriptor) {
	char MSSPBuffer[MAX_MSSP_BUFFER];
	char MSSPPair[128];
	int SizeBuffer = 3; /* IAC SB MSSP */
	int i; /* Loop counter */

	/* Before updating the following table, please read the MSSP specification:
	*
	* http://tintin.sourceforge.net/mssp/
	*
	* It's important that you use the correct format and spelling for the MSSP 
	* variables, otherwise crawlers may reject the data as invalid.
	*/
	static MSSP_t MSSPTable[] = {
		/* Required */
		{ "NAME", FUNCTION_CALL(GetMSSP_MudName) },
		{ "PLAYERS", FUNCTION_CALL(GetMSSP_Players) },
		{ "UPTIME" , FUNCTION_CALL(GetMSSP_Uptime) },

		/* Generic */
		{ "CRAWL DELAY", "-1" },
		
		{ "HOSTNAME", FUNCTION_CALL(GetMSSP_Hostname) },
		{ "PORT", FUNCTION_CALL(GetMSSP_Port) },
		{ "CODEBASE", FUNCTION_CALL(GetMSSP_Codebase) },
		{ "CONTACT", FUNCTION_CALL(GetMSSP_Contact) },
		{ "CREATED", FUNCTION_CALL(GetMSSP_Created) },
		{ "ICON", FUNCTION_CALL(GetMSSP_Icon) },
		{ "IP", FUNCTION_CALL(GetMSSP_IP) },
		{ "LANGUAGE", "English" },
		{ "LOCATION", FUNCTION_CALL(GetMSSP_Location) },
		{ "MINIMUM AGE", FUNCTION_CALL(GetMSSP_Minimum_Age) },
		{ "WEBSITE", FUNCTION_CALL(GetMSSP_Website) },
		
		/* Categorisation */
		{ "FAMILY", "DikuMUD" },
		{ "GENRE", "Fantasy" },
		{ "GAMEPLAY", "Adventure, Hack and Slash, Player versus Player, Player versus Environment, Roleplaying, Simulation" },
		{ "STATUS", FUNCTION_CALL(GetMSSP_Status) },
		{ "GAMESYSTEM", "Custom" },
		{ "INTERMUD", "" },
		{ "SUBGENRE", "Medieval Fantasy" },
		
		/* World */
		{ "AREAS", FUNCTION_CALL(GetMSSP_Areas) },
		{ "HELPFILES", FUNCTION_CALL(GetMSSP_Helpfiles) },
		{ "MOBILES", FUNCTION_CALL(GetMSSP_Mobiles) },
		{ "OBJECTS", FUNCTION_CALL(GetMSSP_Objects) },
		{ "ROOMS", FUNCTION_CALL(GetMSSP_Rooms) },
		{ "CLASSES", FUNCTION_CALL(GetMSSP_Classes) },
		{ "LEVELS", FUNCTION_CALL(GetMSSP_Levels) },
		{ "RACES", "0" },
		{ "SKILLS", FUNCTION_CALL(GetMSSP_Skills) },
		
		/* Protocols */
		{ "ANSI", "1" },
		{ "GMCP", "0" },
		#ifdef USING_MCCP
			{ "MCCP", "1" },
		#else
			{ "MCCP", "0" },
		#endif // USING_MCCP
		{ "MCP", "0" },
		{ "MSDP", "1" },
		{ "MSP", "1" },
		{ "MXP", "1" },
		{ "PUEBLO", "0" },
		{ "UTF-8", "1" },
		{ "VT100", "0" },
		{ "XTERM 256 COLORS", "1" },
		
		/* Commercial */
		{ "PAY TO PLAY", "0" },	// these are prohibited by the license
		{ "PAY FOR PERKS", "0" },
		
		/* Hiring */
		{ "HIRING BUILDERS", FUNCTION_CALL(GetMSSP_Hiring_Builders) },
		{ "HIRING CODERS", FUNCTION_CALL(GetMSSP_Hiring_Coders) },
		
		/* Extended variables */

		/* World */
		{ "DBSIZE", FUNCTION_CALL(GetMSSP_DB_Size) },
		// { "EXITS", "0" },	// not sure we have a valid response due to the world map
		{ "EXTRA DESCRIPTIONS", FUNCTION_CALL(GetMSSP_Extra_Descs) },
		{ "MUDPROGS", "0" },	// feature does not exist
		{ "MUDTRIGS", FUNCTION_CALL(GetMSSP_Triggers) },
		{ "RESETS", "0" },	// feature does not exist
		
		/* Game */
		
		{ "ADULT MATERIAL", "0" },
		{ "MULTICLASSING", "1" },
		{ "NEWBIE FRIENDLY", "1" },
		{ "PLAYER CITIES", "1" },
		{ "PLAYER CLANS", "1" },
		{ "PLAYER CRAFTING", "1" },
		{ "PLAYER GUILDS", "1" },
		{ "EQUIPMENT SYSTEM", "Level and Skill" },
		{ "MULTIPLAYING", "No" },
		{ "PLAYERKILLING", "Restricted" },
		{ "QUEST SYSTEM", "0" },
		{ "ROLEPLAYING", "" },
		{ "TRAINING SYSTEM", "Skill" },
		{ "WORLD ORIGINALITY", "All Original" },
		
		/* Protocols */
		{ "ATCP", "1" },
		{ "SSL", "0" },
		{ "ZMP", "0" },
		
		{ NULL, NULL } /* This must always be last. */
	};

	/* Begin the subnegotiation sequence */
	sprintf(MSSPBuffer, "%c%c%c", IAC, SB, TELOPT_MSSP);

	for (i = 0; MSSPTable[i].pName != NULL; ++i) {
		int SizePair;

		/* Retrieve the next MSSP variable/value pair */
		sprintf(MSSPPair, "%c%s%c%s", MSSP_VAR, MSSPTable[i].pName, MSSP_VAL, MSSPTable[i].pFunction ? (*MSSPTable[i].pFunction)() : MSSPTable[i].pValue);

		/* Make sure we don't overflow the buffer */
		SizePair = strlen(MSSPPair);
		if (SizePair+SizeBuffer < MAX_MSSP_BUFFER-4) {
			strcat(MSSPBuffer, MSSPPair);
			SizeBuffer += SizePair;
		}
	}

	/* End the subnegotiation sequence */
	sprintf(MSSPPair, "%c%c", IAC, SE);
	strcat(MSSPBuffer, MSSPPair);

	/* Send the sequence */
	Write(apDescriptor, MSSPBuffer);
}


/******************************************************************************
 Local MXP functions.
 *****************************************************************************/

static char *GetMxpTag(const char *apTag, const char *apText) {
	static char MXPBuffer [64];
	const char *pStartPos = strstr(apText, apTag);

	if (pStartPos != NULL) {
		const char *pEndPos = apText+strlen(apText);

		pStartPos += strlen(apTag); /* Add length of the tag */

		if (pStartPos < pEndPos) {
			int Index = 0;

			/* Some clients use quotes...and some don't. */
			if (*pStartPos == '\"')
				pStartPos++;

			for (; pStartPos < pEndPos && Index < 60; ++pStartPos) {
				char Letter = *pStartPos;
				if (Letter == '.' || isdigit(Letter) || isalpha(Letter)) {
					MXPBuffer[Index++] = Letter;
				}
				else {	// Return the result
					MXPBuffer[Index] = '\0';
					return MXPBuffer;
				}
			}
		}
	}

	/* Return NULL to indicate no tag was found. */
	return NULL;
}


/******************************************************************************
 Local colour functions.
 *****************************************************************************/

static const char *GetAnsiColour(bool_t abBackground, int aRed, int aGreen, int aBlue) {
	if ( aRed == aGreen && aRed == aBlue && aRed < 2)
		return abBackground ? s_BackBlack : aRed >= 1 ? s_BoldBlack : s_DarkBlack;
	else if ( aRed == aGreen && aRed == aBlue )
		return abBackground ? s_BackWhite : aRed >= 4 ? s_BoldWhite : s_DarkWhite;
	else if ( aRed > aGreen && aRed > aBlue )
		return abBackground ? s_BackRed : aRed >= 3 ? s_BoldRed : s_DarkRed;
	else if ( aRed == aGreen && aRed > aBlue )
		return abBackground ? s_BackYellow : aRed >= 3 ? s_BoldYellow : s_DarkYellow;
	else if ( aRed == aBlue && aRed > aGreen )
		return abBackground ? s_BackMagenta : aRed >= 3 ? s_BoldMagenta : s_DarkMagenta;
	else if ( aGreen > aBlue )
		return abBackground ? s_BackGreen : aGreen >= 3 ? s_BoldGreen : s_DarkGreen;
	else if ( aGreen == aBlue )
		return abBackground ? s_BackCyan : aGreen >= 3 ? s_BoldCyan : s_DarkCyan;
	else /* aBlue is the highest */
		return abBackground ? s_BackBlue : aBlue >= 3 ? s_BoldBlue : s_DarkBlue;
}

static const char *GetRGBColour(bool_t abBackground, int aRed, int aGreen, int aBlue) {
	static char Result[16];
	int ColVal = 16 + (aRed * 36) + (aGreen * 6) + aBlue;
	sprintf(Result, "\033[%c8;5;%c%c%cm", 
		'3'+abBackground,	// Background
		'0'+(ColVal/100),	// Red
		'0'+((ColVal%100)/10),	// Green
		'0'+(ColVal%10)	// Blue
	);
	return Result;
}

static bool_t IsValidColour(const char *apArgument) {
	int i; /* Loop counter */

	/* The sequence is 4 bytes, but we can ignore anything after it. */
	if (apArgument == NULL || strlen(apArgument) < 4)
		return false;

	/* The first byte indicates foreground/background. */
	if (tolower(apArgument[0]) != 'f' && tolower(apArgument[0]) != 'b')
		return false;

	/* The remaining three bytes must each be in the range '0' to '5'. */
	for (i = 1; i <= 3; ++i) {
		if (apArgument[i] < '0' || apArgument[i] > '5') {
			return false;
		}
	}

	/* It's a valid foreground or background colour */
	return true;
}


/******************************************************************************
 Other local functions.
 *****************************************************************************/

static bool_t MatchString(const char *apFirst, const char *apSecond) {
	while (*apFirst && tolower(*apFirst) == tolower(*apSecond)) {
		++apFirst, ++apSecond;
	}
	return (!*apFirst && !*apSecond);
}

static bool_t PrefixString(const char *apPart, const char *apWhole) {
	while (*apPart && tolower(*apPart) == tolower(*apWhole)) {
		++apPart, ++apWhole;
	}
	return (!*apPart);
}

static bool_t IsNumber(const char *apString) {
	while (*apString && isdigit(*apString)) {
		++apString;
	}
	return (!*apString);
}

static char *AllocString(const char *apString) {
	char *pResult = NULL;

	if (apString != NULL) {
		int Size = strlen(apString);
		pResult = malloc(Size+1);
		if (pResult != NULL) {
			strcpy(pResult, apString);
		}
	}

	return pResult;
}
