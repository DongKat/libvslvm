/*
 * vslvmextract - mounts a VSLVM volume group image using vslvmmount.exe
 * (launched in the background), copies all mounted content to an output
 * directory, then terminates the mount process and cleans up.
 *
 * Usage: vslvmextract [-v] <vslvm_image> <output_dir> [path_to_vslvmmount.exe]
 *
 * Options:
 *   -v             Enable verbose logging to log.txt (next to the executable)
 *   -h, --help     Show this help message
 *
 * If vslvmmount.exe is not specified it is looked up next to vslvmextract.exe.
 */

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0600
#endif

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <tchar.h>

/* -------------------------------------------------------------------------
 * Logging
 * ---------------------------------------------------------------------- */

static FILE *g_log = NULL;

static void vslvmextract_log_open(
    void)
{
    wchar_t log_path[MAX_PATH];
    DWORD len = GetModuleFileName(NULL, log_path, MAX_PATH);
    if (len == 0)
    {
        return;
    }
    wchar_t *last_slash = wcsrchr(log_path, L'\\');
    if (last_slash != NULL)
    {
        *(last_slash + 1) = L'\0';
    }
    wcsncat(log_path, L"log.txt", MAX_PATH - (DWORD)wcslen(log_path) - 1);
    _tfopen_s(&g_log, log_path, L"a");
}

static void vslvmextract_log_close(
    void)
{
    if (g_log != NULL)
    {
        fclose(g_log);
        g_log = NULL;
    }
}

static void vslvmextract_log(
    const wchar_t *fmt, ...)
{
    if (g_log == NULL)
    {
        return;
    }
    SYSTEMTIME st;
    GetLocalTime(&st);
    _ftprintf(g_log, L"[%04d-%02d-%02d %02d:%02d:%02d] ",
              st.wYear, st.wMonth, st.wDay,
              st.wHour, st.wMinute, st.wSecond);
    va_list args;
    va_start(args, fmt);
    _vftprintf(g_log, fmt, args);
    va_end(args);
    _ftprintf(g_log, L"\n");
    fflush(g_log);
}

/* -------------------------------------------------------------------------
 * Internal helpers
 * ---------------------------------------------------------------------- */

static int vslvmextract_ensure_directory(
    const wchar_t *path)
{
    if (CreateDirectory(path, NULL))
    {
        return (0);
    }
    if (GetLastError() == ERROR_ALREADY_EXISTS)
    {
        return (0);
    }
    return (-1);
}

/* Recursively copy src_dir into dst_dir (dst_dir is created if needed). */
static int vslvmextract_copy_directory(
    const wchar_t *src_dir,
    const wchar_t *dst_dir)
{
    wchar_t search_path[MAX_PATH];
    wchar_t src_path[MAX_PATH];
    wchar_t dst_path[MAX_PATH];
    WIN32_FIND_DATA fd;
    HANDLE hFind;
    int result = 0;

    if (vslvmextract_ensure_directory(dst_dir) != 0)
    {
        return (-1);
    }

    _sntprintf(search_path, MAX_PATH, L"%s\\*", src_dir);
    hFind = FindFirstFile(search_path, &fd);
    if (hFind == INVALID_HANDLE_VALUE)
    {
        /* Empty or inaccessible directory - not a fatal error. */
        return (0);
    }

    do
    {
        if (wcscmp(fd.cFileName, L".") == 0 || wcscmp(fd.cFileName, L"..") == 0)
        {
            continue;
        }

        _sntprintf(src_path, MAX_PATH, L"%s\\%s", src_dir, fd.cFileName);
        _sntprintf(dst_path, MAX_PATH, L"%s\\%s", dst_dir, fd.cFileName);

        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
        {
            if (vslvmextract_copy_directory(src_path, dst_path) != 0)
            {
                result = -1;
            }
        }
        else
        {
            if (!CopyFile(src_path, dst_path, FALSE))
            {
                result = -1;
            }
        }
    } while (FindNextFile(hFind, &fd));

    FindClose(hFind);
    return (result);
}

/* Best-effort recursive directory removal (temp mount point cleanup). */
static void vslvmextract_remove_directory(
    const wchar_t *path)
{
    wchar_t search_path[MAX_PATH];
    wchar_t child_path[MAX_PATH];
    WIN32_FIND_DATA fd;
    HANDLE hFind;

    _sntprintf(search_path, MAX_PATH, L"%s\\*", path);
    hFind = FindFirstFile(search_path, &fd);
    if (hFind != INVALID_HANDLE_VALUE)
    {
        do
        {
            if (wcscmp(fd.cFileName, L".") == 0 || wcscmp(fd.cFileName, L"..") == 0)
            {
                continue;
            }

            _sntprintf(child_path, MAX_PATH, L"%s\\%s", path, fd.cFileName);

            if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            {
                vslvmextract_remove_directory(child_path);
            }
            else
            {
                SetFileAttributes(child_path, FILE_ATTRIBUTE_NORMAL);
                DeleteFile(child_path);
            }
        } while (FindNextFile(hFind, &fd));

        FindClose(hFind);
    }

    RemoveDirectory(path);
}

/* -------------------------------------------------------------------------
 * Entry point
 * ---------------------------------------------------------------------- */

