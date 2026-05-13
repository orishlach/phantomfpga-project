# Sprint Planning Template

**Team Name:** [Team Name]
**Sprint:** [Week 11 / Week 12 / Week 13]
**Duration:** 5 days (Monday - Friday)
**Team Velocity:** [hours per person per day, estimate: 6-7]

---

## Overview

Fill in this template collaboratively with your team. For each task:
- **What:** Clear description of what's being implemented
- **Who:** Team member responsible
- **Effort:** Estimated hours
- **Dependencies:** What must be done first
- **Acceptance Criteria:** How we know it's done

---

## Week 11 Sprint (Days 16-20)

### Driver Team Tasks

#### Task 1: PCI Device Probe Function
**Description:** Implement the pci_driver probe function. When module loads, device should be detected and initialized.

**Owner:** [Driver Lead / Team Member A]
**Effort:** 8-16 hours (2-4 days)
**Dependencies:** None
**Acceptance Criteria:**
- [ ] Device probes on module load
- [ ] `dmesg | grep PhantomFPGA` shows "device detected"
- [ ] BAR0 is successfully mapped
- [ ] No kernel oops/panic

**Implementation Notes:**
- Use `pci_enable_device()` and `pci_request_region()`
- Use `pci_iomap()` for BAR0 mapping
- Initialize driver state structure
- Set up PCIe bus mastering

---

#### Task 2: DMA Memory Allocation
**Description:** Allocate coherent DMA memory for descriptor ring and frame buffers. Verify allocation via dmesg logging.

**Owner:** [Driver Team Member B]
**Effort:** 6-12 hours (1-2 days)
**Dependencies:** Task 1 (probe complete)
**Acceptance Criteria:**
- [ ] Descriptor ring allocated (256 descriptors × 32 bytes)
- [ ] Frame buffers allocated (256 × 5136 bytes)
- [ ] Physical addresses logged
- [ ] Memory is coherent (DMA-safe)

**Implementation Notes:**
- Use `dma_alloc_coherent()` for allocations
- Store both virtual and physical addresses
- Verify with `dmesg` logging of addresses

---

#### Task 3: MSI-X Interrupt Setup
**Description:** Initialize MSI-X interrupts (3 vectors) and register handlers.

**Owner:** [Driver Team Member C]
**Effort:** 8-12 hours (2 days)
**Dependencies:** Task 1 (probe complete)
**Acceptance Criteria:**
- [ ] MSI-X allocated and enabled
- [ ] 3 interrupt handlers registered (complete, error, no-desc)
- [ ] Handlers print to dmesg on interrupt
- [ ] No interrupt storms

**Implementation Notes:**
- Use `pci_alloc_irq_vectors()` with 3 vectors
- Use `request_irq()` for each handler
- Use `spin_lock_irqsave` in handlers
- Remember: handlers run in atomic context

---

#### Task 4: Descriptor Ring Setup
**Description:** Program the device's descriptor ring address, size, and head/tail indices.

**Owner:** [Driver Team Member A or B]
**Effort:** 6-10 hours (1-2 days)
**Dependencies:** Tasks 1-2 (probe and DMA)
**Acceptance Criteria:**
- [ ] DESC_RING_LO/HI written correctly
- [ ] DESC_RING_SIZE set to 256
- [ ] Descriptor indices readable from device registers
- [ ] No register access errors

---

#### Task 5: Character Device Creation
**Description:** Create /dev/phantomfpga0 character device for userspace access.

**Owner:** [Driver Team Member C]
**Effort:** 4-8 hours (1 day)
**Dependencies:** Task 1 (probe complete)
**Acceptance Criteria:**
- [ ] `/dev/phantomfpga0` appears after module load
- [ ] File can be opened with `open()`
- [ ] Basic ioctl commands work (GET_CONFIG)

**Implementation Notes:**
- Use `cdev_add()` and `device_create()`
- Implement file_operations: open, release, ioctl, mmap (stub for now)
- Use proper error checking and cleanup

---

#### Task 6: Driver Integration Testing
**Description:** Test driver independently before app integration. Write basic test program.

**Owner:** [Whole Driver Team]
**Effort:** 4-6 hours (distributed across team)
**Dependencies:** All driver tasks
**Acceptance Criteria:**
- [ ] Test program opens /dev/phantomfpga0
- [ ] Test program reads device registers via ioctl
- [ ] No crashes or kernel warnings

---

### App Team Tasks

#### Task 1: Device Discovery and Opening
**Description:** Open /dev/phantomfpga0 and verify it exists. Handle errors gracefully.

