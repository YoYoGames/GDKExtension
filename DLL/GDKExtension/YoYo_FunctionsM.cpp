//
// Copyright (C) 2020 Opera Norway AS. All rights reserved.
//
// This file is an original work developed by Opera.
//

#define _GAMING_XBOX

#include "pch.h"

// #include <collection.h>
#include <dxgi.h>
#include <inttypes.h>
#include <json-c/json.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <string.h>
#include <XGameUI.h>
#include <xsapi-c/services_c.h>
#include <xsapi-c/xbox_live_context_c.h>

#include "GDKX.h"
#include "UserManagement.h"
#include "SessionManagement.h"
#include "Stats.h"

extern int GetGamepadIndex(const APP_LOCAL_DEVICE_ID* _id);
extern APP_LOCAL_DEVICE_ID GetGamepadDeviceID(int _index);

#include <Windows.h>
#include <ppltasks.h>

#if !defined(WIN_UAP) && !defined(NO_SECURE_CONNECTION) && YY_CHAT
#include "Multiplayer/GameChat2IntegrationLayer.h"
#endif

// #include "GDKRunner/GDKRunner/Stats.h"

#define YOYO_ORIENTATION_LANDSCAPE_RIGHT		0x0001
#define YOYO_ORIENTATION_LANDSCAPE_LEFT         0x0002
#define YOYO_ORIENTATION_PORTRAIT_CORRECT       0x0004
#define	YOYO_ORIENTATION_PORTRAIT_UPSIDE_DOWN	0x0008

#define YOYO_ORIENTATION_IPHONE_LANDSCAPE       (YOYO_ORIENTATION_LANDSCAPE_RIGHT|YOYO_ORIENTATION_LANDSCAPE_LEFT)
#define YOYO_ORIENTATION_IPHONE_PORTRAIT        (YOYO_ORIENTATION_PORTRAIT_CORRECT|YOYO_ORIENTATION_PORTRAIT_UPSIDE_DOWN)
#define YOYO_ORIENTATION_IPAD_LANDSCAPE         (YOYO_ORIENTATION_LANDSCAPE_RIGHT|YOYO_ORIENTATION_LANDSCAPE_LEFT)
#define YOYO_ORIENTATION_IPAD_PORTRAIT          (YOYO_ORIENTATION_PORTRAIT_CORRECT|YOYO_ORIENTATION_PORTRAIT_UPSIDE_DOWN)


#define YOYO_ORIENTATION_PORTRAIT               (YOYO_ORIENTATION_PORTRAIT_CORRECT|YOYO_ORIENTATION_PORTRAIT_UPSIDE_DOWN)
#define YOYO_ORIENTATION_LANDSCAPE				(YOYO_ORIENTATION_LANDSCAPE_RIGHT|YOYO_ORIENTATION_LANDSCAPE_LEFT)

#define TICKS_PER_MILLISECOND                               10000
#define TICKS_PER_SECOND                                    10000000i64

/* SCID defined in options.ini */
extern char* g_XboxSCID;

// Whether or not IAP should use the MockIAP library
bool g_fIAPSandbox = false;

extern bool AreOnlineAchievementsAvailable( void );

extern bool		g_SocketInitDone;
extern	int		g_IDE_Version;

void F_XboxOnePackageCheckLicense(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg);
void F_XboxOneUserIsActive(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg);
void F_XboxOneUserIsGuest(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg);
void F_XboxOneUserIsSignedIn(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg);
void F_XboxOneUserIsSigningIn(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg);
void F_XboxOneUserIsRemote(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg);
void F_XboxOneGameDisplayNameForUser(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg);
void F_XboxOneAppDisplayNameForUser(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg);
void F_XboxOneGamerTagForUser(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg);
YYEXPORT void F_XboxOneUserIdForUser(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg);
void F_XboxOneGamerScoreForUser(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg);
void F_XboxOneReputationForUser(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg);
void F_XboxOneUserForPad(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg);
void F_XboxOnePadCountForUser(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg);
void F_XboxOneSponsorForUser(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg);
void F_XboxOneSpriteAddFromGamerPicture(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg);
void F_XboxOneShowProfileCardForUser(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg);

void F_XboxOneGenerateSessionId(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg);

void F_XboxOneSetServiceConfigurationID(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg);

void F_XboxLiveNotAvailable(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg);


static uint64 g_stats_user_id;
static char* g_stats_service_config = NULL;
static uint32 g_stats_title_id;

int	g_PictureSprite=-1;

extern bool g_chatIntegrationLayerInited;
extern bool g_XboxOneGameChatEnabled;

// #############################################################################################
/// Function:<summary>
///             Run a per-frame update platform specific YoYo functions
///          </summary>
// #############################################################################################
void UpdateYoYoFunctionsM(void)
{
#if !defined(WIN_UAP) && !defined(NO_SECURE_CONNECTION) && YY_CHAT
	// Update GameChat
	if(g_XboxOneGameChatEnabled && g_chatIntegrationLayerInited)
	{
		GameChat2IntegrationLayer::Get()->Update();
	}
#endif
}

// #############################################################################################
/// Function:<summary>
///             Quit the platform specific YoYo functions
///          </summary>
// #############################################################################################
void QuitYoYoFunctionsM( void )
{
}

// #############################################################################################
/// Function:<summary>
///             Initialise the built in YoYo games functions
///          </summary>
// #############################################################################################
void InitYoYoFunctionsM( void )
{
#if 0
	Function_Add( "xboxone_user_is_active", F_XboxOneUserIsActive, 1, true);
	Function_Add( "xboxone_user_is_guest", F_XboxOneUserIsGuest, 1, true);
	Function_Add( "xboxone_user_is_signed_in", F_XboxOneUserIsSignedIn, 1, true);
	Function_Add( "xboxone_user_is_remote", F_XboxOneUserIsRemote, 1, true);
	Function_Add( "xboxone_gamedisplayname_for_user", F_XboxOneGameDisplayNameForUser, 1, true);
	Function_Add( "xboxone_appdisplayname_for_user", F_XboxOneAppDisplayNameForUser, 1, true);
	Function_Add( "xboxone_user_id_for_user", F_XboxOneUserIdForUser, 1, true);
	Function_Add( "xboxone_agegroup_for_user", F_XboxOneAgeGroupForUser, 1, true);
	Function_Add( "xboxone_gamerscore_for_user", F_XboxOneGamerScoreForUser, 1, true);
	Function_Add( "xboxone_reputation_for_user", F_XboxOneReputationForUser, 1, true);
	Function_Add( "xboxone_user_for_pad", F_XboxOneUserForPad, 1, true);	
	Function_Add( "xboxone_pad_count_for_user", F_XboxOnePadCountForUser, 1, true);
	Function_Add( "xboxone_pad_for_user", F_XboxOnePadForUser, 2, true);	
	Function_Add( "xboxone_sponsor_for_user", F_XboxOneSponsorForUser, 2, true);	
	Function_Add( "xboxone_show_account_picker", F_XboxOneShowAccountPicker, 2, true);	
	Function_Add( "xboxone_sprite_add_from_gamerpicture", F_XboxOneSpriteAddFromGamerPicture, 4, true);
	Function_Add( "xboxone_show_profile_card_for_user", F_XboxOneShowProfileCardForUser, 2, true);

	Function_Add( "xboxone_generate_player_session_id", F_XboxOneGenerateSessionId, 0, true);

	Function_Add( "xboxone_set_savedata_user", F_XboxOneSetSaveDataUser, 1, true);
	Function_Add( "xboxone_get_savedata_user", F_XboxOneGetSaveDataUser, 0, true);
	Function_Add( "xboxone_get_file_error", F_XboxOneGetFileError, 0, true);

	Function_Add( "xboxone_set_service_configuration_id", F_XboxOneSetServiceConfigurationID, 1, true);
#endif
}

// #############################################################################################
/// Function:<summary>
///             Initialise the platform level constants
///          </summary>
// #############################################################################################
void	InitYoYoConstantsM( void )
{
#if 0
	AddRealConstant("xboxone_fileerror_noerror", 0);
	AddRealConstant("xboxone_fileerror_outofmemory", 1);
	AddRealConstant("xboxone_fileerror_usernotfound", 2);
	AddRealConstant("xboxone_fileerror_unknownerror", 3);
	AddRealConstant("xboxone_fileerror_cantopenfile", 4);
	AddRealConstant("xboxone_fileerror_blobnotfound", 5);
	AddRealConstant("xboxone_fileerror_containernotinsync", 6);
	AddRealConstant("xboxone_fileerror_containersyncfailed", 7);
	AddRealConstant("xboxone_fileerror_invalidcontainername", 8);
	AddRealConstant("xboxone_fileerror_noaccess", 9);
	AddRealConstant("xboxone_fileerror_noxboxliveinfo", 10);
	AddRealConstant("xboxone_fileerror_outoflocalstorage", 11);
	AddRealConstant("xboxone_fileerror_providedbuffertoosmall", 12);
	AddRealConstant("xboxone_fileerror_quotaexceeded", 13);
	AddRealConstant("xboxone_fileerror_updatetoobig", 14);
	AddRealConstant("xboxone_fileerror_usercanceled", 15);

	AddRealConstant("xboxone_gamerpic_small", 1);
    AddRealConstant("xboxone_gamerpic_medium", 2);
    AddRealConstant("xboxone_gamerpic_large", 3);

	AddRealConstant("xboxone_agegroup_unknown", 0);
	AddRealConstant("xboxone_agegroup_child", 1);
    AddRealConstant("xboxone_agegroup_teen", 2);
    AddRealConstant("xboxone_agegroup_adult", 3);

	extern void InitMatchmakingConstantsM();
	InitMatchmakingConstantsM();

	// Aliases
	AddRealConstant("xboxlive_fileerror_noerror", 0);
	AddRealConstant("xboxlive_fileerror_outofmemory", 1);
	AddRealConstant("xboxlive_fileerror_usernotfound", 2);
	AddRealConstant("xboxlive_fileerror_unknownerror", 3);
	AddRealConstant("xboxlive_fileerror_cantopenfile", 4);
	AddRealConstant("xboxlive_fileerror_blobnotfound", 5);
	AddRealConstant("xboxlive_fileerror_containernotinsync", 6);
	AddRealConstant("xboxlive_fileerror_containersyncfailed", 7);
	AddRealConstant("xboxlive_fileerror_invalidcontainername", 8);
	AddRealConstant("xboxlive_fileerror_noaccess", 9);
	AddRealConstant("xboxlive_fileerror_noxboxliveinfo", 10);
	AddRealConstant("xboxlive_fileerror_outoflocalstorage", 11);
	AddRealConstant("xboxlive_fileerror_providedbuffertoosmall", 12);
	AddRealConstant("xboxlive_fileerror_quotaexceeded", 13);
	AddRealConstant("xboxlive_fileerror_updatetoobig", 14);
	AddRealConstant("xboxlive_fileerror_usercanceled", 15);

	AddRealConstant("xboxlive_gamerpic_small", 1);
    AddRealConstant("xboxlive_gamerpic_medium", 2);
    AddRealConstant("xboxlive_gamerpic_large", 3);

	AddRealConstant("xboxlive_agegroup_unknown", 0);
	AddRealConstant("xboxlive_agegroup_child", 1);
    AddRealConstant("xboxlive_agegroup_teen", 2);
    AddRealConstant("xboxlive_agegroup_adult", 3);

	AddRealConstant("xboxlive_visibility_open", 1);
	AddRealConstant("xboxlive_visibility_private", 2);

	AddRealConstant("xboxlive_match_visibility_usetemplate", 0);
    AddRealConstant("xboxlive_match_visibility_open", 1);
    AddRealConstant("xboxlive_match_visibility_private", 2);

	AddRealConstant("xboxlive_chat_relationship_none", 0);
	AddRealConstant("xboxlive_chat_relationship_receive_all", 24);
	AddRealConstant("xboxlive_chat_relationship_receive_audio", 8);
	AddRealConstant("xboxlive_chat_relationship_receive_text", 16);
	AddRealConstant("xboxlive_chat_relationship_send_all", 7);
	AddRealConstant("xboxlive_chat_relationship_send_and_receive_all", 31);
	AddRealConstant("xboxlive_chat_relationship_send_microphone_audio", 1);
	AddRealConstant("xboxlive_chat_relationship_send_text", 4);
	AddRealConstant("xboxlive_chat_relationship_send_text_to_speech_audio", 2);
#endif
}


