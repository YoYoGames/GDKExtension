//
// Copyright (C) 2021 Opera Norway AS. All rights reserved.
//
// This file is an original work developed by Opera.
//

/* NOTE: This file exists in two places and should be kept in sync!
 *
 * Locations:
 *
 * GameMaker    - Runner/VC_Runner/Files/Function/GDK/Function_IAPM.cpp
 * GDKExtension - DLL/GDKExtension/Function_IAPM.cpp
*/

#include <inttypes.h>
#include <map>
#include <memory>
#include <mutex>
#include <thread>
#include <tuple>
#include <utility>
#include <vector>
#include <windows.h>
#include <XGameRuntime.h>

#ifdef GDKEXTENSION_EXPORTS /* --- Building in GDK Extension */
#include "GDKX.h"
#include "UserManagement.h"

#define DS_LOCK_MUTEX /* Locking handled within extension API functions */

#else /* --- Building in GDK Runner */
#include "Files/Base/Common.h"
#include "Files/Code/Code_Constant.h"
#include "Files/Code/Code_Function.h"
#include "Files/Code/Code_Main.h"
#include "Files/IO/LoadSave.h"
#include "Files/LLVM/YYGML.h"
#include "Files/Support/Support_Data_Structures.h"
#include "GDKRunner/GDKRunner/UserManagement.h"

#define YYEXPORT static

#define CreateAsyncEventWithDSMap CreateAsynEventWithDSMap
#define DebugConsoleOutput dbg_csol.Output
#define DsMapAddBool AddBoolToDsMap
#define DsMapAddDouble AddToDsMap
#define DsMapAddString AddToDsMap
#define KIND_NAME_RValue KindName
#define YYCreateArray CreateArray
#define YYCreateString YYSetString

void CreateArray(RValue* pRValue, int n_args, ...);
void JS_GenericObjectConstructor(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg);

static inline void DsMapAddRValue(int _index, const char* _pKey, RValue* value)
{
	CDS_Map* dsMap = GetDsMap(_index);

	RValue key = { 0 };
	YYSetString(&key, _pKey);
	dsMap->Add(&key, value);
	FREE_RValue(&key);
}

static inline void YYStructCreate(RValue* _pStruct)
{
	YYObjectBase* obj = YYObjectBase::Alloc(0, VALUE_UNSET, OBJECT_KIND_YYOBJECTBASE);
	_pStruct->kind = VALUE_OBJECT;
	_pStruct->pObj = obj;
	JS_GenericObjectConstructor(*_pStruct, NULL, NULL, 0, NULL);
}

static inline void YYStructAddBool(RValue* _pStruct, const char* _pKey, double _value) { return _pStruct->pObj->Add(_pKey, _value, JS_BUILTIN_LENGTH_PROPERTY_DEFAULT); }
static inline void YYStructAddDouble(RValue* _pStruct, const char* _pKey, double _value) { return _pStruct->pObj->Add(_pKey, _value, JS_BUILTIN_LENGTH_PROPERTY_DEFAULT); }
static inline void YYStructAddInt(RValue* _pStruct, const char* _pKey, int _value) { return _pStruct->pObj->Add(_pKey, _value, JS_BUILTIN_LENGTH_PROPERTY_DEFAULT); }
static inline void YYStructAddRValue(RValue* _pStruct, const char* _pKey, RValue* _pValue) { return _pStruct->pObj->Add(_pKey, *_pValue, JS_BUILTIN_LENGTH_PROPERTY_DEFAULT); }
static inline void YYStructAddString(RValue* _pStruct, const char* _pKey, const char* _pValue) { return _pStruct->pObj->Add(_pKey, _pValue, JS_BUILTIN_LENGTH_PROPERTY_DEFAULT); }
#endif

bool StringToGUID(GUID* _guid, const char* _string);

/* From GDK documentation:
 *
 * > Passing more than 25 items per page will result in multiple service rounds trips being
 * > required for each page.
 *
 * Fetch the max per request for efficiency.
 */
static const uint32_t PRODUCT_QUERY_MAX_ITEMS_PER_PAGE = 25;

static const XStoreProductKind INVALID_PRODUCT_KIND = (XStoreProductKind)(0xFFFFFFFF);
static const XPackageKind INVALID_PACKAGE_KIND = (XPackageKind)(0xFFFFFFFF);
static const XPackageEnumerationScope INVALID_ENUMERATION_SCOPE = (XPackageEnumerationScope)(0xFFFFFFFF);

/* "id" for next IAP async event. */
static unsigned next_iap_id = 1;

/* Task queue used for most processing in this module. Work is performed by the thread pool and
 * completion callbacks are dispatched on the main Runner thread.
*/
static XTaskQueueHandle iap_background_to_main_queue = NULL;

/* Task queue used for some edge case processing in this module where the completion callbacks may
 * block. Work and completion callbacks are both dispatched by the thread pool.
*/
static XTaskQueueHandle iap_background_queue = NULL;

/* Wrapper around XPackageDetails that manages its own memory. */
struct MS_IAP_PackageDetails : public XPackageDetails
{
	MS_IAP_PackageDetails(const XPackageDetails* p)
	{
		packageIdentifier = YYStrDup(p->packageIdentifier);
		version = p->version;
		kind = p->kind;
		displayName = YYStrDup(p->displayName);
		description = YYStrDup(p->description);
		publisher = YYStrDup(p->publisher);
		storeId = YYStrDup(p->storeId);
		installing = p->installing;
	}

	MS_IAP_PackageDetails(const MS_IAP_PackageDetails& p) :
		MS_IAP_PackageDetails(&p) {}

	~MS_IAP_PackageDetails()
	{
		YYFree(storeId);
		YYFree(publisher);
		YYFree(description);
		YYFree(displayName);
		YYFree(packageIdentifier);
	}
};

struct ProductQueryContext
{
	unsigned async_id;
	XStoreContextHandle store_ctx;
	XAsyncBlock async;
	HRESULT(*results_func)(XAsyncBlock*, XStoreProductQueryHandle*);
	const char* async_type;
	const char* func_name;

	unsigned int page_count;

	RValue results;
	int n_results;

	int results_container;

	ProductQueryContext(XStoreContextHandle store_ctx, HRESULT(*results_func)(XAsyncBlock*, XStoreProductQueryHandle*), const char* async_type, const char* func_name) :
		async_id(next_iap_id++),
		store_ctx(store_ctx),
		async({ iap_background_to_main_queue, this, &_AsyncQueryCallback }),
		results_func(results_func),
		async_type(async_type),
		func_name(func_name),
		page_count(0),
		results({ 0 }),
		n_results(0)
	{
		YYCreateArray(&results, 0);

		/* Make sure the GC doesn't trip over unreachable "results" between ticks. */
		results_container = CreateDsMap(0);
		DsMapAddRValue(results_container, "results", &results);
	}

	~ProductQueryContext()
	{
		DestroyDsMap(results_container);
		FREE_RValue(&results);

		XStoreCloseContextHandle(store_ctx);
	}

	ProductQueryContext(const ProductQueryContext& src) = delete;
	ProductQueryContext& operator=(ProductQueryContext& rhs) = delete;

	static void _AsyncQueryCallback(XAsyncBlock* async);
	static bool _ProductQueryCallback(const XStoreProduct* product, void* context);
};

struct MS_IAP_LicenseHandle
{
	std::string id;
	XStoreLicenseHandle handle;
	XTaskQueueRegistrationToken lost_token;
	bool valid;
};

static std::mutex licenses_lock;
static std::map<std::string, MS_IAP_LicenseHandle*> store_licenses;
static std::map<std::string, MS_IAP_LicenseHandle*> pkg_licenses;

struct MS_IAP_MountedPackage
{
	std::string package_id;

	XPackageMountHandle mount_handle;
	std::string mount_path;

	bool unmounting;

	MS_IAP_MountedPackage(const std::string& package_id, XPackageMountHandle mount_handle, const std::string& mount_path) :
		package_id(package_id),
		mount_handle(mount_handle),
		mount_path(mount_path),
		unmounting(false) {}
};

static std::map<std::string, MS_IAP_MountedPackage*> mounted_packages;

static void MS_IAP_GameLicenseChanged(void* context);

static void _MS_IAP_DurableLicenseLost(void* context);
static void _MS_IAP_PackageLicenseLost(void* context);
static void _MS_IAP_CloseLicense(MS_IAP_LicenseHandle* license);

YYEXPORT void F_MS_IAP_AcquireLicenseForDurables(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg);
YYEXPORT void F_MS_IAP_ReleaseLicenseForDurables(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg);
YYEXPORT void F_MS_IAP_AcquireLicenseForPackage(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg);
YYEXPORT void F_MS_IAP_ReleaseLicenseForPackage(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg);
YYEXPORT void F_MS_IAP_CanAcquireLicenseForPackage(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg);
YYEXPORT void F_MS_IAP_CanAcquireLicenseForStoreId(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg);
YYEXPORT void F_MS_IAP_DownloadAndInstallPackages(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg);
YYEXPORT void F_MS_IAP_EnumeratePackages(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg);
YYEXPORT void F_MS_IAP_MountPackage(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg);
YYEXPORT void F_MS_IAP_QueryAddOnLicenses(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg);
YYEXPORT void F_MS_IAP_QueryAssociatedProducts(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg);
YYEXPORT void F_MS_IAP_QueryConsumableBalanceRemaining(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg);
YYEXPORT void F_MS_IAP_QueryEntitledProducts(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg);
YYEXPORT void F_MS_IAP_QueryGameLicense(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg);
YYEXPORT void F_MS_IAP_QueryProductForCurrentGame(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg);
YYEXPORT void F_MS_IAP_QueryProductForPackage(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg);
YYEXPORT void F_MS_IAP_QueryProducts(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg);
YYEXPORT void F_MS_IAP_ReportConsumableFulfillment(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg);
YYEXPORT void F_MS_IAP_ShowAssociatedProductsUI(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg);
YYEXPORT void F_MS_IAP_ShowProductPageUI(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg);
YYEXPORT void F_MS_IAP_ShowPurchaseUI(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg);
YYEXPORT void F_MS_IAP_ShowRateAndReviewUI(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg);
YYEXPORT void F_MS_IAP_ShowRedeemTokenUI(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg);
YYEXPORT void F_MS_IAP_UnmountPackage(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg);

static RValue _MS_IAP_StoreImage_to_Struct(const XStoreImage* image);
static RValue _MS_IAP_StoreProduct_to_Struct(const XStoreProduct* product);
static RValue _MS_IAP_StorePrice_to_Struct(const XStorePrice* price);
static RValue _MS_IAP_AddonLicense_to_Struct(const XStoreAddonLicense* license);
static RValue _MS_IAP_PackageDetails_to_Struct(const XPackageDetails* details);
static void _MS_IAP_AddStringOrUndefined(RValue* obj, const char* _pName, const char* _pString);
static XStoreContextHandle _MS_IAP_GetStoreHandle(RValue* arg, int user_id_arg_idx, const char* func);
static XStoreProductKind _MS_IAP_GetProductKinds(RValue* arg, int product_kinds_arg_idx, const char* func);
static std::vector<const char*> _MS_IAP_GetArrayOfStrings(RValue* arg, int arg_idx, const char* func);
static XPackageKind _MS_IAP_GetPackageKind(RValue* arg, int arg_idx, const char* func);
static XPackageEnumerationScope _MS_IAP_GetPackageEnumerationScope(RValue* arg, int arg_idx, const char* func);

