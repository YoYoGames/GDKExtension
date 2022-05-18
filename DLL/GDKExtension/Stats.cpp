//
// Copyright (C) 2020 Opera Norway AS. All rights reserved.
//
// This file is an original work developed by Opera.
//

/* Xbox stats manifest handling for GDK runner.
 * Derived from XboxOneRunner/XboxOneRunner/DynamicEvents/ProviderGenerator.h.
*/

#include <inttypes.h>
#include <stdexcept>
#include <vector>
#include <winerror.h>
#include <json-c/json.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <xsapi-c/services_c.h>

#include "GDKX.h"
#include "UserManagement.h"
#include "Stats.h"

#define STATS_FLUSH_INTERVAL_US 60000000 /* 1 minute */

extern char* g_XboxSCID;

SXboxOneEventType* g_xbox_one_event_head = NULL;

SXboxOneEventType::SXboxOneEventType(const char* name, int numParams) : m_numParams(numParams)
{
	m_name = (char*)YYStrDup(name);

	m_paramName = new char*[numParams];
	m_paramType = new FieldType[numParams];

	for (int i = 0; i < numParams; ++i)
	{
		m_paramName[i] = NULL;
	}

	m_prev = nullptr;
	m_next = g_xbox_one_event_head;

	g_xbox_one_event_head = this;
}

SXboxOneEventType::~SXboxOneEventType()
{
	if (m_next)
	{
		m_next->m_prev = m_prev;
	}

	if (m_prev)
	{
		m_prev->m_next = m_next;
	}

	m_next = nullptr;
	m_prev = nullptr;

	if (m_name)
	{
		free(m_name);
		m_name = nullptr;
	}

	if (m_paramName)
	{
		for (int i = 0; i < m_numParams; ++i)
		{
			free(m_paramName[i]);
		}

		free(m_paramName);
	}

	if (m_paramType)
	{
		free(m_paramType);
		m_paramType = nullptr;
	}

	m_numParams = 0;
}

void SXboxOneEventType::AddParam(int index, const char* paramName, FieldType paramType)
{
	if (index < m_numParams)
	{
		m_paramName[index] = (char*)(YYStrDup(paramName));
		m_paramType[index] = paramType;
	}
}

#if 0
/* Load Xbox stats data from STAT chunk.
 * See WADSaver.cs/WriteEventManifest in the Asset Compiler for the other side.
*/
bool Xbox_Stat_Load(uint8* _pChunk, uint32 _sz, uint8* _pBase)
{
	class XboxStatLoadError : public std::runtime_error
	{
	public:
		XboxStatLoadError(const char* what) : runtime_error(what) {}
	};

	try {
		uint32 pos = 0;

		auto read_string = [&]()
		{
			const uint8* begin = _pChunk + pos;
			const uint8* end = (const uint8*)(memchr(begin, '\0', (_sz - pos)));

			if (end == NULL)
			{
				throw XboxStatLoadError("Unexpected EOF in Xbox_Stat_Load()::read_string()");
			}

			pos += (uint32)(end - begin) + 1;

			return (const char*)(begin);
		};

		auto read_int32 = [&]()
		{
			if ((pos + sizeof(int32)) > _sz)
			{
				throw XboxStatLoadError("Unexpected EOF in Xbox_Stat_Load()::read_int32()");
			}

			int32 value = *(int32*)(_pChunk + pos);
			pos += sizeof(int32);

			return value;
		};

		/* Skip over EtxManifest/Provider/Name */
		read_string();

		/* Skip over EtxManifest/Provider/Guid */
		pos += 16;

		read_int32(); /* Skip over EtxManifest/DefaultSettings/Latency */
		read_int32(); /* Skip over EtxManifest/DefaultSettings/Priority */
		read_int32(); /* Skip over EtxManifest/DefaultSettings/UploadEnabled */
		read_int32(); /* Skip over EtxManifest/DefaultSettings/PopulationSampleRate */

		int32 num_events = read_int32();

		// iterate over events
		for (int32 j = 0; j < num_events; j++)
		{
			const char* event_name = read_string(); /* Read in EtxManifest/Events/Event.Name */

			read_string(); /* Skip schema */
			read_int32();  /* Skip EtxManifest/Events/Event.Latency */
			read_int32();  /* Skip EtxManifest/Events/Event.Priority */
			read_int32();  /* Skip EtxManifest/Events/Event.UploadEnabled */
			read_int32();  /* Skip EtxManifest/Events/Event.PopulationSampleRate */

			int32 event_index = read_int32(); /* Read event_index */
			if (event_index != (j + 1))
			{
				throw XboxStatLoadError("Unexpected event_index value in Xbox_Stat_Load()");
			}

			read_int32(); /* Skip EtxManifest/Events/Event/PartC.version */

			int32 num_fields = read_int32();

			SXboxOneEventType* sxoet = new SXboxOneEventType(event_name, num_fields - 1);

			read_int32(); /* Skip implicit clock field. */

			for (int32 k = 0; k < (num_fields - 1); ++k)
			{
				const char* field_name = read_string();
				SXboxOneEventType::FieldType field_type = (SXboxOneEventType::FieldType)(read_int32());

				sxoet->AddParam(k, field_name, field_type);
			}
		}
	}
	catch (const XboxStatLoadError& e)
	{
		DebugConsoleOutput("%s\n", e.what());
	}
	return true;
}
#endif

