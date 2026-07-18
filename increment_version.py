import re
import os

Import("env")

filename = "src/version.h"

def before_build(source, target, env):
    if not os.path.exists(filename):
        with open(filename, "w") as f:
            f.write('#ifndef VERSION_H\n#define VERSION_H\n\n#define BUILD_VERSION "v0.1.0"\n\n#endif\n')
        print("Created version.h with v0.1.0")
        return

    with open(filename, "r") as f:
        content = f.read()

    match = re.search(r'"v(\d+)\.(\d+)\.(\d+)"', content)

    if match:
        major, minor, patch = match.groups()
        new_patch = int(patch) + 1
        new_version = f'"v{major}.{minor}.{new_patch}"'
        new_content = content[:match.start()] + new_version + content[match.end():]
        
        with open(filename, "w") as f:
            f.write(new_content)
        print(f"--> Auto-incremented build version to {new_version}")
    else:
        print("--> Could not find version string to increment in version.h")

# Attach the pre-build action
env.AddPreAction("buildprog", before_build)

def after_build(source, target, env):
    import subprocess
    
    # Path to the hex file
    hex_path = os.path.join(env.subst("$BUILD_DIR"), env.subst("$PROGNAME") + ".hex")
    uf2_path = os.path.join(env.subst("$BUILD_DIR"), env.subst("$PROGNAME") + ".uf2")
    
    # Path to uf2conv.py
    packages_dir = env.subst("$PROJECT_PACKAGES_DIR")
    uf2conv_path = os.path.join(packages_dir, "framework-arduinoadafruitnrf52-seeed", "tools", "uf2conv", "uf2conv.py")
    
    if os.path.exists(uf2conv_path) and os.path.exists(hex_path):
        print(f"--> Converting {hex_path} to UF2 format...")
        # Family ID for nRF52840 is 0xada52840
        cmd = [env.subst("$PYTHONEXE"), uf2conv_path, "-f", "0xada52840", "-c", "-o", uf2_path, hex_path]
        
        result = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        if result.returncode == 0:
            print(f"--> Successfully created UF2: {uf2_path}")
        else:
            print(f"--> Failed to create UF2: {result.stderr}")
    else:
        print("--> Could not find uf2conv.py or hex file to generate UF2.")

# Attach the post-build action to the hex file target
env.AddPostAction("$BUILD_DIR/${PROGNAME}.hex", after_build)
