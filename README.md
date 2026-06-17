# AtlasScout

AtlasScout is a high-performance, plugin-based utility designed to locate sprite images within larger sprite sheets (atlases). It is built from the ground up for maximum speed and execution efficiency, utilizing SIMD hardware acceleration (AVX-512, AVX2) and dedicated CPU topology affinity pinning.

---

## Key Features

- **Hardware Accelerated Matching**: SIMD optimized matching backends including AVX-512 and AVX2 with dynamic runtime CPU capability detection.
- **Multithreaded Search Pool**: Lightweight internal thread pool execution with CPU sibling pinning to ensure cache efficiency by preventing thread migrations.
- **Cropping & Padded Alignment**: Automatic cropping of transparent margins around sprites, returning precise 1-indexed top-left boundaries of content.
- **Multiple Output Formats**: Supports structured and flat output formats: `JSON`, `TSV`, `CSV`, and `XML`.
- **Plugin Architecture**: Modular design separating the execution shell from search plugins (`.so` shared objects), dynamically loaded at runtime.
- **Pipeline Composable**: Conforms to standard UNIX philosophy, allowing pipeline piping by suppressing status reports on stdout using quiet mode (`-q`).

---

## Directory Structure

```
.
├── include/                  # Header files
│   ├── Config.hpp            # Configuration data structures
│   ├── ISearchPlugin.hpp     # Dynamic Search Plugin Interface
│   ├── Logger.hpp            # Logging utility
│   ├── bitwisexor_avx2.h     # AVX2 plugin definition
│   ├── bitwisexor_avx512.h   # AVX-512 plugin definition
│   ├── bitwisexor_fallback.h # Fallback plugin definition
|   └──extern/                # External libraries
|       ├──ArgsParser.hpp     # Custom CLI Argument Parser header
|       ├── ...               # Other external libraries
|                             #   -stb-image (Author: Sean Barrett)
|                             #   -nlohmann-json (Author: Niels Lohmann)
|
├── src/                      # Source code
│   ├── main.cpp              # CLI launcher and plugin resolver
│   ├── searcher.cpp          # Search coordinator & threadpool scheduling
│   ├── bitwisexor_avx2.cpp   # AVX2 backend implementation
│   ├── bitwisexor_avx512.cpp # AVX-512 backend implementation
│   └── bitwisexor_fallback.cpp # Fallback backend implementation
├── Makefile                  # Build definitions
├── atlasscout.md             # Format specification document
└── README.md                 # Project README
```

---

## Building the Project

### Prerequisites
- GCC/G++ supporting C++20.
- A CPU supporting AVX2 or AVX-512 (fallback plugin is provided for non-x86 or legacy systems).

### Compile Commands
Simply run:
```bash
make clean && make
```
This builds:
1. The `atlasscout` binary launcher in the root directory.
2. The dynamic search plugins inside the `plugins/` directory:
   - `plugins/avx512_search.so` (if AVX-512 is supported by the compiler/host)
   - `plugins/avx2_search.so`
   - `plugins/fallback_search.so`

---

## Command Line Interface (CLI)

```bash
./atlasscout [options]
```

### Options
| Short | Long | Description |
| :--- | :--- | :--- |
| `-c` | `--config <file>` | Path to a JSON configuration file. Overrides individual CLI flags. |
| `-s` | `--sprites <path>` | Path, directory, or wildcard pattern for the source sprites. |
| `-a` | `--atlases <path>` | Path, directory, or wildcard pattern for the atlases to scan. |
| `-r` | `--recurse` | Recursively scan subdirectories for sprites and atlases. |
| `-o` | `--output <file>` | Output file path (omit or use `-` for stdout). |
| `-f` | `--format <type>` | Output format. Options: `json` (default), `tsv`, `csv`, `xml`. |
| `-t` | `--type <name>` | Force a specific search plugin implementation (e.g., `avx2`). |
| `-l` | `--list-types` | List available search types (dynamically loaded from plugins) and exit. |
| `-q` | `--quiet` | Suppress all status and progress messages (silences stderr). |
| `-h` | `--help` | Display usage information and exit. |

---

## Configuration File Format

Instead of passing CLI parameters, you can run AtlasScout with a JSON configuration file using `-c`:

```json
{
  "atlas_source": "CoffeeBeaners/CoffeeBean*.png",
  "sprite_source": "example/Regions/*.png",
  "rotations": [
    0,
    270
  ],
  "output_file": "AtlasMatches",
  "output_format": "json",
  "search_type": "avx2",
  "recursive": true,
  "quiet": false
}
```

---

## Testing & Verification

We supply a comparison script to verify match output accuracy against the master document:

1. Run a search matching execution:
   ```bash
   ./atlasscout -c testfiles/config.json
   ```
2. Verify results using the comparison report tool:
   ```bash
   python3 testfiles/compare_matches.py
   ```

---

## License

This project is licensed under the GPL v3 License:

```
Copyright (C) 2026 Nikole Smith (ApptsolutioNZ - appsolutionz.com)

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <https://www.gnu.org/licenses/>.
```