void InitIAPFunctionsM()
{
	HRESULT hr = XTaskQueueCreate(XTaskQueueDispatchMode::ThreadPool, XTaskQueueDispatchMode::Manual, &iap_background_to_main_queue);
	if (FAILED(hr))
	{
		YYError("XTaskQueueCreate failed (HRESULT 0x%08X)\n", (unsigned)(hr));
	}

	hr = XTaskQueueCreate(XTaskQueueDispatchMode::ThreadPool, XTaskQueueDispatchMode::ThreadPool, &iap_background_queue);
	if (FAILED(hr))
	{
		YYError("XTaskQueueCreate failed (HRESULT 0x%08X)\n", (unsigned)(hr));
	}

#ifndef GDKEXTENSION_EXPORTS
	Function_Add("ms_iap_AcquireLicenseForDurables", &F_MS_IAP_AcquireLicenseForDurables, 2, false);
	Function_Add("ms_iap_ReleaseLicenseForDurables", &F_MS_IAP_ReleaseLicenseForDurables, 1, false);
	Function_Add("ms_iap_AcquireLicenseForPackage", &F_MS_IAP_AcquireLicenseForPackage, 2, false);
	Function_Add("ms_iap_ReleaseLicenseForPackage", &F_MS_IAP_ReleaseLicenseForPackage, 1, false);
	Function_Add("ms_iap_CanAcquireLicenseForPackage", &F_MS_IAP_CanAcquireLicenseForPackage, 2, false);
	Function_Add("ms_iap_CanAcquireLicenseForStoreId", &F_MS_IAP_CanAcquireLicenseForStoreId, 2, false);
	Function_Add("ms_iap_DownloadAndInstallPackages", &F_MS_IAP_DownloadAndInstallPackages, 2, false);
	// Function_Add("ms_iap_DownloadAndInstallPackageUpdates", &F_MS_IAP_UNIMPLEMENTED, 1, false);
	// Function_Add("ms_iap_DownloadPackageUpdates", &F_MS_IAP_UNIMPLEMENTED, 1, false);
	Function_Add("ms_iap_EnumeratePackages", &F_MS_IAP_EnumeratePackages, 2, false);
	// Function_Add("ms_iap_GetUserCollectionsId", &F_MS_IAP_UNIMPLEMENTED, 1, false);
	// Function_Add("ms_iap_GetUserPurchaseId", &F_MS_IAP_UNIMPLEMENTED, 1, false);
	Function_Add("ms_iap_MountPackage", &F_MS_IAP_MountPackage, 1, false);
	Function_Add("ms_iap_QueryAddOnLicenses", &F_MS_IAP_QueryAddOnLicenses, 1, false);
	Function_Add("ms_iap_QueryAssociatedProducts", &F_MS_IAP_QueryAssociatedProducts, 2, false);
	Function_Add("ms_iap_QueryConsumableBalanceRemaining", &F_MS_IAP_QueryConsumableBalanceRemaining, 2, false);
	Function_Add("ms_iap_QueryEntitledProducts", &F_MS_IAP_QueryEntitledProducts, 2, false);
	// Function_Add("ms_iap_QueryGameAndDlcPackageUpdates", &F_MS_IAP_UNIMPLEMENTED, 1, false);
	Function_Add("ms_iap_QueryGameLicense", &F_MS_IAP_QueryGameLicense, 1, false);
	// Function_Add("ms_iap_QueryLicenseToken", &F_MS_IAP_UNIMPLEMENTED, 1, false);
	// Function_Add("ms_iap_QueryPackageIdentifier", &F_MS_IAP_UNIMPLEMENTED, 1, false);
	Function_Add("ms_iap_QueryProductForCurrentGame", &F_MS_IAP_QueryProductForCurrentGame, 1, false);
	Function_Add("ms_iap_QueryProductForPackage", &F_MS_IAP_QueryProductForPackage, 3, false);
	Function_Add("ms_iap_QueryProducts", &F_MS_IAP_QueryProducts, 4, false);
	Function_Add("ms_iap_ReportConsumableFulfillment", &F_MS_IAP_ReportConsumableFulfillment, 4, false);
	Function_Add("ms_iap_ShowAssociatedProductsUI", &F_MS_IAP_ShowAssociatedProductsUI, 3, false);
	Function_Add("ms_iap_ShowProductPageUI", &F_MS_IAP_ShowProductPageUI, 2, false);
	Function_Add("ms_iap_ShowPurchaseUI", &F_MS_IAP_ShowPurchaseUI, 4, false);
	Function_Add("ms_iap_ShowRateAndReviewUI", &F_MS_IAP_ShowRateAndReviewUI, 1, false);
	Function_Add("ms_iap_ShowRedeemTokenUI", &F_MS_IAP_ShowRedeemTokenUI, 4, false);
	Function_Add("ms_iap_UnmountPackage", &F_MS_IAP_UnmountPackage, 1, false);
#endif
}

void UpdateIAPFunctionsM()
{
	while (XTaskQueueDispatch(iap_background_to_main_queue, XTaskQueuePort::Completion, 0)) {}
}

static void _MS_IAP_QueueTerminated(void* context)
{
	*(bool*)(context) = true;
}

void QuitIAPFunctionsM()
{
	XTaskQueueTerminate(iap_background_queue, true, NULL, NULL);
	XTaskQueueCloseHandle(iap_background_queue);
	iap_background_queue = NULL;

	bool terminated = false;
	XTaskQueueTerminate(iap_background_to_main_queue, false, &terminated, &_MS_IAP_QueueTerminated);
	while (XTaskQueueDispatch(iap_background_to_main_queue, XTaskQueuePort::Completion, 0) || !terminated) {}
	XTaskQueueCloseHandle(iap_background_to_main_queue);
	iap_background_to_main_queue = NULL;
}

static void MS_IAP_GameLicenseChanged(void* context)
{
	abort();
}

YYEXPORT void F_MS_IAP_AcquireLicenseForDurables(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg)
{
	Result.kind = VALUE_REAL;

	std::lock_guard<std::mutex> l(licenses_lock);

	const char* store_id = YYGetString(arg, 1);

	if (store_licenses.find(store_id) != store_licenses.end())
	{
		DebugConsoleOutput("ms_iap_AcquireLicenseForDurables - already acquired/acquiring license for %s\n", store_id);
		return;
	}

	XStoreContextHandle store_ctx = _MS_IAP_GetStoreHandle(arg, 0, "ms_iap_AcquireLicenseForDurables");
	if (store_ctx == NULL)
	{
		return;
	}

	struct AcquireLicenseForDurablesContext
	{
		std::string store_id;
		unsigned async_id;
		XStoreContextHandle store_ctx;
		XAsyncBlock async;

		AcquireLicenseForDurablesContext(const std::string &store_id, unsigned async_id, XStoreContextHandle store_ctx) :
			store_id(store_id),
			async_id(async_id),
			store_ctx(store_ctx) {}

		~AcquireLicenseForDurablesContext()
		{
			XStoreCloseContextHandle(store_ctx);
		}
	};

	unsigned async_id = next_iap_id++;
	AcquireLicenseForDurablesContext* ctx = new AcquireLicenseForDurablesContext(store_id, async_id, store_ctx);

	ctx->async.queue = iap_background_to_main_queue;
	ctx->async.context = ctx;
	ctx->async.callback = [](XAsyncBlock* async)
	{
		AcquireLicenseForDurablesContext* ctx = (AcquireLicenseForDurablesContext*)(async->context);
		std::unique_ptr<AcquireLicenseForDurablesContext> ctx_guard(ctx);

		std::lock_guard<std::mutex> ll(licenses_lock);

		auto licenses_slot = store_licenses.find(ctx->store_id);
		assert(licenses_slot != store_licenses.end());
		assert(licenses_slot->second == NULL);

		XStoreLicenseHandle license;
		HRESULT hr = XStoreAcquireLicenseForDurablesResult(async, &license);
		if (SUCCEEDED(hr))
		{
			bool is_valid = XStoreIsLicenseValid(license);

			if (is_valid)
			{
				MS_IAP_LicenseHandle* l = new MS_IAP_LicenseHandle;
				l->id = ctx->store_id;
				l->handle = license;

				hr = XStoreRegisterPackageLicenseLost(license, NULL, l, &_MS_IAP_DurableLicenseLost, &(l->lost_token));
				if (SUCCEEDED(hr))
				{
					l->valid = true;
					licenses_slot->second = l;

					DS_LOCK_MUTEX;

					int dsMapIndex = CreateDsMap(3,
						"id", (double)(ctx->async_id), NULL,
						"type", (double)(0.0), "ms_iap_AcquireLicenseForDurables_result",
						"store_id", (double)(0.0), ctx->store_id.c_str());

					DsMapAddBool(dsMapIndex, "status", true);

					CreateAsyncEventWithDSMap(dsMapIndex, EVENT_OTHER_WEB_IAP);
				}
				else{
					DebugConsoleOutput("ms_iap_AcquireLicenseForDurables - error from XStoreRegisterPackageLicenseLost (HRESULT 0x%08X)\n", (unsigned)(hr));
					delete l;
					XStoreCloseLicenseHandle(license);
				}
			}
			else {
				DebugConsoleOutput("ms_iap_AcquireLicenseForDurables - got an invalid license\n");
				hr = E_FAIL;

				/* Useless handle, begone. */
				XStoreCloseLicenseHandle(license);
			}
		}
		else {
			DebugConsoleOutput("ms_iap_AcquireLicenseForDurables - error from XStoreAcquireLicenseForDurablesAsync (HRESULT 0x%08X)\n", (unsigned)(hr));
		}

		if (FAILED(hr))
		{
			store_licenses.erase(licenses_slot);

			DS_LOCK_MUTEX;

			int dsMapIndex = CreateDsMap(4,
				"id", (double)(ctx->async_id), NULL,
				"type", (double)(0.0), "ms_iap_AcquireLicenseForDurables_result",
				"store_id", (double)(0.0), ctx->store_id.c_str(),
				"error", (double)(hr), NULL);

			DsMapAddBool(dsMapIndex, "status", false);

			CreateAsyncEventWithDSMap(dsMapIndex, EVENT_OTHER_WEB_IAP);
		}
	};

	HRESULT hr = XStoreAcquireLicenseForDurablesAsync(store_ctx, store_id, &(ctx->async));
	if (SUCCEEDED(hr))
	{
		store_licenses[store_id] = NULL; /* Reserve slot */
		Result.val = async_id;
	}
	else {
		DebugConsoleOutput("ms_iap_AcquireLicenseForDurables - error from XStoreAcquireLicenseForDurablesAsync (HRESULT 0x%08X)\n", (unsigned)(hr));
		delete ctx;

		Result.val = -1.0;
	}
}

YYEXPORT void F_MS_IAP_ReleaseLicenseForDurables(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg)
{
	Result.kind = VALUE_REAL;

	const char* store_id = YYGetString(arg, 0);

	std::lock_guard<std::mutex> l(licenses_lock);

	auto license_slot = store_licenses.find(store_id);
	if (license_slot != store_licenses.end() && license_slot->second != NULL)
	{
		MS_IAP_LicenseHandle* license = license_slot->second;

		license->valid = false;

		store_licenses.erase(license_slot);
		_MS_IAP_CloseLicense(license);

		Result.val = 0;
	}
	else {
		DebugConsoleOutput("ms_iap_ReleaseLicenseForDurables - license for store_id %s isn't held, can't release\n", store_id);
		Result.val = -1;
	}
}

static void _MS_IAP_DurableLicenseLost(void* context)
{
	MS_IAP_LicenseHandle* license = (MS_IAP_LicenseHandle*)(context);
	std::lock_guard<std::mutex> l(licenses_lock);

	if (!license->valid)
	{
		return;
	}

	license->valid = false;

	{
		DS_LOCK_MUTEX;

		int dsMapIndex = CreateDsMap(2,
			"type", (double)(0.0), "ms_iap_DurableLicenseLost",
			"store_id", (double)(0.0), license->id.c_str());

		CreateAsyncEventWithDSMap(dsMapIndex, EVENT_OTHER_WEB_IAP);
	}

	auto license_slot = store_licenses.find(license->id);
	assert(license_slot != store_licenses.end());

	store_licenses.erase(license_slot);
	_MS_IAP_CloseLicense(license);
}

