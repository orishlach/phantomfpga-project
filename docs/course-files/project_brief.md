# PhantomFPGA Final Project Brief

**Project Duration:** 3 weeks (Weeks 11-13)
**Team Size:** 5-6 people (3-4 per team recommended)
**Scope:** Complete software stack for a virtual FPGA streaming device
**Success Criteria:** Kernel driver + userspace app + viewer that streams frames correctly

---

## Executive Summary

You will complete the PhantomFPGA software stack, a system that demonstrates:
- **Kernel driver development:** PCIe device handling, DMA, interrupts, character devices
- **Userspace application:** Device interaction, protocol handling, network streaming
- **Full-stack integration:** Hardware → kernel → userspace → network → UI

The skeleton code is 70-80% complete with detailed TODOs. Your job is to fill in the missing pieces, integrate them, and ship a working system.

---

## The Device

**PhantomFPGA** is a virtual PCIe device (running in QEMU) that:
- Has 250 frames of data, each 5120 bytes
- Transmits frames at 25 fps via scatter-gather DMA
- Raises interrupts when frames are complete
- Exposes 4KB of memory-mapped registers (BAR0)
- Uses three MSI-X interrupt vectors (complete, error, no-descriptor)

**What is it hiding?** The frames contain a hidden message. You'll find out when your driver works.

---

## Deliverables

### Minimum Viable Product (REQUIRED)

Your project MUST deliver these six items:

1. **Kernel Driver** (`driver/phantomfpga_drv.c`)
   - Probes and initializes the device
   - Sets up descriptor-based DMA
   - Handles interrupts (frame completion, errors)
   - Creates `/dev/phantomfpga0` character device
   - Implements ioctl commands for configuration
   - Implements mmap to userspace for frame buffers

2. **Userspace Application** (`app/phantomfpga_app_impl.cpp`)
   - Opens `/dev/phantomfpga0`
   - Configures the driver via ioctl
   - Allocates and prepares DMA buffers
   - Receives and validates frames (magic, CRC-32)
   - Starts transmission and streams frames over TCP
   - Handles errors gracefully (no crashes, clear messages)

3. **Terminal Viewer** (`viewer/phantomfpga_view_impl.cpp`)
   - TCP client connecting to application (default: localhost:5000)
   - Receives frame data
   - Validates CRC-32
   - Displays frames in terminal or records to disk
   - Handles network errors (connection drops, timeouts)

4. **Build System**
   - Kbuild/Makefile flow from the PhantomFPGA skeleton (`driver/`, `app/`, `viewer/`)
   - Build/verification on host + QEMU VM workflow
   - No external dependencies beyond Linux kernel headers
   - Clean build, no warnings