extern int g_GUI_Width;
extern int g_GUI_Height;

extern float g_DisplayScaleX;
extern float g_DisplayScaleY;

// ********************************************************************
// Buy button has been pressed
// ********************************************************************


void YoYo_OF_StartDashboard( void ){}
void YoYo_OF_AddAchievement( const char* _pId, const char* _pOFId ){}
void YoYo_OF_AddLeaderboard( const char* _pId, const char* _pOFId, eLeaderboardType _type ){}
void YoYo_OF_SendChallenge( const char* _pDefinition, const char* _pText, const char* _pData ){}
void YoYo_OF_SendInvite( const char* _pDefinition ){}
void YoYo_OF_SendSocial( const char* _pText, const char* _pSuggestedMessage, const char* _pImageName ){}
void YoYo_OF_SetURL( const char* _pURL ){}


// #############################################################################################
/// Function:<summary>
///             Check to see is online achievements are even available....
///          </summary>
///
/// Out:	 <returns>
///				TRUE for yes, FALSE for no.
///			 </returns>
// #############################################################################################
bool AreOnlineAchievementsAvailable( void )
{
	return false;
}


// #############################################################################################
/// Function:<summary>
///             
///          </summary>
// #############################################################################################
void YoYo_ScoreAchievement( const char* _pDestName, unsigned int* _pValue )
{

}

void	
YoYo_IncrementAchievement( const char* _pDestName, unsigned int* _pValue )
{
}
// #############################################################################################
/// Function:<summary>
///             Login to GameCenter
///          </summary>
// #############################################################################################
void	Achievement_Login( void )
{
}
int Achievement_LoginStatus(){return 0;}

// #############################################################################################
/// Function:<summary>
///             Logout of GameCenter
///          </summary>
// #############################################################################################
void	Achievement_Logout( void )
{
}

// #############################################################################################
/// Function:<summary>
///             Logout of GameCenter
///          </summary>
// #############################################################################################
void	YoYo_CacheAchievement( const char* _pDestName, unsigned int* _pValue )
{

}


// #############################################################################################
/// Function:<summary>
///             Post a high score to game center
///          </summary>
///
/// In:		 <param name="_pGroup">Group to post to</param>
///			 <param name="_Value">The value to post</param>
///				
// #############################################################################################
bool	Achievement_PostHiScore( eCacheType _type, const char * _pGroup, int  _Value )
{	
	DebugConsoleOutput( "Post High Score %s\n", _pGroup );
	return false;	// cant sent coms, so error
}



// #############################################################################################
/// Function:<summary>
///             Post progress on an achievement
///          </summary>
///
/// In:		 <param name="_pAchievementName">Name of achievement</param>
///			 <param name="_PercentageDone">New percentage done (can't go down)</param>
///				
// #############################################################################################
bool Achievement_PostAchievement( eCacheType _type, const char* _pAchievementName, float _PercentageDone )
{
	DebugConsoleOutput( "Post Achievement %s\n", _pAchievementName );
	return false;
}




char* YoYo_OF_AcceptChallenge( void )
{
	return (char*)YYStrDup( "" );
}

void YoYo_OF_SendChallengeResult( eChallengeResult _resultEnum, const char* _description )
{
}

double	LeaveRating(char* _pTextString, const char* _pYES, const char* _pNO, const char* _pURL )
{
	return 0.0;
}

void Achievement_Show_Achievement( void )
{
	XUM_LOCK_MUTEX;

	int size;
	XUMuser** users = XUM::GetUsers(size);

	if (size == 0)
	{
		DebugConsoleOutput("Achievement_Show_Achievement called without any users");
		return;
	}

	XUMuser* user = NULL;

	for (int i = 0; i < size; i++)
	{
		user = users[i];
		if (user->XboxUserIdInt == g_stats_user_id)
		{
			break;
		}
	}

	static XAsyncBlock emptyBlock{ nullptr, nullptr, nullptr };
	XGameUiShowAchievementsAsync(&emptyBlock, user->user, g_stats_title_id);
}

void	Achievement_Show_Leaderboard( void ) { } // end Achievement_Show_Leaderboard 
void	Achievement_Show_Challenge_Notifications(int a,int b,int c){};
void	Achievement_Get_Challenges(){};
void	Achievement_Send_Challenge(char *to, char *challengeid,int score, int type, char *msg){};

static void DoLeaderboardQuery(const char* leaderboard_or_stat_name, uint64 base_user_id, int base_idx, int num_items, int friend_filter, const char* func_name)
{
	assert(base_user_id == 0 || base_idx == 0);

	if (g_stats_service_config == NULL)
	{
		DebugConsoleOutput("%s - No SCID set (did you forget to call xboxone_stats_setup?)\n", func_name);
		return;
	}

	XblLeaderboardQuery query;
	memset(&query, 0, sizeof(query));

	switch (friend_filter)
	{
	case e_achievement_filter_all_players:
		query.leaderboardName = leaderboard_or_stat_name;
		query.socialGroup = XblSocialGroupType::None;
		query.order = XblLeaderboardSortOrder::Ascending;
		break;

	case e_achievement_filter_friends_only:
		query.statName = leaderboard_or_stat_name;
		query.socialGroup = XblSocialGroupType::People;
		query.order = XblLeaderboardSortOrder::Ascending;
		break;

	case e_achievement_filter_friends_alt:
		query.statName = leaderboard_or_stat_name;
		query.socialGroup = XblSocialGroupType::People;
		query.order = XblLeaderboardSortOrder::Descending;
		break;

	case e_achievement_filter_favorites_only:
		query.statName = leaderboard_or_stat_name;
		query.socialGroup = XblSocialGroupType::Favorites;
		query.order = XblLeaderboardSortOrder::Ascending;
		break;

	case e_achievement_filter_favorites_alt:
		query.statName = leaderboard_or_stat_name;
		query.socialGroup = XblSocialGroupType::Favorites;
		query.order = XblLeaderboardSortOrder::Descending;
		break;

	default:
		DebugConsoleOutput("%s - unsupported filter value\n", func_name);
		return;
	}

	XblContextHandle xbl_ctx;

	{
		XUM_LOCK_MUTEX;
		XUMuser* xuser = XUM::GetUserFromId(g_stats_user_id);
		if (xuser == NULL)
		{
			DebugConsoleOutput("%s - couldn't get user\n", func_name);
			return;
		}

		HRESULT hr = XblContextCreateHandle(xuser->user, &xbl_ctx);
		if (!SUCCEEDED(hr))
		{
			DebugConsoleOutput("%s - couldn't create xbl handle (HRESULT 0x%08X)\n", func_name, (unsigned)(hr));
			return;
		}
	}

	struct GetLeaderboardContext
	{
		char* leaderboard_or_stat_name;
		const char* func_name;
		XblContextHandle xbl_ctx;

		XAsyncBlock async;

		GetLeaderboardContext(const char* leaderboard_or_stat_name, const char* func_name, XblContextHandle xbl_ctx) :
			leaderboard_or_stat_name((char*)(YYStrDup(leaderboard_or_stat_name))),
			func_name(func_name),
			xbl_ctx(xbl_ctx) {}

		~GetLeaderboardContext()
		{
			XblContextCloseHandle(xbl_ctx);
			YYFree(leaderboard_or_stat_name);
		};
	};

	GetLeaderboardContext* ctx = new GetLeaderboardContext(leaderboard_or_stat_name, func_name, xbl_ctx);

	ctx->async.queue = NULL;
	ctx->async.context = ctx;
	ctx->async.callback = [](XAsyncBlock* async)
	{
		GetLeaderboardContext* ctx = (GetLeaderboardContext*)(async->context);

		/* Destroy context when callback returns. */
		std::unique_ptr<GetLeaderboardContext> ctx_guard(ctx);

		size_t result_size = -1;
		void* result_buf = NULL;
		XblLeaderboardResult* result = NULL;

		HRESULT status = XblLeaderboardGetLeaderboardResultSize(async, &result_size);
		if (SUCCEEDED(status))
		{
			result_buf = YYAlloc((int)result_size);
			status = XblLeaderboardGetLeaderboardResult(async, result_size, result_buf, &result, NULL);
		}

		if (!SUCCEEDED(status))
		{
			DebugConsoleOutput("%s - error fetching leaderboard (HRESULT 0x%08X)\n", ctx->func_name, (unsigned)(status));
		}

		int dsMapIndex = CreateDsMap(1,
			"id", (double)e_achievement_leaderboard_info, (void*)(NULL));

		DsMapAddDouble(dsMapIndex, "error", status);

		if (SUCCEEDED(status))
		{
			DsMapAddString(dsMapIndex, "leaderboardid", ctx->leaderboard_or_stat_name);
			DsMapAddInt64(dsMapIndex, "numentries", result->rowsCount);

			for (size_t row_idx = 0; row_idx < result->rowsCount; ++row_idx)
			{
				char key[32];

				snprintf(key, sizeof(key), "Player%zu", row_idx);
				DsMapAddString(dsMapIndex, key, result->rows[row_idx].gamertag);

				snprintf(key, sizeof(key), "Playerid%zu", row_idx);
				DsMapAddInt64(dsMapIndex, key, result->rows[row_idx].xboxUserId);

				snprintf(key, sizeof(key), "Rank%zu", row_idx);
				DsMapAddDouble(dsMapIndex, key, result->rows[row_idx].rank);

				/* Concatenate column ("stat") values together, seperated by spaces. */

				char values[2048] = { '\0' };
				size_t values_off = 0;

				for (size_t col_idx = 0;
					col_idx < result->rows[row_idx].columnValuesCount
					&& values_off < sizeof(values);
					++col_idx)
				{
					values_off += snprintf((values + values_off), (sizeof(values) - values_off), "%s%s",
						(col_idx > 0 ? " " : ""),
						result->rows[row_idx].columnValues[col_idx]);
				}

				snprintf(key, sizeof(key), "Score%zu", row_idx);
				DsMapAddString(dsMapIndex, key, values);
			}
		}

		CreateAsyncEventWithDSMap(dsMapIndex, EVENT_OTHER_SOCIAL);

		YYFree(result_buf);
	};

	snprintf(query.scid, sizeof(query.scid), "%s", g_stats_service_config);
	query.xboxUserId = g_stats_user_id;
	query.maxItems = num_items;
	query.skipToXboxUserId = base_user_id;
	query.skipResultToRank = base_idx;

	HRESULT status = XblLeaderboardGetLeaderboardAsync(xbl_ctx, query, &(ctx->async));
	if (!SUCCEEDED(status))
	{
		DebugConsoleOutput("%s - error fetching leaderboard (HRESULT 0x%08X)\n", func_name, (unsigned)(status));
		delete ctx;
	}
}