YYEXPORT void F_MS_IAP_AcquireLicenseForPackage(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg)
{
	Result.kind = VALUE_REAL;

	std::lock_guard<std::mutex> l(licenses_lock);

	const char* package_id = YYGetString(arg, 1);

	if (pkg_licenses.find(package_id) != pkg_licenses.end())
	{
		DebugConsoleOutput("ms_iap_AcquireLicenseForPackage - already acquired/acquiring license for %s\n", package_id);
		return;
	}

	XStoreContextHandle store_ctx = _MS_IAP_GetStoreHandle(arg, 0, "ms_iap_AcquireLicenseForPackage");
	if (store_ctx == NULL)
	{
		return;
	}

	struct AcquireLicenseForPackageContext
	{
		std::string package_id;
		unsigned async_id;
		XStoreContextHandle store_ctx;
		XAsyncBlock async;

		AcquireLicenseForPackageContext(const std::string& package_id, unsigned async_id, XStoreContextHandle store_ctx) :
			package_id(package_id),
			async_id(async_id),
			store_ctx(store_ctx) {}

		~AcquireLicenseForPackageContext()
		{
			XStoreCloseContextHandle(store_ctx);
		}
	};

	unsigned async_id = next_iap_id++;
	AcquireLicenseForPackageContext* ctx = new AcquireLicenseForPackageContext(package_id, async_id, store_ctx);

	ctx->async.queue = iap_background_to_main_queue;
	ctx->async.context = ctx;
	ctx->async.callback = [](XAsyncBlock* async)
	{
		AcquireLicenseForPackageContext* ctx = (AcquireLicenseForPackageContext*)(async->context);
		std::unique_ptr<AcquireLicenseForPackageContext> ctx_guard(ctx);

		std::lock_guard<std::mutex> ll(licenses_lock);

		auto licenses_slot = pkg_licenses.find(ctx->package_id);
		assert(licenses_slot != pkg_licenses.end());
		assert(licenses_slot->second == NULL);

		XStoreLicenseHandle license;
		HRESULT hr = XStoreAcquireLicenseForPackageResult(async, &license);
		if (SUCCEEDED(hr))
		{
			bool is_valid = XStoreIsLicenseValid(license);

			if (is_valid)
			{
				MS_IAP_LicenseHandle* l = new MS_IAP_LicenseHandle;
				l->id = ctx->package_id;
				l->handle = license;

				hr = XStoreRegisterPackageLicenseLost(license, NULL, l, &_MS_IAP_PackageLicenseLost, &(l->lost_token));
				if (SUCCEEDED(hr))
				{
					l->valid = true;
					licenses_slot->second = l;

					DS_LOCK_MUTEX;

					int dsMapIndex = CreateDsMap(3,
						"id", (double)(ctx->async_id), NULL,
						"type", (double)(0.0), "ms_iap_AcquireLicenseForPackage_result",
						"package_id", (double)(0.0), ctx->package_id.c_str());

					DsMapAddBool(dsMapIndex, "status", true);

					CreateAsyncEventWithDSMap(dsMapIndex, EVENT_OTHER_WEB_IAP);
				}
				else {
					DebugConsoleOutput("ms_iap_AcquireLicenseForPackage - error from XStoreRegisterPackageLicenseLost (HRESULT 0x%08X)\n", (unsigned)(hr));
					delete l;
					XStoreCloseLicenseHandle(license);
				}
			}
			else {
				DebugConsoleOutput("ms_iap_AcquireLicenseForPackage - got an invalid license\n");
				hr = E_FAIL;

				/* Useless handle, begone. */
				XStoreCloseLicenseHandle(license);
			}
		}
		else {
			DebugConsoleOutput("ms_iap_AcquireLicenseForPackage - error from XStoreAcquireLicenseForPackageAsync (HRESULT 0x%08X)\n", (unsigned)(hr));
		}

		if (FAILED(hr))
		{
			pkg_licenses.erase(licenses_slot);

			DS_LOCK_MUTEX;

			int dsMapIndex = CreateDsMap(4,
				"id", (double)(ctx->async_id), NULL,
				"type", (double)(0.0), "ms_iap_AcquireLicenseForPackage_result",
				"package_id", (double)(0.0), ctx->package_id.c_str(),
				"error", (double)(hr), NULL);

			DsMapAddBool(dsMapIndex, "status", false);

			CreateAsyncEventWithDSMap(dsMapIndex, EVENT_OTHER_WEB_IAP);
		}
	};

	HRESULT hr = XStoreAcquireLicenseForPackageAsync(store_ctx, package_id, &(ctx->async));
	if (SUCCEEDED(hr))
	{
		pkg_licenses[package_id] = NULL; /* Reserve slot */
		Result.val = async_id;
	}
	else {
		DebugConsoleOutput("ms_iap_AcquireLicenseForPackage - error from XStoreAcquireLicenseForPackageAsync (HRESULT 0x%08X)\n", (unsigned)(hr));
		delete ctx;

		Result.val = -1.0;
	}
}

YYEXPORT void F_MS_IAP_ReleaseLicenseForPackage(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg)
{
	Result.kind = VALUE_REAL;

	const char* package_id = YYGetString(arg, 0);

	std::lock_guard<std::mutex> l(licenses_lock);

	auto license_slot = pkg_licenses.find(package_id);
	if (license_slot != pkg_licenses.end() && license_slot->second != NULL)
	{
		MS_IAP_LicenseHandle* license = license_slot->second;

		license->valid = false;

		pkg_licenses.erase(license_slot);
		_MS_IAP_CloseLicense(license);

		Result.val = 0;
	}
	else {
		DebugConsoleOutput("ms_iap_ReleaseLicenseForPackage - license for package_id %s isn't held, can't release\n", package_id);
		Result.val = -1;
	}
}

static void _MS_IAP_PackageLicenseLost(void* context)
{
	MS_IAP_LicenseHandle* license = (MS_IAP_LicenseHandle*)(context);
	std::lock_guard<std::mutex> l(licenses_lock);

	if (!license->valid)
	{
		return;
	}

	license->valid = false;

	{
		DS_LOCK_MUTEX;

		int dsMapIndex = CreateDsMap(2,
			"type", (double)(0.0), "ms_iap_PackageLicenseLost",
			"package_id", (double)(0.0), license->id.c_str());

		CreateAsyncEventWithDSMap(dsMapIndex, EVENT_OTHER_WEB_IAP);
	}

	auto license_slot = pkg_licenses.find(license->id);
	assert(license_slot != pkg_licenses.end());

	pkg_licenses.erase(license_slot);
	_MS_IAP_CloseLicense(license);
}

static void _MS_IAP_CloseLicense(MS_IAP_LicenseHandle *license)
{
	/* There's no async API for cleaning up the license handle and the
	 * documentation warns not to do it in a time-sensitive thread... so we have
	 * to do it in a dedicated thread where we won't block anything else.
	*/

	std::thread t([license]()
	{
		XStoreUnregisterPackageLicenseLost(license->handle, license->lost_token, true);
		XStoreCloseLicenseHandle(license->handle);
		delete license;
	});

	t.detach();
}

YYEXPORT void F_MS_IAP_CanAcquireLicenseForPackage(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg)
{
	Result.kind = VALUE_REAL;
	Result.val = -1;

	const char* package_id = YYGetString(arg, 1);

	XStoreContextHandle store_ctx = _MS_IAP_GetStoreHandle(arg, 0, "ms_iap_CanAcquireLicenseForPackage");
	if (store_ctx == NULL)
	{
		return;
	}

	struct CanAcquireLicenseForPackageContext
	{
		std::string package_id;
		unsigned async_id;
		XStoreContextHandle store_ctx;
		XAsyncBlock async;

		CanAcquireLicenseForPackageContext(const std::string& package_id, unsigned int async_id, XStoreContextHandle store_ctx) :
			package_id(package_id),
			async_id(async_id),
			store_ctx(store_ctx) {}

		~CanAcquireLicenseForPackageContext()
		{
			XStoreCloseContextHandle(store_ctx);
		}
	};

	unsigned int async_id = next_iap_id++;
	CanAcquireLicenseForPackageContext* ctx = new CanAcquireLicenseForPackageContext(package_id, async_id, store_ctx);

	ctx->async.queue = iap_background_to_main_queue;
	ctx->async.context = ctx;
	ctx->async.callback = [](XAsyncBlock* async)
	{
		CanAcquireLicenseForPackageContext* ctx = (CanAcquireLicenseForPackageContext*)(async->context);
		std::unique_ptr<CanAcquireLicenseForPackageContext> ctx_guard(ctx);

		XStoreCanAcquireLicenseResult result;
		HRESULT hr = XStoreCanAcquireLicenseForPackageResult(async, &result);
		if (SUCCEEDED(hr))
		{
			DS_LOCK_MUTEX;

			int dsMapIndex = CreateDsMap(5,
				"async_id", (double)(ctx->async_id), NULL,
				"type", (double)(0.0), "ms_iap_CanAcquireLicenseForPackage_result",
				"package_id", (double)(0.0), ctx->package_id.c_str(),
				"licensableSku", (double)(0.0), result.licensableSku,
				"licenseStatus", (double)(result.status), NULL);

			DsMapAddBool(dsMapIndex, "async_status", true);

			CreateAsyncEventWithDSMap(dsMapIndex, EVENT_OTHER_WEB_IAP);
		}
		else {
			DebugConsoleOutput("ms_iap_CanAcquireLicenseForPackage - error from XStoreCanAcquireLicenseForPackageAsync (HRESULT 0x%08X)\n", (unsigned)(hr));

			DS_LOCK_MUTEX;

			int dsMapIndex = CreateDsMap(4,
				"async_id", (double)(ctx->async_id), NULL,
				"type", (double)(0.0), "ms_iap_CanAcquireLicenseForPackage_result",
				"package_id", (double)(0.0), ctx->package_id.c_str(),
				"error", (double)(hr), NULL);

			DsMapAddBool(dsMapIndex, "async_status", false);

			CreateAsyncEventWithDSMap(dsMapIndex, EVENT_OTHER_WEB_IAP);
		}
	};

	HRESULT hr = XStoreCanAcquireLicenseForPackageAsync(store_ctx, package_id, &(ctx->async));
	if (SUCCEEDED(hr))
	{
		Result.val = async_id;
	}
	else {
		DebugConsoleOutput("ms_iap_CanAcquireLicenseForPackage - error from XStoreCanAcquireLicenseForPackageAsync (HRESULT 0x%08X)\n", (unsigned)(hr));
		delete ctx;
	}
}

YYEXPORT void F_MS_IAP_CanAcquireLicenseForStoreId(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg)
{
	Result.kind = VALUE_REAL;
	Result.val = -1;

	const char* store_id = YYGetString(arg, 1);

	XStoreContextHandle store_ctx = _MS_IAP_GetStoreHandle(arg, 0, "ms_iap_CanAcquireLicenseForStoreId");
	if (store_ctx == NULL)
	{
		return;
	}

	struct CanAcquireLicenseForStoreIdContext
	{
		std::string store_id;
		unsigned async_id;
		XStoreContextHandle store_ctx;
		XAsyncBlock async;

		CanAcquireLicenseForStoreIdContext(const std::string &store_id, unsigned int async_id, XStoreContextHandle store_ctx) :
			store_id(store_id),
			async_id(async_id),
			store_ctx(store_ctx) {}

		~CanAcquireLicenseForStoreIdContext()
		{
			XStoreCloseContextHandle(store_ctx);
		}
	};

	unsigned int async_id = next_iap_id++;
	CanAcquireLicenseForStoreIdContext* ctx = new CanAcquireLicenseForStoreIdContext(store_id, async_id, store_ctx);

	ctx->async.queue = iap_background_to_main_queue;
	ctx->async.context = ctx;
	ctx->async.callback = [](XAsyncBlock *async)
	{
		CanAcquireLicenseForStoreIdContext* ctx = (CanAcquireLicenseForStoreIdContext*)(async->context);
		std::unique_ptr<CanAcquireLicenseForStoreIdContext> ctx_guard(ctx);

		XStoreCanAcquireLicenseResult result;
		HRESULT hr = XStoreCanAcquireLicenseForStoreIdResult(async, &result);
		if (SUCCEEDED(hr))
		{
			DS_LOCK_MUTEX;

			int dsMapIndex = CreateDsMap(5,
				"async_id", (double)(ctx->async_id), NULL,
				"type", (double)(0.0), "ms_iap_CanAcquireLicenseForStoreId_result",
				"store_id", (double)(0.0), ctx->store_id.c_str(),
				"licensableSku", (double)(0.0), result.licensableSku,
				"licenseStatus", (double)(result.status), NULL);

			DsMapAddBool(dsMapIndex, "async_status", true);

			CreateAsyncEventWithDSMap(dsMapIndex, EVENT_OTHER_WEB_IAP);
		}
		else {
			DebugConsoleOutput("ms_iap_CanAcquireLicenseForStoreId - error from XStoreCanAcquireLicenseForStoreIdAsync (HRESULT 0x%08X)\n", (unsigned)(hr));

			DS_LOCK_MUTEX;

			int dsMapIndex = CreateDsMap(4,
				"async_id", (double)(ctx->async_id), NULL,
				"type", (double)(0.0), "ms_iap_CanAcquireLicenseForStoreId_result",
				"store_id", (double)(0.0), ctx->store_id.c_str(),
				"error", (double)(hr), NULL);

			DsMapAddBool(dsMapIndex, "async_status", false);

			CreateAsyncEventWithDSMap(dsMapIndex, EVENT_OTHER_WEB_IAP);
		}
	};

	HRESULT hr = XStoreCanAcquireLicenseForStoreIdAsync(store_ctx, store_id, &(ctx->async));
	if (SUCCEEDED(hr))
	{
		Result.val = async_id;
	}
	else {
		DebugConsoleOutput("ms_iap_CanAcquireLicenseForStoreId - error from XStoreCanAcquireLicenseForStoreIdAsync (HRESULT 0x%08X)\n", (unsigned)(hr));
		delete ctx;
	}
}

/* Package installation is a fairly convoluted process:
 *
 * First, we must call XStoreDownloadAndInstallPackagesAsync(), which will initiate the package
 * installation - once it is "complete" the packages are registered, but may not be installed.
 *
 * Once the packages are registered, we create an "installation monitor" for each package, which
 * allows us to query the installation state of that package, and receive periodic updates.
 *
 * Once all the packages are fully installed (or an error occurs), we queue up the completion
 * event in GameMaker land.
 *
 * NOTE: Most of the XPackage APIs are blocking, so for extra complexity we have to dispatch the
 * XStoreDownloadAndInstallPackagesAsync() completion handler and the installation monitor
 * callbacks in the thread pool where they won't block anything important. Once the operation
 * completes, a task is queued on the main thread to raise the async event before queuing up yet
 * another task back on the thread pool where we can finally clean up our various XPackage handles
 * and release any memory.
*/

struct DownloadAndInstallPackagesContext
{
	unsigned int async_id;
	XStoreContextHandle store_ctx;
	XAsyncBlock async;

	std::once_flag async_once;
	HRESULT first_error;

	struct Package
	{
		std::string package_id;

		XPackageInstallationMonitorHandle installation_monitor;

		bool progress_change_callback_registered;
		XTaskQueueRegistrationToken progress_changed_token;

