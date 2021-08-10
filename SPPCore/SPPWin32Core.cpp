// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.


#include "SPPWin32Core.h"
#include "SPPString.h"
#include "SPPLogging.h"
#include "SPPFileSystem.h"


#include <filesystem>
#include <list>
#include <stdio.h>
#include <stdarg.h>
#include <memory>
#include <ostream>
#include <optional>
#include <fstream>
#include <sstream>
#include <map>

#include "Windows.h"
#include "sysinfoapi.h"

namespace SPP
{
	SPP_CORE_API LogEntry LOG_WIN32CORE("WIN32CORE");

	std::string GetProcessName()
	{
		char Filename[MAX_PATH]; //this is a char buffer
		GetModuleFileNameA(GetModuleHandle(nullptr), Filename, sizeof(Filename));
		return Filename;
	}

	PlatformInfo GetPlatformInfo()	
	{
		SYSTEM_INFO info = { 0 };
		GetSystemInfo( &info );

		PlatformInfo oInfo = { info.dwPageSize, info.dwNumberOfProcessors };
		return oInfo;
	}

	std::map< uint32_t, std::shared_ptr< PROCESS_INFORMATION > > hostedChildProcesses;

	uint32_t CreateChildProcess(const char* ProcessPath, const char* Commandline, bool bStartVisible)
	{
		SPP_LOG(LOG_WIN32CORE, LOG_INFO, "CreateChildProcess: %s %s", ProcessPath, Commandline);

		BOOL bIsProcessInJob = false;
		BOOL bSuccess = IsProcessInJob(GetCurrentProcess(), NULL, &bIsProcessInJob);
		if (bSuccess == 0) 
		{
			SPP_LOG(LOG_WIN32CORE, LOG_INFO, "CreateChildProcess: IsProcessInJob failed: error %d", GetLastError());
			return 0;
		}
		
		bool bCreateJob = true;

		HANDLE hJob = nullptr;
		if (bIsProcessInJob) 
		{
			bCreateJob = false;
			SPP_LOG(LOG_WIN32CORE, LOG_INFO, "CreateChildProcess: already in job");

			JOBOBJECT_EXTENDED_LIMIT_INFORMATION jeli = { 0 };
			QueryInformationJobObject(NULL, JobObjectExtendedLimitInformation, &jeli, sizeof(jeli), NULL);

			SPP_LOG(LOG_WIN32CORE, LOG_INFO, " -  silent break away %d", (jeli.BasicLimitInformation.LimitFlags & JOB_OBJECT_LIMIT_SILENT_BREAKAWAY_OK));
			SPP_LOG(LOG_WIN32CORE, LOG_INFO, " -  kill on close %d", (jeli.BasicLimitInformation.LimitFlags & JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE));
			SPP_LOG(LOG_WIN32CORE, LOG_INFO, " -  break away %d", (jeli.BasicLimitInformation.LimitFlags & JOB_OBJECT_LIMIT_BREAKAWAY_OK));

			bCreateJob = (jeli.BasicLimitInformation.LimitFlags & (JOB_OBJECT_LIMIT_BREAKAWAY_OK | JOB_OBJECT_LIMIT_SILENT_BREAKAWAY_OK)) != 0;

			SPP_LOG(LOG_WIN32CORE, LOG_INFO, " - bCreateJob %d", bCreateJob);
		}
		
		if(bCreateJob)
		{
			hJob = CreateJobObject(NULL, NULL);
			if (hJob == NULL) 
			{
				SPP_LOG(LOG_WIN32CORE, LOG_INFO, "CreateJobObject failed : error % d", GetLastError());
				return 0;
			}

			JOBOBJECT_EXTENDED_LIMIT_INFORMATION jeli = { 0 };
			jeli.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
			bSuccess = SetInformationJobObject(hJob, JobObjectExtendedLimitInformation, &jeli, sizeof(jeli));
			if (bSuccess == 0) {
				printf("SetInformationJobObject failed: error %d\n", GetLastError());
				return 0;
			}
		}
	
		auto pi = std::make_shared< PROCESS_INFORMATION >(PROCESS_INFORMATION{ 0 });

		std::string OutputCommandline = std::filesystem::path(ProcessPath).filename().generic_string();
		OutputCommandline += " ";
		OutputCommandline += Commandline;

		STARTUPINFOA si = { 0 };
		si.cb = sizeof(si);
		if (bStartVisible == false)
		{
			si.dwFlags = STARTF_USESHOWWINDOW;
			si.wShowWindow = SW_HIDE;
		}
		DWORD dwCreationFlags = 0;
		dwCreationFlags |= CREATE_NEW_CONSOLE;
		bSuccess = CreateProcessA(ProcessPath, (LPSTR)OutputCommandline.c_str(),
			NULL, NULL, FALSE,
			dwCreationFlags, NULL, NULL, &si, pi.get());
		if (bSuccess == 0) 
		{
			auto LastError = GetLastError();
			SPP_LOG(LOG_WIN32CORE, LOG_INFO, "CreateProcess failed : error(%d)0x%X", LastError, LastError);
			return 0;
		}

		// could be null if parent was alreayd in a job so child will auto inherit it
		if (hJob != nullptr)
		{
			bSuccess = AssignProcessToJobObject(hJob, pi->hProcess);
			if (bSuccess == 0) 
			{
				SPP_LOG(LOG_WIN32CORE, LOG_INFO, "AssignProcessToJobObject failed: error %d", GetLastError());
				return 0;
			}
		}

		hostedChildProcesses[pi->dwProcessId] = pi;

		return pi->dwProcessId;
	}


	bool IsChildRunning(uint32_t processID)
	{
		auto foundChild = hostedChildProcesses.find(processID);

		if (foundChild != hostedChildProcesses.end())
		{
			DWORD exit_code;
			GetExitCodeProcess(foundChild->second->hProcess, &exit_code);
			if (exit_code == STILL_ACTIVE) {
				return true;
			}
		}

		return false;
	}

	void CloseChild(uint32_t processID)
	{
		auto foundChild = hostedChildProcesses.find(processID);

		if (foundChild != hostedChildProcesses.end())
		{
			//PostThreadMessage(foundChild->second->dwThreadId, WM_CLOSE, 0, 0);			
			TerminateProcess(foundChild->second->hProcess, 0);
		}
	}

	void AddDLLSearchPath(const char* InPath)
	{
		SetDllDirectoryA(InPath);
	}
}

uint32_t C_CreateChildProcess(const char* ProcessPath, const char* Commandline, bool bStartVisible)
{
	return SPP::CreateChildProcess(ProcessPath, Commandline, bStartVisible);
}

bool C_IsChildRunning(uint32_t processID)
{
	return SPP::IsChildRunning(processID);
}

void C_CloseChild(uint32_t processID)
{
	return SPP::CloseChild(processID);
}