**Owner:** [App Lead / Team Member A]
**Effort:** 4-6 hours (1 day)
**Dependencies:** Driver Task 5 (char device)
**Acceptance Criteria:**
- [ ] App opens device successfully
- [ ] App exits with clear message if device not found
- [ ] File descriptor properly closed on exit

---

#### Task 2: Device Configuration via ioctl
**Description:** Implement ioctl calls to configure the device (frame rate, buffer setup).

**Owner:** [App Team Member B]
**Effort:** 6-8 hours (1-2 days)
**Dependencies:** Task 1, Driver integration
**Acceptance Criteria:**
- [ ] App sends ioctl(SET_CONFIG) successfully
- [ ] App reads configuration back with ioctl(GET_CONFIG)
- [ ] Device responds with valid data

---

#### Task 3: Frame Reading via mmap
**Description:** Memory-map the DMA buffer. Read frame data from mapped region.

**Owner:** [App Team Member C]
**Effort:** 6-10 hours (1-2 days)
**Dependencies:** Task 1, Driver Task 2
**Acceptance Criteria:**
- [ ] mmap() succeeds and returns valid pointer
- [ ] Frame data readable from mapped address
- [ ] Frame #0 data matches expectations
- [ ] No segfaults on access

**Implementation Notes:**
- Use `mmap()` on /dev/phantomfpga0
- Frames start at offset 0 in mapped region
- Each frame is 5120 bytes
- Verify first few bytes match expected format

---

#### Task 4: Frame Validation (Magic + CRC)
**Description:** Validate frame integrity (magic number, CRC-32 checksum).

**Owner:** [App Lead / Team Member A]
**Effort:** 4-8 hours (1-2 days)
**Dependencies:** Task 3
**Acceptance Criteria:**
- [ ] Frame magic (0xF00DFACE) verified
- [ ] CRC-32 validation working (IEEE 802.3 polynomial)
- [ ] Invalid frames detected and logged
- [ ] Valid frames processed correctly

**Implementation Notes:**
- First 4 bytes of frame: magic = 0xF00DFACE
- Next 4 bytes: sequence number (0-249)
- Last 4 bytes: CRC-32 of all prior bytes
- Use provided CRC-32 implementation or zlib

---

#### Task 5: Starting Device Transmission
**Description:** Trigger device to start streaming frames.

**Owner:** [App Team Member B]
**Effort:** 4-6 hours (1 day)
**Dependencies:** Task 2, Driver Task 4
**Acceptance Criteria:**
- [ ] App sends ioctl(START) successfully
- [ ] Device CTRL.RUN bit is set (verify via register read)
- [ ] Device begins generating frames
- [ ] App receives first few frames

---

#### Task 6: Basic Output and Testing
**Description:** Print received frames to stdout for testing. Prepare for TCP integration.

**Owner:** [App Team Member C]
**Effort:** 4-6 hours (1 day)
**Dependencies:** All app tasks
**Acceptance Criteria:**
- [ ] App prints received frames in readable format
- [ ] First 10 frames show correct sequence (0-9)
- [ ] No crashes or hangs

---

### Viewer Team Tasks

#### Task 1: TCP Client Connection
**Description:** Implement TCP client that connects to localhost:5000.

**Owner:** [Viewer Lead / Team Member A]
**Effort:** 4-6 hours (1 day)
**Dependencies:** None (can be developed independently)
**Acceptance Criteria:**
- [ ] Connects to server successfully
- [ ] Handles connection failure gracefully
- [ ] Prints connection status to stdout

**Implementation Notes:**
- Use BSD socket API (socket, connect)
- Use localhost:5000 (configurable)
- Handle SIGPIPE for broken connections

---

#### Task 2: Frame Reception
**Description:** Receive frame data over TCP. Parse magic, sequence, CRC.

**Owner:** [Viewer Team Member B]
**Effort:** 6-8 hours (1-2 days)
**Dependencies:** Task 1, App Task 4
**Acceptance Criteria:**
- [ ] Frames received over TCP
- [ ] Magic number verified
- [ ] Sequence number tracked
- [ ] Correct number of bytes received (5120)

---

#### Task 3: CRC-32 Validation
**Description:** Validate CRC-32 for received frames. Log errors.

**Owner:** [Viewer Team Member C]
**Effort:** 4-6 hours (1 day)
**Dependencies:** Task 2
**Acceptance Criteria:**
- [ ] CRC validation matches app validation
- [ ] Invalid frames logged and skipped
- [ ] No false positives/negatives