		Package(const std::string& package_id, XPackageInstallationMonitorHandle installation_monitor) :
			package_id(package_id),
			installation_monitor(installation_monitor),
			progress_change_callback_registered(false),
			progress_changed_token({}) {}
	};

	std::vector<Package> packages;

	DownloadAndInstallPackagesContext(XStoreContextHandle store_ctx);
	~DownloadAndInstallPackagesContext();

	static void DownloadAndInstallPackagesCallback(XAsyncBlock* async);
	static void InstallationProgressCallback(void* context, XPackageInstallationMonitorHandle monitor);

	void tick();
	
	void queue_async_event(HRESULT error);
	static void dispatch_async_event(void* context, bool canceled);

	static void destroy_self(void* context, bool canceled);
};

DownloadAndInstallPackagesContext::DownloadAndInstallPackagesContext(XStoreContextHandle store_ctx) :
	async_id(next_iap_id++),
	store_ctx(store_ctx),
	async({ iap_background_queue, this, &DownloadAndInstallPackagesCallback }),
	first_error(ERROR_SUCCESS) {}

DownloadAndInstallPackagesContext::~DownloadAndInstallPackagesContext()
{
	for (auto p = packages.begin(); p != packages.end(); ++p)
	{
		if (p->progress_change_callback_registered)
		{
			XPackageUnregisterInstallationProgressChanged(p->installation_monitor, p->progress_changed_token, true);
		}
	}

	for (auto p = packages.begin(); p != packages.end(); ++p)
	{
		XPackageCloseInstallationMonitorHandle(p->installation_monitor);
	}

	XStoreCloseContextHandle(store_ctx);
}

void DownloadAndInstallPackagesContext::DownloadAndInstallPackagesCallback(XAsyncBlock* async)
{
	DownloadAndInstallPackagesContext* ctx = (DownloadAndInstallPackagesContext*)(async->context);

	uint32_t results_count;
	HRESULT hr = XStoreDownloadAndInstallPackagesResultCount(async, &results_count);
	if (SUCCEEDED(hr))
	{
		std::vector<char[XPACKAGE_IDENTIFIER_MAX_LENGTH]> results(results_count);

		hr = XStoreDownloadAndInstallPackagesResult(async, results_count, results.data());
		if (SUCCEEDED(hr))
		{
			ctx->packages.reserve(results_count);

			for (uint32_t j = 0; j < results_count; ++j)
			{
				static const uint32_t PACKAGE_STATE_UPDATE_MS = 1000;

				XPackageInstallationMonitorHandle monitor_handle;

				hr = XPackageCreateInstallationMonitor(results[j], 0, NULL, PACKAGE_STATE_UPDATE_MS, iap_background_queue, &monitor_handle);
				if (SUCCEEDED(hr))
				{
					ctx->packages.emplace_back(results[j], monitor_handle);
				}
				else {
					DebugConsoleOutput("ms_iap_DownloadAndInstallPackages - request failed (XPackageCreateInstallationMonitor returned HRESULT 0x%08X)\n", (unsigned)(hr));
					ctx->queue_async_event(hr);
				}
			}

			for (auto p = ctx->packages.begin(); p != ctx->packages.end(); ++p)
			{
				hr = XPackageRegisterInstallationProgressChanged(p->installation_monitor, ctx, &InstallationProgressCallback, &(p->progress_changed_token));

				if (SUCCEEDED(hr))
				{
					p->progress_change_callback_registered = true;
				}
				else {
					DebugConsoleOutput("ms_iap_DownloadAndInstallPackages - request failed (XPackageRegisterInstallationProgressChanged returned HRESULT 0x%08X)\n", (unsigned)(hr));
					ctx->queue_async_event(hr);
				}
			}
		}
		else {
			DebugConsoleOutput("ms_iap_DownloadAndInstallPackages - request failed (XStoreDownloadAndInstallPackagesResult returned HRESULT 0x%08X)\n", (unsigned)(hr));
			ctx->queue_async_event(hr);
		}
	}
	else {
		DebugConsoleOutput("ms_iap_DownloadAndInstallPackages - request failed (XStoreDownloadAndInstallPackagesResultCount returned HRESULT 0x%08X)\n", (unsigned)(hr));
		ctx->queue_async_event(hr);
	}

	if (SUCCEEDED(hr))
	{
		ctx->tick();
	}
}

void DownloadAndInstallPackagesContext::InstallationProgressCallback(void* context, XPackageInstallationMonitorHandle monitor)
{
	DownloadAndInstallPackagesContext* ctx = (DownloadAndInstallPackagesContext*)(context);
	ctx->tick();
}

void DownloadAndInstallPackagesContext::tick()
{
	bool all_complete = true;

	for (auto p = packages.begin(); p != packages.end(); ++p)
	{
		XPackageInstallationProgress package_progress;
		XPackageGetInstallationProgress(p->installation_monitor, &package_progress);

		if (!package_progress.completed)
		{
			all_complete = false;
		}
	}

	if (all_complete)
	{
		queue_async_event(ERROR_SUCCESS);
	}
}

void DownloadAndInstallPackagesContext::queue_async_event(HRESULT error)
{
	std::call_once(async_once, [&]()
	{
		assert(first_error == ERROR_SUCCESS);
		first_error = error;

		HRESULT hr = XTaskQueueSubmitCallback(iap_background_to_main_queue, XTaskQueuePort::Completion, this, &dispatch_async_event);
		if (FAILED(hr))
		{
			DebugConsoleOutput("DownloadAndInstallPackagesContext::queue_async_event - unable to queue dispatch_async_event (HRESULT 0x%08X)\n", (unsigned)(hr));
		}
	});
}

void DownloadAndInstallPackagesContext::dispatch_async_event(void* context, bool canceled)
{
	DownloadAndInstallPackagesContext* ctx = (DownloadAndInstallPackagesContext*)(context);

	DS_LOCK_MUTEX;

	int dsMapIndex = CreateDsMap(2,
		"id", (double)(ctx->async_id), NULL,
		"type", (double)(0.0), "ms_iap_DownloadAndInstallPackages_result");

	DsMapAddBool(dsMapIndex, "status", SUCCEEDED(ctx->first_error));

	if (SUCCEEDED(ctx->first_error))
	{
		RValue package_ids = { 0 };
		YYCreateArray(&package_ids, 0);

		int idx = 0;
		for(auto p = ctx->packages.begin(); p != ctx->packages.end(); ++p, ++idx)
		{
			RValue package_id = { 0 };
			YYCreateString(&package_id, p->package_id.c_str());

			SET_RValue(&package_ids, &package_id, NULL, idx);
			FREE_RValue(&package_id);
		}

		DsMapAddRValue(dsMapIndex, "package_ids", &package_ids);
	}

	CreateAsyncEventWithDSMap(dsMapIndex, EVENT_OTHER_WEB_IAP);

	HRESULT hr = XTaskQueueSubmitCallback(iap_background_to_main_queue, XTaskQueuePort::Work, ctx, &destroy_self);
	if (FAILED(hr))
	{
		DebugConsoleOutput("DownloadAndInstallPackagesContext::dispatch_async_event - unable to queue destroy_self (HRESULT 0x%08X), resources will be leaked!\n", (unsigned)(hr));
	}
}

void DownloadAndInstallPackagesContext::destroy_self(void* context, bool canceled)
{
	DownloadAndInstallPackagesContext* ctx = (DownloadAndInstallPackagesContext*)(context);
	delete ctx;
}

YYEXPORT void F_MS_IAP_DownloadAndInstallPackages(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg)
{
	Result.kind = VALUE_REAL;

	std::vector<const char*> store_ids = _MS_IAP_GetArrayOfStrings(arg, 1, "ms_iap_DownloadAndInstallPackages");

	XStoreContextHandle store_ctx = _MS_IAP_GetStoreHandle(arg, 0, "ms_iap_DownloadAndInstallPackages");
	if (store_ctx == NULL)
	{
		return;
	}

	DownloadAndInstallPackagesContext* ctx = new DownloadAndInstallPackagesContext(store_ctx);

	HRESULT hr = XStoreDownloadAndInstallPackagesAsync(store_ctx, store_ids.data(), store_ids.size(), &(ctx->async));
	if (SUCCEEDED(hr))
	{
		Result.val = ctx->async_id;
	}
	else {
		DebugConsoleOutput("ms_iap_DownloadAndInstallPackages - request failed (XStoreDownloadAndInstallPackagesAsync returned HRESULT 0x%08X)\n", (unsigned)(hr));
		delete ctx;
	}
}

struct EnumeratePackagesContext
{
	unsigned int async_id;
	XPackageKind kind;
	XPackageEnumerationScope scope;
	std::vector<MS_IAP_PackageDetails> packages;

	HRESULT error;

	EnumeratePackagesContext(XPackageKind kind, XPackageEnumerationScope scope);

	static void work_callback(void* context, bool canceled);
	static bool pkg_enum_callback(void* context, const XPackageDetails* details);
	static void completion_callback(void* context, bool canceled);
};

EnumeratePackagesContext::EnumeratePackagesContext(XPackageKind kind, XPackageEnumerationScope scope) :
	async_id(next_iap_id++),
	kind(kind),
	scope(scope),
	error(ERROR_SUCCESS) {}

void EnumeratePackagesContext::work_callback(void* context, bool canceled)
{
	EnumeratePackagesContext* ctx = (EnumeratePackagesContext*)(context);

	ctx->error = XPackageEnumeratePackages(ctx->kind, ctx->scope, ctx, &EnumeratePackagesContext::pkg_enum_callback);

	HRESULT hr = XTaskQueueSubmitCallback(iap_background_to_main_queue, XTaskQueuePort::Completion, ctx, &EnumeratePackagesContext::completion_callback);
	if (FAILED(hr))
	{
		DebugConsoleOutput("EnumeratePackagesContext::work_callback - unable to queue completion_callback (HRESULT 0x%08X)\n", (unsigned)(hr));
		delete ctx;
	}
}

bool EnumeratePackagesContext::pkg_enum_callback(void* context, const XPackageDetails* details)
{
	EnumeratePackagesContext* ctx = (EnumeratePackagesContext*)(context);

	if (ctx->packages.capacity() < details->count)
	{
		ctx->packages.reserve(details->count);
	}

	ctx->packages.emplace_back(details);

	return true; /* Continue enumeration. */
}

void EnumeratePackagesContext::completion_callback(void* context, bool canceled)
{
	EnumeratePackagesContext* ctx = (EnumeratePackagesContext*)(context);
	std::unique_ptr<EnumeratePackagesContext> ctx_guard(ctx);

	DS_LOCK_MUTEX;

	int dsMapIndex = CreateDsMap(2,
		"id", (double)(ctx->async_id), NULL,
		"type", (double)(0.0), "ms_iap_EnumeratePackages_result");

	DsMapAddBool(dsMapIndex, "status", SUCCEEDED(ctx->error));

	if (SUCCEEDED(ctx->error))
	{
		RValue results = { 0 };
		YYCreateArray(&results, 0);

		for (size_t i = 0; i < ctx->packages.size(); ++i)
		{
			RValue package = _MS_IAP_PackageDetails_to_Struct(&(ctx->packages[i]));
			SET_RValue(&results, &package, NULL, i);
			FREE_RValue(&package);
		}

		DsMapAddRValue(dsMapIndex, "results", &results);
	}
	else {
		DebugConsoleOutput("ms_iap_EnumeratePackages - Unable to enumerate packages (HRESULT 0x%08X)\n", (unsigned)(ctx->error));
		DsMapAddDouble(dsMapIndex, "error", (double)(ctx->error));
	}
	
	CreateAsyncEventWithDSMap(dsMapIndex, EVENT_OTHER_WEB_IAP);
}

YYEXPORT void F_MS_IAP_EnumeratePackages(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg)
{
	Result.kind = VALUE_REAL;
	Result.val = -1;

	XPackageKind package_kind = _MS_IAP_GetPackageKind(arg, 0, "ms_iap_EnumeratePackages");
	XPackageEnumerationScope scope = _MS_IAP_GetPackageEnumerationScope(arg, 1, "ms_iap_EnumeratePackages");

	if (package_kind == INVALID_PACKAGE_KIND || scope == INVALID_ENUMERATION_SCOPE)
	{
		return;
	}

	EnumeratePackagesContext* ctx = new EnumeratePackagesContext(package_kind, scope);

	HRESULT hr = XTaskQueueSubmitCallback(iap_background_to_main_queue, XTaskQueuePort::Work, ctx, &EnumeratePackagesContext::work_callback);
	if (SUCCEEDED(hr))
	{
		Result.val = ctx->async_id;
	}
	else {
		DebugConsoleOutput("ms_iap_EnumeratePackages - Unable to queue work (HRESULT 0x%08X)\n", (unsigned)(hr));
		delete ctx;
	}
}

struct MountPackageContext
{
	unsigned int async_id;
	std::string package_id;
	XAsyncBlock async;

	HRESULT error;
	XPackageMountHandle mount_handle;
	std::string mount_path;

