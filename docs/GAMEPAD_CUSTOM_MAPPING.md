# Custom Gamepad Mapping File Guide

## Overview

Vimix allows you to specify a custom gamepad mapping database file to replace the embedded default database. This is useful when:

- You have controllers not in the default database
- You want to use an updated version of SDL_GameControllerDB
- You've created custom mappings for your specific hardware
- You need to maintain organization-specific controller configurations

## Quick Start

1. **Open vimix Settings**: Settings > Gamepad
2. **Download a mapping file**: Get the latest from [SDL_GameControllerDB](https://github.com/gabomdq/SDL_GameControllerDB)
3. **Specify the file**:
   - Type the path in the text field, or
   - Click "Browse" to select the file
4. **Automatic reload**: Mappings load immediately when you specify a file

## Vimix Settings for Gamepad Input

### Device Selector
- **Purpose**: Shows currently selected gamepad
- **Display**: Shows device name if recognized, "Not recognized" otherwise
- **Dropdown**: Lists all detected gamepads

### Custom Mapping File Input

**Text Field**:
- Enter path to your `gamecontrollerdb.txt` file
- Supports `~` for home directory (e.g., `~/gamecontrollerdb.txt`)
- Supports absolute paths (e.g., `/home/user/configs/gamecontrollerdb.txt`)
- Supports relative paths

**Browse Button** (📁 Browse):
- Opens file browser dialog
- Filters for `gamecontrollerdb.txt` and `*.txt` files
- Automatically applies selection

**Reload Button** (🔄 Reload):
- Manually reloads the mapping file
- Useful after editing the file externally
- Re-validates file existence

## File Format

The gamepad mapping file uses SDL's `gamecontrollerdb.txt` format:

```
# Comments start with #
# Format: GUID,Name,platform:Platform,a:button,b:button,x:button,...

# Linux Xbox Controller
030000005e040000120b000011010000,Xbox Series X Controller,platform:Linux,a:b0,b:b1,x:b2,y:b3,back:b6,start:b7,guide:b8,leftshoulder:b4,rightshoulder:b5,leftstick:b9,rightstick:b10,leftx:a0,lefty:a1,rightx:a3,righty:a4,lefttrigger:a2,righttrigger:a5,dpup:h0.1,dpdown:h0.4,dpleft:h0.8,dpright:h0.2,

# Multiple platforms can be in the same file
030000005e040000120b000000000000,Xbox Series X Controller,platform:Windows,a:b0,b:b1,x:b2,y:b3,...
```

## Creating Custom Mappings

### Option 1: Download Latest Database

Get the community-maintained database:

```bash
wget https://raw.githubusercontent.com/gabomdq/SDL_GameControllerDB/master/gamecontrollerdb.txt
mv gamecontrollerdb.txt ~/gamecontrollerdb.txt
```

Then in vimix Settings > Gamepad, enter: `~/gamecontrollerdb.txt`

### Option 2: Create Your Own Mapping

If your controller isn't in the database:

1. **Use SDL2 Gamepad Tool**:
   - Download from: https://generalarcade.com/gamepadtool/
   - Run the tool
   - Follow prompts to press buttons
   - Copy the generated mapping line

2. **Add to file**:
   ```bash
   # Create or edit your custom file
   echo "YOUR_MAPPING_LINE_HERE" >> ~/gamecontrollerdb.txt
   ```

3. **Load in vimix**: Settings > Gamepad > specify `~/gamecontrollerdb.txt`


### See Also

- [GAMEPAD_SUPPORT.md](GAMEPAD_SUPPORT.md) - General gamepad documentation
- [CONTRIBUTING_GAMEPAD_MAPPINGS.md](CONTRIBUTING_GAMEPAD_MAPPINGS.md) - How to create mappings
- [SDL_GameControllerDB](https://github.com/gabomdq/SDL_GameControllerDB) - Community database
- [SDL2 Gamepad Tool](https://generalarcade.com/gamepadtool/) - Mapping creation tool
