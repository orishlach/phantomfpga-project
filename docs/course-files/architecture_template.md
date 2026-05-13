# Team Architecture Design Template

**Team Name:** [Team A / Team B / etc.]
**Members:** [List names]
**Date:** March 26, 2026
**Assigned Roles:**
- Driver Lead: [Name]
- App/Protocol Lead: [Name]
- Viewer Lead: [Name]

---

## High-Level Architecture

```
[Draw a box diagram here showing the major components and how they connect]

Example:
┌─────────────────────────────────────────┐
│            Linux Kernel                 │
│  ┌──────────────────────────────────┐   │
│  │   phantomfpga_drv (Your Driver)  │   │
│  │  - PCI probe                      │   │
│  │  - DMA setup                      │   │
│  │  - Interrupt handling             │   │
│  │  - Char device interface          │   │
│  └──────────────────────────────────┘   │
│           ↓ (ioctl, mmap)               │
└─────────────────────────────────────────┘
         ↓
┌─────────────────────────────────────────┐
│        Userspace (Your App)             │
│  ┌──────────────────────────────────┐   │
│  │  phantomfpga_app                 │   │
│  │  - Device configuration          │   │
│  │  - Frame validation              │   │
│  │  - TCP server (port 5000)        │   │
│  └──────────────────────────────────┘   │
│           ↓ (TCP)                       │
└─────────────────────────────────────────┘
         ↓
┌─────────────────────────────────────────┐
│        Viewer (Your Viewer)             │
│  ┌──────────────────────────────────┐   │
│  │  phantomfpga_view                │   │
│  │  - TCP client                    │   │
│  │  - Frame display                 │   │
│  │  - Terminal UI                   │   │
│  └──────────────────────────────────┘   │
│                                         │
│        (Displays to stdout)             │
└─────────────────────────────────────────┘
```

---

## Component Responsibilities

### Kernel Driver

**What it does:**
- Initializes the PhantomFPGA device (PCI enumeration)
- Sets up DMA descriptors and memory buffers
- Handles interrupts from the device
- Provides userspace interface via character device

**Key interfaces:**
- **Input:** PCIe device discovery, interrupt signals from hardware
- **Output:** `/dev/phantomfpga0` character device, ioctl API, mmap buffers
- **Errors to handle:** Device not found, DMA failure, interrupt handling errors

**Data flow:**
```
Hardware interrupt → IRQ handler → Update DMA tail index → Wake userspace
```

### Userspace Application

**What it does:**
- Opens and configures the kernel driver
- Receives frame data from the device (via kernel driver)
- Validates frames (magic, CRC)
- Streams frames over TCP

**Key interfaces:**
- **Input:** `/dev/phantomfpga0` (kernel driver), configuration parameters
- **Output:** TCP server on port 5000, serving frame data
- **Errors to handle:** Device not responding, frame validation failure, network errors

**Data flow:**
```
Open /dev/phantomfpga0 → Configure (ioctl) → mmap DMA buffer → Poll for frames → TCP stream
```

### Terminal Viewer

**What it does:**
- Connects to TCP server (app)
- Receives frame data
- Validates frames
- Displays frames in terminal

**Key interfaces:**
- **Input:** TCP connection to app, frame data
- **Output:** Terminal display, optional file recording
- **Errors to handle:** Connection drops, CRC validation failure, malformed frames

**Data flow:**
```
TCP connect → Receive frames → Validate CRC → Display/Record
```

---

## Key Design Decisions

### DMA Strategy
[ ] We will use ring buffers with N descriptors
[ ] Ring size: [how many?]
[ ] Each descriptor points to: [one frame buffer / multiple frames?]
[ ] Completion handling: [interrupt-driven / poll-based?]

### Error Handling
[ ] Frame validation: [magic number, CRC-32, sequence check]
[ ] On validation failure: [skip frame / stop streaming / log error?]
[ ] Device error: [reset device / exit cleanly / retry?]

### Frame Transmission
[ ] Protocol: [raw bytes / header + payload + checksum?]
[ ] Streaming approach: [continuous / on-demand?]
[ ] Backpressure: [queue frames if viewer slow / drop oldest frames?]

### Threading Model (if any)
[ ] Single-threaded (no threads)
[ ] Multi-threaded: [which parts are threaded?]
[ ] Thread communication: [pipes / queues / shared memory?]

---

## Data Structures

### Driver → Userspace (ioctl commands)

```c
// Proposed ioctl commands
PHANTOMFPGA_IOCTL_SET_CONFIG   // Configure frame rate, etc.
PHANTOMFPGA_IOCTL_GET_CONFIG   // Read current configuration
PHANTOMFPGA_IOCTL_START        // Start streaming
PHANTOMFPGA_IOCTL_STOP         // Stop streaming
PHANTOMFPGA_IOCTL_GET_STATS    // Read frame counters
PHANTOMFPGA_IOCTL_CONSUME_FRAME // Mark frame as consumed
```