	MountPackageContext(const std::string& package_id);
};

MountPackageContext::MountPackageContext(const std::string& package_id) :
	async_id(next_iap_id++),
	package_id(package_id) {}

YYEXPORT void F_MS_IAP_MountPackage(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg)
{
	Result.kind = VALUE_REAL;
	Result.val = -1;

	const char* package_id = YYGetString(arg, 0);

	if (mounted_packages.find(package_id) != mounted_packages.end())
	{
		DebugConsoleOutput("ms_iap_MountPackage - Package %s is already mounted (or mounting)\n", package_id);
		return;
	}

	struct MountPackageContext* ctx = new MountPackageContext(package_id);

	ctx->async.queue = iap_background_to_main_queue;
	ctx->async.context = ctx;
	ctx->async.callback = [](XAsyncBlock* async)
	{
		struct MountPackageContext* ctx = (struct MountPackageContext*)(async->context);
		std::unique_ptr<MountPackageContext> ctx_guard(ctx);

		HRESULT hr = XPackageMountWithUiResult(async, &(ctx->mount_handle));

		if (SUCCEEDED(hr))
		{
			size_t mount_path_size;
			hr = XPackageGetMountPathSize(ctx->mount_handle, &mount_path_size);
			if (SUCCEEDED(hr))
			{
				std::vector<char> mount_path(mount_path_size);

				hr = XPackageGetMountPath(ctx->mount_handle, mount_path_size, mount_path.data());
				if (SUCCEEDED(hr))
				{
					ctx->mount_path = mount_path.data();
				}
			}

			if (FAILED(hr))
			{
				XPackageCloseMountHandle(ctx->mount_handle);
			}
		}

		ctx->error = hr;

		auto mp = mounted_packages.find(ctx->package_id);
		assert(mp != mounted_packages.end());
		assert(mp->second == NULL);

		DS_LOCK_MUTEX;

		int dsMapIndex = CreateDsMap(3,
			"id", (double)(ctx->async_id), NULL,
			"type", (double)(0.0), "ms_iap_MountPackage_result",
			"package_id", (double)(0.0), ctx->package_id.c_str());

		DsMapAddBool(dsMapIndex, "status", SUCCEEDED(ctx->error));

		if (SUCCEEDED(ctx->error))
		{
			mp->second = new MS_IAP_MountedPackage(ctx->package_id, ctx->mount_handle, ctx->mount_path);
			DsMapAddString(dsMapIndex, "mount_path", ctx->mount_path.c_str());

			/* DLC content is mounted outside of the sandbox, so allow loading from the mount point. */
#ifdef GDKEXTENSION_EXPORTS
			AddDirectoryToBundleWhitelist(ctx->mount_path.c_str());
#else
			if (!IsDirectoryInWhitelist(g_pLoadWhitelist, ctx->mount_path.c_str()))
			{
				AddToWhiteList(&g_pLoadWhitelist, ctx->mount_path.c_str(), true);
			}
#endif
		}
		else {
			DebugConsoleOutput("ms_iap_MountPackage - error mounting package %s (HRESULT 0x%08X)\n", ctx->package_id.c_str(), (unsigned)(ctx->error));
			DsMapAddDouble(dsMapIndex, "error", (double)(ctx->error));

			mounted_packages.erase(mp);
		}

		CreateAsyncEventWithDSMap(dsMapIndex, EVENT_OTHER_WEB_IAP);
	};

	HRESULT hr = XPackageMountWithUiAsync(package_id, &(ctx->async));
	if (SUCCEEDED(hr))
	{
		mounted_packages[package_id] = NULL; /* Reserve slot. */
		Result.val = ctx->async_id;
	}
	else {
		DebugConsoleOutput("ms_iap_MountPackage - Unable to mount package (HRESULT 0x%08X)\n", (unsigned)(hr));
		delete ctx;
	}
}

YYEXPORT void F_MS_IAP_QueryAddOnLicenses(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg)
{
	Result.kind = VALUE_REAL;
	Result.val = -1;

	XStoreContextHandle store_ctx = _MS_IAP_GetStoreHandle(arg, 0, "ms_iap_QueryAddOnLicenses");
	if (store_ctx == NULL)
	{
		return;
	}

	struct QueryAddOnLicensesContext
	{
		unsigned int async_id;
		XStoreContextHandle store_ctx;
		XAsyncBlock async;

		QueryAddOnLicensesContext(XStoreContextHandle store_ctx) :
			async_id(next_iap_id++),
			store_ctx(store_ctx) {}

		~QueryAddOnLicensesContext()
		{
			XStoreCloseContextHandle(store_ctx);
		}
	};

	QueryAddOnLicensesContext* ctx = new QueryAddOnLicensesContext(store_ctx);

	ctx->async.queue = iap_background_to_main_queue;
	ctx->async.context = ctx;
	ctx->async.callback = [](XAsyncBlock* async)
	{
		QueryAddOnLicensesContext* ctx = (QueryAddOnLicensesContext*)(async->context);
		std::unique_ptr<QueryAddOnLicensesContext> ctx_guard(ctx);

		uint32_t result_count;
		std::vector<XStoreAddonLicense> results_buf;

		HRESULT hr = XStoreQueryAddOnLicensesResultCount(async, &result_count);
		if (SUCCEEDED(hr))
		{
			results_buf.resize(result_count);

			hr = XStoreQueryAddOnLicensesResult(async, results_buf.size(), results_buf.data());
			if (FAILED(hr))
			{
				DebugConsoleOutput("ms_iap_QueryAddOnLicenses - query failed (XStoreQueryAddOnLicensesResult returned HRESULT 0x%08X)\n", (unsigned)(hr));
			}
		}
		else {
			DebugConsoleOutput("ms_iap_QueryAddOnLicenses - query failed (XStoreQueryAddOnLicensesResultCount returned HRESULT 0x%08X)\n", (unsigned)(hr));
		}

		DS_LOCK_MUTEX;

		int dsMapIndex = CreateDsMap(2,
			"id", (double)(ctx->async_id), NULL,
			"type", (double)(0.0), "ms_iap_QueryAddOnLicenses_result");

		DsMapAddBool(dsMapIndex, "status", SUCCEEDED(hr));

		if (SUCCEEDED(hr))
		{
			RValue results = { 0 };
			YYCreateArray(&results, 0);

			for (uint32_t j = result_count; j > 0;)
			{
				--j;

				RValue result = _MS_IAP_AddonLicense_to_Struct(&(results_buf[j]));
				SET_RValue(&results, &result, NULL, j);
				FREE_RValue(&result);
			}
			
			DsMapAddRValue(dsMapIndex, "results", &results);

			FREE_RValue(&results);
		}
		else {
			DsMapAddDouble(dsMapIndex, "error", hr);
		}

		CreateAsyncEventWithDSMap(dsMapIndex, EVENT_OTHER_WEB_IAP);
	};

	HRESULT hr = XStoreQueryAddOnLicensesAsync(store_ctx, &(ctx->async));
	if (SUCCEEDED(hr))
	{
		Result.val = ctx->async_id;
	}
	else {
		DebugConsoleOutput("ms_iap_QueryAddOnLicenses - query failed (XStoreQueryAddOnLicensesAsync returned HRESULT 0x%08X)\n", (unsigned)(hr));
		delete ctx;
	}
}

YYEXPORT void F_MS_IAP_QueryAssociatedProducts(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg)
{
	Result.kind = VALUE_REAL;
	Result.val = -1;

	XStoreProductKind product_kinds_e = _MS_IAP_GetProductKinds(arg, 1, "ms_iap_QueryAssociatedProducts");
	if (product_kinds_e == INVALID_PRODUCT_KIND)
	{
		return;
	}

	XStoreContextHandle store_ctx = _MS_IAP_GetStoreHandle(arg, 0, "ms_iap_QueryAssociatedProducts");
	if (store_ctx == NULL)
	{
		return;
	}

	ProductQueryContext* ctx = new ProductQueryContext(
		store_ctx,
		&XStoreQueryAssociatedProductsResult,
		"ms_iap_QueryAssociatedProducts_result",
		"ms_iap_QueryAssociatedProducts");

	HRESULT hr = XStoreQueryAssociatedProductsAsync(store_ctx, product_kinds_e, PRODUCT_QUERY_MAX_ITEMS_PER_PAGE, &(ctx->async));
	if (SUCCEEDED(hr))
	{
		Result.val = ctx->async_id;
	}
	else {
		DebugConsoleOutput("ms_iap_QueryAssociatedProducts - error from XStoreQueryAssociatedProductsAsync (HRESULT 0x%08X)\n", (unsigned)(hr));
		delete ctx;
	}
}

YYEXPORT void F_MS_IAP_QueryConsumableBalanceRemaining(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg)
{
	Result.kind = VALUE_REAL;
	Result.val = -1;

	const char* store_id = YYGetString(arg, 1);

	XStoreContextHandle store_ctx = _MS_IAP_GetStoreHandle(arg, 0, "ms_iap_QueryConsumableBalanceRemaining");
	if (store_ctx == NULL)
	{
		return;
	}

	struct QueryConsumableBalanceRemainingContext
	{
		std::string store_id;
		unsigned int async_id;
		XStoreContextHandle store_ctx;
		XAsyncBlock async;

		QueryConsumableBalanceRemainingContext(const std::string& store_id, unsigned int async_id, XStoreContextHandle store_ctx) :
			store_id(store_id),
			async_id(async_id),
			store_ctx(store_ctx) {}

		~QueryConsumableBalanceRemainingContext()
		{
			XStoreCloseContextHandle(store_ctx);
		}
	};

	unsigned int async_id = next_iap_id++;
	QueryConsumableBalanceRemainingContext* ctx = new QueryConsumableBalanceRemainingContext(store_id, async_id, store_ctx);

	ctx->async.queue = iap_background_to_main_queue;
	ctx->async.context = ctx;
	ctx->async.callback = [](XAsyncBlock* async)
	{
		QueryConsumableBalanceRemainingContext* ctx = (QueryConsumableBalanceRemainingContext*)(async->context);
		std::unique_ptr<QueryConsumableBalanceRemainingContext> ctx_guard(ctx);

		XStoreConsumableResult result;
		HRESULT hr = XStoreQueryConsumableBalanceRemainingResult(async, &result);

		DS_LOCK_MUTEX;

		int dsMapIndex = CreateDsMap(3,
			"id", (double)(ctx->async_id), NULL,
			"type", (double)(0.0), "ms_iap_QueryConsumableBalanceRemaining_result",
			"store_id", (double)(0.0), ctx->store_id.c_str());

		DsMapAddBool(dsMapIndex, "status", SUCCEEDED(hr));

		if (SUCCEEDED(hr))
		{
			DsMapAddDouble(dsMapIndex, "quantity", result.quantity);
		}
		else {
			DebugConsoleOutput("ms_iap_QueryConsumableBalanceRemaining - error from XStoreQueryConsumableBalanceRemainingAsync (HRESULT 0x%08X)\n", (unsigned)(hr));
			DsMapAddDouble(dsMapIndex, "error", hr);
		}

		CreateAsyncEventWithDSMap(dsMapIndex, EVENT_OTHER_WEB_IAP);
	};

	HRESULT hr = XStoreQueryConsumableBalanceRemainingAsync(store_ctx, store_id, &(ctx->async));
	if (SUCCEEDED(hr))
	{
		Result.val = async_id;
	}
	else {
		DebugConsoleOutput("ms_iap_QueryConsumableBalanceRemaining - error from XStoreQueryConsumableBalanceRemainingAsync (HRESULT 0x%08X)\n", (unsigned)(hr));
		delete ctx;
	}
}

YYEXPORT void F_MS_IAP_QueryEntitledProducts(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg)
{
	Result.kind = VALUE_REAL;
	Result.val = -1;

	XStoreProductKind product_kinds_e = _MS_IAP_GetProductKinds(arg, 1, "ms_iap_QueryEntitledProducts");
	if (product_kinds_e == INVALID_PRODUCT_KIND)
	{
		return;
	}

	XStoreContextHandle store_ctx = _MS_IAP_GetStoreHandle(arg, 0, "ms_iap_QueryEntitledProducts");
	if (store_ctx == NULL)
	{
		return;
	}

	ProductQueryContext* ctx = new ProductQueryContext(
		store_ctx,
		&XStoreQueryEntitledProductsResult,
		"ms_iap_QueryEntitledProducts_result",
		"ms_iap_QueryEntitledProducts");

	HRESULT hr = XStoreQueryEntitledProductsAsync(store_ctx, product_kinds_e, PRODUCT_QUERY_MAX_ITEMS_PER_PAGE, &(ctx->async));
	if (SUCCEEDED(hr))
	{
		Result.val = ctx->async_id;
	}
	else {
		DebugConsoleOutput("ms_iap_QueryEntitledProducts - error from XStoreQueryEntitledProductsAsync (HRESULT 0x%08X)\n", (unsigned)(hr));
		delete ctx;
	}
}