YYEXPORT
void F_XboxReadPlayerLeaderboard(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg)
{
	const char* ident = YYGetString(arg, 0);
	uint64 player = (uint64)(YYGetInt64(arg, 1));
	int numitems = YYGetInt32(arg, 2);
	int friendfilter = YYGetInt32(arg, 3);

	DoLeaderboardQuery(ident, player, 0, numitems, friendfilter, "xboxone_read_player_leaderboard");
}

void Achievement_Load_Leaderboard(char *ident,int minindex, int maxindex,int friendfilter)
{
	DoLeaderboardQuery(ident, 0, minindex, ((maxindex - minindex) + 1), friendfilter, "achievement_load_leaderboard");
}

void	Achievement_Load_Pic(char *id){};

// ------------------------------------------------------------

void	Achievement_Load_Friends(void)
{
	XblContextHandle xbl_ctx;

	{
		XUM_LOCK_MUTEX;

		XUserLocalId activating_user_id = XUM::GetActivatingUser();
		XUMuser* activating_user = XUM::GetUserFromId(g_stats_user_id);

		if (activating_user == NULL)
		{
			DebugConsoleOutput("achievement_load_friends - couldn't get stats user\n");
			return;
		}

		HRESULT hr = XblContextCreateHandle(activating_user->user, &xbl_ctx);
		if (!SUCCEEDED(hr))
		{
			DebugConsoleOutput("achievement_load_friends - couldn't create xbl handle (HRESULT 0x%08X)\n", (unsigned)(hr));
			return;
		}
	}

	struct LoadFriendsContext
	{
		XblContextHandle xbl_ctx;
		XAsyncBlock async;

		LoadFriendsContext(XblContextHandle xbl_ctx) :
			xbl_ctx(xbl_ctx) {}

		~LoadFriendsContext()
		{
			XblContextCloseHandle(xbl_ctx);
		}
	};

	LoadFriendsContext* ctx = new LoadFriendsContext(xbl_ctx);

	ctx->async.queue = NULL;
	ctx->async.context = ctx;
	ctx->async.callback = [](XAsyncBlock* async)
	{
		LoadFriendsContext* ctx = (LoadFriendsContext*)(async->context);

		/* Destroy context when callback returns. */
		std::unique_ptr<LoadFriendsContext> ctx_guard(ctx);

		int dsMapIndex;

		{
			dsMapIndex = CreateDsMap(1,
				"id", (double)e_achievement_friends_info, NULL);

			XblSocialRelationshipResultHandle result_handle;
			HRESULT status = XblSocialGetSocialRelationshipsResult(async, &result_handle);
			if (SUCCEEDED(status))
			{
				const XblSocialRelationship* results;
				size_t n_results;

				status = XblSocialRelationshipResultGetRelationships(result_handle, &results, &n_results);
				if (SUCCEEDED(status))
				{
					DsMapAddDouble(dsMapIndex, "succeeded", 1);
					DsMapAddDouble(dsMapIndex, "numfriends", (double)n_results);

					for (size_t results_idx = 0; results_idx < n_results; ++results_idx)
					{
						char key[32];
						snprintf(key, sizeof(key), "Friendid%zu", results_idx);

						DsMapAddInt64(dsMapIndex, key, results[results_idx].xboxUserId);
					}
				}
				else {
					DebugConsoleOutput("achievement_load_friends - couldn't read results (HRESULT 0x%08X)\n", (unsigned)(status));
					DsMapAddDouble(dsMapIndex, "succeeded", 0);
				}

				XblSocialRelationshipResultCloseHandle(result_handle);
			}
			else {
				DebugConsoleOutput("achievement_load_friends - couldn't get social relationships (HRESULT 0x%08X)\n", (unsigned)(status));
				DsMapAddDouble(dsMapIndex, "succeeded", 0);
			}
		}

		CreateAsyncEventWithDSMap(dsMapIndex, EVENT_OTHER_SOCIAL);
	};

	HRESULT status = XblSocialGetSocialRelationshipsAsync(xbl_ctx, g_stats_user_id, XblSocialRelationshipFilter::All, 0, 0, &(ctx->async));
	if (!SUCCEEDED(status))
	{
		DebugConsoleOutput("achievement_load_friends - couldn't get social relationships (HRESULT 0x%08X)\n", (unsigned)(status));

		delete ctx;
		return;
	}

}

// ------------------------------------------------------------

#if 0
void	Achievement_Load_Progress(void){};
void	Achievement_Reset(void){};
char*	Achievement_PlayerID(void) { return "player";};
void	Achievement_Get_Info(char *id){}
void	Achievement_Event(char *id){}
void	Achievement_Show(int type,char *optarg, int numarg){}
#endif

char *utf8_replace(char *original, char *replacement, char *haystack)
{
	size_t len_replacement = strlen(replacement);
	size_t len_original= strlen(original);
	size_t string_len = strlen(haystack);

	int32_t diff = (int32_t) (len_replacement - len_original);

	int num_replaces = 0;

	char *ptr = haystack;
	while (ptr = strstr(ptr, original))
	{
		ptr += len_original;
		num_replaces++;
	}

	char *new_string = (char*)YYAlloc((int)((string_len + (num_replaces * diff) + 1) * sizeof(char)) );

	if (num_replaces == 0)
	{
		strcpy(new_string, haystack);
		return new_string;
	}

	char * lastpos = haystack;
	char * pos = haystack;
	char * outpos = new_string;
	while (pos = strstr(pos, original))
	{
		size_t num_chars = pos - lastpos;

		memcpy(outpos, haystack, num_chars);

		memcpy(outpos + num_chars, replacement, len_replacement);

		lastpos = pos = pos + len_original;
		outpos = outpos + (num_chars + len_replacement);
	}

	memcpy(outpos, lastpos, (haystack + string_len) - lastpos);

	return new_string;
}

void F_XboxOneGameDisplayNameForUser(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg)
{
	uint64 user_id = (uint64)YYGetInt64(arg, 0);

	XUM_LOCK_MUTEX
	XUMuser* user = XUM::GetUserFromId(user_id);

	if (user != NULL)
	{
		YYCreateString(&Result, user->DisplayName);
	}
	else {
		// May be a remote user
		const char* gamerTag = NULL;
		gamerTag = XSM::GetSessionUserGamerTagFromID(user_id);
		if (gamerTag == NULL)
		{
			DebugConsoleOutput("xboxone_gamedisplayname_for_user() - user not found\n");
			YYCreateString(&Result, "");
		}
		else
		{
			YYCreateString(&Result, gamerTag);
			YYFree(gamerTag);
		}
	}
}

void F_XboxOneAppDisplayNameForUser(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg)
{
#ifdef WIN_UAP	
#if defined XBOX_SERVICES
	XUM_LOCK_MUTEX

		auto users = XUM::GetUsers();
	int size = users->Size;
	if (size > 0)
	{
		XUMuser^ user = users->GetAt(0);

		char* tag = ConvertFromWideCharToUTF8((wchar_t*)(user->AppDisplayName->Data()));
		char *new_tag = utf8_replace("#", "\\#", tag);
		YYFree(tag);

		YYCreateString(&Result, new_tag);
		YYFree(new_tag);

		return;
	}
	else
	{
		YYCreateString(&Result, "");
		DebugConsoleOutput("xboxlive_appdisplayname_for_user() - user not found", false);
	}
#endif
#else	
#if defined XBOX_SERVICES
	XUM_LOCK_MUTEX
	//auto users = Windows::Xbox::System::User::Users;
	auto users = XUM::GetUsers();	
	int size = users->Size;

	uint64 id = (uint64)(YYGetInt64( arg, 0)); 

	for(int i = 0 ; i < size; i++)
	{
		auto user = users->GetAt(i);
		if (user->XboxUserIdInt == id)
		{
			char* tag = ConvertFromWideCharToUTF8((wchar_t*)(user->AppDisplayName->Data()));
			YYCreateString( &Result, tag );
			YYFree(tag);
			
			return;
		}
	}
#endif
	YYCreateString( &Result, "" );
	DebugConsoleOutput("xboxone_appdisplayname_for_user() - user not found", false);	
#endif
}

