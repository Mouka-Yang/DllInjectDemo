#include <stdio.h>
#include <Windows.h>
#include <TlHelp32.h>

#include "..\Common\utils.hpp"

int main()
{
	const char *dllpath = "R:\\DllInjectDemo\\Bin\\MyDll.dll";
	UINT32 procID;
	HANDLE process;
	LPVOID llAddr;
	LPVOID remotePath;
	HANDLE threadID;

	// Get injected process handle by PID
	if (!(procID = FindProcessId("windbg.exe")))
	{
		printf_s("Error: Cannot find process ID!\nExited.\n");
		exit(1);
	}
	printf_s("Get process ID : %d\n", procID);

	if (!(process = OpenProcess(PROCESS_ALL_ACCESS, FALSE, procID)))
	{
		printf_s("Error: Open injected process failed!\nExited.\n");
		exit(1);
	}
	printf_s("Get process handle : %p\n", &process);

	// Get address of the function LoadLibraryA 
	llAddr = (LPVOID)GetProcAddress(GetModuleHandle("kernel32.dll"), "LoadLibraryA");
	if (!llAddr)
	{
		printf_s("Error: Get address of the LoadLibrary failed!\n");
		exit(1);
	}
	printf_s("Get LoadLibrary entry address : %p\n", llAddr);

	//// Allocate new memory region inside the injected process's memory space
	//// remotePath is the start address of the allocated memory
	remotePath = VirtualAllocEx(process, NULL, strlen(dllpath) + 1,  MEM_COMMIT, PAGE_READWRITE);
	if (!remotePath)
	{
		printf_s("Error: Cannot allocate memory region in the injected process!\n");
		exit(1);
	}
	printf_s("Get newly allocated memory address : %p\n", remotePath);

	//// Write the remotePath of LoadLibrary to the process's newly allocated memory
	if (!WriteProcessMemory(process, remotePath, (LPVOID)dllpath, strlen(dllpath) + 1, NULL))
	{
		printf_s("Error: Cannot write the dllpath into the process's memory\n");
		exit(1);
	}

	//// Inject dll into the tremotePathet process using CreateRemoteThread
	threadID = CreateRemoteThread(process, NULL, 0, (LPTHREAD_START_ROUTINE)llAddr, remotePath, NULL, NULL);
	if (!threadID)
	{
		printf_s("Error: Cannot create remote thread!\n");
		exit(1);
	}
	else
	{
		printf_s("Success: the remote thread was successfully created!\n");
	}
	//CloseHandle(process);
	//getchar();

	WaitForSingleObject(threadID, INFINITE);

	DWORD exitCode = NULL;
	if (!GetExitCodeThread(threadID, &exitCode))
		return NULL;

	CloseHandle(threadID);

	return exitCode;
}