YYEXPORT void F_MS_IAP_QueryGameLicense(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg)
{
	Result.kind = VALUE_REAL;
	Result.val = -1;

	XStoreContextHandle store_ctx = _MS_IAP_GetStoreHandle(arg, 0, "ms_iap_QueryGameLicense");
	if (store_ctx == NULL)
	{
		return;
	}

	struct QueryGameLicenseContext
	{
		unsigned int async_id;
		XStoreContextHandle store_ctx;
		XAsyncBlock async;

		QueryGameLicenseContext(XStoreContextHandle store_ctx) :
			async_id(next_iap_id++),
			store_ctx(store_ctx) {}

		~QueryGameLicenseContext()
		{
			XStoreCloseContextHandle(store_ctx);
		}
	};

	QueryGameLicenseContext* ctx = new QueryGameLicenseContext(store_ctx);

	ctx->async.queue = iap_background_to_main_queue;
	ctx->async.context = ctx;
	ctx->async.callback = [](XAsyncBlock* async)
	{
		QueryGameLicenseContext* ctx = (QueryGameLicenseContext*)(async->context);
		std::unique_ptr<QueryGameLicenseContext> ctx_guard(ctx);

		XStoreGameLicense license;
		HRESULT hr = XStoreQueryGameLicenseResult(async, &license);

		if (FAILED(hr))
		{
			DebugConsoleOutput("ms_iap_QueryGameLicense - query failed (XStoreQueryGameLicenseAsync returned HRESULT 0x%08X)\n", (unsigned)(hr));
		}

		DS_LOCK_MUTEX;

		int dsMapIndex = CreateDsMap(2,
			"type", (double)(0.0), "ms_iap_QueryGameLicense_result",
			"id", (double)(ctx->async_id), NULL);

		DsMapAddBool(dsMapIndex, "status", SUCCEEDED(hr));

		if (SUCCEEDED(hr))
		{
			DsMapAddString(dsMapIndex, "skuStoreId", license.skuStoreId);
			DsMapAddBool(dsMapIndex, "isActive", license.isActive);
			DsMapAddBool(dsMapIndex, "isTrialOwnedByThisUser", license.isTrialOwnedByThisUser);
			DsMapAddBool(dsMapIndex, "isDiscLicense", license.isDiscLicense);
			DsMapAddBool(dsMapIndex, "isTrial", license.isTrial);
			DsMapAddDouble(dsMapIndex, "trialTimeRemainingInSeconds", license.trialTimeRemainingInSeconds);
			DsMapAddString(dsMapIndex, "trialUniqueId", license.trialUniqueId);
			DsMapAddDouble(dsMapIndex, "expirationDate", (double)(license.expirationDate));
		}
		else {
			DsMapAddDouble(dsMapIndex, "error", hr);
		}

		CreateAsyncEventWithDSMap(dsMapIndex, EVENT_OTHER_WEB_IAP);
	};

	HRESULT hr = XStoreQueryGameLicenseAsync(store_ctx, &(ctx->async));
	if (SUCCEEDED(hr))
	{
		Result.val = ctx->async_id;
	}
	else {
		DebugConsoleOutput("ms_iap_QueryGameLicense - query failed (XStoreQueryGameLicenseAsync returned HRESULT 0x%08X)\n", (unsigned)(hr));
		delete ctx;
	}
}

YYEXPORT void F_MS_IAP_QueryProductForCurrentGame(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg)
{
	Result.kind = VALUE_REAL;
	Result.val = -1;

	XStoreContextHandle store_ctx = _MS_IAP_GetStoreHandle(arg, 0, "ms_iap_QueryProductForCurrentGame");
	if (store_ctx == NULL)
	{
		return;
	}

	ProductQueryContext* ctx = new ProductQueryContext(
		store_ctx,
		&XStoreQueryProductForCurrentGameResult,
		"ms_iap_QueryProductForCurrentGame_result",
		"ms_iap_QueryProductForCurrentGame");

	HRESULT hr = XStoreQueryProductForCurrentGameAsync(store_ctx, &(ctx->async));
	if (SUCCEEDED(hr))
	{
		Result.val = ctx->async_id;
	}
	else {
		DebugConsoleOutput("ms_iap_QueryProductForCurrentGame - query failed (XStoreQueryProductForCurrentGameAsync returned HRESULT 0x%08X)\n", (unsigned)(hr));
		delete ctx;
	}
}

YYEXPORT void F_MS_IAP_QueryProductForPackage(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg)
{
	Result.kind = VALUE_REAL;
	Result.val = -1;

	const char* store_id = YYGetString(arg, 1);

	XStoreProductKind product_kinds = _MS_IAP_GetProductKinds(arg, 2, "ms_iap_QueryProductForPackage");
	if (product_kinds == INVALID_PRODUCT_KIND)
	{
		return;
	}

	XStoreContextHandle store_ctx = _MS_IAP_GetStoreHandle(arg, 0, "ms_iap_QueryProductForPackage");
	if (store_ctx == NULL)
	{
		return;
	}

	ProductQueryContext* ctx = new ProductQueryContext(
		store_ctx,
		&XStoreQueryProductForPackageResult,
		"ms_iap_QueryProductForPackage_result",
		"ms_iap_QueryProductForPackage");

	HRESULT hr = XStoreQueryProductForPackageAsync(store_ctx, product_kinds, store_id, &(ctx->async));
	if (SUCCEEDED(hr))
	{
		Result.val = ctx->async_id;
	}
	else {
		DebugConsoleOutput("ms_iap_QueryProductForPackage - query failed (XStoreQueryProductForPackageAsync returned HRESULT 0x%08X)\n", (unsigned)(hr));
		delete ctx;
	}
}

YYEXPORT void F_MS_IAP_QueryProducts(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg)
{
	Result.kind = VALUE_REAL;
	Result.val = -1;

	XStoreProductKind product_kinds = _MS_IAP_GetProductKinds(arg, 1, "ms_iap_QueryProducts");
	if (product_kinds == INVALID_PRODUCT_KIND)
	{
		return;
	}

	std::vector<const char*> store_ids = _MS_IAP_GetArrayOfStrings(arg, 2, "ms_iap_QueryProducts");
	std::vector<const char*> action_filters = _MS_IAP_GetArrayOfStrings(arg, 3, "ms_iap_QueryProducts");

	XStoreContextHandle store_ctx = _MS_IAP_GetStoreHandle(arg, 0, "ms_iap_QueryProducts");
	if (store_ctx == NULL)
	{
		return;
	}

	ProductQueryContext* ctx = new ProductQueryContext(
		store_ctx,
		&XStoreQueryProductForPackageResult,
		"ms_iap_QueryProducts_result",
		"ms_iap_QueryProducts");

	HRESULT hr = XStoreQueryProductsAsync(
		store_ctx,
		product_kinds,
		store_ids.data(), store_ids.size(),
		action_filters.data(), action_filters.size(),
		&(ctx->async));

	if (SUCCEEDED(hr))
	{
		Result.val = ctx->async_id;
	}
	else {
		DebugConsoleOutput("ms_iap_QueryProducts - query failed (XStoreQueryProductsAsync returned HRESULT 0x%08X)\n", (unsigned)(hr));
		delete ctx;
	}
}

void ProductQueryContext::_AsyncQueryCallback(XAsyncBlock *async)
{
	ProductQueryContext* ctx = (ProductQueryContext*)(async->context);
	std::unique_ptr<ProductQueryContext> ctx_guard(ctx);

	XStoreProductQueryHandle query_handle;
	HRESULT hr;

	if (ctx->page_count == 0)
	{
		hr = ctx->results_func(async, &query_handle);
	}
	else {
		hr = XStoreProductsQueryNextPageResult(async, &query_handle);
	}

	if (FAILED(hr))
	{
		DebugConsoleOutput("%s - query failed (HRESULT 0x%08X)\n", ctx->func_name, (unsigned)(hr));
	}

	if (SUCCEEDED(hr))
	{
		hr = XStoreEnumerateProductsQuery(query_handle, ctx, &_ProductQueryCallback);

		if (SUCCEEDED(hr))
		{
			if (XStoreProductsQueryHasMorePages(query_handle))
			{
				hr = XStoreProductsQueryNextPageAsync(query_handle, async);

				if (SUCCEEDED(hr))
				{
					ctx_guard.release(); /* Forward ctx ownership to next callback. */
				}
				else {
					DebugConsoleOutput("%s - query failed (XStoreProductsQueryNextPageAsync returned HRESULT 0x%08X)\n",
						ctx->func_name, (unsigned)(hr));
				}
			}
			else
			{
				/* Got all results. Raise async event. */

				DS_LOCK_MUTEX;

				int dsMapIndex = CreateDsMap(2,
					"id", (double)(ctx->async_id), NULL,
					"type", (double)(0.0), ctx->async_type);

				DsMapAddBool(dsMapIndex, "status", true);
				DsMapAddRValue(dsMapIndex, "results", &(ctx->results));

				CreateAsyncEventWithDSMap(dsMapIndex, EVENT_OTHER_WEB_IAP);
			}
		}
		else
		{
			DebugConsoleOutput("%s - error from XStoreEnumerateProductsQuery (HRESULT 0x%08X)\n", ctx->func_name, (unsigned)(hr));
		}
	}

	if (FAILED(hr))
	{
		/* One of the XStore calls failed. */

		DS_LOCK_MUTEX;

		int dsMapIndex = CreateDsMap(3,
			"id", (double)(ctx->async_id), NULL,
			"type", (double)(0.0), ctx->async_type,
			"error", (double)(hr), NULL);

		DsMapAddBool(dsMapIndex, "status", false);

		CreateAsyncEventWithDSMap(dsMapIndex, EVENT_OTHER_WEB_IAP);
	}
}

bool ProductQueryContext::_ProductQueryCallback(const XStoreProduct* product, void* context)
{
	ProductQueryContext* ctx = (ProductQueryContext*)(context);

	RValue product_rv = _MS_IAP_StoreProduct_to_Struct(product);

	SET_RValue(&(ctx->results), &product_rv, NULL, ctx->n_results);
	++(ctx->n_results);

	FREE_RValue(&product_rv);

	return true;
}

YYEXPORT void F_MS_IAP_ReportConsumableFulfillment(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg)
{
	Result.kind = VALUE_REAL;
	Result.val = -1;

	const char* store_id = YYGetString(arg, 1);
	uint32_t quantity = YYGetUint32(arg, 2);
	const char *tracking_id_s = YYGetString(arg, 3);

	GUID tracking_id;
	if (!StringToGUID(&tracking_id, tracking_id_s))
	{
		DebugConsoleOutput("ms_iap_ReportConsumableFulfillment - invalid GUID in tracking_id parameter\n");
		return;
	}

	XStoreContextHandle store_ctx = _MS_IAP_GetStoreHandle(arg, 0, "ms_iap_ReportConsumableFulfillment");
	if (store_ctx == NULL)
	{
		return;
	}

	struct ReportConsumableFulfillmentContext
	{
		std::string store_id;
		uint32_t quantity;
		unsigned int async_id;
		XStoreContextHandle store_ctx;
		XAsyncBlock async;

		ReportConsumableFulfillmentContext(const std::string& store_id, uint32_t quantity, unsigned int async_id, XStoreContextHandle store_ctx) :
			store_id(store_id),
			quantity(quantity),
			async_id(async_id),
			store_ctx(store_ctx) {}

		~ReportConsumableFulfillmentContext()
		{
			XStoreCloseContextHandle(store_ctx);
		}
	};

	unsigned int async_id = next_iap_id++;
	ReportConsumableFulfillmentContext* ctx = new ReportConsumableFulfillmentContext(store_id, quantity, async_id, store_ctx);

	ctx->async.queue = iap_background_to_main_queue;
	ctx->async.context = ctx;
	ctx->async.callback = [](XAsyncBlock* async)
	{
		ReportConsumableFulfillmentContext* ctx = (ReportConsumableFulfillmentContext*)(async->context);
		std::unique_ptr<ReportConsumableFulfillmentContext> ctx_guard(ctx);

		XStoreConsumableResult result;
		HRESULT hr = XStoreReportConsumableFulfillmentResult(async, &result);

		DS_LOCK_MUTEX;
		
		int dsMapIndex = CreateDsMap(4,
			"id", (double)(ctx->async_id), NULL,
			"type", (double)(0.0), "ms_iap_ReportConsumableFulfillment_result",
			"store_id", (double)(0.0), ctx->store_id.c_str(),
			"consumed_quantity", (double)(ctx->quantity), NULL);

		DsMapAddBool(dsMapIndex, "status", SUCCEEDED(hr));

		if (SUCCEEDED(hr))
		{
			DsMapAddDouble(dsMapIndex, "available_quantity", result.quantity);
		}
		else {
			DebugConsoleOutput("ms_iap_ReportConsumableFulfillment - error from XStoreReportConsumableFulfillmentAsync (HRESULT 0x%08X)\n", (unsigned)(hr));
			DsMapAddDouble(dsMapIndex, "error", hr);
		}

		CreateAsyncEventWithDSMap(dsMapIndex, EVENT_OTHER_WEB_IAP);
	};

	HRESULT hr = XStoreReportConsumableFulfillmentAsync(store_ctx, store_id, quantity, tracking_id, &(ctx->async));
	if (SUCCEEDED(hr))
	{
		Result.val = async_id;
	}
	else {
		DebugConsoleOutput("ms_iap_ReportConsumableFulfillment - error from XStoreReportConsumableFulfillmentAsync (HRESULT 0x%08X)\n", (unsigned)(hr));
		delete ctx;
	}
}

