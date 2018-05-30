#include <stdio.h>
#include <windows.h>
#include <TlHelp32.h>
#include <strsafe.h>

#include "log.h"

DWORD FindProcessId(const char *processname);
HANDLE GetMainThread(DWORD dwOwnerPID);
VOID ErrorExit(LPCSTR lpszFunction);
/*
* bytecode of the MessageBox function
* ******
	push 0x30
	push 0
	push 0xcccc
	push 0
	mov eax, 0xffffffff
	call eax
	ret
*/
byte wrapper[] = { 0x6a,0x30,0x6a,0x00,0x68,0xcc,0xcc,0xcc,0xcc,0x6a,0x00,0xb8,0xff,0xff,0xff,0xff,0xff,0xd0,0xc3 };


int main()
{
	UINT32 procID;
	HANDLE process;
	LPVOID remoteWrapper;
	LPVOID remoteText;
	HANDLE hThread;
	const char *text = "Injected Hello World";


	// Get injected process handle by PID
	if (!(procID = FindProcessId("MyProgram.exe")))
	{
		printf_s("Error: Cannot find process ID!\nExited.\n");

	}
	//procID = 15756;
	log_debug("Get process ID : %d", procID);

	if (!(process = OpenProcess(PROCESS_ALL_ACCESS, FALSE, procID)))
	{
		ErrorExit(TEXT("Open injected process failed!\nExited."));

	}
	log_debug("Get process handle : %p", &process);

	if (!(remoteWrapper = VirtualAllocEx(process, NULL,
		sizeof(wrapper), MEM_COMMIT, PAGE_EXECUTE_READWRITE)))
	{
		ErrorExit(TEXT("Cannot allocate memory for remote wrapper!"));

	}
	log_debug("Get allocated remote wrapper address : %p", remoteWrapper);


	if (!(remoteText = VirtualAllocEx(process, NULL,
		sizeof(text) + 1, MEM_COMMIT, PAGE_READWRITE)))
	{
		ErrorExit(TEXT("Cannot allocate memory for output text!"));

	}
	log_debug("Get allocated remote text address: %p", remoteText);

	if (!WriteProcessMemory(process, remoteText, (LPVOID)text, strlen(text) + 1, NULL))
	{
		ErrorExit(TEXT("Cannot write  text to process memory!"));

	}
	log_debug("Write text content to process memory.");

	MEMORY_BASIC_INFORMATION bi;
	VirtualQuery(wrapper, &bi, sizeof(bi));
	if (!VirtualProtect(wrapper, sizeof(wrapper), PAGE_READWRITE, &(bi.Protect)))
	{
		ErrorExit(TEXT("VirtualProtect failed!"));
		GetLastError();

	}

	*(DWORD*)(wrapper + 5) = (DWORD)remoteText;
	if (!(*(DWORD*)(wrapper + 12) =
		(DWORD)GetProcAddress(LoadLibrary("USER32.DLL"), "MessageBoxA")))
	{
		ErrorExit(TEXT("Cannot find the address of function MessageBoxA! "));

	}

	if (!WriteProcessMemory(process, remoteWrapper,
		(LPVOID)wrapper, sizeof(wrapper), NULL))
	{
		ErrorExit(TEXT("Cannot write wrapper to process memory!"));

	}

	CONTEXT context;
	hThread = GetMainThread(procID);
	if (SuspendThread(hThread) == -1)
	{
		ErrorExit(TEXT("SuspendThread"));
	}
	memset(&context, NULL, sizeof(context));
	context.ContextFlags = CONTEXT_ALL;

	if (!GetThreadContext(hThread, &context))
	{
		ErrorExit(TEXT("Get thread context failed!"));
	}

	context.Eip = (DWORD)wrapper;
	if (!SetThreadContext(hThread, &context))
	{
		ErrorExit(TEXT("Set thread context failed"));

	}
	ResumeThread(hThread);

	WaitForSingleObject(hThread, INFINITE);

	DWORD exitCode = NULL;
	if (!GetExitCodeThread(hThread, &exitCode))
		return NULL;

	CloseHandle(hThread);


}

DWORD FindProcessId(const char *processname)
{
	HANDLE hProcessSnap;
	PROCESSENTRY32 pe32;
	DWORD result = NULL;

	// Take a snapshot of all processes in the system.
	hProcessSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	if (INVALID_HANDLE_VALUE == hProcessSnap) return(FALSE);

	pe32.dwSize = sizeof(PROCESSENTRY32); // <----- IMPORTANT

										  // Retrieve information about the first process,
										  // and exit if unsuccessful
	if (!Process32First(hProcessSnap, &pe32))
	{
		CloseHandle(hProcessSnap);          // clean the snapshot object
		ErrorExit(TEXT(" Failed to gather information on system processes! "));
	}

	do
	{
		if (0 == strcmp(processname, pe32.szExeFile))
		{
			result = pe32.th32ProcessID;
			break;
		}
	} while (Process32Next(hProcessSnap, &pe32));

	CloseHandle(hProcessSnap);

	return result;
}

HANDLE GetMainThread(DWORD dwOwnerPID)
{
	HANDLE  hThreadSnap = INVALID_HANDLE_VALUE;
	HANDLE hThread = INVALID_HANDLE_VALUE;
	THREADENTRY32 te32;

	// Take a snapshot of all running threads
	if ((hThreadSnap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0)) ==
		INVALID_HANDLE_VALUE)
	{
		ErrorExit(TEXT("Cannot create thread snapshot!"));

	}

	te32.dwSize = sizeof(THREADENTRY32);

	// Retrieve infomation about the frist thread
	if (!Thread32First(hThreadSnap, &te32))
	{
		ErrorExit(TEXT("Cannot get the handle of the first thread!"));

	}

	while (te32.th32OwnerProcessID != dwOwnerPID)
	{
		Thread32Next(hThreadSnap, &te32);
	}

	// Return the main thread's handle
	if (!(hThread = OpenThread(THREAD_ALL_ACCESS, FALSE, te32.th32ThreadID)))
	{
		ErrorExit(TEXT("Open main thread of the remote process failed!"));

	}

	return hThread;

}

VOID ErrorExit(LPCSTR lpszFunction)
{
	// Retrieve the system error message for the last-error code

	LPVOID lpMsgBuf;
	LPVOID lpDisplayBuf;
	DWORD dw = GetLastError();

	FormatMessage(
		FORMAT_MESSAGE_ALLOCATE_BUFFER |
		FORMAT_MESSAGE_FROM_SYSTEM |
		FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL,
		dw,
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(LPTSTR)&lpMsgBuf,
		0, NULL);

	// Display the error message and exit the process

	lpDisplayBuf = (LPVOID)LocalAlloc(LMEM_ZEROINIT,
		(lstrlen((LPCTSTR)lpMsgBuf) + lstrlen((LPCTSTR)lpszFunction) + 40) * sizeof(TCHAR));
	StringCchPrintf((LPTSTR)lpDisplayBuf,
		LocalSize(lpDisplayBuf) / sizeof(TCHAR),
		TEXT("%s ---- error code %d: %s"),
		lpszFunction, dw, lpMsgBuf);
	//MessageBox(NULL, (LPCTSTR)lpDisplayBuf, TEXT("Error"), MB_OK);
	log_error((LPCSTR)lpDisplayBuf);

	LocalFree(lpMsgBuf);
	LocalFree(lpDisplayBuf);
	ExitProcess(dw);
}