/* Load Xbox stats data from a manifest. */
void Xbox_Stat_Load_XML(const char *manifest_filename)
{
	xmlDocPtr xml = xmlReadFile(manifest_filename, NULL, 0);
	if (xml == NULL)
	{
		throw XboxStatLoadError((std::string("Unable to parse ") + manifest_filename).c_str());
	}

	/* Find the Events node... */

	xmlNodePtr root_node = xmlDocGetRootElement(xml);
	if (root_node == NULL
		|| root_node->type != XML_ELEMENT_NODE
		|| strcmp((const char*)(root_node->name), "EtxManifest") != 0)
	{
		xmlFreeDoc(xml);
		throw XboxStatLoadError((std::string("ExtManifest node not found in ") + manifest_filename).c_str());
	}

	xmlNodePtr events_node = root_node->children;
	while (events_node != NULL && (events_node->type != XML_ELEMENT_NODE || strcmp((const char*)(events_node->name), "Events") != 0))
	{
		events_node = events_node->next;
	}

	if (events_node == NULL)
	{
		xmlFreeDoc(xml);
		throw XboxStatLoadError((std::string("Events node not found in ") + manifest_filename).c_str());
	}

	/* Iterate over the Event nodes and register each one... */

	auto get_attr_value = [&](xmlNodePtr node, const char* attr_name)
	{
		assert(node != NULL);
		assert(node->type == XML_ELEMENT_NODE);

		xmlAttrPtr attr = node->properties;

		while (attr != NULL)
		{
			assert(attr->type == XML_ATTRIBUTE_NODE);

			if (strcmp((const char*)(attr->name), attr_name) == 0)
			{
				xmlChar* s = xmlNodeListGetString(xml, attr->children, 1);
				std::string s2 = (const char*)(s);
				xmlFree(s);

				return s2;
			}

			attr = attr->next;
		}

		xmlFreeDoc(xml);
		throw XboxStatLoadError((std::string("Missing '") + attr_name + " attribute in '" + (const char*)(node->name) + "' element").c_str());
	};

	for (xmlNodePtr event_node = events_node->children; event_node != NULL; event_node = event_node->next)
	{
		if (event_node->type != XML_ELEMENT_NODE || strcmp((const char*)(event_node->name), "Event") != 0)
		{
			/* Not an Event node. Skip. */
			continue;
		}

		std::string event_name = get_attr_value(event_node, "Name");

		/* Field nodes are nested within PartB/PartC nodes within the Event, so we have iterate two
		 * levels deep to find them...
		*/

		std::vector< std::pair< std::string, SXboxOneEventType::FieldType> > fields;

		for (xmlNodePtr event_child_node = event_node->children; event_child_node != NULL; event_child_node = event_child_node->next)
		{
			if (event_child_node->type != XML_ELEMENT_NODE)
			{
				continue;
			}

			for (xmlNodePtr fields_node = event_child_node->children; fields_node != NULL; fields_node = fields_node->next)
			{
				if (fields_node->type != XML_ELEMENT_NODE)
				{
					continue;
				}

				if (strcmp((const char*)(fields_node->name), "Fields") != 0)
				{
					/* Not the Fields. Skip. */
					continue;
				}

				for (xmlNodePtr field_node = fields_node->children; field_node != NULL; field_node = field_node->next)
				{
					if (field_node->type != XML_ELEMENT_NODE)
					{
						continue;
					}

					if (strcmp((const char*)(field_node->name), "Field") != 0)
					{
						/* Not a Field node. Skip. */
						continue;
					}

					std::string field_name = get_attr_value(field_node, "Name");
					std::string field_type = get_attr_value(field_node, "Type");

					struct XmlFieldTypeMapping {
						const char* xml_attr_string;
						SXboxOneEventType::FieldType type;
					};

					static const XmlFieldTypeMapping xml_field_types[] = {
						{ "Int32",          SXboxOneEventType::FieldType::Int32 },
						{ "UInt32",         SXboxOneEventType::FieldType::UInt32 },
						{ "Int64",          SXboxOneEventType::FieldType::Int64 },
						{ "UInt64",         SXboxOneEventType::FieldType::UInt64 },
						{ "Float",          SXboxOneEventType::FieldType::Float },
						{ "Double",         SXboxOneEventType::FieldType::Double },
						{ "Boolean",        SXboxOneEventType::FieldType::Boolean },
						{ "UnicodeString",  SXboxOneEventType::FieldType::UnicodeString },
						{ "GUID",           SXboxOneEventType::FieldType::GUID },
					};

					bool found_type = false;
					SXboxOneEventType::FieldType mapped_field_type;

					for (int i = 0; i < (sizeof(xml_field_types) / sizeof(*xml_field_types)); ++i)
					{
						if (field_type == xml_field_types[i].xml_attr_string)
						{
							mapped_field_type = xml_field_types[i].type;
							found_type = true;
							break;
						}
					}

					if (found_type)
					{
						fields.push_back(std::make_pair(field_name, mapped_field_type));
					}
					else {
						DebugConsoleOutput("Unsupported field type '%s' found in event '%s', skipping event\n", field_type.c_str(), event_name.c_str());
						goto NEXT_EVENT;
					}
				}
			}
		}

		{
			/* Register the event for F_XboxOneFireEvent() to make use of. */

			DebugConsoleOutput("Loaded event '%s' with %zu fields in %s\n", event_name.c_str(), fields.size(), manifest_filename);

			SXboxOneEventType* sxoet = new SXboxOneEventType(event_name.c_str(), (int)(fields.size()));

			for (int i = 0; i < (int)(fields.size()); ++i)
			{
				sxoet->AddParam(i, fields[i].first.c_str(), fields[i].second);
			}
		}

		NEXT_EVENT:;
	}

	xmlFreeDoc(xml);
}