void F_XboxOneGamerTagForUser(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg)
{
#ifdef WIN_UAP	
#if defined XBOX_SERVICES
	XUM_LOCK_MUTEX

		auto users = XUM::GetUsers();
	int size = users->Size;
	if (size > 0)
	{
		XUMuser^ user = users->GetAt(0);

		char* tag = ConvertFromWideCharToUTF8((wchar_t*)(user->GameDisplayName->Data()));
		char *new_tag = utf8_replace("#", "\\#", tag);
		YYFree(tag);

		YYCreateString(&Result, new_tag);
		YYFree(new_tag);

		return;
	}
	else
	{
		YYCreateString(&Result, "");
		DebugConsoleOutput("xboxlive_gamertag_for_user() - user not found", false);
	}
#endif
#else

#if defined XBOX_SERVICES
	XUM_LOCK_MUTEX
		//auto users = Windows::Xbox::System::User::Users;
		auto users = XUM::GetUsers();
	int size = users->Size;

	uint64 id = (uint64)(YYGetInt64(arg, 0));

	for (int i = 0; i < size; i++)
	{
		auto user = users->GetAt(i);
		if (user->XboxUserIdInt == id)
		{
			char* tag = ConvertFromWideCharToUTF8((wchar_t*)(user->GameDisplayName->Data()));
			YYCreateString(&Result, tag);
			YYFree(tag);

			return;
		}
	}

#ifdef NOV_XDK
	// Could be a multiplayer user
	// Check our current session list and see if we have a user matching this ID in it somewhere
	// For multiplayer users this'll be the gamertag rather than the game display name
	Platform::String^ xboxUserID = ui64tostring(id);

	XSM_LOCK_MUTEX
	Windows::Foundation::Collections::IVector<XSMsession^> ^sessions = XSM::GetSessions();
	int numsessions = sessions->Size;
	for (int i = 0; i < numsessions; i++)
	{
		XSMsession ^sess = sessions->GetAt(i);
		if ((sess->session != nullptr) && (sess->session->Members != nullptr))
		{
			int nummembers = sess->session->Members->Size;
			for (int j = 0; j < nummembers; j++)
			{
				Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionMember^ member = sess->session->Members->GetAt(j);
				if (member->XboxUserId->Equals(xboxUserID))
				{
					// Yay!
					char* tag = ConvertFromWideCharToUTF8((wchar_t*)(member->Gamertag->Data()));
					YYCreateString(&Result, tag);
					YYFree(tag);
					return;
				}
			}
		}
	}
#endif
#endif
	YYCreateString(&Result, "");
	DebugConsoleOutput("xboxlive_gamertag_for_user() - user not found", false);
#endif
}

YYEXPORT
void F_XboxOneUserIdForUser(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg)
{
	int numusers = 0;
	XUMuser** users = XUM::GetUsers(numusers);

	uint64 id = (uint64)YYGetInt64(arg, 0);

	for (int i = 0; i < numusers; i++)
	{
		XUMuser* user = users[i];
		
		if (user->XboxUserIdInt == id)
		{
			YYCreateString(&Result, std::to_string(user->XboxUserIdInt).c_str());
			return;
		}
	}

	YYCreateString( &Result, "" );
	DebugConsoleOutput("xboxone_user_id_for_user() - user not found\n");		
}

void F_XboxOneGamerScoreForUser(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg)
{
	Result.kind = VALUE_REAL;
	Result.val = -1;
	
	uint64 user_id = (uint64)YYGetInt64(arg, 0);

	XUM_LOCK_MUTEX
	XUMuser* user = XUM::GetUserFromId(user_id);

	if (user != NULL)
	{
		Result.val = (double)(user->GamerScore);
		return;
	}

	DebugConsoleOutput("xboxone_gamerscore_for_user() - user not found", false);		
}

void F_XboxOneReputationForUser(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg)
{
	Result.kind = VALUE_REAL;
	Result.val = -1;
	
#if defined XBOX_SERVICES
	XUM_LOCK_MUTEX
	//auto users = Windows::Xbox::System::User::Users;
	auto users = XUM::GetUsers();	
	int size = users->Size;

	uint64 id = (uint64)YYGetInt64(arg, 0); // don't do any rounding on this

	for(int i = 0 ; i < size; i++)
	{
		auto user = users->GetAt(i);
		if (user->XboxUserIdInt == id)
		{
			Result.val = (double)(user->Reputation);
			return;
		}
	}
#endif
	DebugConsoleOutput("xboxone_reputation_for_user() - user not found", false);		
}

void F_XboxOneUserForPad(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg)
{
	Result.kind = VALUE_INT64;
	Result.v64 = 0;
	
	int index = YYGetInt32(arg, 0);

	APP_LOCAL_DEVICE_ID devid; // = GetGamepadDeviceID(index); TODO

	if (memcmp(&devid, &XUserNullDeviceId, sizeof(APP_LOCAL_DEVICE_ID)) != 0)		// not a NULL device ID
	{
		int numusers = 0;
		XUMuser** users = XUM::GetUsers(numusers);

		for (int i = 0; i < numusers; i++)
		{
			XUMuser* user = users[i];
			if (user != NULL)
			{				
				for (size_t j = 0; j < user->Controllers.size(); j++)
				{					
					if (memcmp(&(user->Controllers[j]), &devid, sizeof(APP_LOCAL_DEVICE_ID)) == 0)
					{
						// Found it
						Result.v64 = user->XboxUserIdInt;
						return;
					}
				}
			}
		}
	}
}

void F_XboxOnePadCountForUser(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg)
{
	Result.kind = VALUE_REAL;
	Result.val = 0;	
	
	XUM_LOCK_MUTEX;

	int numusers = 0;
	XUMuser** users = XUM::GetUsers(numusers);

	uint64 id = (uint64)YYGetInt64(arg, 0);

	for (int i = 0; i < numusers; i++)
	{
		XUMuser* user = users[i];
		if (user != NULL)
		{
			if (user->XboxUserIdInt == id)
			{
				Result.val = (double)user->Controllers.size();
				return;
			}
		}
	}
}

void F_XboxOneUserIsGuest(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg)
{
	Result.kind = VALUE_REAL;
	Result.val = 0;	

	uint64 user_id = (uint64)YYGetInt64(arg, 0);

	XUM_LOCK_MUTEX
	XUMuser* user = XUM::GetUserFromId(user_id);

	if (user != NULL)
	{
		Result.val = user->IsGuest;
	}
	else {
		DebugConsoleOutput("xboxone_user_is_guest() - user not found\n");
	}

	// No valid user found	
	return;	
}

void F_XboxOneUserIsSignedIn(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg)
{
	Result.kind = VALUE_REAL;
	Result.val = 0;	

	uint64 user_id = (uint64)YYGetInt64(arg, 0);

	XUM_LOCK_MUTEX
	XUMuser* user = XUM::GetUserFromId(user_id);

	if (user != NULL)
	{
		Result.val = user->IsSignedIn;
	}
	else {
		DebugConsoleOutput("xboxone_user_is_signed_in() - user not found\n");
	}
}

void F_XboxOneSpriteAddFromGamerPicture(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg)
{
	Result.kind = VALUE_REAL;
	Result.val = -1;	

	XblContextHandle xbl_ctx;

	{
		XUM_LOCK_MUTEX;

		XUserLocalId activating_user_id = XUM::GetActivatingUser();
		XUMuser* activating_user = XUM::GetUserFromLocalId(activating_user_id);

		if (activating_user == NULL)
		{
			DebugConsoleOutput("xboxone_sprite_add_from_gamerpicture() - couldn't get activating user\n");
			return;
		}

		HRESULT hr = XblContextCreateHandle(activating_user->user, &xbl_ctx);
		if (!SUCCEEDED(hr))
		{
			DebugConsoleOutput("xboxone_sprite_add_from_gamerpicture() - couldn't create xbl handle (HRESULT 0x%08X)\n", (unsigned)(hr));
			return;
		}
	}

	uint64 user_id = (uint64)YYGetInt64(arg, 0);

	int imagesize = YYGetInt32(arg, 1);
	int xorig = YYGetInt32(arg, 2);
	int yorig = YYGetInt32(arg, 3);

	// Right, need to kind-of mirror what Sprite_Add does, in terms of async events and what-not
	// Looks like we need to create the sprite stuff here too so we can return the correct index
	// Set up the new sprite details

	int sprite_idx = -1;
	HSPRITEASYNC pAS = CreateSpriteAsync(&sprite_idx, xorig, yorig, 0, 0);

	struct SpriteAddFromGamerPictureContext
	{
		uint64_t user_id;
		int imagesize;
		int xorig;
		int yorig;

		XblContextHandle xbl_ctx;

		int sprite_idx;
		HSPRITEASYNC pAS;

		SpriteAddFromGamerPictureContext(uint64_t user_id, int imagesize, int xorig, int yorig, XblContextHandle xbl_ctx, int sprite_idx, HSPRITEASYNC pAS) :
			user_id(user_id), imagesize(imagesize), xorig(xorig), yorig(yorig), xbl_ctx(xbl_ctx), sprite_idx(sprite_idx), pAS(pAS) {}

		~SpriteAddFromGamerPictureContext()
		{
			XblContextCloseHandle(xbl_ctx);
		}
	};

	SpriteAddFromGamerPictureContext* ctx = new SpriteAddFromGamerPictureContext(user_id, imagesize, xorig, yorig, xbl_ctx, sprite_idx, pAS);

	XAsyncBlock* async = new XAsyncBlock;
	async->queue = NULL;
	async->context = ctx;
	async->callback = [](XAsyncBlock* async)
	{
		SpriteAddFromGamerPictureContext* ctx = (SpriteAddFromGamerPictureContext*)(async->context);

		{
			std::unique_ptr<XblUserProfile> profile = std::make_unique<XblUserProfile>();

			HRESULT hr = XblProfileGetUserProfileResult(async, profile.get());
			if (SUCCEEDED(hr))
			{
				std::string pic_uri = profile->gameDisplayPictureResizeUri;
				pic_uri += "&format=png";

				switch (ctx->imagesize)
				{
				case 3: pic_uri += "&w=424&h=424"; break;
				case 2: pic_uri += "&w=208&h=208"; break;
				case 1:
				case 0:
				default: pic_uri += "&w=64&h=64"; break;
				}

				// Use standard HTTP get
				HTTP_Get(pic_uri.c_str(), ARG_SPRITE, g_pYYRunnerInterface->ASYNCFunc_SpriteAdd, g_pYYRunnerInterface->ASYNCFunc_SpriteCleanup, ctx->pAS);
			}
			else {
				DebugConsoleOutput("xboxone_sprite_add_from_gamerpicture() - XblProfileGetUserProfileAsync failed (HRESULT 0x%08X)\n", (unsigned)(hr));

				int dsmap = CreateDsMap(4,
					"filename", 0.0, "gamerpicture",
					"id", (double)(ctx->sprite_idx), (void*)NULL,
					"http_status", 0.0, (void*)NULL,
					"status", -1.0, (void*)NULL);

				CreateAsyncEventWithDSMap(dsmap, EVENT_OTHER_WEB_IMAGE_LOAD);
			}
		}

		delete ctx;
		delete async;
	};

	HRESULT hr = XblProfileGetUserProfileAsync(ctx->xbl_ctx, user_id, async);
	if (!SUCCEEDED(hr))
	{
		DebugConsoleOutput("xboxone_sprite_add_from_gamerpicture() - XblProfileGetUserProfileAsync failed (HRESULT 0x%08X)\n", (unsigned)(hr));
	}

	Result.val = sprite_idx;
}

