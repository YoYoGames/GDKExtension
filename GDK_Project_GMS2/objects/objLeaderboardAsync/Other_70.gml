/// @description Insert description here
// You can write your code in this editor

if (async_load[? "id"] != achievement_leaderboard_info) exit;

if (async_load[? "event"] != "GetLeaderboardComplete") exit;

show_message(json_encode(async_load));