std::map<uint64_t, XboxStatsManager::User> XboxStatsManager::m_users;
std::mutex XboxStatsManager::m_lock;

bool XboxStatsManager::add_user(uint64_t user_id, const char* gm_func_name)
{
	XblContextHandle xbl_ctx;

	{
		XUM_LOCK_MUTEX;
		XUMuser* xuser = XUM::GetUserFromId(user_id);
		if (xuser == NULL)
		{
			DebugConsoleOutput("%s - couldn't get user\n", gm_func_name);
			return false;
		}

		HRESULT hr = XblContextCreateHandle(xuser->user, &xbl_ctx);
		if (!SUCCEEDED(hr))
		{
			DebugConsoleOutput("%s - couldn't create xbl handle (HRESULT 0x%08X)\n", gm_func_name, (unsigned)(hr));
			return false;
		}
	}

	XblHttpCallHandle http_req;

	char stats_url[256];
	snprintf(stats_url, sizeof(stats_url), "https://statsread.xboxlive.com/stats/users/%" PRIu64 "/scids/%s", user_id, g_XboxSCID);

	HRESULT hr = XblHttpCallCreate(xbl_ctx, "GET", stats_url, &http_req);
	if (!SUCCEEDED(hr))
	{
		DebugConsoleOutput("%s - couldn't create HTTP request handle (HRESULT 0x%08X)\n", gm_func_name, (unsigned)(hr));

		XblContextCloseHandle(xbl_ctx);
		return false;
	}

	struct AddUserContext
	{
		uint64_t user_id;
		const char* gm_func_name;

		XblContextHandle xbl_ctx;
		XblHttpCallHandle http_req;

		XAsyncBlock async;

		AddUserContext(uint64_t user_id, const char *gm_func_name, XblContextHandle xbl_ctx, XblHttpCallHandle http_req) :
			user_id(user_id), gm_func_name(gm_func_name), xbl_ctx(xbl_ctx), http_req(http_req) {}

		~AddUserContext()
		{
			XblHttpCallCloseHandle(http_req);
			XblContextCloseHandle(xbl_ctx);
		}
	};

	AddUserContext* ctx = new AddUserContext(user_id, gm_func_name, xbl_ctx, http_req);

	ctx->async.queue = NULL;
	ctx->async.context = ctx;
	ctx->async.callback = [](XAsyncBlock* async)
	{
		AddUserContext* ctx = (AddUserContext*)(async->context);

		/* Delete context when callback returns. */
		std::unique_ptr<AddUserContext> ctx_guard(ctx);

		std::unique_lock<std::mutex> lock(m_lock);

		auto user = m_users.find(ctx->user_id);
		if (user == m_users.end())
		{
			/* User was removed before adding finished. */
			return;
		}

		std::map<std::string, Statistic>& stats = user->second.m_stats;

		const char* stats_response_body = NULL;

		HRESULT hr = XAsyncGetStatus(async, true);
		if (SUCCEEDED(hr))
		{
			hr = XblHttpCallGetResponseString(ctx->http_req, &stats_response_body);
		}

		if (SUCCEEDED(hr))
		{
			/* Example response payload (pretty-ified):
			 *
			 * {
			 *   "$schema": "http://stats.xboxlive.com/2017-1/schema#",
			 *   "revision": 29736851929,
			 *   "previousRevision": 29736572560,
			 *   "timestamp": "2021-03-05T14:17:12.8040952Z",
			 *   "stats": {
			 *     "title": {
			 *       "stat_name_1": { "value": 78 },
			 *       "stat_name_2": { "value": 68 }
			 *     }
			 *   }
			 * }
			*/

			json_object* json_root = json_tokener_parse(stats_response_body);
			if (json_root != NULL)
			{
				json_object* stats_obj = json_object_object_get(json_root, "stats");

				if (stats_obj != NULL)
				{
					stats_obj = json_object_object_get(stats_obj, "title");

					if (stats_obj != NULL && !json_object_is_type(stats_obj, json_type_object))
					{
						stats_obj = NULL;
					}
				}

				if (stats_obj == NULL)
				{
					DebugConsoleOutput("%s - stats not found in data from Xbox Live\n", ctx->gm_func_name);
				}
				else{
					json_object_object_foreach(stats_obj, stats_key, stats_val)
					{
						if (stats.find(stats_key) != stats.end())
						{
							/* Probably means someone set a statistic without waiting for the LocalUserAdded event. */
							DebugConsoleOutput("%s - skipping already-defined stat '%s'\n", ctx->gm_func_name, stats_key);
						}
						else {
							json_object* value = json_object_object_get(stats_val, "value");
							if (value != NULL)
							{
								if (json_object_is_type(value, json_type_double) || json_object_is_type(value, json_type_int))
								{
									stats[stats_key].m_numval = json_object_get_double(value);
									stats[stats_key].m_type = XblTitleManagedStatType::Number;
								}
								else if (json_object_is_type(value, json_type_string))
								{
									stats[stats_key].m_stringval = json_object_get_string(value);
									stats[stats_key].m_type = XblTitleManagedStatType::String;
								}
								else {
									DebugConsoleOutput("%s - unexpected value type for stat '%s'\n", ctx->gm_func_name, stats_key);
								}
							}
							else {
								DebugConsoleOutput("%s - value not found for stat '%s'\n", ctx->gm_func_name, stats_key);
							}
						}
					}
				}

				/* Free deserialised JSON object. */
				json_object_put(json_root);
			}
			else {
				DebugConsoleOutput("%s - couldn't to parse stats data from Xbox Live\n", ctx->gm_func_name);
			}
		}
		else {
			DebugConsoleOutput("%s - couldn't download stats from Xbox Live (HRESULT 0x%08X)\n", ctx->gm_func_name, (unsigned)(hr));
		}

		lock.unlock();

		// Fire off LocalUserAdded event
		int dsMapIndex = CreateDsMap(3,
			"id", (double)e_achievement_stat_event, nullptr,
			"event", (double)(0.0), "LocalUserAdded",
			"error", (double)(hr), nullptr);

		// Add user id
		DsMapAddInt64(dsMapIndex, "userid", ctx->user_id);

		CreateAsyncEventWithDSMap(dsMapIndex, EVENT_OTHER_SOCIAL);
	};

	std::unique_lock<std::mutex> lock(m_lock);

	hr = XblHttpCallPerformAsync(http_req, XblHttpCallResponseBodyType::String, &(ctx->async));
	if (SUCCEEDED(hr))
	{
		m_users[user_id] = User();
		return true;
	}
	else
	{
		DebugConsoleOutput("%s - couldn't download stats from Xbox Live (HRESULT 0x%08X)\n", gm_func_name, (unsigned)(hr));

		delete ctx;

		return false;
	}
}

