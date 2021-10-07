#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <tlhelp32.h>
#include <psapi.h>
#include <cstdio>
#include <iostream>
#include <memory>

struct scoped_handle
{
  scoped_handle(HANDLE h) : h(h) {}
  ~scoped_handle() {
    if (h != INVALID_HANDLE_VALUE)
      CloseHandle(h);
  }

  scoped_handle(const scoped_handle&) = delete;
  scoped_handle& operator=(const scoped_handle&) = delete;

  operator HANDLE() const { return h; }

  HANDLE h = INVALID_HANDLE_VALUE;
};

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

  scoped_handle hp = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
  if ((HANDLE)hp == INVALID_HANDLE_VALUE) {
    return -2;
  }

  std::cout << "process opened" << std::endl;

  PVOID mbase = 0;
  {
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
  }
  if (mbase == NULL) {
    std::cout << "error: couldn't find main module" << std::endl;
    return -3;
  }

  std::cout << "main module HorizonZeroDawn.exe found at " << mbase << std::endl;

  // get .text section

  char pe_header_buf[0x1000];
  {
    SIZE_T readlen = 0;
    const bool ok = ReadProcessMemory(hp, mbase, pe_header_buf, 0x1000, &readlen);
    if (!ok || readlen != 0x1000) {
      std::cout << "error: couldn't read image pe" << std::endl;
      return -4;
    }
  }

  std::cout << "fetched PE header" << std::endl;

  PIMAGE_NT_HEADERS pNtHdr = nullptr;
  {
    const auto pDosHdr = (PIMAGE_DOS_HEADER)pe_header_buf;
    if (pDosHdr->e_magic == IMAGE_DOS_SIGNATURE && pDosHdr->e_lfanew >= sizeof(PIMAGE_NT_HEADERS))
    {
      pNtHdr = (PIMAGE_NT_HEADERS)((LPCSTR)pe_header_buf + pDosHdr->e_lfanew);
      if (pNtHdr->Signature != IMAGE_NT_SIGNATURE)
        pNtHdr = nullptr;
    }
  }
  if (!pNtHdr) {
    std::cout << "error: unexpected pe header" << std::endl;
    return -4;
  }

  PVOID stext_start = 0;
  SIZE_T stext_size = 0;
  {
    const PIMAGE_SECTION_HEADER pSectionHdr = IMAGE_FIRST_SECTION(pNtHdr);
    for (SSIZE_T i = 0; i < pNtHdr->FileHeader.NumberOfSections; ++i)
    {
      const IMAGE_SECTION_HEADER& SectionHdr = pSectionHdr[i];
      if (0 == strcmp(".text", (LPCSTR)SectionHdr.Name))
      {
        stext_start = (PBYTE)mbase + SectionHdr.VirtualAddress;
        stext_size = SectionHdr.Misc.VirtualSize;
        break;
      }
    }
  }
  if (!stext_start) {
    std::cout << "error: couldn't locate text section" << std::endl;
    return -4;
  }

  std::cout << ".text section located ["
    << stext_start << "-"
    << (PVOID)((PBYTE)stext_start + stext_size)
    << "]" << std::endl;

  auto text_buf = std::make_unique<char[]>(stext_size);
  {
    SIZE_T readlen = 0;
    const bool ok = ReadProcessMemory(hp, stext_start, text_buf.get(), stext_size, &readlen);
    if (!ok || readlen != stext_size) {
      std::cout << "error: couldn't read text section" << std::endl;
      return -4;
    }
  }

/*
  let's disable func at base+13793D0
  it sets up camera panning offset depending on character speed
  returning directly just leaves the by-ref argument at 0 (center)
*/

  PVOID target = nullptr;
  {
    const char pattern[] = "\x48\x8B\xC4\x48\x89\x58\x08\x48\x89\x68\x10\x48\x89\x70\x18\x57\x48\x81\xEC\xC0\x00\x00\x00\x48\x8B\x79\x48";
    SIZE_T patlen = sizeof(pattern)-1;

    for (size_t i = 0; i < stext_size - patlen; ++i) {
      if (memcmp(text_buf.get() + i, pattern, patlen) == 0) {
        target = (PBYTE)stext_start + i;
        break;
      }
    }
  }
  if (!target) {
    std::cout << "error: couldn't find the target function to patch" << std::endl;
    return -5;
  }

  const size_t patch_size = 1;

  DWORD dwOld = 0;
  {
    const bool ok = VirtualProtectEx(hp, target, patch_size, PAGE_EXECUTE_READWRITE, &dwOld);
    if (!ok) {
      std::cout << "error: couldn't change memory protection to write" << std::endl;
      return -7;
    }
  }
  
  SIZE_T cbWritten = 0;
  CHAR ret_op = 0xC3;
  {
    const bool ok = WriteProcessMemory(hp, target, &ret_op, 1, &cbWritten);
    if (!ok || cbWritten != 1) {
      std::cout << "error: failed to write patch" << std::endl;
      return -8;
    }
  }

  VirtualProtectEx(hp, target, patch_size, dwOld, &dwOld);

  std::cout << "successfully patched runtime image" << std::endl;

  system("pause");

  return 0;
}

