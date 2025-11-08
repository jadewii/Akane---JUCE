# 🚀 BUILD INSTRUCTIONS - WiiPluck Ultimate

## ⚡ QUICK START

### Step 1: Get JUCE
Download from: https://juce.com/get-juce/
Or clone: `git clone https://github.com/juce-framework/JUCE.git`

### Step 2: Build
```bash
cd WiiPluckProject
mkdir build && cd build
cmake ..
cmake --build . --config Release
```

### Step 3: Done!
Your plugin is in `build/WiiPluckUltimate_artefacts/Release/`

## 🎛️ What You Get
- 5 Engine Modes (Rings/Clouds/Karplus/Hybrid)
- 16-voice polyphony
- 30+ parameters
- VST3/AU/Standalone

## 🎵 Try It
Load in your DAW and play!

## 🐛 Troubleshooting
**"JUCE not found"** → Put JUCE folder in project or use: `cmake .. -DCMAKE_PREFIX_PATH=/path/to/JUCE`

**"Plugin not showing in DAW"** → Restart DAW, rescan plugins

## 📚 Full docs in ULTIMATE_PLUCK_README.md
