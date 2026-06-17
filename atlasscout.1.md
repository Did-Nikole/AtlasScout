% ATLASSCOUT(1) Version 1.0.0 | AtlasScout User Manuals
% Nikole Smith
% June 2026

# NAME

atlasscout - locate sprite images within larger sprite sheets (atlases)

# SYNOPSIS

**atlasscout** [**-c** *config*] [**-s** *sprites*] [**-a** *atlases*] [**-o** *output*] [**-f** *format*] [**-t** *type*] [**-r**] [**-q**] [**-l**] [**-h**]

# DESCRIPTION

**atlasscout** is a fast, plugin-based utility designed to locate sprite images within larger sprite sheets (atlases). It supports AVX2 and AVX-512 SIMD matching backends and dedicated physical core CPU pinning for high-speed parallel matching.

# OPTIONS

-c, \--config *file*
:   Path to a JSON configuration file. Overrides individual CLI flags.

-s, \--sprites *path*
:   Path, directory, or pattern for the source sprites.

-a, \--atlases *path*
:   Path, directory, or pattern for the atlases to scan.

-r, \--recurse
:   Recursively search subdirectories for both source sprites and atlases.

-o, \--output *file*
:   Output file path. If omitted or set to `-`, defaults to stdout.

-f, \--format *type*
:   Built-in output format. Options: `json` (default), `tsv`, `csv`, `xml`.

-t, \--type *name*
:   Force a specific search `.so` plugin implementation (e.g. `avx2`, `fallback`).

-l, \--list-types
:   List available search types dynamically loaded from plugins and exit.

-q, \--quiet
:   Suppress all progress/status messages (silences stderr).

-h, \--help
:   Print usage information and exit.

# LICENSE

GPL v3