bool XboxStatsManager::remove_user(uint64_t user_id, const char *gm_func_name)
{
	std::unique_lock<std::mutex> lock(m_lock);

	auto user = m_users.find(user_id);
	if (user == m_users.end())
	{
		DebugConsoleOutput("%s - user_id %" PRIu64 " not known, did you forget to call xboxone_stats_add_user?\n", gm_func_name, user_id);
		return false;
	}

	if (user->second.m_removing)
	{
		DebugConsoleOutput("%s - user is already being removed\n", gm_func_name);
		return false;
	}

	user->second.m_removing = true;

	if (!user->second.m_flushing)
	{
		std::vector<User::FlushType> queued_flushes = { User::FlushType::RemoveUser };
		_flush_user(lock, user_id, queued_flushes, false, gm_func_name);
	}
	else {
		user->second.m_queued_flushes.push_back(User::FlushType::RemoveUser);
	}

	return true;
}

bool XboxStatsManager::flush_user(uint64_t user_id, bool high_priority, const char *gm_func_name)
{
	std::unique_lock<std::mutex> lock(m_lock);

	auto user = m_users.find(user_id);
	if (user == m_users.end())
	{
		DebugConsoleOutput("%s - user_id %" PRIu64 " not known, did you forget to call xboxone_stats_add_user?\n", gm_func_name, user_id);
		return false;
	}

	if (user->second.m_removing)
	{
		DebugConsoleOutput("%s - user is being removed\n", gm_func_name);
		return false;
	}

	if (user->second.m_flushing)
	{
		/* This user is already being flushed. Queue it to be flushed again afterwards, but only
		 * if high priority.
		*/

		if (high_priority)
		{
			user->second.m_queued_flushes.push_back(User::FlushType::ManualFlush);
			return true;
		}
		else {
			DebugConsoleOutput("%s - a flush is already in progress\n", gm_func_name);
			return false;
		}
	}
	else {
		std::vector<User::FlushType> queued_flushes = { User::FlushType::ManualFlush };
		return _flush_user(lock, user_id, queued_flushes, false, gm_func_name);
	}
}