5. **Git Repository**
   - Proper branching (main/develop/feature/*)
   - Clear commit messages
   - Documentation in README.md
   - Code organized by component (driver/, app/, viewer/)

6. **Documentation**
   - README.md with architecture overview
   - Build and run instructions
   - List of implemented features and known limitations
   - Any assumptions or design decisions

### Stretch Goals (IF TIME PERMITS)

These are nice-to-haves that can earn bonus points:

7. **Comprehensive Error Handling**
   - Device disconnection handling
   - DMA error recovery
   - Frame drop detection and logging
   - Graceful shutdown sequence

8. **Performance Optimization**
   - Interrupt coalescing (batch completions)
   - Zero-copy frame streaming (if possible)
   - Reduced CPU usage

9. **Testing & Validation**
   - Unit tests with GoogleTest (frame validation, CRC)
   - Fault injection testing (corrupt frames, skip sequences)
   - Stress test (run for hours without crashing)

10. **Documentation & Diagrams**
    - UML component diagram
    - Sequence diagram for frame transmission
    - State machine for driver states
    - Detailed architecture documentation

11. **Advanced Features**
    - TCP command channel for runtime configuration
    - Statistics dashboard
    - Real-time frame rate monitoring
    - Frame filtering/searching

12. **CI/CD Pipeline**
    - GitHub Actions or Azure Pipelines
    - Auto-build on push, auto-run tests
    - Coverage reporting

---

## Evaluation Rubric

| Criterion | Weight | Excellent (90-100%) | Good (70-89%) | Acceptable (50-69%) | Unsatisfactory (<50%) |
|-----------|--------|---|---|---|---|
| **Correctness (40%)** | 40% | Device works perfectly; all features complete | Minor bugs; mostly working | Major bugs; core functionality works | Doesn't build or run |
| **Code Quality (30%)** | 30% | Clean, well-organized, excellent error handling | Good structure, adequate error handling | Messy, minimal error handling | Unreadable, dangerous |
| **Documentation (20%)** | 20% | Clear README, detailed architecture, inline comments | Adequate README, basic architecture | Minimal docs, unclear | No documentation |
| **Responsiveness (10%)** | 10% | App never hangs; all operations complete quickly | Occasional slowness, generally responsive | Noticeable delays, but not breaking | Frequent hangs, crashes |

---

## Project Phases

### Phase 1: Skeleton Exploration (Days 16-19, Week 11)

**Driver Team:**
- Read and understand the skeleton code
- Understand the PCI probe sequence (in provided guide)
- Implement the probe function to initialize the device
- Milestone: `dmesg` shows "PhantomFPGA device detected" when driver loads

**App Team:**
- Understand the device API (ioctl commands)
- Implement device opening and configuration
- Understand DMA descriptor structure
- Milestone: App can open device and read first frame (print to stdout)

**Viewer Team:**
- Understand TCP client networking
- Implement connection to app TCP server
- Milestone: Viewer connects and prints "Connected to server"

### Phase 2: Integration (Days 20-24, Week 12)

**Driver Team:**
- Implement ISR handlers for interrupts
- Implement mmap for frame buffers
- Test with app connecting and reading frames

**App Team:**
- Implement TCP server for streaming frames
- Integrate frame validation (magic, CRC-32)
- Test with viewer connecting and receiving frames

**Viewer Team:**
- Implement frame display logic
- Implement CRC-32 validation
- Test complete pipeline

**Cross-Team:**
- Daily integration testing
- Fix protocol mismatches
- Performance testing

### Phase 3: Polish & Testing (Days 25-27, Week 13)

**All Teams:**
- Bug fixes based on integration issues
- Performance optimization
- Documentation completion
- Final system testing

**Friday, Week 13: Final Presentations**
- Demo working system
- Code review with instructors
- Presentation of architecture and decisions

---

## Technical Requirements

### Kernel Driver

**Must implement:**
- PCI device discovery (probe/remove functions)
- BAR0 mapping for register access
- Descriptor-based scatter-gather DMA
- MSI-X interrupt handling (3 vectors: complete, error, no-descriptor)
- Character device interface (`/dev/phantomfpga0`)
- Ioctl commands: SET_CFG, GET_CFG, START, STOP, GET_STATS, CONSUME_FRAME
- mmap support for DMA buffer access

**Architecture hints:**
- See provided skeleton in `driver/phantomfpga_drv.c` (with TODOs)
- Reference: `docs/driver-guide.md` (step-by-step implementation guide)
- Reference: `docs/phantomfpga-datasheet.md` (register map)

### Userspace Application

**Must implement:**
- Device discovery and opening
- Configuration via ioctl
- Mmap DMA buffer for frame reading
- Frame header validation (magic = 0xF00DFACE)
- CRC-32 validation (IEEE 802.3)
- TCP server listening on port 5000
- Frame transmission over TCP
- Graceful error handling

**Architecture hints:**
- See skeleton in `app/phantomfpga_app_impl.cpp`
- Reference: `docs/architecture.md` (system overview)

### Terminal Viewer

**Must implement:**
- TCP client connecting to app
- Frame reception and parsing
- CRC-32 validation
- Terminal output or file recording
- Error handling

**Display requirements:**
- Minimum terminal size: 110 columns × 45 rows
- ASCII art representation or hex dump
- Real-time frame rate display

---

## Tools & Environment

### Build System
- **Host compiler:** gcc/clang
- **Cross-compiler:** aarch64-linux-gnu-gcc (for ARM target)
- **Build tool:** Make / Kbuild (project-provided Makefiles)
- **Language:** C (driver), C++ (app, viewer)

### Testing Environment
- **QEMU ARM64 system** with PhantomFPGA device emulation
- **SSH access:** `ssh -p 2222 root@localhost` (password: root)
- **Deployment:** `scp` files to QEMU for testing

### Code Organization
```
your-team-repo/
├── driver/
│   ├── phantomfpga_drv.c
│   ├── phantomfpga_regs.h
│   ├── phantomfpga_uapi.h
│   └── Makefile
├── app/
│   ├── phantomfpga_app.cpp
│   ├── phantomfpga_app.h
│   ├── phantomfpga_app_impl.cpp
│   └── Makefile
├── viewer/
│   ├── phantomfpga_view.h
│   ├── phantomfpga_view.cpp
│   ├── phantomfpga_view_impl.cpp
│   └── Makefile
├── docs/
│   ├── architecture.md
│   ├── driver-guide.md
│   └── phantomfpga-datasheet.md
└── README.md
```

---

## Known Constraints

- **QEMU emulation:** Timing is not cycle-accurate; don't expect microsecond-level precision
- **DMA descriptors:** Limited to 256 descriptors in default config (can be increased)
- **Frame rate:** Default 25 fps (can be configured down to 1 fps if debugging)
- **No real hardware:** All testing is in QEMU; real hardware has different constraints
- **Fault injection:** Available for testing error handling

---

## Success Stories

### Day 1 Realistic Milestone
- Driver loads and probes device
- App connects and reads frame #0
- No crashes

### Day 5 Realistic Milestone
- Driver handles interrupt completion
- App receives multiple frames
- Viewer connects over TCP

### Day 10 Realistic Milestone
- Full pipeline working (device → app → viewer)
- Frames streaming at target rate
- Error handling in place

### Day 15 Realistic Milestone
- System stable for hours
- All features implemented
- Documentation complete
- Ready for presentation

---

## Frequently Asked Questions

**Q: How much skeleton code is provided?**
A: 70-80%. The structure is there; you fill in the critical TODOs.

**Q: Will the QEMU device always work?**
A: Yes. It's simulated; no hardware issues.

**Q: What if my team finishes early?**
A: Stretch goals await. See list above. Or help other teams.

**Q: What if someone on my team is stuck?**
A: Office hours, code review, ask peers. But also: struggle is how we learn. Push through.

**Q: Can we use existing libraries?**
A: For C++: yes (STL, standard Boost). For driver: no (kernel APIs only). Ask instructor if unsure.

**Q: What's the hidden message?**
A: You'll know when your viewer works. Don't spoil it; let teams discover it.

---

## Resources

1. **PhantomFPGA Documentation**
   - `docs/phantomfpga-datasheet.md` - Register reference
   - `docs/architecture.md` - System architecture
   - `docs/driver-guide.md` - Driver implementation guide
   - `docs/glossary.md` - PCIe/DMA terminology

2. **Skeleton Code**
   - `driver/phantomfpga_drv.c` - With detailed TODOs
   - `app/phantomfpga_app_impl.cpp` - With detailed TODOs
   - `viewer/phantomfpga_view_impl.cpp` - With detailed TODOs

3. **External Resources**
   - Linux Kernel Driver Development (search online)
   - PCIe specification (free PDFs available)
   - DMA and scatter-gather documentation
   - CRC-32 implementation (provided)

---

## Timeline

| Week | Focus | Deliverable |
|------|-------|-------------|
| **Week 11** | Driver probe, app device access, viewer connection | Device probes; app reads frames; viewer connects |
| **Week 12** | Integration, error handling, performance | Full pipeline working; error recovery in place |
| **Week 13** | Polish, testing, documentation | Stable system; presentation-ready |

---

## Contact

- **Instructor:** Leon Vak, CTO Embedded, abra R&D Solutions
- **Office Hours:** [Schedule posted on Day 15]
- **Slack Channel:** #phantomfpga-project
- **Code Review:** Pull request to main require instructor approval

---

*"Three weeks of training got you here. Three more weeks of execution will get you across the finish line. You've got the skills, the resources, and the team. Go build it."*