YYEXPORT void F_MS_IAP_ShowAssociatedProductsUI(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg)
{
	Result.kind = VALUE_REAL;
	Result.val = 0;

	const char* store_id = YYGetString(arg, 1);

	XStoreProductKind product_kinds = _MS_IAP_GetProductKinds(arg, 2, "ms_iap_ShowAssociatedProductsUI");
	if (product_kinds == INVALID_PRODUCT_KIND)
	{
		return;
	}

	XStoreContextHandle store_ctx = _MS_IAP_GetStoreHandle(arg, 0, "ms_iap_ShowAssociatedProductsUI");
	if (store_ctx == NULL)
	{
		return;
	}

	struct ShowAssociatedProductsUIContext
	{
		XStoreContextHandle store_ctx;
		XAsyncBlock async;

		ShowAssociatedProductsUIContext(XStoreContextHandle store_ctx) :
			store_ctx(store_ctx) {}

		~ShowAssociatedProductsUIContext()
		{
			XStoreCloseContextHandle(store_ctx);
		}
	};

	ShowAssociatedProductsUIContext* ctx = new ShowAssociatedProductsUIContext(store_ctx);

	ctx->async.queue = iap_background_to_main_queue;
	ctx->async.context = ctx;
	ctx->async.callback = [](XAsyncBlock* async)
	{
		ShowAssociatedProductsUIContext* ctx = (ShowAssociatedProductsUIContext*)(async->context);
		std::unique_ptr<ShowAssociatedProductsUIContext> ctx_guard(ctx);

		HRESULT hr = XStoreShowAssociatedProductsUIResult(async);
		if (FAILED(hr))
		{
			DebugConsoleOutput("ms_iap_ShowAssociatedProductsUI - error from XStoreShowAssociatedProductsUIAsync (HRESULT 0x%08X)\n", (unsigned)(hr));
		}
	};

	HRESULT hr = XStoreShowAssociatedProductsUIAsync(store_ctx, store_id, product_kinds, &(ctx->async));
	if (FAILED(hr))
	{
		DebugConsoleOutput("ms_iap_ShowAssociatedProductsUI - error from XStoreShowAssociatedProductsUIAsync (HRESULT 0x%08X)\n", (unsigned)(hr));
		delete ctx;
	}
}

YYEXPORT void F_MS_IAP_ShowProductPageUI(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg)
{
	Result.kind = VALUE_REAL;
	Result.val = 0;

	const char* store_id = YYGetString(arg, 1);

	XStoreContextHandle store_ctx = _MS_IAP_GetStoreHandle(arg, 0, "ms_iap_ShowProductPageUI");
	if (store_ctx == NULL)
	{
		return;
	}

	struct ShowProductPageUIContext
	{
		XStoreContextHandle store_ctx;
		XAsyncBlock async;

		ShowProductPageUIContext(XStoreContextHandle store_ctx) :
			store_ctx(store_ctx) {}

		~ShowProductPageUIContext()
		{
			XStoreCloseContextHandle(store_ctx);
		}
	};

	ShowProductPageUIContext* ctx = new ShowProductPageUIContext(store_ctx);

	ctx->async.queue = iap_background_to_main_queue;
	ctx->async.context = ctx;
	ctx->async.callback = [](XAsyncBlock* async)
	{
		ShowProductPageUIContext* ctx = (ShowProductPageUIContext*)(async->context);
		std::unique_ptr<ShowProductPageUIContext> ctx_guard(ctx);

		HRESULT hr = XStoreShowProductPageUIResult(async);
		if (FAILED(hr))
		{
			DebugConsoleOutput("ms_iap_ShowProductPageUI - error from XStoreShowProductPageUIAsync (HRESULT 0x%08X)\n", (unsigned)(hr));
		}
	};

	HRESULT hr = XStoreShowProductPageUIAsync(store_ctx, store_id, &(ctx->async));
	if (FAILED(hr))
	{
		DebugConsoleOutput("ms_iap_ShowProductPageUI - error from XStoreShowProductPageUIAsync (HRESULT 0x%08X)\n", (unsigned)(hr));
		delete ctx;
	}
}

YYEXPORT void F_MS_IAP_ShowPurchaseUI(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg)
{
	Result.kind = VALUE_REAL;
	Result.val = 0;

	const char* store_id = YYGetString(arg, 1);
	const char* name = (KIND_RValue(&(arg[2])) == VALUE_UNDEFINED) ? NULL : YYGetString(arg, 2);
	const char* json = (KIND_RValue(&(arg[3])) == VALUE_UNDEFINED) ? NULL : YYGetString(arg, 3);

	XStoreContextHandle store_ctx = _MS_IAP_GetStoreHandle(arg, 0, "ms_iap_ShowPurchaseUI");
	if (store_ctx == NULL)
	{
		return;
	}

	struct ShowPurchaseUIContext
	{
		XStoreContextHandle store_ctx;
		XAsyncBlock async;

		ShowPurchaseUIContext(XStoreContextHandle store_ctx) :
			store_ctx(store_ctx) {}

		~ShowPurchaseUIContext()
		{
			XStoreCloseContextHandle(store_ctx);
		}
	};

	ShowPurchaseUIContext* ctx = new ShowPurchaseUIContext(store_ctx);

	ctx->async.queue = iap_background_to_main_queue;
	ctx->async.context = ctx;
	ctx->async.callback = [](XAsyncBlock* async)
	{
		ShowPurchaseUIContext* ctx = (ShowPurchaseUIContext*)(async->context);
		std::unique_ptr<ShowPurchaseUIContext> ctx_guard(ctx);

		HRESULT hr = XStoreShowPurchaseUIResult(async);
		if (FAILED(hr))
		{
			DebugConsoleOutput("ms_iap_ShowPurchaseUI - error from XStoreShowPurchaseUIAsync (HRESULT 0x%08X)\n", (unsigned)(hr));
		}
	};

	HRESULT hr = XStoreShowPurchaseUIAsync(store_ctx, store_id, name, json, &(ctx->async));
	if (FAILED(hr))
	{
		DebugConsoleOutput("ms_iap_ShowPurchaseUI - error from XStoreShowPurchaseUIAsync (HRESULT 0x%08X)\n", (unsigned)(hr));
		delete ctx;
	}
}

YYEXPORT void F_MS_IAP_ShowRateAndReviewUI(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg)
{
	Result.kind = VALUE_REAL;

	XStoreContextHandle store_ctx = _MS_IAP_GetStoreHandle(arg, 0, "ms_iap_ShowRateAndReviewUI");
	if (store_ctx == NULL)
	{
		Result.val = -1.0;
		return;
	}

	struct ShowRateAndReviewUIContext
	{
		unsigned async_id;
		XStoreContextHandle store_ctx;
		XAsyncBlock async;

		ShowRateAndReviewUIContext(unsigned async_id, XStoreContextHandle store_ctx) :
			async_id(async_id),
			store_ctx(store_ctx) {}

		~ShowRateAndReviewUIContext()
		{
			XStoreCloseContextHandle(store_ctx);
		}
	};

	unsigned async_id = next_iap_id++;
	ShowRateAndReviewUIContext* ctx = new ShowRateAndReviewUIContext(async_id, store_ctx);

	ctx->async.queue = iap_background_to_main_queue;
	ctx->async.context = ctx;
	ctx->async.callback = [](XAsyncBlock* async)
	{
		ShowRateAndReviewUIContext* ctx = (ShowRateAndReviewUIContext*)(async->context);
		std::unique_ptr<ShowRateAndReviewUIContext> ctx_guard(ctx);

		XStoreRateAndReviewResult result;
		HRESULT hr = XStoreShowRateAndReviewUIResult(async, &result);
		if (SUCCEEDED(hr))
		{
			DS_LOCK_MUTEX;

			int dsMapIndex = CreateDsMap(3,
				"id", (double)(ctx->async_id), NULL,
				"type", (double)(0.0), "ms_iap_ShowRateAndReviewUI_result",
				"wasUpdated", (double)(result.wasUpdated), NULL);

			DsMapAddBool(dsMapIndex, "status", true);

			CreateAsyncEventWithDSMap(dsMapIndex, EVENT_OTHER_WEB_IAP);
		}
		else {
			DebugConsoleOutput("ms_iap_ShowRateAndReviewUI - error from XStoreShowRateAndReviewUIAsync (HRESULT 0x%08X)\n", (unsigned)(hr));

			DS_LOCK_MUTEX;

			int dsMapIndex = CreateDsMap(3,
				"id", (double)(ctx->async_id), NULL,
				"type", (double)(0.0), "ms_iap_ShowRateAndReviewUI_result",
				"error", (double)(hr), NULL);

			DsMapAddBool(dsMapIndex, "status", false);

			CreateAsyncEventWithDSMap(dsMapIndex, EVENT_OTHER_WEB_IAP);
		}
	};

	HRESULT hr = XStoreShowRateAndReviewUIAsync(store_ctx, &(ctx->async));
	if (SUCCEEDED(hr))
	{
		Result.val = async_id;
	}
	else {
		DebugConsoleOutput("ms_iap_ShowRateAndReviewUI - error from XStoreShowRateAndReviewUIAsync (HRESULT 0x%08X)\n", (unsigned)(hr));
		delete ctx;

		Result.val = -1.0;
	}
}

YYEXPORT void F_MS_IAP_ShowRedeemTokenUI(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg)
{
	Result.kind = VALUE_REAL;

	const char* token = YYGetString(arg, 1);
	std::vector<const char*> allowed_store_ids = _MS_IAP_GetArrayOfStrings(arg, 2, "ms_iap_ShowRedeemTokenUI");
	bool disallow_cvs_redemption = YYGetBool(arg, 3);

	XStoreContextHandle store_ctx = _MS_IAP_GetStoreHandle(arg, 0, "ms_iap_ShowRedeemTokenUI");
	if (store_ctx == NULL)
	{
		Result.val = -1.0;
		return;
	}

	struct ShowRedeemTokenUIContext
	{
		unsigned async_id;
		XStoreContextHandle store_ctx;
		XAsyncBlock async;

		ShowRedeemTokenUIContext(XStoreContextHandle store_ctx) :
			async_id(next_iap_id++),
			store_ctx(store_ctx) {}

		~ShowRedeemTokenUIContext()
		{
			XStoreCloseContextHandle(store_ctx);
		}
	};

	ShowRedeemTokenUIContext* ctx = new ShowRedeemTokenUIContext(store_ctx);

	ctx->async.queue = iap_background_to_main_queue;
	ctx->async.context = ctx;
	ctx->async.callback = [](XAsyncBlock* async)
	{
		ShowRedeemTokenUIContext* ctx = (ShowRedeemTokenUIContext*)(async->context);
		std::unique_ptr<ShowRedeemTokenUIContext> ctx_guard(ctx);

		HRESULT hr = XStoreShowRedeemTokenUIResult(async);
		if (FAILED(hr))
		{
			DebugConsoleOutput("ms_iap_ShowRedeemTokenUI - error (HRESULT 0x%08X)\n", (unsigned)(hr));
		}

		DS_LOCK_MUTEX;

		int dsMapIndex = CreateDsMap(2,
			"type", (double)(0.0), "ms_iap_ShowRedeemTokenUI_result",
			"id", (double)(ctx->async_id), NULL);

		DsMapAddBool(dsMapIndex, "status", SUCCEEDED(hr));

		if (FAILED(hr))
		{
			DsMapAddDouble(dsMapIndex, "error", hr);
		}

		CreateAsyncEventWithDSMap(dsMapIndex, EVENT_OTHER_WEB_IAP);
	};

	HRESULT hr = XStoreShowRedeemTokenUIAsync(store_ctx, token, allowed_store_ids.data(), allowed_store_ids.size(), disallow_cvs_redemption, &(ctx->async));
	if (SUCCEEDED(hr))
	{
		Result.val = ctx->async_id;
	}
	else {
		DebugConsoleOutput("ms_iap_ShowRedeemTokenUI - error (HRESULT 0x%08X)\n", (unsigned)(hr));
		delete ctx;

		Result.val = -1.0;
	}
}

struct UnmountPackageContext
{
	unsigned int async_id;
	std::string package_id;
	XPackageMountHandle mount_handle;

	UnmountPackageContext(const std::string& package_id, XPackageMountHandle mount_handle);

	static void work_callback(void* context, bool canceled);
	static void completion_callback(void* context, bool canceled);
};

UnmountPackageContext::UnmountPackageContext(const std::string& package_id, XPackageMountHandle mount_handle) :
	async_id(next_iap_id++),
	package_id(package_id),
	mount_handle(mount_handle) {}