/* Initiate a flush of a user's stats
 * There MUST NOT be a flush in progress when this function is called.
 *
 * When stats are flushed, first this function does some setup, then any locally created or
 * modified stats are pushed to the service by _flush_user_update(), then any stats that have been
 * deleted are removed from the service by _flush_user_delete() and finally any events are fired
 * and any pending flush initiated by the _flush_user_finish() function.
*/
bool XboxStatsManager::_flush_user(std::unique_lock<std::mutex>& lock, uint64_t user_id, const std::vector<User::FlushType>& queued_flushes, bool fire_events, const char* gm_func_name)
{
	auto user = m_users.find(user_id);
	if (user == m_users.end())
	{
		DebugConsoleOutput("%s - user_id %" PRIu64 " not known, did you forget to call xboxone_stats_add_user?\n", gm_func_name, user_id);
		return false;
	}

	assert(!user->second.m_flushing);

	user->second.m_last_flush = Timing_Time();

	XblContextHandle xbl_ctx;

	{
		XUM_LOCK_MUTEX;
		XUMuser* xuser = XUM::GetUserFromId(user_id);
		if (xuser == NULL)
		{
			DebugConsoleOutput("%s - couldn't get user\n", gm_func_name);
			return false;
		}

		HRESULT hr = XblContextCreateHandle(xuser->user, &xbl_ctx);
		if (!SUCCEEDED(hr))
		{
			DebugConsoleOutput("%s - couldn't create xbl handle (HRESULT 0x%08X)\n", gm_func_name, (unsigned)(hr));
			return false;
		}
	}

	FlushUserContext* ctx = new FlushUserContext(user_id, queued_flushes, gm_func_name, xbl_ctx, fire_events);

	user->second.m_flushing = true;

	return _flush_user_update(ctx, lock, user);
}