int wmain(
    int argc,
    wchar_t *argv[])
{
    wchar_t mount_exe[MAX_PATH];
    wchar_t mount_point[MAX_PATH];
    wchar_t temp_dir[MAX_PATH];
    wchar_t cmdline[32768];
    STARTUPINFO si;
    PROCESS_INFORMATION pi;
    int result = 0;
    int verbose = 0;

    /* ------------------------------------------------------------------ */
    /* Parse flags                                                         */
    /* ------------------------------------------------------------------ */
    int first_pos = 1; /* index of first positional argument */
    for (; first_pos < argc; first_pos++)
    {
        if (wcscmp(argv[first_pos], L"-v") == 0)
        {
            verbose = 1;
        }
        else if (wcscmp(argv[first_pos], L"-h") == 0 || wcscmp(argv[first_pos], L"--help") == 0)
        {
            _tprintf(
                L"Usage: vslvmextract [-v] <vslvm_image> <output_dir>"
                L" [path_to_vslvmmount.exe]\n"
                L"\n"
                L"Options:\n"
                L"  -v             Enable verbose logging to log.txt\n"
                L"  -h, --help     Show this help\n"
                L"\n"
                L"Mounts the VSLVM image via vslvmmount.exe (Dokan), copies all\n"
                L"content to output_dir, then unmounts and cleans up.\n"
                L"If vslvmmount.exe is not specified it is looked up next to\n"
                L"vslvmextract.exe.\n");
            return (0);
        }
        else
        {
            break; /* first non-flag argument */
        }
    }

    /* Positional: <vslvm_image> <output_dir> [vslvmmount_path] */
    int pos_argc = argc - first_pos;
    wchar_t **pos_argv = argv + first_pos;

    if (pos_argc < 2)
    {
        _ftprintf(stderr,
                  L"Error: insufficient arguments.\n"
                  L"Run with -h for help.\n");
        return (1);
    }

    const wchar_t *vslvm_image = pos_argv[0];
    const wchar_t *output_dir = pos_argv[1];

    if (verbose)
    {
        vslvmextract_log_open();
    }

    vslvmextract_log(L"Image : %s", vslvm_image);
    vslvmextract_log(L"Output: %s", output_dir);

    /* ------------------------------------------------------------------ */
    /* Resolve vslvmmount.exe path                                         */
    /* ------------------------------------------------------------------ */
    if (pos_argc >= 3)
    {
        _sntprintf(mount_exe, MAX_PATH, L"%s", pos_argv[2]);
    }
    else
    {
        /* Default: same directory as this executable. */
        DWORD len = GetModuleFileName(NULL, mount_exe, MAX_PATH);
        if (len == 0)
        {
            vslvmextract_log(L"Error: GetModuleFileName failed (%u)", GetLastError());
            vslvmextract_log_close();
            return (1);
        }

        wchar_t *last_slash = wcsrchr(mount_exe, L'\\');
        if (last_slash != NULL)
        {
            *(last_slash + 1) = L'\0';
        }
        wcsncat(mount_exe, L"vslvmmount.exe",
                MAX_PATH - (DWORD)wcslen(mount_exe) - 1);
    }

    vslvmextract_log(L"vslvmmount: %s", mount_exe);

    /* ------------------------------------------------------------------ */
    /* Create a unique temporary mount point                               */
    /* ------------------------------------------------------------------ */
    if (GetTempPath(MAX_PATH, temp_dir) == 0)
    {
        vslvmextract_log(L"Error: GetTempPath failed (%u)", GetLastError());
        vslvmextract_log_close();
        return (1);
    }

    _sntprintf(mount_point, MAX_PATH, L"%svslvm_mount_%u_%u",
               temp_dir, GetCurrentProcessId(), GetTickCount());

    if (!CreateDirectory(mount_point, NULL))
    {
        vslvmextract_log(L"Error: cannot create mount point: %s (%u)", mount_point, GetLastError());
        vslvmextract_log_close();
        return (1);
    }

    vslvmextract_log(L"Mount point: %s", mount_point);

    /* ------------------------------------------------------------------ */
    /* Launch vslvmmount in the background                                 */
    /* ------------------------------------------------------------------ */
    _sntprintf(cmdline, 32768, L"\"%s\" \"%s\" \"%s\"",
               mount_exe, vslvm_image, mount_point);

    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));

    si.dwFlags |= STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;

    if (!CreateProcess(NULL, cmdline,
                       NULL, NULL,
                       FALSE,
                       CREATE_NO_WINDOW,
                       NULL, NULL,
                       &si, &pi))
    {
        vslvmextract_log(L"Error: failed to launch vslvmmount (%u)", GetLastError());
        vslvmextract_remove_directory(mount_point);
        vslvmextract_log_close();
        return (1);
    }

    CloseHandle(pi.hThread);
    vslvmextract_log(L"Mounted (PID %u) - waiting for filesystem to become ready...", pi.dwProcessId);

    /* Give Dokan/fuse time to initialise the mount. */
    Sleep(2000);

    /* ------------------------------------------------------------------ */
    /* Copy files                                                          */
    /* ------------------------------------------------------------------ */
    vslvmextract_log(L"Copying files...");
    result = vslvmextract_copy_directory(mount_point, output_dir);
    vslvmextract_log(L"Copy %s", result == 0 ? L"complete" : L"completed with warnings");

    /* ------------------------------------------------------------------ */
    /* Unmount: terminate vslvmmount                                       */
    /* ------------------------------------------------------------------ */
    vslvmextract_log(L"Unmounting (PID %u)...", pi.dwProcessId);
    TerminateProcess(pi.hProcess, 0);
    WaitForSingleObject(pi.hProcess, 5000);
    CloseHandle(pi.hProcess);

    /* Brief pause to allow the driver to release the mount point. */
    Sleep(2000);

    /* ------------------------------------------------------------------ */
    /* Clean up temporary mount point                                      */
    /* ------------------------------------------------------------------ */
    vslvmextract_remove_directory(mount_point);
    vslvmextract_log(L"Done (exit code 0)");
    vslvmextract_log_close();

    return (0);
}