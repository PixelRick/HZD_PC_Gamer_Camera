#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
//
#include <Psapi.h>
#include <TlHelp32.h>
//
#include <stdio.h>
#include <stdlib.h>
#include <tchar.h>

typedef unsigned char U8;

enum Game {
	LEGACY,
	REMASTERED,
	NUM_GAMES = 2
};

static const TCHAR* const exeFileNames[NUM_GAMES] = { _T("HorizonZeroDawn.exe"), _T("HorizonZeroDawnRemastered.exe") };

static BOOL PatchLegacy(HANDLE processHandle, ULONG_PTR textAddress, const U8* text, SIZE_T textSize);
static BOOL PatchRemastered(HANDLE processHandle, ULONG_PTR textAddress, const U8* text, SIZE_T textSize);

static DWORD FindProcessId(const TCHAR* exeFileName);
static ULONG_PTR FindModule(HANDLE processHandle, const TCHAR* targetName);
static ULONG_PTR FindModuleText(HANDLE processHandle, ULONG_PTR moduleBase, DWORD* outTextSize);
static ULONG_PTR FindCode(ULONG_PTR textAddress, const U8* text, SIZE_T textSize, const U8* code, SIZE_T codeSize);
static BOOL ReadProcessMemory2(HANDLE processHandle, ULONG_PTR address, U8* buffer, SIZE_T size);
static BOOL PatchProcessMemory(HANDLE processHandle, ULONG_PTR address, const U8* buffer, SIZE_T size);

int main() {
	DWORD processId;
	HANDLE processHandle;
	ULONG_PTR exeBase;
	ULONG_PTR textAddress;
	DWORD textSize;
	int gameIndex;
	int status = 0;

	for (gameIndex = 0; gameIndex < NUM_GAMES; ++gameIndex) {
		processId = FindProcessId(exeFileNames[gameIndex]);
		if (processId != 0) {
			break;
		}
	}
	if (processId == 0) {
		fprintf(stderr, "Error: couldn't find game process\n");
		return -1;
	}

	processHandle = OpenProcess(PROCESS_ALL_ACCESS, FALSE, processId);
	if (processHandle == INVALID_HANDLE_VALUE) {
		fprintf(stderr, "Error: couldn't open game process\n");
		return -2;
	}
	printf("Game process opened\n");

	exeBase = FindModule(processHandle, exeFileNames[gameIndex]);
	if (exeBase == 0) {
		fprintf(stderr, "Error: couldn't find main module in game process\n");
		CloseHandle(processHandle);
		return -3;
	}
	printf("Main module found at %p\n", (PVOID)exeBase);

	// get .text section

	textAddress = FindModuleText(processHandle, exeBase, &textSize);
	if (textAddress == 0) {
		fprintf(stderr, "Error: couldn't locate text section\n");
		CloseHandle(processHandle);
		return -4;
	}
	printf("Section .text located [%p-%p]\n", (PVOID)textAddress, (PVOID)(textAddress + textSize));

	U8* textBuffer = malloc(textSize);
	if (!ReadProcessMemory2(processHandle, textAddress, textBuffer, textSize)) {
		fprintf(stderr, "Error: couldn't read text section\n");
		CloseHandle(processHandle);
		free(textBuffer);
		return -5;
	}

	switch (gameIndex) {
	case LEGACY:
	{
		if (!PatchLegacy(processHandle, textAddress, textBuffer, textSize)) {
			fprintf(stderr, "Error: couldn't patch game (legacy)\n");
			status = -6;
		}
		break;
	}
	case REMASTERED:
	{
		if (!PatchRemastered(processHandle, textAddress, textBuffer, textSize)) {
			fprintf(stderr, "Error: couldn't patch game (remastered)\n");
			status = -6;
		}
		break;
	}
	default:
	{
		fprintf(stderr, "Error: unknown gameIndex\n");
		status = -7;
	}
	}

	if (status == 0) {
		printf("Successfully patched game's runtime image\n");
	}

	CloseHandle(processHandle);
	free(textBuffer);
	system("pause");
	return status;
}

// patchers

static BOOL PatchLegacy(HANDLE processHandle, ULONG_PTR textAddress, const U8* text, SIZE_T textSize) {
	/*
	  let's disable func.
	  it sets up camera panning offset depending on character speed
	  returning directly just leaves the by-ref argument at 0 (center)
	*/
	const U8 code[] =
		"\x48\x8B\xC4\x48\x89\x58\x08\x48\x89\x68\x10\x48\x89\x70\x18\x57"
		"\x48\x81\xEC\xC0\x00\x00\x00\x48\x8B\x79\x48";
	const U8 patch[] = "\xC3";
	ULONG_PTR targetAddress;

	targetAddress = FindCode(textAddress, text, textSize, code, sizeof(code) - 1);
	if (targetAddress == 0) {
		fprintf(stderr, "Error: couldn't find the target function to patch\n");
		return FALSE;
	}

	return PatchProcessMemory(processHandle, targetAddress, patch, sizeof(patch) - 1);
}

