//
// Copyright (C) 2020 Opera Norway AS. All rights reserved.
//
// This file is an original work developed by Opera.
//

#pragma once

#include <assert.h>
#include <stdint.h>

#define __YYDEFINE_EXTENSION_FUNCTIONS__
#include "Extension_Interface.h"
#include "Ref.h"
#include "YYRValue.h"

#define YYEXPORT __declspec(dllexport)

#define stricmp	_stricmp

#define	MAX_COMMAND_LINE	8192

#define YYASSERT(...) assert(__VA_ARGS__)

enum eAchievementFilter
{
	e_achievement_filter_all_players = 0,
	e_achievement_filter_friends_only = 1,
	e_achievement_filter_favorites_only = 2,
	e_achievement_filter_friends_alt = 3, // used on Xbox One to specify reversed sort order
	e_achievement_filter_favorites_alt = 4 // used on Xbox One to specify reversed sort order
};

enum eAchievementMessages {

	e_achievement_our_info = 1002,
	e_achievement_friends_info = 1003,
	e_achievement_leaderboard_info = 1004,
	e_achievement_achievement_info = 1005,
	e_achievement_pic_loaded = 1006,
	e_achievement_challenge_completed = 1007,
	e_achievement_challenge_completed_by_remote = 1008,
	e_achievement_challenge_received = 1009,
	e_achievement_challenge_list_received = 1010,
	e_achievement_challenge_launched = 1011,
	e_achievement_player_info = 1012,
	e_achievement_purchase_info = 1013,
	e_achievement_msg_result = 1014,
	e_achievement_stat_event = 1015
};

enum eCacheType
{
	eCacheType_Score = 1,
	eCacheType_Achievement,
	eCacheType_Achievement_Increment,

	eCacheType_Max = 0x7fffffff
};

enum eChallengeResult
{
	eCR_Win,
	eCR_Lose,
	eCR_Tie,
};

enum eLeaderboardType
{
	eLT_Number,
	eLT_TimeMinsSecs,
};

enum eNetworkingEventType
{
	eNetworkingEventType_None = 0,
	eNetworkingEventType_Connecting,
	eNetworkingEventType_Disconnecting,
	eNetworkingEventType_IncomingData,
	eNetworkingEventType_NonBlockingConnect,
};

enum eXboxFileError
{
	eXboxFileError_NoError = 0,
	eXboxFileError_OutOfMemory,
	eXboxFileError_UserNotFound,
	eXboxFileError_UnknownError,
	eXboxFileError_CantOpenFile,
	eXboxFileError_BlobNotFound,
	eXboxFileError_ContainerNotInSync,
	eXboxFileError_ContainerSyncFailed,
	eXboxFileError_InvalidContainerName,
	eXboxFileError_NoAccess,
	eXboxFileError_NoXboxLiveInfo,
	eXboxFileError_OutOfLocalStorage,
	eXboxFileError_ProvidedBufferTooSmall,
	eXboxFileError_QuotaExceeded,
	eXboxFileError_UpdateTooBig,
	eXboxFileError_UserCanceled,
	eXboxFileError_NoServiceConfiguration,
	eXboxFileError_UserNotRegisteredInService,
};

static const int EVENT_OTHER_DIALOG_ASYNC = 63;
static const int EVENT_OTHER_WEB_IAP = 66;
static const int EVENT_OTHER_WEB_NETWORKING = 68;
static const int EVENT_OTHER_SOCIAL = 70;
static const int EVENT_OTHER_ASYNC_SAVE_LOAD = 72;
static const int EVENT_OTHER_SYSTEM_EVENT = 75;
static const int EVENT_OTHER_WEB_IMAGE_LOAD = 60;

extern char* g_XboxSCID;