static bool g_XboxProfileCardLaunching = false;

void F_XboxOneShowProfileCardForUser(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg)
{
	Result.kind = VALUE_REAL;
	Result.val = 0;	

	uint64 requestinguserid = (uint64)YYGetInt64(arg, 0);
	uint64 targetuserid = (uint64)YYGetInt64(arg, 1);

	if (g_XboxProfileCardLaunching)
	{
		// If a profile card is in the process of launching, ignore this request
		Result.val = -2;
		return;
	}

	XUM_LOCK_MUTEX;
	XUMuser* xuser = XUM::GetUserFromId(requestinguserid);
	if (xuser == NULL)
	{
		DebugConsoleOutput("xboxone_show_profile_card_for_user - couldn't get user\n");
		Result.val = -1;
		return;
	}

	static XAsyncBlock async = { NULL, NULL, [](XAsyncBlock* async)
		{
			assert(g_XboxProfileCardLaunching);

			HRESULT status = XAsyncGetStatus(async, false);
			if (!SUCCEEDED(status))
			{
				DebugConsoleOutput("xboxone_show_profile_card_for_user - error (HRESULT 0x%08X)\n", (unsigned)(status));
			}

			g_XboxProfileCardLaunching = false;
		}
	};

	g_XboxProfileCardLaunching = true;

	HRESULT status = XGameUiShowPlayerProfileCardAsync(&async, xuser->user, targetuserid);
	if (!SUCCEEDED(status))
	{
		DebugConsoleOutput("xboxone_show_profile_card_for_user - error (HRESULT 0x%08X)\n", (unsigned)(status));
		g_XboxProfileCardLaunching = false;
	}
}

YYEXPORT
void F_XboxOneSetSaveDataUser(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg)
{
	Result.kind = VALUE_REAL;
	Result.val = -1;	

	XUM_LOCK_MUTEX
	if (arg[0].kind == VALUE_PTR && arg[0].ptr == NULL)
	{
		XUM::SetSaveDataUser(XUserNullUserLocalId);
	}
	else if ((YYGetInt64(arg, 0) == 0) || (YYGetInt64(arg, 0) == -1))		// the docs specify that a user ID of -1 denotes a null user
	{
		XUM::SetSaveDataUser(XUserNullUserLocalId);
	}
	else
	{				
		uint64 id = (uint64)YYGetInt64(arg, 0); // don't do any rounding on this
		XUMuser* xuser = XUM::GetUserFromId(id);
		if (xuser != NULL)
		{
			XUM::SetSaveDataUser(xuser->LocalId);
		}		
	}

	// No valid user found
}

YYEXPORT
void F_XboxOneGetSaveDataUser(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg)
{
	Result.kind = VALUE_INT64;
	Result.v64 = 0;

	if (argc != 0)
	{
		YYError("xboxone_get_savedata_user() - function does not take any arguments", false);		
		return;
	}

	XUM_LOCK_MUTEX;
	XUserLocalId user = XUM::GetSaveDataUser();
	XUMuser* xuser = XUM::GetUserFromLocalId(user);
	if (xuser == NULL)
		return;

	Result.v64 = xuser->XboxUserIdInt;		
}

// (user, privilege_id, attempt_resolution)
YYEXPORT
void F_XboxCheckPrivilege(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg)
{


	Result.kind = VALUE_REAL;
	Result.val = 0;	
	
	uint64 xb_user = (uint64)YYGetInt64(arg, 0);
	uint32 privilege = YYGetUint32(arg, 1);
	bool attempt_resolution = YYGetInt32(arg, 2);

	XUM_LOCK_MUTEX;
	XUMuser* xuser = XUM::GetUserFromId(xb_user);
	if (xuser == NULL)
	{
		DebugConsoleOutput("xboxone_check_privilege - couldn't get user\n");
		return;
	}

	struct CheckPrivilegeContext
	{
		uint64 user_id;
		uint32 privilege;
		int result;

		CheckPrivilegeContext(uint64 user_id, uint32 privilege) :
			user_id(user_id),
			privilege(privilege),
			result(0) {}

		~CheckPrivilegeContext()
		{
			int dsMapIndex = CreateDsMap(3,
				"event_type", 0.0, "check_privilege_result",
				"result", (double)(result), NULL,
				"privilege", (double)(privilege), NULL);

			// Need to add user field seperately as CreateDsMap doesn't handle VALUE_INT64  types
			DsMapAddInt64(dsMapIndex, "user", user_id);

			CreateAsyncEventWithDSMap(dsMapIndex, EVENT_OTHER_SYSTEM_EVENT);
		}
	};

	CheckPrivilegeContext* ctx = new CheckPrivilegeContext(xb_user, privilege);

	/* First, check if the user already has the privilege... */

	bool hasPrivilege;
	XUserPrivilegeDenyReason reason;

	HRESULT status = XUserCheckPrivilege(xuser->user, XUserPrivilegeOptions::None, (XUserPrivilege)(privilege), &hasPrivilege, &reason);
	if (SUCCEEDED(status))
	{
		if (!hasPrivilege)
		{
			switch (reason)
			{
			case XUserPrivilegeDenyReason::PurchaseRequired:
				ctx->result = 1; /* xboxone_privilege_result_purchase_required  */
				break;

			case XUserPrivilegeDenyReason::Restricted:
				ctx->result = 8; /* xboxone_privilege_result_restricted */
				break;

			case XUserPrivilegeDenyReason::Banned:
				ctx->result = 4; /* xboxone_privilege_result_banned  */
				break;

			default:
				DebugConsoleOutput("xboxone_check_privilege - unexpected deny reason (%u)\n", (unsigned)(reason));
				ctx->result = -1;
				break;
			}
		}
	}
	else {
		DebugConsoleOutput("xboxone_check_privilege - error checking privilege (HRESULT 0x%08X)\n", (unsigned)(status));
		ctx->result = -1;
	}

	if (ctx->result > 0 && attempt_resolution)
	{
		/* User doesn't have the privilege and attempt_resolution argument is true, try to get it. */

		XAsyncBlock* async = new XAsyncBlock;

		async->queue = NULL;
		async->context = ctx;
		async->callback = [](XAsyncBlock* async)
		{
			CheckPrivilegeContext* ctx = (CheckPrivilegeContext*)(async->context);

			std::unique_ptr<XAsyncBlock> async_guard(async);
			std::unique_ptr<CheckPrivilegeContext> ctx_guard(ctx);

			HRESULT status = XUserResolvePrivilegeWithUiResult(async);

			switch(status)
			{
			case S_OK:
				ctx->result = 0;
				break;
			
			case E_ABORT:
				ctx->result = 2; /* xboxone_privilege_result_aborted */
				break;

			default:
				DebugConsoleOutput("xboxone_check_privilege - error resolving privilege (HRESULT 0x%08X)\n", (unsigned)(status));
				break;
			}
		};

		HRESULT status = XUserResolvePrivilegeWithUiAsync(xuser->user, XUserPrivilegeOptions::None, (XUserPrivilege)(privilege), async);
		if (!SUCCEEDED(status))
		{
			DebugConsoleOutput("xboxone_check_privilege - error resolving privilege (HRESULT 0x%08X)\n", (unsigned)(status));
			delete async;
			delete ctx;
		}
	}
	else {
		delete ctx;
	}
}

#ifndef WIN_UAP
SXboxOneEventType *findXboxOneEvent(const char *name)
{
	SXboxOneEventType *ptr = g_xbox_one_event_head;

	while (ptr)
	{
		if (strcmp(ptr->m_name, name) == 0)
		{
			return ptr;
		}

		ptr = ptr->m_next;
	}

	return nullptr;
}
#endif