/* Push locally set stats to Xbox Live */
bool XboxStatsManager::_flush_user_update(FlushUserContext *ctx, std::unique_lock<std::mutex>& lock, UsersMapIter user)
{
	ctx->async.callback = [](XAsyncBlock* async)
	{
		FlushUserContext* ctx = (FlushUserContext*)(async->context);

		std::unique_lock<std::mutex> lock(m_lock);

		auto user = m_users.find(ctx->user_id);
		assert(user != m_users.end());

		std::map<std::string, Statistic>& stats = user->second.m_stats;

		HRESULT status = XAsyncGetStatus(async, false);

		if (SUCCEEDED(status))
		{
			/* Call to XblTitleManagedStatsUpdateStatsAsync() succeeded, mark any statistics that
			 * were flushed as part of the call as clean and then proceed to deletion step.
			*/

			for (auto stat = stats.begin(); stat != stats.end(); ++stat)
			{
				if (stat->second.m_flushing)
				{
					stat->second.m_dirty = false;
					stat->second.m_flushing = false;
				}
			}

			_flush_user_delete(ctx, lock, user);
		}
		else {
			DebugConsoleOutput("%s - XblTitleManagedStatsUpdateStatsAsync failed (HRESULT 0x%08X)\n", ctx->gm_func_name, (unsigned)(status));

			for (auto stat = stats.begin(); stat != stats.end(); ++stat)
			{
				stat->second.m_flushing = false;
			}

			_flush_user_finish(ctx, lock, user, status);
		}
	};

	std::map<std::string, Statistic>& stats = user->second.m_stats;

	/* Build a list of stats which have been set to be uploaded to Xbox Live. */

	std::vector<XblTitleManagedStatistic> xbl_stats;

	for (auto stat = stats.begin(); stat != stats.end(); ++stat)
	{
		if (!stat->second.m_dirty)
		{
			/* Stat doesn't need updating. */
			continue;
		}

		XblTitleManagedStatistic xbl_stat;

		xbl_stat.statisticName = stat->first.c_str();

		switch (stat->second.m_type)
		{
		case XblTitleManagedStatType::Number:
			xbl_stat.statisticType = XblTitleManagedStatType::Number;
			xbl_stat.numberValue = stat->second.m_numval;
			xbl_stat.stringValue = NULL;
			break;

		case XblTitleManagedStatType::String:
			xbl_stat.statisticType = XblTitleManagedStatType::String;
			xbl_stat.stringValue = stat->second.m_stringval.c_str();
			xbl_stat.numberValue = NAN;
			break;

		default:
			abort(); /* Unreachable. */
		}

		xbl_stats.push_back(xbl_stat);

		stat->second.m_flushing = true;
	}

	if (!xbl_stats.empty())
	{
		HRESULT hr = XblTitleManagedStatsUpdateStatsAsync(ctx->xbl_ctx, xbl_stats.data(), xbl_stats.size(), &(ctx->async));
		if (SUCCEEDED(hr))
		{
			ctx->fire_events = true;
			return true;
		}
		else {
			DebugConsoleOutput("%s - XblTitleManagedStatsUpdateStatsAsync failed (HRESULT 0x%08X)\n", ctx->gm_func_name, (unsigned)(hr));

			for (auto stat = stats.begin(); stat != stats.end(); ++stat)
			{
				stat->second.m_flushing = false;
			}

			_flush_user_finish(ctx, lock, user, hr);
			return false;
		}
	}
	else {
		/* No stats to update. Proceed to deletion step. */
		return _flush_user_delete(ctx, lock, user);
	}
}

/* Remove locally deleted stats from Xbox Live */
bool XboxStatsManager::_flush_user_delete(FlushUserContext* ctx, std::unique_lock<std::mutex>& lock, UsersMapIter user)
{
	ctx->async.callback = [](XAsyncBlock* async)
	{
		FlushUserContext* ctx = (FlushUserContext*)(async->context);

		std::unique_lock<std::mutex> lock(m_lock);

		auto user = m_users.find(ctx->user_id);
		assert(user != m_users.end());

		auto& deleted_stats = user->second.m_deleted_stats;

		HRESULT status = XAsyncGetStatus(async, false);

		if (SUCCEEDED(status))
		{
			/* Call to XblTitleManagedStatsDeleteStatsAsync() succeeded, erase any deleted stats
			 * that were erased in this transaction and proceed to fire events.
			*/

			for (auto s = deleted_stats.begin(); s != deleted_stats.end();)
			{
				if (s->second.m_flushing)
				{
					s = deleted_stats.erase(s);
				}
			}
		}
		else {
			DebugConsoleOutput("%s - XblTitleManagedStatsDeleteStatsAsync failed (HRESULT 0x%08X)\n", ctx->gm_func_name, (unsigned)(status));

			for (auto s = deleted_stats.begin(); s != deleted_stats.end(); ++s)
			{
				s->second.m_flushing = false;
			}
		}

		_flush_user_finish(ctx, lock, user, status);
	};

	auto& deleted_stats = user->second.m_deleted_stats;

	if (deleted_stats.empty())
	{
		/* No stats to delete. Proceed to firing events and cleaning up. */
		_flush_user_finish(ctx, lock, user, S_OK);
		return true;
	}
	else {
		std::vector<const char*> deleted_stat_names;
		deleted_stat_names.reserve(deleted_stats.size());

		for (auto s = deleted_stats.begin(); s != deleted_stats.end(); ++s)
		{
			assert(!s->second.m_flushing);

			deleted_stat_names.push_back(s->first.c_str());
			s->second.m_flushing = true;
		}

		HRESULT hr = XblTitleManagedStatsDeleteStatsAsync(ctx->xbl_ctx, deleted_stat_names.data(), deleted_stat_names.size(), &(ctx->async));
		if (SUCCEEDED(hr))
		{
			ctx->fire_events = true;
			return true;
		}
		else {
			DebugConsoleOutput("%s - XblTitleManagedStatsDeleteStatsAsync failed (HRESULT 0x%08X)\n", ctx->gm_func_name, (unsigned)(hr));

			for (auto s = deleted_stats.begin(); s != deleted_stats.end(); ++s)
			{
				assert(s->second.m_flushing);
				s->second.m_flushing = false;
			}
		}
	}
	return true;
}

