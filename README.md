# ArmA Helper

A minimalist overlay shell for **Arma Reforger 1.8.0.13**, built with Direct3D 11 and Dear ImGui. The renderer and settings cover character boxes, names, distances, health, skeletons, and a Game Master camera indicator.

This public copy leaves game-data acquisition as a placeholder. It contains no driver, IOCTL implementation, or live memory access, so visualizations will not populate until a data source is integrated. The fake-memory tests exercise the projection and sampling logic without a game process.

Build on Windows x64 with Visual Studio C++ tools and the Windows SDK:

```powershell
.\build-user.cmd
.\out\user\controller-read-tests.exe
```

See the [architecture research](STRUCTURES.md) and [research highlights](docs/REVERSE_ENGINEERING.md). Add screenshots to [`docs/screenshots/`](docs/screenshots/).

Independent project; not affiliated with Bohemia Interactive. Dear ImGui is included under its [upstream license](third_party/imgui/LICENSE.txt).