YYEXPORT
void F_XboxOneFireEvent(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg)
{
	Result.kind = VALUE_REAL;

	const char* pEventName = YYGetString(arg, 0);
	SXboxOneEventType* sxoet = findXboxOneEvent(pEventName);

	if (!sxoet)
	{
		YYError("xboxone_fire_event() - event type not registered", false);
		Result.val = -1;
		return;
	}

	if (sxoet->m_numParams != (argc - 1))
	{
		char errmsg[2048];
		snprintf(errmsg, sizeof(errmsg), "xboxone_fire_event() - event type '%s' has wrong number of parameters (%d / %d)", pEventName, sxoet->m_numParams, argc - 1);
		YYError(errmsg, false);

		Result.val = -1;
		return;
	}

	json_object* dimensions = json_object_new_object();

	for (int i = 1; i < argc; i++)
	{
		const char* param_name = sxoet->m_paramName[i - 1];
		
		switch (sxoet->m_paramType[i - 1])
		{
		case SXboxOneEventType::FieldType::Int32:
		{
			int32 v = YYGetInt32(arg, i);
			json_object_object_add(dimensions, param_name, json_object_new_int(v));
			break;
		}

		case SXboxOneEventType::FieldType::UInt32:
		{
			uint32 v = (uint32)YYGetInt32(arg, i);
			/* Our version of json-c is old and doesn't have 64-bit integer support.
			 * json_object_object_add(dimensions, param_name, json_object_new_int64(v)); */
			json_object_object_add(dimensions, param_name, json_object_new_double(v));
			break;
		}

		case SXboxOneEventType::FieldType::Int64:
		{
			int64 v = (int64)YYGetInt64(arg, i);
			/* Our version of json-c is old and doesn't have 64-bit integer support.
			 * json_object_object_add(dimensions, param_name, json_object_new_int64(v)); */
			json_object_object_add(dimensions, param_name, json_object_new_double((double)v));
			break;
		}

		case SXboxOneEventType::FieldType::UInt64:
		{
			uint64 v = (uint64)YYGetInt64(arg, i);
			/* json-c doesn't have a uint64 type... double is probably the least likely to overflow. */
			json_object_object_add(dimensions, param_name, json_object_new_double((double)v));
			break;
		}

		case SXboxOneEventType::FieldType::Float:
		{
			float v = (float)YYGetFloat(arg, i);
			json_object_object_add(dimensions, param_name, json_object_new_double(v));
			break;
		}

		case SXboxOneEventType::FieldType::Double:
		{
			double v = (double)YYGetReal(arg, i);
			json_object_object_add(dimensions, param_name, json_object_new_double(v));
			break;
		}

		case SXboxOneEventType::FieldType::Boolean:
		{
			bool v = YYGetBool(arg, i);
			json_object_object_add(dimensions, param_name, json_object_new_boolean(v ? TRUE : FALSE));
			break;
		}

		case SXboxOneEventType::FieldType::UnicodeString:
		{
			const char* pArg = YYGetString(arg, i);
			if (pArg == nullptr)
			{
				json_object_object_add(dimensions, param_name, json_object_new_null());
			}
			else {
				json_object_object_add(dimensions, param_name, json_object_new_string(pArg));
			}

			break;
		}

		case SXboxOneEventType::FieldType::GUID:
		{
			const char* pArg = YYGetString(arg, i);
			if (pArg == nullptr)
			{
				json_object_object_add(dimensions, param_name, json_object_new_null());
			}
			else {
				/* Remove leading/trailing braces from GUID string. */

				size_t len = strlen(pArg);

				if (pArg[0] == '{')
				{
					++pArg;
					--len;

					if (len > 0 && pArg[len - 1] == '}')
					{
						--len;
					}
				}

				json_object_object_add(dimensions, param_name, json_object_new_string_len(pArg, (int)len));
			}

			break;
		}
		}
	}
	
	const char* dimensions_json = json_object_to_json_string(dimensions);

	XblContextHandle xbl_ctx;

	{
		XUM_LOCK_MUTEX;
		XUserLocalId user = XUM::GetActivatingUser();
		XUMuser* xuser = XUM::GetUserFromLocalId(user);
		if (xuser == NULL)
		{
			DebugConsoleOutput("xboxone_fire_event() - couldn't get activating user\n");
			json_object_put(dimensions);
			Result.val = -1;
			return;
		}

		HRESULT hr = XblContextCreateHandle(xuser->user, &xbl_ctx);
		if (!SUCCEEDED(hr))
		{
			DebugConsoleOutput("xboxone_fire_event() - couldn't create xbl handle (HRESULT 0x%08X)\n", (unsigned)(hr));
			json_object_put(dimensions);
			Result.val = -1;
			return;
		}
	}

	const char* measurements = "{}";

	HRESULT hr = XblEventsWriteInGameEvent(xbl_ctx, pEventName, dimensions_json, measurements);

	if (SUCCEEDED(hr))
	{
		Result.val = 0;
	}
	else {
		DebugConsoleOutput("xboxone_fire_event() - couldn't write stats event (HRESULT 0x%08X)\n", (unsigned)(hr));
		Result.val = -1;
	}

	XblContextCloseHandle(xbl_ctx);

	json_object_put(dimensions);
	
}

YYEXPORT
void F_XboxOneGetStatsForUser(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg)
{
	// user, service config id, list of stat names to read	

	uint64 user_id = (uint64)YYGetInt64(arg, 0); // don't do any rounding on this
	const char* serviceconfig_id = YYGetString(arg, 1);

	static const unsigned MAX_STATISTIC_NAMES = 14;

	const char* statisticNames[MAX_STATISTIC_NAMES];
	size_t statisticNamesCount = 0;

	for (int i = 2; i < argc; i++)
	{
		if (statisticNamesCount >= MAX_STATISTIC_NAMES)
		{
			YYError("xboxone_get_stats_for_user - too many stat names requested (maximum %u)\n", MAX_STATISTIC_NAMES);
			return;
		}

		statisticNames[statisticNamesCount++] = YYGetString(arg, i);
	}

	XblContextHandle xbl_ctx;

	{
		XUM_LOCK_MUTEX;
		XUMuser* xuser = XUM::GetUserFromId(user_id);
		if (xuser == NULL)
		{
			DebugConsoleOutput("xboxone_get_stats_for_user - couldn't get activating user\n");
			Result.val = -1;
			return;
		}

		HRESULT hr = XblContextCreateHandle(xuser->user, &xbl_ctx);
		if (!SUCCEEDED(hr))
		{
			DebugConsoleOutput("xboxone_get_stats_for_user - couldn't create xbl handle (HRESULT 0x%08X)\n", (unsigned)(hr));
			Result.val = -1;
			return;
		}
	}

	struct GetStatsForUserContext
	{
		uint64 user_id;
		XblContextHandle xbl_ctx;

		int map_index;

		GetStatsForUserContext(uint64 user_id, XblContextHandle xbl_ctx):
			user_id(user_id),
			xbl_ctx(xbl_ctx)
		{
			map_index = CreateDsMap(1, "event_type", 0.0, "stat_result");
			DsMapAddInt64(map_index, "user", user_id);
		}

		~GetStatsForUserContext()
		{
			XblContextCloseHandle(xbl_ctx);
		}
	};

	GetStatsForUserContext* ctx = new GetStatsForUserContext(user_id, xbl_ctx);
	
	XAsyncBlock *async = new XAsyncBlock;
	async->queue = NULL;
	async->context = ctx;
	async->callback = [](XAsyncBlock* async)
	{
		GetStatsForUserContext* ctx = (GetStatsForUserContext*)(async->context);

		std::unique_ptr<GetStatsForUserContext> ctx_guard(ctx);
		std::unique_ptr<XAsyncBlock> async_guard(async);

		size_t result_buf_size;
		HRESULT hr = XblUserStatisticsGetMultipleUserStatisticsResultSize(async, &result_buf_size);
		if (!SUCCEEDED(hr))
		{
			DebugConsoleOutput("xboxone_get_stats_for_user - exception occurred getting results - 0x%08X\n", (unsigned)(hr));

			DsMapAddDouble(ctx->map_index, "succeeded", 0);
			CreateAsyncEventWithDSMap(ctx->map_index, EVENT_OTHER_SYSTEM_EVENT);

			return;
		}

		void* result_buf = YYAlloc((int)result_buf_size);

		XblUserStatisticsResult* results;
		size_t results_count;

		hr = XblUserStatisticsGetMultipleUserStatisticsResult(async,
			result_buf_size,
			result_buf,
			&results,
			&results_count,
			NULL);

		if (!SUCCEEDED(hr))
		{
			DebugConsoleOutput("xboxone_get_stats_for_user - exception occurred getting results - 0x%08X\n", (unsigned)(hr));

			DsMapAddDouble(ctx->map_index, "succeeded", 0);
			CreateAsyncEventWithDSMap(ctx->map_index, EVENT_OTHER_SYSTEM_EVENT);

			YYFree(result_buf);
			return;
		}

		/* "How should we improve our API?"
		 * "How about more deeply nested data structures?"
		 * "...give this man a raise!"
		*/

		for (size_t results_idx = 0; results_idx < results_count; ++results_idx)
		{
			XblUserStatisticsResult* result = results + results_idx;

			XblServiceConfigurationStatistic* sc_stats = result->serviceConfigStatistics;
			uint32_t sc_stats_count = result->serviceConfigStatisticsCount;

			for (size_t sc_stats_idx = 0; sc_stats_idx < sc_stats_count; ++sc_stats_idx)
			{
				XblServiceConfigurationStatistic* sc_stat = sc_stats + sc_stats_idx;

				XblStatistic* stats = sc_stat->statistics;
				uint32_t stats_count = sc_stat->statisticsCount;

				for (size_t stat_idx = 0; stat_idx < sc_stat->statisticsCount; ++stat_idx)
				{
					XblStatistic* stat = stats + stat_idx;

					/* DsMapAddString() doesn't modify the strings given and should really take
					 * them as const arguments, but there are dupliate declarations for it spread
					 * throughout the codebase rather than using a header, so I don't want to try
					 * and find/fix them all right now. Just cast and assume it won't suddenly
					 * start modifying its arguments in the future.
					*/
					DsMapAddString(ctx->map_index, (char*)(stat->statisticName), (char*)(stat->value));
				}
			}
		}

		DsMapAddDouble(ctx->map_index, "succeeded", 1);
		CreateAsyncEventWithDSMap(ctx->map_index, EVENT_OTHER_SYSTEM_EVENT);

		YYFree(result_buf);
	};

	HRESULT hr = XblUserStatisticsGetMultipleUserStatisticsAsync(
		xbl_ctx,
		&user_id,
		1,
		serviceconfig_id,
		statisticNames,
		statisticNamesCount,
		async);

	if (SUCCEEDED(hr))
	{
		Result.kind = VALUE_REAL;
		Result.val = 0;
	}
	else
	{
		DebugConsoleOutput("xboxone_get_stats_for_user - exception occurred getting stats - 0x%08X\n", hr);

		delete async;
		delete ctx;

		Result.kind = VALUE_REAL;
		Result.val = -1;
	}
}

YYEXPORT
void F_XboxOneStatsSetup(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg)
{
	if (arg[2].kind == VALUE_STRING)
	{
		YYError("xboxone_stats_setup - argument 3 should be a number, e.g. $1234AB66 or 0x1234AB66, not '1234AB66'\n");

		return;
	}

	g_stats_user_id = (uint64) YYGetInt64(arg, 0);

	YYFree(g_stats_service_config);
	g_stats_service_config = (char*)(YYStrDup(YYGetString(arg, 1)));

	g_stats_title_id = YYGetInt32(arg, 2);
}

YYEXPORT
void F_XboxOneSetUserStatReal(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg)
{
	uint64 user_id = (uint64)YYGetInt64(arg, 0);

	const char* stat = YYGetString(arg, 1);
	double value = YYGetReal(arg, 2);

	Result.kind = VALUE_REAL;
	Result.val = XboxStatsManager::set_stat_numeric(user_id, stat, value, "xboxone_stats_set_stat_real")
		? 0
		: -1;
}