/* Finish a user stats flush operation. */
void XboxStatsManager::_flush_user_finish(FlushUserContext* ctx, std::unique_lock<std::mutex>& lock, UsersMapIter user, HRESULT error)
{
	user->second.m_flushing = false;

	/* We should only raise events if running from an async callback (fire_events is true), since
	 * an early failure when starting a flush is communicated via the return value of the function
	 * that initiated it rather than through an async event.
	 *
	 * We also fire outside of the async callback if there was no error, which should only happen
	 * if a flush is explicitly requested or a user is removed from the stats manager with no stat
	 * updates pending, which isn't ideal, but unlikely to break anything since these requests
	 * don't have an ID returned by the stats functions.
	*/
	if (ctx->fire_events || SUCCEEDED(error))
	{
		for (auto i = ctx->queued_flushes.begin(); i != ctx->queued_flushes.end(); ++i)
		{
			if (*i == User::FlushType::ManualFlush)
			{
				/* We must release our hold of m_lock when dispatching events, otherwise we could
				 * deadlock if the game code tries calling a stats API function.
				*/

				lock.unlock();

				int dsMapIndex = CreateDsMap(3,
					"id", (double)e_achievement_stat_event, nullptr,
					"event", (double)(0.0), "StatisticUpdateComplete",
					"error", (double)(error), nullptr);

				// Add user id
				DsMapAddInt64(dsMapIndex, "userid", ctx->user_id);

				CreateAsyncEventWithDSMap(dsMapIndex, EVENT_OTHER_SOCIAL);

				lock.lock();

				/* It shouldn't be possible for the user to be erased yet. */
				assert(m_users.find(ctx->user_id) == user);
			}
			else if (*i == User::FlushType::RemoveUser)
			{
				/* A call to xboxone_stats_remove_user() was made, it should be the last thing a
				 * flush was queued for.
				 *
				 * If the flush succeeded, remove the user, otherwise mark it as no longer being
				 * removed so the program can try again.
				*/

				assert(user->second.m_removing);
				assert(user->second.m_queued_flushes.empty());
				assert(std::next(i) == ctx->queued_flushes.end());

				if (SUCCEEDED(error))
				{
					m_users.erase(user);
				}
				else {
					user->second.m_removing = false;
				}

				lock.unlock();

				int dsMapIndex = CreateDsMap(3,
					"id", (double)e_achievement_stat_event, nullptr,
					"event", (double)(0.0), "LocalUserRemoved",
					"error", (double)(error), nullptr);

				// Add user id
				DsMapAddInt64(dsMapIndex, "userid", ctx->user_id);

				CreateAsyncEventWithDSMap(dsMapIndex, EVENT_OTHER_SOCIAL);

				delete ctx;
				return;
			}
		}
	}

	if (!user->second.m_queued_flushes.empty())
	{
		/* One or more further flushes were queued while this flush was in progress - either
		 * high-priority ones or the user is being removed. Start them.
		*/

		std::vector<User::FlushType> queued_flushes;
		queued_flushes.swap(user->second.m_queued_flushes);

		_flush_user(lock, ctx->user_id, queued_flushes, true, "queued stats flush");
	}

	delete ctx;
}

XboxStatsManager::FlushUserContext::FlushUserContext(uint64_t user_id, const std::vector<User::FlushType>& queued_flushes, const char* gm_func_name, XblContextHandle xbl_ctx, bool fire_events) :
	user_id(user_id),
	queued_flushes(queued_flushes),
	gm_func_name(gm_func_name),
	xbl_ctx(xbl_ctx),
	fire_events(fire_events)
{
	async.queue = NULL;
	async.context = this;
	async.callback = [](XAsyncBlock* async)
	{
		/* The correct callback should be installed before an async op begins. */
		abort();
	};
}

XboxStatsManager::FlushUserContext::~FlushUserContext()
{
	XblContextCloseHandle(xbl_ctx);
}

