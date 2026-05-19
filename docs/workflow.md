# Complete Workflow Guide: PhantomFPGA

### Work Environments (Terminals):
For a convenient workflow, open two terminal windows:
*   **Terminal 1 (Host):** Your local computer (Ubuntu). Used for compiling the code and transferring files.

    ```bash
    cd phantomfpga-project
    ```

*   **Terminal 2 (QEMU):** The virtual machine. Used for running the driver and the application.

    ```bash
    cd PhantomFPGA
    ./platform/run_qemu.sh --arch aarch64
    ```

---

## Step 1: Compile and Load the Driver

In this step, we will create the driver file (`.ko`), transfer it to QEMU, and load it into the kernel.

**In Terminal 1 (Host):**
```bash
# 1. Enter the driver directory
cd driver

# 2. Clean previous build outputs
make clean

# 3. Compile the driver for the arm64 architecture using the Buildroot compiler
make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- KDIR=../../PhantomFPGA/platform/buildroot/output/aarch64/build/linux-6.6.70

# 4. Transfer the compiled file into QEMU
# (It is recommended to make sure the /mnt/driver directory exists in QEMU)
scp -O -P 2222 phantomfpga.ko root@localhost:/mnt/driver/
```

**In Terminal 2 (QEMU):**

```bash
# 5. Load the driver into the QEMU kernel
insmod /mnt/driver/phantomfpga.ko

# 6. Check that the driver loaded successfully by printing the latest kernel messages
dmesg | tail -50
```

> **What should happen?** In the logs, you should see the line:
> `phantomfpga: PhantomFPGA v3.0 driver initialized (major=511)`

---

## Step 1b: Update the Driver After Code Changes (Reload)

If you changed something in the driver code and compiled it again (steps 1-4 above), you must remove the old driver from memory before loading the new one.

**In Terminal 2 (QEMU):**

```bash
# 1. Stop the application if it is running and using the driver
pkill phantomfpga_app 2>/dev/null || true

# 2. Remove the old driver from the kernel
rmmod phantomfpga 2>/dev/null || true

# 3. Load the updated driver that you just transferred with scp
insmod /mnt/driver/phantomfpga.ko
```

---

## Step 2: Compile and Run the Application

The application runs inside QEMU. Its job is to "communicate" with the driver loaded in the previous step.

**In Terminal 1 (Host):**

```bash
# 1. Enter the application directory
cd ../app

# 2. Clean previous build outputs
make clean

# 3. Compile the application with the dedicated compiler (aarch64-linux-g++)
make CXX=$PWD/../../PhantomFPGA/platform/buildroot/output/aarch64/host/bin/aarch64-linux-g++

# 4. Transfer the compiled application to QEMU
scp -O -P 2222 phantomfpga_app root@localhost:/mnt/app/
```

**In Terminal 2 (QEMU):**

```bash
# 5. Run the application with the help flag to make sure it is valid and starts correctly
/mnt/app/phantomfpga_app --help || true
```

> **What should happen?** The application's help menu should be printed to the screen and show the available run options, such as opening a TCP server.

---

## Step 3: Compile and Run the Viewer

The Viewer is a user interface, either graphical or text-based, that is meant to run **on your computer (Host)** and not inside QEMU. It connects over the network (TCP) to the application running inside QEMU.

**In Terminal 1 (Host):**

```bash
# 1. Enter the Viewer directory
cd ../viewer

# 2. Clean previous build outputs
make clean

# 3. Compile the Viewer
# Here we compile for your local computer, so no special compiler is needed
make

# 4. Run the Viewer to make sure it works
./phantomfpga_view --help || true
```

> **What should happen?** The Viewer should start and display its run commands. To see actual data, you will need to run the application in QEMU in TCP server mode, and run the Viewer on the Host so it connects to it, as described in the help instructions.
