#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <tlhelp32.h>
#include <psapi.h>
#include <stdio.h>

int main()
{
  DWORD pid = 0;
  {
    PROCESSENTRY32 pe;
    pe.dwSize = sizeof(PROCESSENTRY32);
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, NULL);
    if (Process32First(snap, &pe) == TRUE) {
      while (Process32Next(snap, &pe) == TRUE) {
        if (_wcsicmp(pe.szExeFile, L"HorizonZeroDawn.exe") == 0) {
          pid = pe.th32ProcessID;
        }
      }
    }
    CloseHandle(snap);
  }
  if (pid == 0) {
    return -1;
  }

  HANDLE hp = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
  if (hp == INVALID_HANDLE_VALUE) {
    return -2;
  }

  PVOID mbase = 0;
  HMODULE hMods[100];
  DWORD cbMods = 0;
  if (EnumProcessModules(hp, hMods, 100, &cbMods)) {
    TCHAR szModName[MAX_PATH];
    for (int i = 0; i < (cbMods / sizeof(HMODULE)); ++i) {
      if (GetModuleFileNameEx(hp, hMods[i], szModName, sizeof(szModName) / sizeof(TCHAR))) {
        SSIZE_T off = (SSIZE_T)wcslen(szModName) - sizeof(L"HorizonZeroDawn.exe") / sizeof(TCHAR) + 1;
        if (off >= 0 && _wcsicmp(szModName + off, L"HorizonZeroDawn.exe") == 0) {
          mbase = (PVOID)hMods[i];
          break;
        }
        wprintf(L"%s\n", szModName + off);
      }
    }
  }
  if (mbase == NULL) {
    CloseHandle(hp);
    return -3;
  }

/*
  let's disable func at base+13793D0
  it sets up camera panning offset depending on character speed
  returning directly just leaves the by-ref argument at 0 (center)
*/

  char match[] = "\x48\x8B\xC4\x48\x89\x58\x08";
  SIZE_T matchlen = sizeof(match)-1;

  PVOID target = (PBYTE)mbase + 0x13793D0;

  BOOL ok;
  char read[sizeof(match)];

  SIZE_T readlen = 0;
  ok = ReadProcessMemory(hp, target, read, matchlen, &readlen);
  if (!ok || readlen != matchlen) {
    CloseHandle(hp);
    return -4;
  }

  if (read[0] == 0xC3) {
    MessageBoxA(0, "memory already patched", "info", 0);
    CloseHandle(hp);
    return -5;
  }

  if (memcmp(match, read, matchlen) != 0) {
    MessageBoxA(0, "new game version, update..", "error", 0);
    CloseHandle(hp);
    return -6;
  }

  DWORD dwOld = 0;
  ok = VirtualProtectEx(hp, target, matchlen, PAGE_EXECUTE_READWRITE, &dwOld);
  if (!ok) {
    CloseHandle(hp);
    return -7;
  }

  SIZE_T cbWritten = 0;
  CHAR ret_op = 0xC3;
  ok = WriteProcessMemory(hp, target, &ret_op, 1, &cbWritten);
  if (!ok || cbWritten != 1) {
    CloseHandle(hp);
    return -8;
  }

  ok = VirtualProtectEx(hp, target, matchlen, dwOld, &dwOld);
  if (!ok) {
    CloseHandle(hp);
    return -9;
  }

  CloseHandle(hp);
  return 0;
}

