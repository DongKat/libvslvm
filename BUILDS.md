# Build with Visual Studio

## Prerequisites

- **Visual Studio 2022** (Community, Professional, or Enterprise) with the **Desktop development with C++** workload installed.
- **Dokan v2.3.1** driver and library — required by the `vslvmmount` project to expose mounted LVM volumes as Windows drive letters.
- **Python 3.11+** — used by the `build.ps1` script to convert the `msvscpp` into Visual Studio solution format.

## Step 1: Install the Dokan Driver and Library

1. Download the **DokanSetup.exe** installer from the [Dokany releases page](https://github.com/dokan-dev/dokany/releases) (version **2.3.1**).
2. Run the installer and select **both** components:
   - **Dokan User-Mode Library** — installs headers and `.lib` files.
   - **Dokan Kernel Driver** — installs the filesystem driver (`dokan2.sys`).
3. Use the default installation path: `C:\Program Files\Dokan\DokanLibrary-2.3.1`.

After installation, verify the following paths exist:

| File | Path |
|------|------|
| Header | `C:\Program Files\Dokan\DokanLibrary-2.3.1\include\dokan\dokan.h` |
| x64 lib | `C:\Program Files\Dokan\DokanLibrary-2.3.1\lib\dokan2.lib` |
| x86 lib | `C:\Program Files\Dokan\DokanLibrary-2.3.1\x86\lib\dokan2.lib` |

> **Note:** If you install a different Dokan version or to a non-default path, you must update the `AdditionalIncludeDirectories` and `AdditionalDependencies` in `vs2022\vslvmmount\vslvmmount.vcxproj` to match.

## Step 2: Sync dependencies libraries and autogen necessary files
```powershell
.\synclib.ps1
.\autogen.ps1
```

## Step 3: Generate the VS2022 Solution and Build

This step converts the base Visual Studio solution to VS2022 format and compiles all projects.

```powershell
.\build.ps1 -PythonPath <your_python_path> -Configuration "Release" -Platform "x64"
```

The default expected python path is `C:\Python311`, if your Python is installed in a different location (e.g., `C:\Python312` or `C:\Users\<username>\AppData\Local\Programs\Python\Python311`), specify the correct path.

**Note:** Ignore the build error when compiling `pyvslvm` project because we don't need it for CLI tools

## Step 4: Fix vslvmmount project to link to installed dokan library

Open the solution in Visual Studio and update the `vslvmmount` project settings to match your Dokan installation.

1. Open `vs2022\libvslvm.sln` in Visual Studio 2022.
2. In **Solution Explorer**, right-click **vslvmmount** and choose **Properties**.
3. Set **Configuration** = `All Configurations` and **Platform** = `All Platforms`.
4. Update the following fields (paths shown for the default Dokan install):

    - **C/C++ > General > Additional Include Directories**
       `C:\Program Files\Dokan\DokanLibrary-2.3.1\include\dokan`

    - **Linker > General > Additional Library Directories**
       - x64: `C:\Program Files\Dokan\DokanLibrary-2.3.1\lib`
       - Win32: `C:\Program Files\Dokan\DokanLibrary-2.3.1\x86\lib`

    - **Linker > Input > Additional Dependencies**
       `dokan2.lib`

5. Save the project and rebuild `vslvmmount`.

> If Dokan is installed in a different location or version, replace the paths above with your install path.


## Step 5: Verify the Build Output

Build artifacts are placed in `vs2022\Release\<Platform>\`.

```
vs2022\Release\x64\vslvmmount.exe    # LVM mount tool (uses Dokan)
vs2022\Release\x64\vslvminfo.exe     # LVM info tool
vs2022\Release\x64\libvslvm.dll      # Core library
```

Test that `vslvmmount.exe` loads correctly:

```powershell
.\vs2022\Release\x64\vslvmmount.exe -h
```

## How Dokan is Linked to `vslvmmount`

The `vslvmmount` project (`vs2022\vslvmmount\vslvmmount.vcxproj`) is pre-configured to link against Dokan:

- **Include path:** `C:\Program Files\Dokan\DokanLibrary-2.3.1\include\dokan` is added to `AdditionalIncludeDirectories`.
- **Preprocessor:** `HAVE_LIBDOKAN` is defined, which enables the Dokan code path in `mount_dokan.c` (guarded by `#if defined( HAVE_LIBDOKAN )`).
- **Linker:** `dokan2.lib` is added to `AdditionalDependencies` (x86 uses `x86\lib\dokan2.lib`, x64 uses `lib\dokan2.lib`).
- **Runtime DLL:** At runtime, `vslvmmount.exe` requires `dokan2.dll` to be on the system `PATH` (the Dokan installer handles this).

## Troubleshooting

| Problem | Solution |
|---------|----------|
| `dokan.h` not found | Check that the Dokan include path in the vcxproj matches your installed version. |
| `dokan2.lib` linker error | Verify the `.lib` path. x64 builds need `lib\dokan2.lib`, x86 builds need `x86\lib\dokan2.lib`. |
| `dokan2.dll` not found at runtime | Ensure the Dokan driver is installed and `C:\Program Files\Dokan\DokanLibrary-2.3.1` is on your system `PATH`. |
| `vslvmmount` hangs on exit | The Dokan kernel driver may not be running. Start it with `sc start dokan2` (requires admin). |