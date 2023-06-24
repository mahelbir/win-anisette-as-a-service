#define _CRT_SECURE_NO_WARNINGS
#include <filesystem>
#include <vector>
#include <string>
#include <map>
#include <exception>
#include <memory>
#include <functional>
#include <iostream>
#include "Error.h"
#include <sstream>
#include <set>
#include <ShlObj.h>
#include "base64.h"
#include "Log.h"
#include "http.h"
#include "Anisette.h"

namespace fs = std::filesystem;

class AnisetteDataManager
{
public:
	static AnisetteDataManager* instance();

	std::shared_ptr<AnisetteData> FetchAnisetteData();
	bool LoadDependencies();

	bool ResetProvisioning();

private:
	AnisetteDataManager();
	~AnisetteDataManager();

	static AnisetteDataManager* _instance;

	bool ReprovisionDevice(std::function<void(void)> provisionCallback);
	bool LoadiCloudDependencies();

	bool loadedDependencies;
};


std::string getServerID() {
	return "5E03738B-03F6-46B5-9A6A-F022279B3040";
}

std::string appleFolderPath() {
	wchar_t* programFilesCommonDirectory;
	SHGetKnownFolderPath(FOLDERID_ProgramFilesCommon, 0, NULL, &programFilesCommonDirectory);

	fs::path appleDirectoryPath(programFilesCommonDirectory);
	appleDirectoryPath.append("Apple");

	return appleDirectoryPath.string();
}

std::string internetServicesFolderPath()
{
	fs::path internetServicesDirectoryPath(appleFolderPath());
	internetServicesDirectoryPath.append("Internet Services");
	return internetServicesDirectoryPath.string();
}

std::string applicationSupportFolderPath()
{
	fs::path applicationSupportDirectoryPath(appleFolderPath());
	applicationSupportDirectoryPath.append("Apple Application Support");
	return applicationSupportDirectoryPath.string();
}

#define id void*
#define SEL void*

typedef id(__cdecl* GETCLASSFUNC)(const char* name);
typedef id(__cdecl* REGISTERSELFUNC)(const char* name);
typedef id(__cdecl* SENDMSGFUNC)(id self, void* _cmd);
typedef id(__cdecl* SENDMSGFUNC_OBJ)(id self, void* _cmd, id parameter1);
typedef id(__cdecl* SENDMSGFUNC_INT)(id self, void* _cmd, int parameter1);
typedef id* (__cdecl* COPYMETHODLISTFUNC)(id cls, unsigned int* outCount);
typedef id(__cdecl* GETMETHODNAMEFUNC)(id method);
typedef const char* (__cdecl* GETSELNAMEFUNC)(SEL sel);
typedef id(__cdecl* GETOBJCCLASSFUNC)(id obj);

typedef id(__cdecl* GETOBJECTFUNC)();

typedef id(__cdecl* CLIENTINFOFUNC)(id obj);
typedef id(__cdecl* COPYANISETTEDATAFUNC)(void*, int, void*);

#define odslog(msg) { std::stringstream ss; ss << msg << std::endl; OutputDebugStringA(ss.str().c_str()); }

extern std::string StringFromWideString(std::wstring wideString);

GETCLASSFUNC objc_getClass;
REGISTERSELFUNC sel_registerName;
SENDMSGFUNC objc_msgSend;
COPYMETHODLISTFUNC class_copyMethodList;
GETMETHODNAMEFUNC method_getName;
GETSELNAMEFUNC sel_getName;
GETOBJCCLASSFUNC object_getClass;

GETOBJECTFUNC GetDeviceID;
GETOBJECTFUNC GetLocalUserID;
CLIENTINFOFUNC GetClientInfo;
COPYANISETTEDATAFUNC CopyAnisetteData;

class ObjcObject
{
public:
	id isa;

	std::string description() const
	{
		id descriptionSEL = sel_registerName("description");
		id descriptionNSString = objc_msgSend((void*)this, descriptionSEL);

		id cDescriptionSEL = sel_registerName("UTF8String");
		const char* cDescription = ((const char* (*)(id, SEL))objc_msgSend)(descriptionNSString, cDescriptionSEL);

		std::string description(cDescription);
		return description;
	}
};