static BOOL PatchRemastered(HANDLE processHandle, ULONG_PTR textAddress, const U8* text, SIZE_T textSize) {
	const U8 code[] =
		"\x48\x8B\xC4\x48\x89\x58\x08\x48\x89\x68\x10\x48\x89\x70\x18\x48"
		"\x89\x78\x20\x41\x56\x48\x81\xEC\xB0\x00\x00\x00\x48\x8B\x71\x48";
	const U8 patch[] = "\xC3";
	ULONG_PTR targetAddress;

	targetAddress = FindCode(textAddress, text, textSize, code, sizeof(code) - 1);
	if (targetAddress == 0) {
		fprintf(stderr, "Error: couldn't find the target function to patch\n");
		return FALSE;
	}

	return PatchProcessMemory(processHandle, targetAddress, patch, sizeof(patch) - 1);
}

// helpers

static DWORD FindProcessId(const TCHAR* name) {
	DWORD dwProcessId = 0;
	HANDLE hModuleSnap;
	PROCESSENTRY32 pe;

	hModuleSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	if (hModuleSnap == INVALID_HANDLE_VALUE) {
		fprintf(stderr, "Error: couldn't create process snapshot\n");
		return 0;
	}

	pe.dwSize = sizeof(PROCESSENTRY32);
	if (Process32First(hModuleSnap, &pe)) {
		do {
			if (_wcsicmp(pe.szExeFile, name) == 0) {
				dwProcessId = pe.th32ProcessID;
				break;
			}
		} while (Process32Next(hModuleSnap, &pe));
	}

	CloseHandle(hModuleSnap);
	return dwProcessId;
}

static ULONG_PTR FindModule(HANDLE processHandle, const TCHAR* targetName) {
	const SSIZE_T targetNameLen = _tcslen(targetName);
	HMODULE modules[1024];
	DWORD numBytesNeeded;
	unsigned int i;

	if (!EnumProcessModules(processHandle, modules, sizeof(modules), &numBytesNeeded)) {
		fprintf(stderr, "Error: couldn't enumerate process modules\n");
		return 0;
	}

	for (i = 0; i < (numBytesNeeded / sizeof(HMODULE)); ++i) {
		TCHAR modName[MAX_PATH];
		if (GetModuleFileNameEx(processHandle, modules[i], modName, _countof(modName))) {
			SSIZE_T off = (SSIZE_T)_tcslen(modName) - targetNameLen;
			if (off >= 0 && _tcsicmp(&modName[off], targetName) == 0) {
				return (ULONG_PTR)modules[i];
			}
		}
	}

	return 0;
}

static ULONG_PTR FindModuleText(HANDLE processHandle, ULONG_PTR moduleBase, DWORD* outTextSize) {
	U8 header[0x1000];
	PIMAGE_DOS_HEADER pDosHdr;
	PIMAGE_NT_HEADERS pNtHdr;
	const IMAGE_SECTION_HEADER* pSectionHdr;
	unsigned int i;

	if (!ReadProcessMemory2(processHandle, moduleBase, header, sizeof(header))) {
		fprintf(stderr, "Error: couldn't read image PE\n");
		return 0;
	}

	pNtHdr = NULL;
	pDosHdr = (PIMAGE_DOS_HEADER)header;
	if (pDosHdr->e_magic == IMAGE_DOS_SIGNATURE &&
		pDosHdr->e_lfanew >= sizeof(PIMAGE_NT_HEADERS)) {
		pNtHdr = (PIMAGE_NT_HEADERS)((LPCSTR)pDosHdr + pDosHdr->e_lfanew);
		if (pNtHdr->Signature != IMAGE_NT_SIGNATURE) {
			pNtHdr = NULL;
		}
	}
	if (!pNtHdr) {
		fprintf(stderr, "Error: unexpected PE header\n");
		return 0;
	}

	pSectionHdr = IMAGE_FIRST_SECTION(pNtHdr);
	for (i = 0; i < pNtHdr->FileHeader.NumberOfSections; ++i, ++pSectionHdr) {
		if (0 == strcmp(".text", (LPCSTR)pSectionHdr->Name)) {
			*outTextSize = pSectionHdr->Misc.VirtualSize;
			return moduleBase + pSectionHdr->VirtualAddress;
		}
	}

	return 0;
}

static ULONG_PTR FindCode(ULONG_PTR textAddress, const U8* text, SIZE_T textSize, const U8* code, SIZE_T codeSize) {
	SIZE_T i;
	for (i = 0; i < textSize - codeSize; ++i) {
		if (memcmp(text + i, code, codeSize) == 0) {
			return textAddress + i;
		}
	}
	return 0;
}

static BOOL ReadProcessMemory2(HANDLE processHandle, ULONG_PTR address, U8* buffer, SIZE_T size) {
	SIZE_T numBytesRead = 0;

	if (!ReadProcessMemory(processHandle, (PVOID)address, buffer, size, &numBytesRead)) {
		return FALSE;
	}

	return numBytesRead == size;
}

static BOOL PatchProcessMemory(HANDLE processHandle, ULONG_PTR address, const U8* buffer, SIZE_T size) {
	SIZE_T numBytesWritten;
	DWORD oldProtect;
	BOOL ok;

	if (!VirtualProtectEx(processHandle, (PVOID)address, size, PAGE_EXECUTE_READWRITE, &oldProtect)) {
		fprintf(stderr, "Error: couldn't change memory protection to PAGE_EXECUTE_READWRITE\n");
		return FALSE;
	}

	ok = WriteProcessMemory(processHandle, (PVOID)address, buffer, size, &numBytesWritten);

	if (!VirtualProtectEx(processHandle, (PVOID)address, size, oldProtect, &oldProtect)) {
		fprintf(stderr, "Warning: couldn't restore memory protection after patching\n");
	}

	return ok && numBytesWritten == size;
}


