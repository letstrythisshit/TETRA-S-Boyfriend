# GUI Setup Guide for Raspberry Pi Debian ARM

## Issue: "Can't see GUI even with libraries installed"

This guide will help you get the GTK+ graphical interface working on your Raspberry Pi.

## Quick Diagnosis

The problem is that **GTK3 development libraries are not installed**, so the application was compiled **without GUI support**. You need to:
1. Install GTK3 development libraries
2. Rebuild the application

## Automated Installation (Recommended)

Run the provided installation script:

```bash
cd /path/to/TETRA-S-Boyfriend
./install_gui_support.sh
```

This script will:
- Check if GTK3 is installed
- Install GTK3 if needed (will prompt for sudo password)
- Rebuild the application with GUI support
- Verify the installation

## Manual Installation

If you prefer to do it manually:

### Step 1: Install GTK3

```bash
sudo apt-get update
sudo apt-get install -y libgtk-3-dev
```

### Step 2: Verify Installation

```bash
pkg-config --modversion gtk+-3.0
```

You should see a version number like `3.24.x`

### Step 3: Rebuild the Application

```bash
cd /path/to/TETRA-S-Boyfriend
make clean
make
```

Watch the build output for this line:
```
-- GTK3 found - GUI support enabled
```

If you see:
```
-- GTK3 not found - GUI support will be disabled
```

Then GTK3 wasn't detected. Try:
```bash
rm -rf build
mkdir build
cd build
cmake ..
make
cd ..
```

### Step 4: Test the GUI

```bash
./build/tetra_analyzer -f 420000000 -G
```

## Raspberry Pi Specific Notes

### Display Configuration

Make sure you have X11 running:

```bash
# Check if DISPLAY is set
echo $DISPLAY

# If empty, you need to be in a graphical session
# SSH won't work without X11 forwarding
```

### SSH with X11 Forwarding

If connecting via SSH, enable X11 forwarding:

```bash
# From your computer:
ssh -X pi@raspberry-pi-ip

# Then run:
./build/tetra_analyzer -f 420000000 -G
```

### VNC Session

If using VNC, the GUI should work directly in the VNC session.

### Headless Mode

For headless Raspberry Pi (no display), you'll need:
- X11 forwarding via SSH, OR
- VNC server installed, OR
- Use CLI mode without `-G` flag

## Troubleshooting

### Error: "GUI support not compiled in"

**Cause:** GTK3 wasn't installed when you compiled.

**Solution:**
```bash
sudo apt-get install libgtk-3-dev
make clean && make
```

### Error: "cannot open display"

**Cause:** No X11 display available.

**Solutions:**
1. Use SSH with X11 forwarding: `ssh -X user@pi`
2. Use VNC to access the desktop
3. Run from the physical Raspberry Pi desktop

### Build shows "GTK3 not found"

**Cause:** pkg-config can't find GTK3.

**Solution:**
```bash
# Verify GTK3 is installed
dpkg -l | grep libgtk-3-dev

# If not installed:
sudo apt-get install libgtk-3-dev

# Clear CMake cache
rm -rf build
make clean
make
```

### GUI is slow/laggy on Raspberry Pi

The GUI uses GTK3 which can be resource-intensive. Try:
- Close other applications
- Use CLI mode for better performance
- Consider using Raspberry Pi 4 or newer

## Verifying GUI Support

Check if your binary has GUI support:

```bash
./build/tetra_analyzer -h | grep -i gui
```

Should show:
```
-G, --gui              Enable GTK+ graphical interface 🖥️
```

Try running with `-G`:
```bash
./build/tetra_analyzer -f 420000000 -G
```

If it shows:
```
GUI support not compiled in. Rebuild with GTK3 installed.
```

Then GTK3 wasn't enabled during compilation. Follow the installation steps above.

## GUI Features

Once working, the GUI provides:

### Real-time Controls
- **Frequency:** Adjust center frequency (380-470 MHz)
- **Min Signal Power:** Noise rejection threshold (1.0-20.0)
- **Strong Match Threshold:** Primary detection (18-22 bits)
- **Moderate Match Threshold:** Secondary detection (15-22 bits)
- **Correlation Thresholds:** Match quality requirements
- **Low-Pass Filter:** Filter strength (0.1-1.0)

### Live Status Display
- Current signal power (RMS)
- Last match count (X/22 bits)
- Last correlation value
- Total detections counter
- Visual LED indicator:
  - 🟢 Green = Burst detected
  - 🟡 Yellow = Signal present
  - 🔴 Red = No signal

## Performance on Raspberry Pi

### Raspberry Pi 4/5
- GUI works smoothly
- Real-time parameter adjustment
- No performance issues

### Raspberry Pi 3
- GUI may be slightly slower
- Still usable for parameter tuning
- Consider CLI mode for continuous operation

### Raspberry Pi Zero/2
- GUI may be laggy
- Recommended to use CLI mode
- Use GUI only for initial parameter setup

## Alternative: CLI Mode

If GUI doesn't work or is too slow, you can still use all the improvements in CLI mode:

```bash
./build/tetra_analyzer -f 420000000 -v
```

All detection parameters are initialized to optimal defaults:
- Min Signal Power: 8.0
- Strong Match: 20/22 bits (90.9%)
- Moderate Match: 19/22 bits (86.4%)
- Strong Correlation: 0.8
- Moderate Correlation: 0.75
- LPF Cutoff: 0.5

## Need Help?

1. Check build output for GTK3 detection
2. Verify GTK3 is installed: `pkg-config --modversion gtk+-3.0`
3. Check binary has GUI: `strings ./build/tetra_analyzer | grep -i gtk`
4. Verify X11 display: `echo $DISPLAY`

## Summary

The most common issue is that **GTK3 development libraries are not installed** when you build the application. Simply install `libgtk-3-dev` and rebuild to enable GUI support.