id __cdecl ALTClientInfoReplacementFunction(void*)
{
	ObjcObject* NSString = (ObjcObject*)objc_getClass("NSString");
	id stringInit = sel_registerName("stringWithUTF8String:");

	ObjcObject* clientInfo = (ObjcObject*)((id(*)(id, SEL, const char*))objc_msgSend)(NSString, stringInit, "<MacBookPro16,1> <Mac OS X;13.1;22C65> <com.apple.AuthKit/1 (com.apple.dt.Xcode/3594.4.19)>");

	odslog("Swizzled Client Info: " << clientInfo->description());

	return clientInfo;
}

id __cdecl ALTDeviceIDReplacementFunction()
{
	ObjcObject* NSString = (ObjcObject*)objc_getClass("NSString");
	id stringInit = sel_registerName("stringWithUTF8String:");

	auto deviceIDString = getServerID();

	ObjcObject* deviceID = (ObjcObject*)((id(*)(id, SEL, const char*))objc_msgSend)(NSString, stringInit, deviceIDString.c_str());

	odslog("Swizzled Device ID: " << deviceID->description());

	return deviceID;
}

void convert_filetime(struct timeval* out_tv, const FILETIME* filetime)
{
	// Microseconds between 1601-01-01 00:00:00 UTC and 1970-01-01 00:00:00 UTC
	static const uint64_t EPOCH_DIFFERENCE_MICROS = 11644473600000000ull;

	// First convert 100-ns intervals to microseconds, then adjust for the
	// epoch difference
	uint64_t total_us = (((uint64_t)filetime->dwHighDateTime << 32) | (uint64_t)filetime->dwLowDateTime) / 10;
	total_us -= EPOCH_DIFFERENCE_MICROS;

	// Convert to (seconds, microseconds)
	out_tv->tv_sec = (time_t)(total_us / 1000000);
	out_tv->tv_usec = (long)(total_us % 1000000);
}

AnisetteDataManager* AnisetteDataManager::_instance = nullptr;

AnisetteDataManager* AnisetteDataManager::instance()
{
	if (_instance == 0)
	{
		info("Initializing new AnisetteDataManager");
		_instance = new AnisetteDataManager();
	}

	return _instance;
}

AnisetteDataManager::AnisetteDataManager() : loadedDependencies(false)
{
}

AnisetteDataManager::~AnisetteDataManager()
{
}

#define JUMP_INSTRUCTION_SIZE 5 // 0x86 jump instruction is 5 bytes total (opcode + 4 byte address).