---

#### Task 4: Terminal Display
**Description:** Display frames in terminal (ASCII art, hex dump, or raw numbers).

**Owner:** [Viewer Lead / Team Member A]
**Effort:** 6-10 hours (1-2 days)
**Dependencies:** Task 2
**Acceptance Criteria:**
- [ ] Frames displayed in readable format
- [ ] Terminal cursor managed (no flicker)
- [ ] Works on 110×45+ terminals

**Stretch:** ASCII art representation of frame data

---

#### Task 5: Error Handling
**Description:** Handle connection drops, malformed data, timeouts.

**Owner:** [Viewer Team Member B]
**Effort:** 4-6 hours (1 day)
**Dependencies:** All viewer tasks
**Acceptance Criteria:**
- [ ] Connection drops handled gracefully
- [ ] Malformed frames logged and skipped
- [ ] Timeout handling (retry connection)

---

#### Task 6: Integration Testing with Mock App
**Description:** Test viewer against mock TCP server (netcat or simple script).

**Owner:** [Whole Viewer Team]
**Effort:** 4-6 hours (distributed)
**Dependencies:** All viewer tasks
**Acceptance Criteria:**
- [ ] Viewer receives and displays test frames
- [ ] Works with simple TCP echo server
- [ ] Ready for real app integration

---

## Week 12 Sprint (Days 21-27)

### Integrated Sprint: Full Pipeline

**Major Milestone:** Device → Kernel → App → Viewer, end-to-end working

#### Integration Task 1: Full System Test
- [ ] Load driver
- [ ] Run app
- [ ] Connect viewer
- [ ] Receive and display frames
- [ ] Verify frame #0 through #249, then loop
- [ ] Run for >60 seconds without crashes

#### Integration Task 2: Error Scenarios
- [ ] Device disconnection
- [ ] CRC validation failure
- [ ] TCP connection drops
- [ ] Frame drops / backpressure

#### Integration Task 3: Performance & Stability
- [ ] Frame rate meets spec (25 fps)
- [ ] No CPU hangs
- [ ] Memory usage stable
- [ ] Interrupt handling is responsive

#### Documentation Task
- [ ] Architecture diagram
- [ ] Build instructions
- [ ] API documentation
- [ ] Known limitations

---

## Velocity and Burndown

**Team Velocity Estimate:** [hours/day]
- Driver Team: ~18 hours/day (3 people × 6 hours)
- App Team: ~18 hours/day (3 people × 6 hours)
- Viewer Team: ~18 hours/day (3 people × 6 hours)

**Sprint Capacity:** 18 × 5 = 90 hours per team

**Task Allocation:** Ensure tasks fit within capacity

---

## Risk Mitigation

**Risk:** Driver development is slow (common in kernel code)
**Mitigation:** Start with provided skeleton; focus on TODOs; ask for help early

**Risk:** DMA memory alignment issues
**Mitigation:** Use provided dma_alloc_coherent; it handles alignment

**Risk:** Interrupt handling crashes kernel
**Mitigation:** Test with printk statements; use QEMU with -g 1234 for gdb debugging

**Risk:** App and driver interfaces don't match
**Mitigation:** Define ioctl structures in shared header; test early and often

**Risk:** TCP networking has timeout issues
**Mitigation:** Use provided TCP server skeleton; test with netcat first

---

## Communication Plan

**Daily Standup:** 15 min (same time each day)
- What did you do yesterday?
- What will you do today?
- Any blockers?

**Integration Meetings:** Mon/Wed/Fri (30 min)
- Check cross-team dependencies
- Demo progress
- Adjust plan if needed

**Slack Channel:** #phantomfpga-project
- Quick questions
- Code review links
- Status updates

---

## Success Criteria for Sprint

### Week 11 (Skeleton Complete)
- [ ] Driver loads and probes device
- [ ] App opens device and reads frame #0
- [ ] Viewer connects to mock TCP server
- [ ] Each component works independently

### Week 12 (Integration Complete)
- [ ] Full pipeline working (device → app → viewer)
- [ ] Frames stream continuously for >60 seconds
- [ ] Error cases handled gracefully
- [ ] Documentation started

### Week 13 (Polish Complete)
- [ ] System stable for hours
- [ ] All features documented
- [ ] Presentation ready

---

## Notes

[To be filled in during standup meetings]

---

**Approved by:** _________________________ (Instructor)
**Date:** _________________________

---

*Update this document daily during standups. It's your roadmap to success.*