YYEXPORT
void F_XboxOneSetUserStatInt(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg)
{
	uint64 user_id = (uint64)YYGetInt64(arg, 0);

	const char* stat = YYGetString(arg, 1);
	int64 value = YYGetInt64(arg, 2);

	Result.kind = VALUE_REAL;
	Result.val = XboxStatsManager::set_stat_numeric(user_id, stat, (double)value, "xboxone_stats_set_stat_int")
		? 0
		: -1;
}

YYEXPORT
void F_XboxOneSetUserStatString(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg)
{
	uint64 user_id = (uint64)YYGetInt64(arg, 0);

	const char* stat = YYGetString(arg, 1);
	const char* value = YYGetString(arg, 2);

	Result.kind = VALUE_REAL;
	Result.val = XboxStatsManager::set_stat_string(user_id, stat, value, "xboxone_stats_set_stat_string")
		? 0
		: -1;
}

YYEXPORT
void F_XboxOneDeleteUserStat(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg)
{
	uint64 user_id = (uint64)YYGetInt64(arg, 0);

	const char* stat = YYGetString(arg, 1);

	Result.kind = VALUE_REAL;
	Result.val = XboxStatsManager::delete_stat(user_id, stat, "xboxone_stats_delete_stat")
		? 0
		: -1;
}

YYEXPORT
void F_XboxOneGetUserStat(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg)
{
	uint64 user_id = (uint64)YYGetInt64(arg, 0);
	const char* stat = YYGetString(arg, 1);

	XboxStatsManager::StatValue stat_val = XboxStatsManager::get_stat(user_id, stat, "xboxone_stats_get_stat");

	if(stat_val.type == XboxStatsManager::StatValue::Type::None)
	{
		DebugConsoleOutput("xboxone_stats_get_stat - stat doesn't exist\n");

		Result.kind = VALUE_UNDEFINED;
		Result.val = 0;
	}
	else if (stat_val.type == XboxStatsManager::StatValue::Type::Numeric)
	{
		Result.kind = VALUE_REAL;
		Result.val = stat_val.numval;
	}
	else if (stat_val.type == XboxStatsManager::StatValue::Type::String)
	{
		YYCreateString(&Result, stat_val.stringval.c_str());
	}
	else {
		abort(); /* Unreachable */
	}

}

YYEXPORT
void F_XboxOneGetUserStatNames(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg)
{
	uint64 user_id = (uint64)YYGetInt64(arg, 0);

	std::vector<std::string> stat_names = XboxStatsManager::get_stat_names(user_id, "xboxone_stats_get_stat_names");

	YYCreateArray(&Result);

	/* Initialize Result array in reverse to avoid unnecessary resizing */
	size_t stat_idx = stat_names.size();

	while (stat_idx > 0)
	{
		--stat_idx;

		RValue tmp;
		tmp.kind = VALUE_REAL;
		YYCreateString(&tmp, stat_names[stat_idx].c_str());

		SET_RValue(&Result, &tmp, (YYObjectBase*)(selfinst), (int)stat_idx);

		FREE_RValue(&tmp);
	}
}

YYEXPORT
void F_XboxOneAddUserToStats(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg)
{
	uint64 user_id = (uint64)YYGetInt64(arg, 0);

	Result.kind = VALUE_REAL;
	Result.val = XboxStatsManager::add_user(user_id, "xboxone_stats_add_user")
		? 0
		: -1;
}

YYEXPORT
void F_XboxOneRemoveUserFromStats(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg)
{
	uint64 user_id = (uint64)YYGetInt64(arg, 0);

	Result.kind = VALUE_REAL;
	Result.val = XboxStatsManager::remove_user(user_id, "xboxone_stats_remove_user")
		? 0
		: -1;
}

YYEXPORT
void F_XboxOneFlushUserStats(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg)
{
	uint64 user_id = (uint64)YYGetInt64(arg, 0);
	bool high_priority = YYGetBool(arg, 1);

	Result.kind = VALUE_REAL;
	Result.val = XboxStatsManager::flush_user(user_id, high_priority, "xboxone_stats_flush_user")
		? 0
		: -1;
}

static void DoTitleManagedLeaderboardQuery(uint64 user_id, XblLeaderboardQuery query, bool reverse_rows, const char* func_name)
{
	XblContextHandle xbl_ctx;

	{
		XUM_LOCK_MUTEX;
		XUMuser* xuser = XUM::GetUserFromId(user_id);
		if (xuser == NULL)
		{
			DebugConsoleOutput("%s - couldn't get user\n", func_name);
			return;
		}

		HRESULT hr = XblContextCreateHandle(xuser->user, &xbl_ctx);
		if (!SUCCEEDED(hr))
		{
			DebugConsoleOutput("%s - couldn't create xbl handle (HRESULT 0x%08X)\n", func_name, (unsigned)(hr));
			return;
		}
	}

	struct GetLeaderboardContext
	{
		bool reverse_rows;
		const char* func_name;
		XblContextHandle xbl_ctx;

		XAsyncBlock async;

		GetLeaderboardContext(bool reverse_rows, const char* func_name, XblContextHandle xbl_ctx) :
			reverse_rows(reverse_rows),
			func_name(func_name),
			xbl_ctx(xbl_ctx) {}

		~GetLeaderboardContext()
		{
			XblContextCloseHandle(xbl_ctx);
		};
	};

	GetLeaderboardContext* ctx = new GetLeaderboardContext(reverse_rows, func_name, xbl_ctx);

	ctx->async.queue = NULL;
	ctx->async.context = ctx;
	ctx->async.callback = [](XAsyncBlock* async)
	{
		GetLeaderboardContext* ctx = (GetLeaderboardContext*)(async->context);

		/* Destroy context when callback returns. */
		std::unique_ptr<GetLeaderboardContext> ctx_guard(ctx);

		size_t result_size = -1;
		void* result_buf = NULL;
		XblLeaderboardResult* result = NULL;

		HRESULT status = XblLeaderboardGetLeaderboardResultSize(async, &result_size);
		if (SUCCEEDED(status))
		{
			result_buf = YYAlloc((int)result_size);
			status = XblLeaderboardGetLeaderboardResult(async, result_size, result_buf, &result, NULL);
		}

		if (!SUCCEEDED(status))
		{
			DebugConsoleOutput("%s - error fetching leaderboard (HRESULT 0x%08X)\n", ctx->func_name, (unsigned)(status));
		}

		int dsMapIndex = CreateDsMap(2,
			"id", (double)e_achievement_leaderboard_info, (void*)(NULL),
			"event", (double)(0.0), "GetLeaderboardComplete");

		DsMapAddDouble(dsMapIndex, "error", status);

		if (SUCCEEDED(status))
		{
			DsMapAddString(dsMapIndex, "display_name", ""); /* XDK runner sets this to empty string... */
			DsMapAddInt64(dsMapIndex, "numentries", result->rowsCount);

			for (size_t row_idx = 0; row_idx < result->rowsCount; ++row_idx)
			{
				size_t row_idx_translated = ctx->reverse_rows
					? ((result->rowsCount - 1) - row_idx)
					: row_idx;

				XblLeaderboardRow* row = &(result->rows[row_idx_translated]);

				char key[32];

				snprintf(key, sizeof(key), "Player%zu", row_idx);
				DsMapAddString(dsMapIndex, key, row->gamertag);

				snprintf(key, sizeof(key), "Playerid%zu", row_idx);
				DsMapAddInt64(dsMapIndex, key, row->xboxUserId);

				snprintf(key, sizeof(key), "Rank%zu", row_idx);
				DsMapAddDouble(dsMapIndex, key, row->rank);

				/* Concatenate column ("stat") values together, seperated by spaces. */

				char values[2048] = { '\0' };
				size_t values_off = 0;

				for (size_t col_idx = 0;
					col_idx < row->columnValuesCount
					&& values_off < sizeof(values);
					++col_idx)
				{
					values_off += snprintf((values + values_off), (sizeof(values) - values_off), "%s%s",
						(col_idx > 0 ? " " : ""),
						row->columnValues[col_idx]);
				}

				snprintf(key, sizeof(key), "Score%zu", row_idx);
				DsMapAddString(dsMapIndex, key, values);
			}
		}

		CreateAsyncEventWithDSMap(dsMapIndex, EVENT_OTHER_SOCIAL);

		YYFree(result_buf);
	};

	HRESULT status = XblLeaderboardGetLeaderboardAsync(xbl_ctx, query, &(ctx->async));
	if (!SUCCEEDED(status))
	{
		DebugConsoleOutput("%s - error fetching leaderboard (HRESULT 0x%08X)\n", func_name, (unsigned)(status));
		delete ctx;
	}
}

YYEXPORT
void F_XboxOneGetStatLeaderboard(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg)
{

	Result.kind = VALUE_REAL;
	Result.val = 0;

	uint64 user_id = (uint64)YYGetInt64(arg, 0);
	const char* stat = YYGetString(arg, 1);
	int num_entries = YYGetInt32(arg, 2);
	int start_rank = YYGetInt32(arg, 3);
	bool start_at_user = YYGetBool(arg, 4);
	bool ascending = YYGetBool(arg, 5);

	/* NOTE: XblLeaderboardQuery::order seems to be ignored when doing a global leaderboard
	 * query, so we always ask for descending (we'd get it anyway) and reverse the order of
	 * rows in the returned data when we want it in ascending order.
	*/

	XblLeaderboardQuery query;
	memset(&query, 0, sizeof(query));

	snprintf(query.scid, sizeof(query.scid), "%s", g_XboxSCID);
	query.xboxUserId = user_id;
	query.leaderboardName = stat;
	query.statName = stat;
	query.socialGroup = XblSocialGroupType::None;
	query.order = XblLeaderboardSortOrder::Descending;
	query.maxItems = num_entries;
	query.skipToXboxUserId = start_at_user ? user_id : 0;
	query.skipResultToRank = start_rank;
	query.queryType = XblLeaderboardQueryType::TitleManagedStatBackedGlobal;

	DoTitleManagedLeaderboardQuery(user_id, query, ascending, "xboxone_stats_get_leaderboard");

}