bool AnisetteDataManager::LoadiCloudDependencies()
{
	wchar_t* programFilesCommonDirectory;
	SHGetKnownFolderPath(FOLDERID_ProgramFilesCommon, 0, NULL, &programFilesCommonDirectory);

	fs::path appleDirectoryPath(programFilesCommonDirectory);
	appleDirectoryPath.append("Apple");

	fs::path internetServicesDirectoryPath(appleDirectoryPath);
	internetServicesDirectoryPath.append("Internet Services");

	fs::path iCloudMainPath(internetServicesDirectoryPath);
	iCloudMainPath.append("iCloud_main.dll");

	HINSTANCE iCloudMain = LoadLibrary(iCloudMainPath.c_str());
	if (iCloudMain == NULL)
	{
		return false;
	}

	// Retrieve known exported function address to provide reference point for accessing private functions.
	uintptr_t exportedFunctionAddress = (uintptr_t)GetProcAddress(iCloudMain, "PL_FreeArenaPool");
	size_t exportedFunctionDisassembledOffset = 0x1aa2a0;


	/* Reprovision Anisette Function */

	size_t anisetteFunctionDisassembledOffset = 0x241ee0;
	size_t difference = anisetteFunctionDisassembledOffset - 0x1aa2a0;

	CopyAnisetteData = (COPYANISETTEDATAFUNC)(exportedFunctionAddress + difference);
	if (CopyAnisetteData == NULL)
	{
		return false;
	}


	/* Anisette Data Functions */

	size_t clientInfoFunctionDisassembledOffset = 0x23e730;
	size_t clientInfoFunctionRelativeOffset = clientInfoFunctionDisassembledOffset - exportedFunctionDisassembledOffset;
	GetClientInfo = (CLIENTINFOFUNC)(exportedFunctionAddress + clientInfoFunctionRelativeOffset);

	size_t deviceIDFunctionDisassembledOffset = 0x23d8b0;
	size_t deviceIDFunctionRelativeOffset = deviceIDFunctionDisassembledOffset - exportedFunctionDisassembledOffset;
	GetDeviceID = (GETOBJECTFUNC)(exportedFunctionAddress + deviceIDFunctionRelativeOffset);

	size_t localUserIDFunctionDisassembledOffset = 0x23db30;
	size_t localUserIDFunctionRelativeOffset = localUserIDFunctionDisassembledOffset - exportedFunctionDisassembledOffset;
	GetLocalUserID = (GETOBJECTFUNC)(exportedFunctionAddress + localUserIDFunctionRelativeOffset);

	if (GetClientInfo == NULL || GetDeviceID == NULL || GetLocalUserID == NULL)
	{
		return false;
	}

	{
		/** Client Info Swizzling */

		int64_t* targetFunction = (int64_t*)GetClientInfo;
		int64_t* replacementFunction = (int64_t*)&ALTClientInfoReplacementFunction;

		SYSTEM_INFO system;
		GetSystemInfo(&system);
		int pageSize = system.dwAllocationGranularity;

		uintptr_t startAddress = (uintptr_t)targetFunction;
		uintptr_t endAddress = startAddress + 1;
		uintptr_t pageStart = startAddress & -pageSize;

		// Mark page containing the target function implementation as writable so we can inject our own instruction.
		DWORD permissions = 0;
		BOOL value = VirtualProtect((LPVOID)pageStart, endAddress - pageStart, PAGE_EXECUTE_READWRITE, &permissions);

		if (!value)
		{
			return false;
		}

		int32_t jumpOffset = (int64_t)replacementFunction - ((int64_t)targetFunction + JUMP_INSTRUCTION_SIZE); // Add jumpInstructionSize because offset is relative to _next_ instruction.

		// Construct jump instruction.
		// Jump doesn't return execution to target function afterwards, allowing us to completely replace the implementation.
		char instruction[5];
		instruction[0] = '\xE9'; // E9 = "Jump near (relative)" opcode
		((int32_t*)(instruction + 1))[0] = jumpOffset; // Next 4 bytes = jump offset

		// Replace first instruction in target target function with our unconditional jump to replacement function.
		char* functionImplementation = (char*)targetFunction;
		for (int i = 0; i < JUMP_INSTRUCTION_SIZE; i++)
		{
			functionImplementation[i] = instruction[i];
		}
	}

	{
		/** Device ID Swizzling */

		int64_t* targetFunction = (int64_t*)GetDeviceID;
		int64_t* replacementFunction = (int64_t*)&ALTDeviceIDReplacementFunction;

		SYSTEM_INFO system;
		GetSystemInfo(&system);
		int pageSize = system.dwAllocationGranularity;

		uintptr_t startAddress = (uintptr_t)targetFunction;
		uintptr_t endAddress = startAddress + 1;
		uintptr_t pageStart = startAddress & -pageSize;

		// Mark page containing the target function implementation as writable so we can inject our own instruction.
		DWORD permissions = 0;
		BOOL value = VirtualProtect((LPVOID)pageStart, endAddress - pageStart, PAGE_EXECUTE_READWRITE, &permissions);

		if (!value)
		{
			return false;
		}

		int32_t jumpOffset = (int64_t)replacementFunction - ((int64_t)targetFunction + JUMP_INSTRUCTION_SIZE); // Add jumpInstructionSize because offset is relative to _next_ instruction.

		// Construct jump instruction.
		// Jump doesn't return execution to target function afterwards, allowing us to completely replace the implementation.
		char instruction[5];
		instruction[0] = '\xE9'; // E9 = "Jump near (relative)" opcode
		((int32_t*)(instruction + 1))[0] = jumpOffset; // Next 4 bytes = jump offset

		// Replace first instruction in target target function with our unconditional jump to replacement function.
		char* functionImplementation = (char*)targetFunction;
		for (int i = 0; i < JUMP_INSTRUCTION_SIZE; i++)
		{
			functionImplementation[i] = instruction[i];
		}
	}
}