void UnmountPackageContext::work_callback(void* context, bool canceled)
{
	UnmountPackageContext* ctx = (UnmountPackageContext*)(context);

	XPackageCloseMountHandle(ctx->mount_handle);

	HRESULT hr = XTaskQueueSubmitCallback(iap_background_to_main_queue, XTaskQueuePort::Completion, ctx, &UnmountPackageContext::completion_callback);
	if (FAILED(hr))
	{
		DebugConsoleOutput("UnmountPackageContext::WorkCallback - unable to queue completion_callback (HRESULT 0x%08X)\n", (unsigned)(hr));
		delete ctx;
	}
}

void UnmountPackageContext::completion_callback(void* context, bool canceled)
{
	UnmountPackageContext* ctx = (UnmountPackageContext*)(context);
	std::unique_ptr<UnmountPackageContext> ctx_guard(ctx);

	auto mp = mounted_packages.find(ctx->package_id);
	assert(mp != mounted_packages.end());
	assert(mp->second != NULL);
	assert(mp->second->unmounting);

	DS_LOCK_MUTEX;

	int dsMapIndex = CreateDsMap(4,
		"id", (double)(ctx->async_id), NULL,
		"type", (double)(0.0), "ms_iap_UnmountPackage_result",
		"package_id", (double)(0.0), ctx->package_id.c_str(),
		"mount_path", (double)(0.0), mp->second->mount_path.c_str());

	mounted_packages.erase(mp);

	CreateAsyncEventWithDSMap(dsMapIndex, EVENT_OTHER_WEB_IAP);
}

YYEXPORT void F_MS_IAP_UnmountPackage(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg)
{
	Result.kind = VALUE_REAL;
	Result.val = -1;

	const char* package_id = YYGetString(arg, 0);

	auto mp = mounted_packages.find(package_id);

	if (mp == mounted_packages.end())
	{
		DebugConsoleOutput("ms_iap_UnmountPackage - Package %s isn't mounted\n", package_id);
		return;
	}

	if (mp->second->unmounting)
	{
		DebugConsoleOutput("ms_iap_UnmountPackage - Package %s is already being unmounted\n", package_id);
		return;
	}

	UnmountPackageContext* ctx = new UnmountPackageContext(package_id, mp->second->mount_handle);
	mp->second->unmounting = true;

	HRESULT hr = XTaskQueueSubmitCallback(iap_background_to_main_queue, XTaskQueuePort::Work, ctx, &UnmountPackageContext::work_callback);
	if (SUCCEEDED(hr))
	{
		Result.val = ctx->async_id;
	}
	else {
		DebugConsoleOutput("ms_iap_UnmountPackage - Unable to queue work (HRESULT 0x%08X)\n", (unsigned)(hr));
		mp->second->unmounting = false;
		delete ctx;
	}
}

static RValue _MS_IAP_StoreImage_to_Struct(const XStoreImage* image)
{
	RValue image_rv = { 0 };
	YYStructCreate(&image_rv);

	if (image->uri != NULL)
	{
		/* The 'uri' seems to omit the protocol for some reason and the InGameStore
		 * GDK sample prefixes it with an "https:" to work around it.
		*/

		std::string uri = std::string("https:") + image->uri;
		YYStructAddString(&image_rv, "uri", uri.c_str());
	}
	else {
		_MS_IAP_AddStringOrUndefined(&image_rv, "uri", NULL);
	}
	
	YYStructAddInt(&image_rv, "height", (int)(image->height));
	YYStructAddInt(&image_rv, "width", (int)(image->width));

	_MS_IAP_AddStringOrUndefined(&image_rv, "caption", image->caption);
	_MS_IAP_AddStringOrUndefined(&image_rv, "imagePurposeTag", image->imagePurposeTag);

	return image_rv;
}

static RValue _MS_IAP_StoreProduct_to_Struct(const XStoreProduct* product)
{
	RValue product_rv = { 0 };
	YYStructCreate(&product_rv);

	_MS_IAP_AddStringOrUndefined(&product_rv, "storeId", product->storeId);
	_MS_IAP_AddStringOrUndefined(&product_rv, "title", product->title);
	_MS_IAP_AddStringOrUndefined(&product_rv, "description", product->description);
	_MS_IAP_AddStringOrUndefined(&product_rv, "language", product->language);
	_MS_IAP_AddStringOrUndefined(&product_rv, "inAppOfferToken", product->inAppOfferToken);
	_MS_IAP_AddStringOrUndefined(&product_rv, "linkUri", product->linkUri);
	YYStructAddDouble(&product_rv, "productKind", (double)(product->productKind));
	YYStructAddBool(&product_rv, "hasDigitalDownload", product->hasDigitalDownload);
	YYStructAddBool(&product_rv, "isInUserCollection", product->isInUserCollection);

	RValue price = _MS_IAP_StorePrice_to_Struct(&(product->price));
	YYStructAddRValue(&product_rv, "price", &price);
	FREE_RValue(&price);

	{
		RValue keywords = { 0 };
		YYCreateArray(&keywords, 0);

		for (uint32_t j = product->keywordsCount; j > 0;)
		{
			--j;

			RValue keyword = { 0 };
			YYCreateString(&keyword, product->keywords[j]);

			SET_RValue(&keywords, &keyword, NULL, j);
			FREE_RValue(&keyword);
		}

		YYStructAddRValue(&product_rv, "keywords", &keywords);
		FREE_RValue(&keywords);
	}

	{
		RValue images = { 0 };
		YYCreateArray(&images, 0);

		for (uint32_t j = product->imagesCount; j > 0;)
		{
			--j;

			RValue image = _MS_IAP_StoreImage_to_Struct(&(product->images[j]));

			SET_RValue(&images, &image, NULL, j);
			FREE_RValue(&image);
		}

		YYStructAddRValue(&product_rv, "images", &images);
		FREE_RValue(&images);
	}

	return product_rv;
}

static RValue _MS_IAP_StorePrice_to_Struct(const XStorePrice* price)
{
	RValue price_rv = { 0 };
	YYStructCreate(&price_rv);

	YYStructAddDouble(&price_rv, "basePrice", price->basePrice);
	YYStructAddDouble(&price_rv, "price", price->price);
	YYStructAddDouble(&price_rv, "recurrencePrice", price->recurrencePrice);
	_MS_IAP_AddStringOrUndefined(&price_rv, "currencyCode", price->currencyCode);
	_MS_IAP_AddStringOrUndefined(&price_rv, "formattedBasePrice", price->formattedBasePrice);
	_MS_IAP_AddStringOrUndefined(&price_rv, "formattedPrice", price->formattedPrice);
	_MS_IAP_AddStringOrUndefined(&price_rv, "formattedRecurrencePrice", price->formattedRecurrencePrice);
	YYStructAddBool(&price_rv, "isOnSale", price->isOnSale);
	YYStructAddDouble(&price_rv, "saleEndDate", price->saleEndDate);

	return price_rv;
}

static RValue _MS_IAP_AddonLicense_to_Struct(const XStoreAddonLicense* license)
{
	RValue license_rv = { 0 };
	YYStructCreate(&license_rv);

	_MS_IAP_AddStringOrUndefined(&license_rv, "skuStoreId", license->skuStoreId);
	_MS_IAP_AddStringOrUndefined(&license_rv, "inAppOfferToken", license->inAppOfferToken);
	YYStructAddBool(&license_rv, "isActive", license->isActive);
	YYStructAddDouble(&license_rv, "expirationDate", license->expirationDate);

	return license_rv;
}

static RValue _MS_IAP_PackageDetails_to_Struct(const XPackageDetails* details)
{
	RValue details_rv = { 0 };
	YYStructCreate(&details_rv);
	
	char version_s[48];
	snprintf(version_s, sizeof(version_s),
		"%" PRIu16 ".%" PRIu16 ".%" PRIu16 ".%" PRIu16,
		details->version.major, details->version.minor, details->version.build, details->version.revision);

	_MS_IAP_AddStringOrUndefined(&details_rv, "packageIdentifier", details->packageIdentifier);
	YYStructAddString(&details_rv, "version", version_s);
	YYStructAddDouble(&details_rv, "kind", (double)(details->kind));
	_MS_IAP_AddStringOrUndefined(&details_rv, "displayName", details->displayName);
	_MS_IAP_AddStringOrUndefined(&details_rv, "description", details->description);
	_MS_IAP_AddStringOrUndefined(&details_rv, "publisher", details->publisher);
	_MS_IAP_AddStringOrUndefined(&details_rv, "storeId", details->storeId);
	YYStructAddBool(&details_rv, "installing", details->installing);

	return details_rv;
}

static void _MS_IAP_AddStringOrUndefined(RValue* obj, const char* _pName, const char* _pString)
{
	if (_pString != NULL)
	{
		YYStructAddString(obj, _pName, _pString);
	}
	else {
		RValue undef = { 0 };
		undef.kind = VALUE_UNDEFINED;

		YYStructAddRValue(obj, _pName, &undef);
	}
}

static XStoreContextHandle _MS_IAP_GetStoreHandle(RValue *arg, int user_id_arg_idx, const char *func)
{
	uint64 user_id = (uint64)YYGetInt64(arg, user_id_arg_idx);

	XStoreContextHandle store_ctx = NULL;

	{
		XUM_LOCK_MUTEX;

		XUMuser* user = XUM::GetUserFromId(user_id);
		if (user == NULL)
		{
			DebugConsoleOutput("%s - user_id %" PRIu64 " not found\n", func, user_id);
			return NULL;
		}

		HRESULT hr = XStoreCreateContext(user->user, &store_ctx);
		if (FAILED(hr))
		{
			DebugConsoleOutput("%s - Unable to create store context (HRESULT 0x%08X)\n", func, (unsigned)(hr));
			return NULL;
		}
	}

	return store_ctx;
}

static XStoreProductKind _MS_IAP_GetProductKinds(RValue *arg, int product_kinds_arg_idx, const char *func)
{
	uint32 product_kinds = YYGetUint32(arg, product_kinds_arg_idx);

	XStoreProductKind product_kinds_e = XStoreProductKind::None;

	auto add_pk = [&](XStoreProductKind pk)
	{
		if ((product_kinds & (uint32)(pk)) != 0)
		{
			product_kinds_e = (XStoreProductKind)((uint32)(product_kinds_e) | (uint32)(pk));
			product_kinds &= ~(uint32)(pk);
		}
	};

	add_pk(XStoreProductKind::Consumable);
	add_pk(XStoreProductKind::Durable);
	add_pk(XStoreProductKind::Game);
	add_pk(XStoreProductKind::Pass);
	add_pk(XStoreProductKind::UnmanagedConsumable);

	if (product_kinds != 0)
	{
		DebugConsoleOutput("%s - Unsupported product_kinds value\n", func);
		return INVALID_PRODUCT_KIND;
	}

	return product_kinds_e;
}

static std::vector<const char*> _MS_IAP_GetArrayOfStrings(RValue* arg, int arg_idx, const char* func)
{
	RValue* pV = &(arg[arg_idx]);

	std::vector<const char*> strings;

	if (KIND_RValue(pV) == VALUE_ARRAY)
	{
		int arraylength = YYArrayGetLength(pV);
		RValue elem;
		for (int i = 0; i < arraylength; ++i)
		{
			if (GET_RValue(&elem, pV, NULL, i) == false)
				break;
		
			if (KIND_RValue(&elem) != VALUE_STRING)
			{
				YYError("%s argument %d [array element %d] incorrect type (%s) expecting a String", func, (arg_idx + 1), i, KIND_NAME_RValue(pV));
			}

			strings.push_back(elem.GetString());
		}
	}
	else {
		YYError("%s argument %d incorrect type (%s) expecting an Array", func, (arg_idx + 1), KIND_NAME_RValue(pV));
	}

	return strings;
}

static XPackageKind _MS_IAP_GetPackageKind(RValue* arg, int arg_idx, const char* func)
{
	uint32 package_kind = YYGetUint32(arg, arg_idx);

	switch (package_kind)
	{
		case (uint32_t)(XPackageKind::Game) : return XPackageKind::Game;
		case (uint32_t)(XPackageKind::Content) : return XPackageKind::Content;

		default:
			DebugConsoleOutput("%s - Unsupported package_kind value\n", func);
			return INVALID_PACKAGE_KIND;
	}
}

static XPackageEnumerationScope _MS_IAP_GetPackageEnumerationScope(RValue* arg, int arg_idx, const char* func)
{
	uint32 scope = YYGetUint32(arg, arg_idx);

	switch (scope)
	{
		case (uint32_t)(XPackageEnumerationScope::ThisOnly) : return XPackageEnumerationScope::ThisOnly;
		case (uint32_t)(XPackageEnumerationScope::ThisAndRelated) : return XPackageEnumerationScope::ThisAndRelated;

		default:
			DebugConsoleOutput("%s - Unsupported scope value\n", func);
			return INVALID_ENUMERATION_SCOPE;
	}
}