YYEXPORT
void F_XboxOneGetStatSocialLeaderboard(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg)
{
	uint64 user_id = (uint64)YYGetInt64(arg, 0);
	const char* stat = YYGetString(arg, 1);
	int num_entries = YYGetInt32(arg, 2);
	int start_rank = YYGetInt32(arg, 3);
	bool start_at_user = YYGetBool(arg, 4);
	bool ascending = YYGetBool(arg, 5);
	bool favourites_only = YYGetBool(arg, 5);

	XblLeaderboardQuery query;
	memset(&query, 0, sizeof(query));

	snprintf(query.scid, sizeof(query.scid), "%s", g_XboxSCID);
	query.xboxUserId = user_id;
	query.statName = stat;
	query.socialGroup = favourites_only ? XblSocialGroupType::Favorites : XblSocialGroupType::People;
	query.order = ascending ? XblLeaderboardSortOrder::Ascending : XblLeaderboardSortOrder::Descending;
	query.maxItems = num_entries;
	query.skipToXboxUserId = start_at_user ? user_id : 0;
	query.skipResultToRank = start_rank;
	query.queryType = XblLeaderboardQueryType::TitleManagedStatBackedSocial;

	DoTitleManagedLeaderboardQuery(user_id, query, false, "xboxone_stats_get_social_leaderboard");

}

#define MAX_XBL_ACHIEVEMENT_LENGTH	128

struct XBLAchievement
{
	int requestID;

	uint64 userId;
	char pAchievement[MAX_XBL_ACHIEVEMENT_LENGTH];
	uint32 progress;
};

int g_currXBLAchievementRequestID = 0;

YYEXPORT
void F_XboxOneSetAchievementProgress(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg)
{
	Result.kind = VALUE_REAL;
	Result.val = 0;

	uint64 user_id = (uint64)YYGetInt64(arg, 0);
	const char* achievement = YYGetString(arg, 1);
	int32 progress = YYGetInt32(arg, 2);

	if (progress < 0 || progress > 100)
	{
		DebugConsoleOutput("xboxone_achievements_set_progress - progres value %" PRId32 " is invalid - must be in the range 0-100\n", progress);

		Result.val = -1;
		return;
	}

	XblContextHandle xbl_ctx;

	{
		XUM_LOCK_MUTEX;
		XUMuser* user = XUM::GetUserFromId(user_id);

		if (user == NULL)
		{
			DebugConsoleOutput("xboxone_achievements_set_progress - couldn't find user_id\n");

			Result.val = -1;
			return;
		}

		HRESULT hr = XblContextCreateHandle(user->user, &xbl_ctx);
		if (!SUCCEEDED(hr))
		{
			DebugConsoleOutput("xboxone_achievements_set_progress - couldn't create xbl handle (HRESULT 0x%08X)\n", (unsigned)(hr));

			Result.val = -2;
			return;
		}
	}

	struct SetAchievementProgressContext
	{
		uint64 user_id;
		char* achievement;
		uint32 progress;

		XblContextHandle xbl_ctx;
		int request_id;

		XAsyncBlock async;

		SetAchievementProgressContext(uint64 user_id, const char* achievement, uint32 progress, XblContextHandle xbl_ctx, int request_id) :
			user_id(user_id),
			achievement((char*)(YYStrDup(achievement))),
			progress(progress),
			xbl_ctx(xbl_ctx),
			request_id(request_id) {}

		~SetAchievementProgressContext()
		{
			XblContextCloseHandle(xbl_ctx);
			YYFree(achievement);
		}
	};

	int request_id = g_currXBLAchievementRequestID++;
	if (g_currXBLAchievementRequestID < 0)
	{
		/* In case of overflow. */
		g_currXBLAchievementRequestID = 0;
	}

	SetAchievementProgressContext* ctx = new SetAchievementProgressContext(user_id, achievement, progress, xbl_ctx, request_id);

	ctx->async.queue = NULL;
	ctx->async.context = ctx;
	ctx->async.callback = [](XAsyncBlock* async)
	{
		SetAchievementProgressContext* ctx = (SetAchievementProgressContext*)(async->context);

		/* Delete context when callback returns. */
		std::unique_ptr<SetAchievementProgressContext> ctx_guard(ctx);

		HRESULT status = XAsyncGetStatus(async, false);
		if (!SUCCEEDED(status))
		{
			DebugConsoleOutput("xboxone_achievements_set_progress - XblAchievementsUpdateAchievementAsync failed (HRESULT 0x%08X)\n", (unsigned)(status));
		}

		int dsMapIndex = CreateDsMap(5,
			"event_type", (double)0.0, "achievement result",
			"error", (double)(status), NULL,
			"requestID", (double)(ctx->request_id), NULL,
			"achievement", 0.0, ctx->achievement,
			"progress", (double)(ctx->progress), NULL);

		DsMapAddInt64(dsMapIndex, "userID", ctx->user_id);

		CreateAsyncEventWithDSMap(dsMapIndex, EVENT_OTHER_SYSTEM_EVENT);
	};

	HRESULT status = XblAchievementsUpdateAchievementAsync(xbl_ctx, user_id, achievement, progress, &(ctx->async));
	if (SUCCEEDED(status))
	{
		Result.val = request_id;
	}
	else{
		DebugConsoleOutput("xboxone_achievements_set_progress - XblAchievementsUpdateAchievementAsync failed (HRESULT 0x%08X)\n", (unsigned)(status));
		delete ctx;

		Result.val = -1 * status;
		return;
	}

}

YYEXPORT
void F_XboxOneSetRichPresence(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg)
{
	Result.kind = VALUE_REAL;

	uint64 user_id = (uint64)YYGetInt64(arg, 0);
	bool is_user_active = YYGetBool(arg, 1);
	const char* rich_presence_string = YYGetString(arg, 2);

	const char* scid = (argc > 3)
		? YYGetString(arg, 3)
		: g_stats_service_config;
	
	if (scid == NULL)
	{
		DebugConsoleOutput("xboxone_set_rich_presence - No service_config_id provided and xboxone_stats_setup() not called\n");

		Result.val = -1;
		return;
	}

	XblContextHandle xbl_ctx;

	{
		XUM_LOCK_MUTEX;
		XUMuser* user = XUM::GetUserFromId(user_id);

		if (user == NULL)
		{
			DebugConsoleOutput("xboxone_set_rich_presence - couldn't find user_id\n");

			Result.val = -1;
			return;
		}

		HRESULT hr = XblContextCreateHandle(user->user, &xbl_ctx);
		if (!SUCCEEDED(hr))
		{
			DebugConsoleOutput("xboxone_set_rich_presence - couldn't create xbl handle (HRESULT 0x%08X)\n", (unsigned)(hr));

			Result.val = -1;
			return;
		}
	}

	XblPresenceRichPresenceIds presence;
	snprintf(presence.scid, sizeof(presence.scid), "%s", scid);
	presence.presenceId = rich_presence_string;
	presence.presenceTokenIds = NULL;
	presence.presenceTokenIdsCount = 0;

	XAsyncBlock* async = new XAsyncBlock;
	async->queue = NULL;
	async->context = xbl_ctx;
	async->callback = [](XAsyncBlock* async)
	{
		XblContextHandle xbl_ctx = (XblContextHandle)(async->context);

		HRESULT hr = XAsyncGetStatus(async, false);
		if (!SUCCEEDED(hr))
		{
			DebugConsoleOutput("xboxone_set_rich_presence - couldn't update presence info (HRESULT 0x%08X)\n", (unsigned)(hr));
		}

		XblContextCloseHandle(xbl_ctx);
		delete async;
	};

	HRESULT hr = XblPresenceSetPresenceAsync(xbl_ctx, is_user_active, &presence, async);
	if (!SUCCEEDED(hr))
	{
		DebugConsoleOutput("xboxone_set_rich_presence - couldn't update presence info (HRESULT 0x%08X)\n", (unsigned)(hr));

		XblContextCloseHandle(xbl_ctx);
		delete async;

		Result.val = -1;
		return;
	}

	Result.val = 0;
}

void F_XboxOneSetServiceConfigurationID(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg)
{
	Result.kind = VALUE_REAL;
	Result.val = -1;

#ifndef _GAMING_XBOX
	g_PrimaryServiceConfigId = ConvertCharArrayToManagedString(YYGetString(arg, 0));	
#endif
}

void F_XboxLiveNotAvailable(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg)
{
	Result.kind = VALUE_REAL;
	Result.val = -1;

	DebugConsoleOutput("Function not available without Xbox Live\n");
}

YYEXPORT
void F_XboxOneRequestUserToken(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg)
{
	uint64 id = (uint64)YYGetInt64(arg, 0);
	const char* url = YYGetString(arg, 1);
	const char* method = YYGetString(arg, 2);
	const char* headerJsonString = YYGetString(arg, 3); /* A json string of http headers. TODO: Can we pass a GML ds_map or struct instead? */
	const bool forceRefresh = (argc > 4) ? YYGetBool(arg, 4) : false;

	/* Optional body, which should be a GML buffer. */
	char* buffer_data = nullptr;
	int buffer_idx = (argc > 5) ? YYGetInt32(arg, 5) : 0;
	int buffer_size = 0;
	if (argc > 5){
		if (!BufferGetContent(buffer_idx, (void**)(&buffer_data), &buffer_size))
		{
			DebugConsoleOutput("xboxone_get_token_and_signature() - error: specified buffer not found\n");
			Result.val = -1;
			return;
		}
	}

	int numusers = 0;
	XUMuser** users = XUM::GetUsers(numusers);
	for (int i = 0; i < numusers; i++)
	{
		XUMuser* user = users[i];
		if (user->XboxUserIdInt == id)
		{
			/* Parse the json string into XUserGetTokenAndSignatureHttpHeader*/
			json_object* headerObj = json_tokener_parse(headerJsonString);
			if (headerObj != NULL) {
				size_t headerCount = json_object_object_length(headerObj);
				XUserGetTokenAndSignatureHttpHeader* headers = new XUserGetTokenAndSignatureHttpHeader[headerCount];
				uint64_t i = 0;
				json_object_object_foreach(headerObj, stats_key, stats_val)
				{
					const char* key = stats_key;
					if (stats_val != NULL)
					{
						headers[i].name = key;
						if (json_object_is_type(stats_val, json_type_string))
						{
							headers[i].value = json_object_get_string(stats_val);
						}
						else {
							DebugConsoleOutput("Unexpected value type for key '%s'\n", stats_key);
						}
					}
					else {
						DebugConsoleOutput("value not found for key '%s'\n", stats_key);
					}
					++i;
				}
				Result.val = 0;
				XUM::GetTokenAndSignatureAsync(user->user, url, method, headerCount, headers, buffer_data, buffer_size, forceRefresh);
				delete[] headers;
				return;
			}
			else {
				Result.val = -1;
				DebugConsoleOutput("Malformed json string for headers '%s'\n", headerJsonString);
				return;
			}
		}
	}
	Result.val = -1;
	DebugConsoleOutput("xboxone_get_token_and_signature() - error: user not found\n");
	return;

}