bool AnisetteDataManager::LoadDependencies()
{
	info("Loading Apple dynamic libraries..");
	const std::string ap_f = appleFolderPath();

	fs::path appleFolderPath(ap_f);
	if (!fs::exists(appleFolderPath)) {
		std::cerr << "Could not find iTunes folder." << std::endl;
		exit(-1);
	}

	fs::path internetServicesDirectoryPath(internetServicesFolderPath());
	if (!fs::exists(internetServicesDirectoryPath)) {
		std::cerr << "Could not find iCloud folder." << std::endl;
		exit(-1);
	}

	fs::path aosKitPath(internetServicesDirectoryPath);
	aosKitPath.append("AOSKit.dll");

	if (!fs::exists(aosKitPath)) {
		std::cerr << "Could not find AOSKit.dll (iCloud)" << std::endl;
		exit(-1);
	}

	fs::path applicationSupportDirectoryPath(applicationSupportFolderPath());
	if (!fs::exists(applicationSupportDirectoryPath)) {
		std::cerr << "Could not find Application Support folder." << std::endl;
		exit(-1);
	}

	fs::path objcPath(applicationSupportDirectoryPath);
	objcPath.append("objc.dll");

	if (!fs::exists(objcPath)) {
		std::cerr << "Could not find objc.dll!" << std::endl;
		exit(-1);
	}

	fs::path foundationPath(applicationSupportDirectoryPath);
	foundationPath.append("Foundation.dll");

	if (!fs::exists(foundationPath)) {
		std::cerr << "Could not find Foundation.dll" << std::endl;
		exit(-1);
	}

	BOOL result = SetCurrentDirectory(applicationSupportDirectoryPath.c_str());
	DWORD dwError = GetLastError();

	HINSTANCE objcLibrary = LoadLibrary(objcPath.c_str());
	HINSTANCE foundationLibrary = LoadLibrary(foundationPath.c_str());
	HINSTANCE AOSKit = LoadLibrary(aosKitPath.c_str());

	dwError = GetLastError();

	if (objcLibrary == NULL || AOSKit == NULL || foundationLibrary == NULL)
	{
		char buffer[256];
		FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, GetLastError(),
			MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), buffer, 256, NULL);

		std::cerr << "Could not load ObjectiveC library: " << buffer << std::endl;
		exit(-1);
	}

	/* Objective-C runtime functions */

	objc_getClass = (GETCLASSFUNC)GetProcAddress(objcLibrary, "objc_getClass");
	sel_registerName = (REGISTERSELFUNC)GetProcAddress(objcLibrary, "sel_registerName");
	objc_msgSend = (SENDMSGFUNC)GetProcAddress(objcLibrary, "objc_msgSend");

	class_copyMethodList = (COPYMETHODLISTFUNC)GetProcAddress(objcLibrary, "class_copyMethodList");
	method_getName = (GETMETHODNAMEFUNC)GetProcAddress(objcLibrary, "method_getName");
	sel_getName = (GETSELNAMEFUNC)GetProcAddress(objcLibrary, "sel_getName");
	object_getClass = (GETOBJCCLASSFUNC)GetProcAddress(objcLibrary, "object_getClass");

	if (objc_getClass == NULL) {
		std::cout << "Could not find get objc getClass function. (?)" << std::endl;
		exit(-1);
	}

	this->loadedDependencies = true;

	return true;
}

