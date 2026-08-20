# Geany Python Venv  ![Version](https://img.shields.io/badge/version-0.5-green)![Platform](https://img.shields.io/badge/platform-Linux-blue)
-   Run the current Python file with the right virtual environment — directly from Geany.

A lightweight Geany plugin for discovering, selecting and executing Python
virtual environments directly from Geany.

**Platform status: **Currently developed and tested on Linux.
> Windows and macOS are not currently tested.

## Features

- Detect Python virtual environments from configurable directories.
- Support multiple virtualenvs.
- Select the virtualenv directly from the Geany menu.
- Execute the current Python file with the selected virtualenv.
- Run Python programs in an external terminal.
- Configure and manage the directories scanned for virtual environments.
- Add and remove search directories from the plugin configuration.

## How it works

The plugin searches the configured directories for Python virtual
environments.

Typical virtual environments include:

- `venv`
- `.venv`
- Python virtual environments containing `bin/python`

Once detected, the environments are available from the **Execute Venv**
menu in Geany.

The selected environment is used to execute the currently open Python
file.

## Configuration

Open the plugin configuration from Geany and configure the directories
where your Python virtual environments are stored.

For example:

```text
/home/user/venvs
/home/user/projects
/home/user/Documents/python
The plugin can search these locations and detect available virtual
environments.
```
Building

Requirements:

Geany development files
GTK 3
GLib
CMake
GCC

Build with:
cmake -S . -B build
cmake --build build
The resulting plugin is:

build/python_venv.so
Installation

Copy or link the compiled plugin into the Geany user plugin directory.

For example:
```
mkdir -p ~/.config/geany/plugins

ln -sf \
    "$(pwd)/build/python_venv.so" \
    ~/.config/geany/plugins/python_venv.so
```
Restart Geany and enable the plugin from:

Tools → Plugin Manager

Usage
Open a Python file in Geany.
Configure the directories containing your virtual environments.
Select a virtual environment from Execute Venv.
Execute the current Python file.
The program runs using the selected virtual environment in an external terminal.
Status

Early development version.

The plugin is currently developed and tested on Linux with Geany 2.x.

License
![License](https://img.shields.io/badge/license-GPL--2.0--or--later-blue)
GPL-2.0-or-later
