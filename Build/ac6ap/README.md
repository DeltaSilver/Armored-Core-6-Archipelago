# ac6ap DLL

Archipelago client DLL for Armored Core VI.

## Requirements

- Visual Studio with the **Desktop development with C++** workload
- [vcpkg](https://github.com/microsoft/vcpkg) with static OpenSSL:

  ```
  vcpkg install openssl:x64-windows-static
  ```

## Build

Open `ac6ap.slnx` and build **Release | x64**, or from a Developer Command Prompt:

```
msbuild ac6ap.vcxproj /p:Configuration=Release /p:Platform=x64
```

Output: `x64\Release\ac6ap.dll`

If vcpkg isn't installed at `C:\vcpkg`, pass its location:

```
msbuild ac6ap.vcxproj /p:Configuration=Release /p:Platform=x64 /p:OpenSslDir=D:\vcpkg\installed\x64-windows-static
```

The project targets the v145 toolset (Visual Studio 2026). On Visual Studio 2022, accept the retarget prompt or add `/p:PlatformToolset=v143`.
