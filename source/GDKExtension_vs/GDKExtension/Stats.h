//
// Copyright (C) 2020 Opera Norway AS. All rights reserved.
//
// This file is an original work developed by Opera.
//

/* Xbox stats manifest handling for GDK runner.
 * Derived from XboxOneRunner/XboxOneRunner/DynamicEvents/ProviderGenerator.h.
*/

#pragma once

#include <map>
#include <mutex>
#include <stdexcept>
#include <string>
#include <vector>
#include <xsapi-c/services_c.h>

struct SXboxOneEventType
{
	enum class FieldType
	{
		Int32 = 5,
		UInt32 = 6,
		Int64 = 7,
		UInt64 = 8,
		Float = 9,
		Double = 10,
		Boolean = 11,
		UnicodeString = 0,
		GUID = 13,
	};

	char* m_name;
	int	m_numParams;

	char** m_paramName;
	FieldType* m_paramType;

	SXboxOneEventType* m_prev;
	SXboxOneEventType* m_next;

	SXboxOneEventType(const char* name, int numParams);
	~SXboxOneEventType();
	
	void AddParam(int index, const char* paramName, FieldType paramType);
};

class XboxStatsManager
{
public:
	struct StatValue
	{
		enum class Type {
			None,
			Numeric,
			String,
		};

		Type type;

		double numval;
		std::string stringval;

		StatValue() : type(Type::None) {}
		StatValue(double numval) : type(Type::Numeric), numval(numval) {}
		StatValue(const std::string& stringval) : type(Type::String), stringval(stringval) {}
	};

	static bool add_user(uint64_t user_id, const char* gm_func_name);
	static bool remove_user(uint64_t user_id, const char *gm_func_name);

	static bool flush_user(uint64_t user_id, bool high_priority, const char* gm_func_name);
	static void background_flush();

	static StatValue get_stat(uint64_t user_id, const std::string& stat_name, const char *gm_func_name);
	static bool set_stat_numeric(uint64_t user_id, const std::string& stat_name, double stat_value, const char* gm_func_name);
	static bool set_stat_string(uint64_t user_id, const std::string& stat_name, const std::string &stat_value, const char* gm_func_name);
	static bool delete_stat(uint64_t user_id, const std::string& stat_name, const char* gm_func_name);

	static std::vector<std::string> get_stat_names(uint64_t user_id, const char* gm_func_name);

private:
	struct Statistic
	{
		XblTitleManagedStatType m_type;

		double m_numval;
		std::string m_stringval;

		/* Possible states of a stat:
		 *
		 * Status                  | m_dirty | m_flushing
		 * ------------------------+---------+-----------
		 * Up to date              | false   | false
		 * Changed locally         | true    | false
		 * Uploading current value | true    | true
		 * Uploading old value     | false   | true
		*/

		bool m_dirty;
		bool m_flushing;

		Statistic() :
			m_dirty(false),
			m_flushing(false) {}
	};

	struct User
	{
		std::map<std::string, Statistic> m_stats;
		std::map<std::string, Statistic> m_deleted_stats;

		bool m_flushing;  /* A flush is in progress. */
		bool m_removing;  /* The user is being removed. No flushes can be queued. */
		
		int64 m_last_flush;

		enum class FlushType
		{
			ManualFlush,
			RemoveUser
		};

		std::vector<FlushType> m_queued_flushes;  /* Queued up flush event types. Multiple flushes
												   * of the same type may be queued so that
												   * back-to-back flushes of the same type get
												   * merged into a single upload but still fire the
												   * expected number of events.
												  */

		User() :
			m_flushing(false),
			m_removing(false),
			m_last_flush(Timing_Time()) {}
	};

	typedef std::map<uint64_t, User>::iterator UsersMapIter;

	static std::map<uint64_t, User> m_users;
	static std::mutex m_lock;

	struct FlushUserContext
	{
		uint64_t user_id;
		std::vector<User::FlushType> queued_flushes;
		const char* gm_func_name;

		XblContextHandle xbl_ctx;
		bool fire_events;

		XAsyncBlock async;

		FlushUserContext(uint64_t user_id, const std::vector<User::FlushType>& queued_flushes, const char* gm_func_name, XblContextHandle xbl_ctx, bool fire_events);
		~FlushUserContext();
	};

	static bool _flush_user(std::unique_lock<std::mutex>& lock, uint64_t user_id, const std::vector<User::FlushType>& queued_flushes, bool fire_events, const char* gm_func_name);
	static bool _flush_user_update(FlushUserContext *ctx, std::unique_lock<std::mutex>& lock, UsersMapIter user);
	static bool _flush_user_delete(FlushUserContext* ctx, std::unique_lock<std::mutex>& lock, UsersMapIter user);
	static void _flush_user_finish(FlushUserContext* ctx, std::unique_lock<std::mutex>& lock, UsersMapIter user, HRESULT error);
};

extern SXboxOneEventType* g_xbox_one_event_head;

class XboxStatLoadError : public std::runtime_error
{
public:
	XboxStatLoadError(const char* what) : runtime_error(what) {}
};

void Xbox_Stat_Load_XML(const char *manifest_filename);