std::shared_ptr<AnisetteData> AnisetteDataManager::FetchAnisetteData()
{
	info("Checking if dependencies are loaded..");
	if (!this->loadedDependencies)
	{
		info("Dependencies are not yet loaded.");
		this->LoadDependencies();
	}
	else {
		info("Dynamic libraries already loaded!");
	}

	std::shared_ptr<AnisetteData> anisetteData = NULL;

	this->ReprovisionDevice([&anisetteData]() {
		// Device is temporarily provisioned as a Mac, so access anisette data now.

		ObjcObject* NSString = (ObjcObject*)objc_getClass("NSString");
		id stringInit = sel_registerName("stringWithUTF8String:");

		/* One-Time Pasword */
		ObjcObject* dsidString = (ObjcObject*)((id(*)(id, SEL, const char*))objc_msgSend)(NSString, stringInit, "-2");
		ObjcObject* machineIDKey = (ObjcObject*)((id(*)(id, SEL, const char*))objc_msgSend)(NSString, stringInit, "X-Apple-MD-M");
		ObjcObject* otpKey = (ObjcObject*)((id(*)(id, SEL, const char*))objc_msgSend)(NSString, stringInit, "X-Apple-MD");

		ObjcObject* AOSUtilities = (ObjcObject*)objc_getClass("AOSUtilities");
		ObjcObject* headers = (ObjcObject*)((id(*)(id, SEL, id))objc_msgSend)(AOSUtilities, sel_registerName("retrieveOTPHeadersForDSID:"), dsidString);

		ObjcObject* machineID = (ObjcObject*)((id(*)(id, SEL, id))objc_msgSend)(headers, sel_registerName("objectForKey:"), machineIDKey);
		ObjcObject* otp = (ObjcObject*)((id(*)(id, SEL, id))objc_msgSend)(headers, sel_registerName("objectForKey:"), otpKey);

		if (otp == NULL || machineID == NULL)
		{
			std::cerr << "otp or machine is Null!" << std::endl;
			return;
		}

		odslog("OTP: " << otp->description() << " MachineID: " << machineID->description());

		/* Device Hardware */

		ObjcObject* deviceDescription = (ObjcObject*)ALTClientInfoReplacementFunction(NULL);
		ObjcObject* deviceID = (ObjcObject*)ALTDeviceIDReplacementFunction();

		if (deviceDescription == NULL || deviceID == NULL)
		{
			std::cerr << "device Info is Null!" << std::endl;
			return;
		}

		std::string description = deviceID->description();

		std::vector<unsigned char> deviceIDData(description.begin(), description.end());
		//auto encodedDeviceID = StringFromWideString(utility::conversions::to_base64(deviceIDData));
		auto encodedDeviceID = macaron::Base64::Encode(deviceIDData);

		ObjcObject* localUserID = (ObjcObject*)((id(*)(id, SEL, const char*))objc_msgSend)(NSString, stringInit, encodedDeviceID.c_str());

		std::string deviceSerialNumber = "C02LKHBBFD57";

		if (localUserID == NULL)
		{
			std::cerr << "localUserID is Null!" << std::endl;
			return;
		}

		anisetteData = std::make_shared<AnisetteData>();
		anisetteData->insert(std::make_pair("X-Apple-I-MD-M", machineID->description()));
		anisetteData->insert(std::make_pair("X-Apple-I-MD", otp->description()));
		anisetteData->insert(std::make_pair("X-Apple-I-MD-LU", localUserID->description()));
		anisetteData->insert(std::make_pair("X-Apple-I-MD-RINFO", "17106176"));
		anisetteData->insert(std::make_pair("X-Mme-Device-Id", deviceID->description()));
		anisetteData->insert(std::make_pair("X-Apple-I-SRL-NO", deviceSerialNumber));
		anisetteData->insert(std::make_pair("X-MMe-Client-Info", deviceDescription->description()));

		time_t ts = time(NULL);
		char dateString[100] = { 0 };
		strftime(dateString, sizeof dateString, "%Y-%m-%dT%H:%M:%SZ", gmtime(&ts));
		anisetteData->insert(std::make_pair("X-Apple-I-Client-Time", dateString));
		anisetteData->insert(std::make_pair("X-Apple-Locale", "en_US"));
		anisetteData->insert(std::make_pair("X-Apple-I-TimeZone", "PST"));
		
		});

	return anisetteData;
}

bool AnisetteDataManager::ReprovisionDevice(std::function<void(void)> provisionCallback)
{
	provisionCallback();
	return true;
}

bool AnisetteDataManager::ResetProvisioning()
{
	std::string adiDirectoryPath = "C:\\ProgramData\\Apple Computer\\iTunes\\adi";

	// Remove existing AltServer .pb files so we can create new ones next time we provision this device.
	for (const auto& entry : fs::directory_iterator(adiDirectoryPath))
	{
		if (entry.path().extension() == ".altserver")
		{
			fs::remove(entry.path());
		}
	}

	return true;
}

std::shared_ptr<AnisetteData> fetchAnisette() {
	return AnisetteDataManager::instance()->FetchAnisetteData();
}

int main()
{
	info("Starting Anisette server for win32!");
	info("Anisette code taken from https://github.com/NyaMisty/alt-anisette-server.");
	info("Webserver code by zimsneexh");
	std::cout << std::endl;
	info("Serving on any interface on Port 8080");
	if (!setup_http_server()) {
		error("Could not start. Exiting!");
		return -1;
	}
}