### App → Viewer (TCP protocol)

```c
// Proposed frame structure over TCP
struct FrameHeader {
    uint32_t magic;        // 0xF00DFACE
    uint32_t sequence;     // 0-249, loops
    uint32_t size;         // 5120
};

// Followed by 5120 bytes of frame data
// Followed by 4 bytes of CRC-32
```

---

## Risk Assessment

| Risk | Probability | Impact | Mitigation |
|------|-------------|--------|-----------|
| Driver fails to probe device | Low | High | Read docs, test early, ask for help |
| DMA not working correctly | Medium | High | Use QEMU debugging tools, write test driver |
| Frame data corruption | Low | Medium | Implement CRC validation |
| Network connection drops | Medium | Low | Add reconnection logic |
| Terminal display issues | Low | Low | Test on different terminals, fallback to hex dump |

---

## Integration Points

### Driver ↔ App

**Interface:** Character device `/dev/phantomfpga0` + ioctl + mmap

**Contract:**
- App sends ioctl(SET_CONFIG) to configure
- App calls ioctl(START) to begin streaming
- App mmaps the DMA buffer to read frame data
- App calls ioctl(GET_STATS) to check progress
- App calls ioctl(CONSUME_FRAME) to mark frames as read

**Testing approach:**
- Test each ioctl independently
- Mock the device with test driver
- Use strace to verify ioctl calls

### App ↔ Viewer

**Interface:** TCP socket on port 5000

**Protocol:**
- Viewer connects to localhost:5000
- App sends frame data (magic + sequence + payload + CRC)
- Viewer validates and displays
- On disconnect, app can serve new viewers

**Testing approach:**
- Test TCP connection with netcat
- Send test frames and verify display
- Test disconnect/reconnect

---

## Team Milestones

### Week 11 (Days 16-20)

**Driver Team:**
- [ ] Day 16: Understand PCI probe sequence
- [ ] Day 17: Implement probe function, verify device is detected
- [ ] Day 18: Implement DMA descriptor setup
- [ ] Day 19: Implement basic interrupt handler
- [ ] Day 20: Character device creation, basic ioctl support

**Milestone:** Device probes successfully; dmesg shows "PhantomFPGA detected"

**App Team:**
- [ ] Day 16: Understand ioctl API and mmap
- [ ] Day 17: Implement device opening and configuration
- [ ] Day 18: Implement frame reading via mmap
- [ ] Day 19: Implement frame validation (magic check)
- [ ] Day 20: Test with driver reading frames

**Milestone:** App opens device, reads and prints frame #0

**Viewer Team:**
- [ ] Day 16: Understand TCP client programming
- [ ] Day 17: Implement TCP connection logic
- [ ] Day 18: Implement frame reception
- [ ] Day 19: Implement basic terminal output
- [ ] Day 20: Test connection to mock app

**Milestone:** Viewer connects to TCP server and receives test frames

### Week 12 (Days 21-27)

**Integrated Team:**
- [ ] Day 21: Full integration test (driver + app + viewer)
- [ ] Day 22-23: Error handling and edge cases
- [ ] Day 24: Performance optimization
- [ ] Day 25: Documentation
- [ ] Day 26: Final testing and bug fixes
- [ ] Day 27: Presentation preparation

**Milestone:** System stable, all features working, presentation ready

### Week 13 (Days 28-30)

- [ ] Day 28: Final polish and testing
- [ ] Day 29: Documentation completion
- [ ] Day 30: Final presentations

---

## Questions for the Team

Before you start coding, discuss:

1. **How will you handle a frame CRC error?** Skip and continue? Stop streaming? Log and continue?
2. **What if the device sends frames faster than your viewer can display?** Queue them? Drop oldest? Slow down transmission?
3. **How will you test the driver independently?** Write a test program? Use QEMU gdb?
4. **Who owns the frame buffer memory?** Kernel or userspace? How is it shared?
5. **What's your error logging strategy?** stderr? syslog? File?

---

## Sign-Off

**Team Name:** _________________________

**Team Members (sign below):**

- _______________________________ (Date: _______)
- _______________________________ (Date: _______)
- _______________________________ (Date: _______)
- _______________________________ (Date: _______)

**Instructor Review:**

- _______________________________ (Approved on: _______)

---

## Notes from Instructor Feedback

[Will be filled in after instructor review]

---

*Use this document to guide your design discussions. Update it as your understanding evolves. Refer back to it during integration to ensure components match the plan.*
