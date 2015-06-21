//NSudo 3.0 (Build 825)
//(C) 2015 NSudo Team. All rights reserved.

#include "stdafx.h"

extern "C"
{
	wchar_t *PrivilegeList[] = {
		SE_CREATE_TOKEN_NAME,
		SE_ASSIGNPRIMARYTOKEN_NAME,
		SE_LOCK_MEMORY_NAME,
		SE_INCREASE_QUOTA_NAME,
		SE_UNSOLICITED_INPUT_NAME,
		SE_MACHINE_ACCOUNT_NAME,
		SE_TCB_NAME,
		SE_SECURITY_NAME,
		SE_TAKE_OWNERSHIP_NAME,
		SE_LOAD_DRIVER_NAME,
		SE_SYSTEM_PROFILE_NAME,
		SE_SYSTEMTIME_NAME,
		SE_PROF_SINGLE_PROCESS_NAME,
		SE_INC_BASE_PRIORITY_NAME,
		SE_CREATE_PAGEFILE_NAME,
		SE_CREATE_PERMANENT_NAME,
		SE_BACKUP_NAME,
		SE_RESTORE_NAME,
		SE_SHUTDOWN_NAME,
		SE_DEBUG_NAME,
		SE_AUDIT_NAME,
		SE_SYSTEM_ENVIRONMENT_NAME,
		SE_CHANGE_NOTIFY_NAME,
		SE_REMOTE_SHUTDOWN_NAME,
		SE_UNDOCK_NAME,
		SE_SYNC_AGENT_NAME,
		SE_ENABLE_DELEGATION_NAME,
		SE_MANAGE_VOLUME_NAME,
		SE_IMPERSONATE_NAME,
		SE_CREATE_GLOBAL_NAME,
		SE_TRUSTED_CREDMAN_ACCESS_NAME,
		SE_RELABEL_NAME,
		SE_INC_WORKING_SET_NAME,
		SE_TIME_ZONE_NAME,
		SE_CREATE_SYMBOLIC_LINK_NAME };

	//设置令牌权限
	bool SetTokenPrivilege(HANDLE TokenHandle, LPCWSTR lpName, bool bEnable)
	{
		TOKEN_PRIVILEGES TP;

		TP.PrivilegeCount = 1;
		TP.Privileges[0].Attributes = bEnable ? SE_PRIVILEGE_ENABLED : NULL;

		if (LookupPrivilegeValueW(NULL, lpName, &TP.Privileges[0].Luid))
		{
			if (AdjustTokenPrivileges(TokenHandle, FALSE, &TP, sizeof(TP), NULL, NULL)) return true;
		}

		return false;
	}

	//设置当前进程令牌权限
	bool SetCurrentProcessPrivilege(LPCWSTR lpName, bool bEnable)
	{
		bool bRet = false;
		HANDLE hCurrentProcessToken;
		if (OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hCurrentProcessToken))
		{
			if (SetTokenPrivilege(hCurrentProcessToken, lpName, bEnable)) bRet = true;
			CloseHandle(hCurrentProcessToken);
		}
		return bRet;
	}

	//一键开启全部权限令牌
	void EnableAllTokenPrivileges(HANDLE TokenHandle)
	{
		for (int i = 0; i < sizeof(PrivilegeList) / sizeof(wchar_t*); i++)
		{
			SetTokenPrivilege(TokenHandle, PrivilegeList[i], true);
		}
	}

	//一键禁用全部权限令牌
	void DisableAllTokenPrivileges(HANDLE TokenHandle)
	{
		for (int i = 0; i < sizeof(PrivilegeList) / sizeof(wchar_t*); i++)
		{
			SetTokenPrivilege(TokenHandle, PrivilegeList[i], false);
		}
	}

	//获取当前会话ID下winlogon的PID
	DWORD GetWinLogonProcessID()
	{
		DWORD dwWinLogonPID = -1; //winlogon.exe的ProcessID

		//获取当前会话ID
		DWORD dwUserSessionId = WTSGetActiveConsoleSessionId();
		if (dwUserSessionId != 0xFFFFFFFF)
		{
			//获取当前会话ID下的winlogon的PID
			PROCESSENTRY32W ProcessEntry;
			ProcessEntry.dwSize = sizeof(PROCESSENTRY32W);
			HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0); //调用CreateToolhelp32Snapshot快照所有进程
			if (hSnapshot != INVALID_HANDLE_VALUE)
			{
				if (Process32FirstW(hSnapshot, &ProcessEntry)) //便利进程
				{
					do
					{
						if (_wcsicmp(L"winlogon.exe", ProcessEntry.szExeFile) == 0) //寻找winlogon进程
						{
							DWORD dwSessionID;
							if (ProcessIdToSessionId(ProcessEntry.th32ProcessID, &dwSessionID)) //获取winlogon的会话ID
							{
								if (dwSessionID != dwUserSessionId) continue; //判断是否是当前用户ID
								dwWinLogonPID = ProcessEntry.th32ProcessID;
								break;
							}
						}
					} while (Process32NextW(hSnapshot, &ProcessEntry));
				}
				CloseHandle(hSnapshot);
			}
		}
		return dwWinLogonPID;
	}

	//获取TrustedInstaller的PID
	DWORD GetTrustedInstallerProcessID()
	{
		DWORD dwTIPID = -1; //TrustedInstaller.exe的ProcessID

		SC_HANDLE hSC = OpenSCManagerW(NULL, NULL, GENERIC_EXECUTE);
		if (hSC != NULL)
		{
			SC_HANDLE hSvc = OpenServiceW(hSC, L"TrustedInstaller", SERVICE_START | SERVICE_QUERY_STATUS | SERVICE_STOP);
			if (hSvc != NULL)
			{
				SERVICE_STATUS status;
				if (QueryServiceStatus(hSvc, &status))
				{
					if (status.dwCurrentState == SERVICE_STOPPED)
					{
						// 启动服务
						if (StartServiceW(hSvc, NULL, NULL))
						{
							// 等待服务启动
							while (::QueryServiceStatus(hSvc, &status) == TRUE)
							{
								Sleep(status.dwWaitHint);
								if (status.dwCurrentState == SERVICE_RUNNING) break;
							}
						}
					}
				}
				CloseServiceHandle(hSvc);
			}
			CloseServiceHandle(hSC);
		}

		PROCESSENTRY32W ProcessEntry;
		ProcessEntry.dwSize = sizeof(PROCESSENTRY32W);
		HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0); //调用CreateToolhelp32Snapshot快照所有进程
		if (hSnapshot != INVALID_HANDLE_VALUE)
		{
			if (Process32FirstW(hSnapshot, &ProcessEntry)) //便利进程
			{
				do
				{
					if (_wcsicmp(L"TrustedInstaller.exe", ProcessEntry.szExeFile) == 0) //寻找TrustedInstaller进程
					{
						dwTIPID = ProcessEntry.th32ProcessID;
						break;
					}
				} while (Process32NextW(hSnapshot, &ProcessEntry));
			}
			CloseHandle(hSnapshot);
		}
		return dwTIPID;
	}

	//获取System权限令牌(需要Debug特权)
	bool GetSystemToken(PHANDLE hNewToken)
	{
		bool bRet = false;

		//获取当前会话ID下的winlogon的PID
		DWORD dwWinLogonPID = GetWinLogonProcessID();
		if (dwWinLogonPID != -1)
		{
			HANDLE hProc = OpenProcess(PROCESS_ALL_ACCESS, FALSE, dwWinLogonPID);
			if (hProc != NULL)
			{
				HANDLE hToken;
				if (OpenProcessToken(hProc, TOKEN_DUPLICATE, &hToken))
				{
					if (DuplicateTokenEx(hToken, TOKEN_ALL_ACCESS, NULL, SecurityIdentification, TokenPrimary, hNewToken))
					{
						bRet = true;
					}
					else wprintf(L"DuplicateTokenEx Failed");
					CloseHandle(hToken);
				}
				else wprintf(L"OpenProcessToken Failed");
				CloseHandle(hProc);
			}
			else wprintf(L"OpenProcess Failed");
		}
		else wprintf(L"GetWinLogonProcessID Failed");
		return bRet;
	}

	//判断是否在WoW64下运行
	BOOL IsWoW64()
	{
		BOOL bIsWoW64 = false;

		typedef BOOL(WINAPI *LPFN_ISWOW64PROCESS) (HANDLE, PBOOL);

		LPFN_ISWOW64PROCESS fnIsWow64Process = (LPFN_ISWOW64PROCESS)GetProcAddress(
			GetModuleHandleW(L"kernel32.dll"), "IsWow64Process");

		if (NULL != fnIsWow64Process) fnIsWow64Process(GetCurrentProcess(), &bIsWoW64);

		return bIsWoW64;
	}

	//获取当前用户令牌(需要Debug特权)
	bool GetCurrentUserToken(PHANDLE hNewToken)
	{
		bool bRet = false;

		HANDLE hSystemToken;
		if (GetSystemToken(&hSystemToken))
		{
			if (ImpersonateLoggedOnUser(hSystemToken))
			{
				if (SetCurrentProcessPrivilege(SE_TCB_NAME, true))
				{
					DWORD dwUserSessionId = WTSGetActiveConsoleSessionId();
					if (0xFFFFFFFF != dwUserSessionId)
					{
						if (WTSQueryUserToken(dwUserSessionId, hNewToken)) bRet = true;
					}
				}
				RevertToSelf();
			}
			CloseHandle(hSystemToken);
		}

		return bRet;
	}

	//获取TrustedInstaller权限令牌(需要Debug特权)
	bool GetTIToken(PHANDLE hNewToken)
	{
		bool bRet = false;

		HANDLE hSystemToken;
		if (GetSystemToken(&hSystemToken))
		{
			if (ImpersonateLoggedOnUser(hSystemToken))
			{
				//获取当前会话ID下的TrustedInstaller的PID
				DWORD dwTIPID = GetTrustedInstallerProcessID();

				if (dwTIPID != -1)
				{
					HANDLE hProc = OpenProcess(PROCESS_ALL_ACCESS, FALSE, dwTIPID);
					if (hProc != NULL)
					{
						HANDLE hToken;
						if (OpenProcessToken(hProc, TOKEN_DUPLICATE | TOKEN_QUERY, &hToken))
						{
							if (DuplicateTokenEx(hToken, TOKEN_ALL_ACCESS, NULL, SecurityIdentification, TokenPrimary, hNewToken))
							{
								bRet = true;
							}
							else wprintf(L"DuplicateTokenEx Failed");
							CloseHandle(hToken);
						}
						else wprintf(L"OpenProcessToken Failed");
						CloseHandle(hProc);
					}
					else wprintf(L"OpenProcess Failed");
				}
				else wprintf(L"GetTrustedInstallerProcessID Failed");

				RevertToSelf();
			}
			CloseHandle(hSystemToken);
		}
		return bRet;
	}

	//设置令牌完整性
	bool SetTokenIntegrity(HANDLE hToken, LPCWSTR szIntegritySID)
	{
		bool bRet = false;
		PSID pIntegritySid = NULL; // PSID 结构

		if (ConvertStringSidToSidW(szIntegritySID, &pIntegritySid)) //获取低完整性 SID 的 PSID 结构
		{
			TOKEN_MANDATORY_LABEL TokenMandatoryLabel = { 0 };  //令牌完整性结构

			TokenMandatoryLabel.Label.Attributes = SE_GROUP_INTEGRITY;
			TokenMandatoryLabel.Label.Sid = pIntegritySid;

			DWORD TokenInformationLength = sizeof(TOKEN_MANDATORY_LABEL) + GetLengthSid(pIntegritySid);

			//设置进程完整性级别
			if (SetTokenInformation(hToken, TokenIntegrityLevel, &TokenMandatoryLabel, TokenInformationLength))
			{
				bRet = true; // 函数执行成功
			}
			LocalFree(pIntegritySid); // 释放 pIntegritySid 结构
		}

		return bRet;
	}

	//创建和指定进程一样的环境块
	bool CreateEnvironmentBlockFromProcess(DWORD dwPID, LPVOID* lpEnv)
	{
		bool bRet = false;

		HANDLE hSystemToken;
		if (GetSystemToken(&hSystemToken))
		{
			
				HANDLE hProc = OpenProcess(MAXIMUM_ALLOWED, FALSE, dwPID);
				if (hProc != NULL)
				{
					HANDLE hToken;
					if (OpenProcessToken(hProc, TOKEN_DUPLICATE | TOKEN_QUERY, &hToken))
					{
						if (CreateEnvironmentBlock(lpEnv, hToken, 1))
						{
							bRet = true;
						}
						else wprintf(L"CreateEnvironmentBlock Failed");
						CloseHandle(hToken);
					}
					else wprintf(L"OpenProcessToken Failed");
					CloseHandle(hProc);
				}
				else wprintf(L"OpenProcess Failed");
			CloseHandle(hSystemToken);
		}

		return bRet;
	}
}