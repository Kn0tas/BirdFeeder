# Suggested Commands

## Build and Flash
```powershell
# Build only
idf.py build

# Flash and monitor (replace COM3 with actual port)
idf.py -p COM3 flash monitor

# Full clean build
idf.py fullclean && idf.py build
```

## Training
```powershell
# Train model and export TFLite int8
python tools/train_and_convert.py

# With custom options
python tools/train_and_convert.py --epochs 30 --fine-tune-epochs 20 --img-size 96
```

## Debugging Camera Frames
```powershell
# Decode base64 frame from serial log
python tools/decode_frame.py
```

## Git
```powershell
git status
git diff HEAD --name-only
git log --oneline -10
```
