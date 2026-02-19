import os
import argparse
import uuid
from pathlib import Path

def rename_dataset(directory, prefix):
    dir_path = Path(directory)
    if not dir_path.exists():
        print(f"Directory not found: {dir_path}")
        return

    # Get all files
    files = [f for f in dir_path.iterdir() if f.is_file() and f.name != ".keep"]
    files.sort() # Ensure deterministic order if run multiple times (though UUID step breaks this, sorting helps initial mapping)

    print(f"Found {len(files)} files in {directory}")

    # Step 1: Rename all to temporary UUIDs to avoid collisions
    temp_files = []
    for f in files:
        ext = f.suffix.lower()
        if ext == '.jpeg': ext = '.jpg' # Normalize jpeg to jpg
        
        tmp_name = dir_path / f"{uuid.uuid4()}{ext}"
        os.rename(f, tmp_name)
        temp_files.append(tmp_name)

    # Step 2: Rename to target format
    count = 0
    for i, f in enumerate(temp_files):
        ext = f.suffix
        new_name = dir_path / f"{prefix}{i+1}{ext}"
        os.rename(f, new_name)
        count += 1

    print(f"Renamed {count} files to {prefix}1{ext} ... {prefix}{count}{ext}")

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Rename files in a dataset directory")
    parser.add_argument("directory", help="Path to the dataset directory")
    parser.add_argument("prefix", help="Prefix for the new filenames (e.g., 'squirrel')")
    args = parser.parse_args()

    rename_dataset(args.directory, args.prefix)