void XboxStatsManager::background_flush()
{
	std::unique_lock<std::mutex> lock(m_lock);

	int64 now = Timing_Time();

	for (auto u = m_users.begin(); u != m_users.end(); ++u)
	{
		if ((now - u->second.m_last_flush) >= STATS_FLUSH_INTERVAL_US && !u->second.m_flushing && !u->second.m_removing)
		{
			DebugConsoleOutput("Starting background stats sync for user id %" PRIu64 "\n", u->first);

			const std::vector<User::FlushType> NO_EVENTS;
			_flush_user(lock, u->first, NO_EVENTS, false, "background stats flush");
		}
	}
}

XboxStatsManager::StatValue XboxStatsManager::get_stat(uint64_t user_id, const std::string& stat_name, const char* gm_func_name)
{
	std::unique_lock<std::mutex> lock(m_lock);

	auto user = m_users.find(user_id);
	if (user == m_users.end())
	{
		DebugConsoleOutput("%s - user_id %" PRIu64 " not known, did you forget to call xboxone_stats_add_user?\n", gm_func_name, user_id);
		return StatValue();
	}

	std::map<std::string, Statistic>& stats = user->second.m_stats;

	auto stat = stats.find(stat_name);

	if (stat != stats.end())
	{
		if (stat->second.m_type == XblTitleManagedStatType::Number)
		{
			return StatValue(stat->second.m_numval);
		}
		else {
			return StatValue(stat->second.m_stringval);
		}
	}
	else {
		return StatValue();
	}
}

bool XboxStatsManager::set_stat_numeric(uint64_t user_id, const std::string& stat_name, double stat_value, const char* gm_func_name)
{
	std::unique_lock<std::mutex> lock(m_lock);

	auto user = m_users.find(user_id);
	if (user == m_users.end())
	{
		DebugConsoleOutput("%s - user_id %" PRIu64 " not known, did you forget to call xboxone_stats_add_user?\n", gm_func_name, user_id);
		return false;
	}

	user->second.m_deleted_stats.erase(stat_name);

	user->second.m_stats[stat_name].m_type = XblTitleManagedStatType::Number;
	user->second.m_stats[stat_name].m_numval = stat_value;
	user->second.m_stats[stat_name].m_dirty = true;
	user->second.m_stats[stat_name].m_flushing = false;

	return true;
}

bool XboxStatsManager::set_stat_string(uint64_t user_id, const std::string& stat_name, const std::string& stat_value, const char* gm_func_name)
{
	std::unique_lock<std::mutex> lock(m_lock);

	auto user = m_users.find(user_id);
	if (user == m_users.end())
	{
		DebugConsoleOutput("%s - user_id %" PRIu64 " not known, did you forget to call xboxone_stats_add_user?\n", gm_func_name, user_id);
		return false;
	}

	user->second.m_deleted_stats.erase(stat_name);

	user->second.m_stats[stat_name].m_type = XblTitleManagedStatType::String;
	user->second.m_stats[stat_name].m_stringval = stat_value;
	user->second.m_stats[stat_name].m_dirty = true;
	user->second.m_stats[stat_name].m_flushing = false;

	return true;
}

bool XboxStatsManager::delete_stat(uint64_t user_id, const std::string& stat_name, const char *gm_func_name)
{
	std::unique_lock<std::mutex> lock(m_lock);

	auto user = m_users.find(user_id);
	if (user == m_users.end())
	{
		DebugConsoleOutput("%s - user_id %" PRIu64 " not known, did you forget to call xboxone_stats_add_user?\n", gm_func_name, user_id);
		return false;
	}

	auto& stats = user->second.m_stats;
	auto& deleted_stats = user->second.m_deleted_stats;

	auto stat = stats.find(stat_name);
	if (stat == stats.end())
	{
		DebugConsoleOutput("%s - stat_name \"%s\" not found\n", gm_func_name, stat_name.c_str());
		return false;
	}

	stat->second.m_flushing = false;

	deleted_stats.insert(*stat);
	stats.erase(stat);

	return true;
}

std::vector<std::string> XboxStatsManager::get_stat_names(uint64_t user_id, const char* gm_func_name)
{
	std::unique_lock<std::mutex> lock(m_lock);

	auto user = m_users.find(user_id);
	if (user == m_users.end())
	{
		DebugConsoleOutput("%s - user_id %" PRIu64 " not known, did you forget to call xboxone_stats_add_user?\n", gm_func_name, user_id);
		return std::vector<std::string>();
	}

	std::map<std::string, Statistic>& stats = user->second.m_stats;

	std::vector<std::string> stat_names;
	stat_names.reserve(stats.size());

	for (auto i = stats.begin(); i != stats.end(); ++i)
	{
		stat_names.push_back(i->first);
	}

	return stat_names;
}
