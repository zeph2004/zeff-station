# Zeff Station: 1:1 Low-Level Game Boy Advance Emulator in C

```
 ███████╗███████╗███████╗███████╗    ███████╗████████╗ █████╗ ████████╗██╗ ██████╗ ███╗   ██╗
 ╚══███╔╝██╔════╝██╔════╝██╔════╝    ██╔════╝╚══██╔══╝██╔══██╗╚══██╔══╝██║██╔═══██╗████╗  ██║
   ███╔╝ █████╗  █████╗  █████╗      ███████╗   ██║   ███████║   ██║   ██║██║   ██║██╔██╗ ██║
  ███╔╝  ██╔══╝  ██╔══╝  ██╔══╝      ╚════██║   ██║   ██╔══██║   ██║   ██║██║   ██║██║╚██╗██║
 ███████╗███████╗██║     ██║         ███████║   ██║   ██║  ██║   ██║   ██║╚██████╔╝██║ ╚████║
 ╚══════╝╚══════╝╚═╝     ╚═╝         ╚══════╝   ╚═╝   ╚═╝  ╚═╝   ╚═╝   ╚═╝ ╚═════╝ ╚═╝  ╚═══╝
                      G B A   E M U L A T I O N   S Y S T E M
```

**Zeff Station** is a 1:1, cycle-stepped Game Boy Advance (GBA) emulator written in C from first principles. Built to run natively on macOS with zero external third-party dependencies, Zeff Station models the complete internal hardware architecture of the classic 32-bit handheld system—featuring custom **Zeff Station** branding, an animated retro boot sequence, a synthesized harmonic boot chime, and high-performance Cocoa + CoreAudio frontend integration.

---

## Table of Contents

1. [Architectural Overview](#architectural-overview)
2. [Processor Core (ARM7TDMI)](#processor-core-arm7tdmi)
   - [3-Stage Instruction Pipeline](#3-stage-instruction-pipeline)
   - [Register Banking & CPU Modes](#register-banking--cpu-modes)
   - [Barrel Shifter & ALU Flag Logic](#barrel-shifter--alu-flag-logic)
   - [Dual Instruction Set Decoding (ARM & THUMB)](#dual-instruction-set-decoding-arm--thumb)
3. [Memory Bus & Addressing Subsystem](#memory-bus--addressing-subsystem)
   - [Memory Map & Mirroring](#memory-map--mirroring)
   - [Unaligned Memory Access Mechanics](#unaligned-memory-access-mechanics)
   - [Hardware Bus Quirks & Open Bus](#hardware-bus-quirks--open-bus)
   - [Cartridge Backup Persistence (SRAM & Serial EEPROM)](#cartridge-backup-persistence-sram--serial-eeprom)
   - [Game Pak ROM Mirroring & Waitstates](#game-pak-rom-mirroring--waitstates)
4. [Picture Processing Unit (PPU)](#picture-processing-unit-ppu)
   - [Scanline Timing & Refresh Cycle](#scanline-timing--refresh-cycle)
   - [Video Modes (Modes 0 through 5)](#video-modes-modes-0-through-5)
   - [Affine Transformation Mathematics](#affine-transformation-mathematics)
   - [Sprite Engine (OAM) & Priority Sorting](#sprite-engine-oam--priority-sorting)
   - [Color Special Effects, Blending, & Windowing](#color-special-effects-blending--windowing)
5. [Direct Memory Access (DMA)](#direct-memory-access-dma)
6. [Timers & Interrupt Controller](#timers--interrupt-controller)
   - [Low-Level IRQ Vector & User Handler Trampoline](#low-level-irq-vector--user-handler-trampoline)
7. [Audio Processing Unit (APU)](#audio-processing-unit-apu)
8. [High-Level BIOS (HLE) & Zeff Station Boot Sequence](#high-level-bios-hle--zeff-station-boot-sequence)
9. [Host Frontend (macOS Cocoa & CoreAudio)](#host-frontend-macos-cocoa--coreaudio)
10. [Hardware Quirks & Commercial Title Case Studies](#hardware-quirks--commercial-title-case-studies)
    - [Super Mario Advance 2: Compositor Priority Occlusion](#super-mario-advance-2-compositor-priority-occlusion)
    - [Dragon Ball - Advanced Adventure: IRQ Return, Pipeline Refills, & VRAM Bus Quirks](#dragon-ball---advanced-adventure-irq-return-pipeline-refills--vram-bus-quirks)
11. [Building, Running, & Testing](#building-running--testing)
12. [Controls & Command Line Options](#controls--command-line-options)

---

## Architectural Overview

The Game Boy Advance hardware operates synchronously around a **16.777216 MHz** system clock:

```
┌────────────────────────────────────────────────────────────────────────┐
│                        ZEFF STATION SYSTEM BUS                         │
│                                                                        │
│   ┌───────────────┐     ┌───────────────┐     ┌────────────────────┐   │
│   │   ARM7TDMI    │     │   4-CHANNEL   │     │    INTERRUPTS &    │   │
│   │ 32-Bit CPU    │     │      DMA      │     │      TIMERS        │   │
│   │ (ARM / THUMB) │     │  CONTROLLER   │     │ (0-3 + Cascading)  │   │
│   └───────▲───────┘     └───────▲───────┘     └─────────▲──────────┘   │
│           │                     │                       │              │
│   ════════╪═════════════════════╪═══════════════════════╪══════════    │
│           │                     │                       │              │
│   ┌───────▼───────┐     ┌───────▼───────┐     ┌─────────▼──────────┐   │
│   │   PPU (GFX)   │     │   APU (AUDIO) │     │     CARTRIDGE      │   │
│   │  Modes 0 - 5  │     │  DMG 1-4 +    │     │  ROM (WS0, 1, 2)   │   │
│   │ 128 Sprites   │     │  DirectSound  │     │    SRAM / Flash    │   │
│   └───────────────┘     └───────────────┘     └────────────────────┘   │
└────────────────────────────────────────────────────────────────────────┘
```

- **CPU**: 32-bit ARM7TDMI core running at 16.78 MHz with 3-stage pipeline.
- **Memory**: 16 KB BIOS, 256 KB On-Board EWRAM, 32 KB On-Chip IWRAM, 1 KB Palette RAM, 96 KB VRAM, 1 KB OAM, and up to 32 MB ROM.
- **Display**: 240×160 resolution @ 59.7275 Hz (1232 cycles per scanline, 228 scanlines per frame).
- **Audio**: Dual DirectSound FIFO 8-bit PCM channels + 4 legacy DMG sound channels (2 pulse channels, 1 wave table, 1 noise).
- **Direct Memory Access (DMA)**: 4 channels with priority arbitration and auto-reload capabilities.
- **Timers**: 4 hardware timers with prescalers (/1, /64, /256, /1024) and cascade chaining.

---

## Processor Core (ARM7TDMI)

The ARM7TDMI processor in Zeff Station is implemented in `src/arm7tdmi.c`, `src/arm_instructions.c`, and `src/thumb_instructions.c`. It supports the complete **ARMv4T** architecture across both 32-bit ARM and 16-bit THUMB instruction sets.

### 3-Stage Instruction Pipeline

The physical ARM7TDMI processor employs a 3-stage pipeline:
1. **Fetch**: The instruction is retrieved from memory at the current PC address.
2. **Decode**: The instruction bitfields are decoded into control signals.
3. **Execute**: The instruction operands are read, ALU/multiplier computed, and results written.

Because instructions move through three stages simultaneously:
- When an instruction at address $A$ is in the **Execute** stage, the instruction at $A+4$ is in **Decode**, and the instruction at $A+8$ is in **Fetch**.
- Therefore, in **ARM state**, reading register `R15` yields $A + 8$.
- In **THUMB state**, instructions are 2 bytes wide, so reading `R15` yields $A + 4$.

```
Address   Pipeline Stage   R15 Value
A         Execute  ◄────── Reads (A + 8) in ARM / (A + 4) in THUMB
A + 4     Decode
A + 8     Fetch
```

Zeff Station models this behavior with two pipeline prefetch slots (`pipeline[0]` and `pipeline[1]`) and an explicit `cpu->branched` flag. When a branch or exception occurs, `arm7tdmi_fill_pipeline_arm` or `arm7tdmi_fill_pipeline_thumb` flushes and refills the pipeline from the new target address.

### Register Banking & CPU Modes

The CPU operates in one of 7 hardware modes:

| Mode | Bits `M[4:0]` | Type | Banked Registers |
| :--- | :--- | :--- | :--- |
| **User (USR)** | `0x10` | Non-privileged | Shared with System |
| **FIQ** | `0x11` | Privileged | `R8_fiq`–`R14_fiq`, `SPSR_fiq` |
| **IRQ** | `0x12` | Privileged | `R13_irq`, `R14_irq`, `SPSR_irq` |
| **Supervisor (SVC)** | `0x13` | Privileged | `R13_svc`, `R14_svc`, `SPSR_svc` |
| **Abort (ABT)** | `0x17` | Privileged | `R13_abt`, `R14_abt`, `SPSR_abt` |
| **Undefined (UND)** | `0x1B` | Privileged | `R13_und`, `R14_und`, `SPSR_und` |
| **System (SYS)** | `0x1F` | Privileged | Shares USR registers, no SPSR |

When switching modes, `arm7tdmi_switch_mode()` saves the current mode's registers into dedicated backup fields and swaps in the target mode's banked stack pointer (`R13`), link register (`R14`), and SPSR.

### Barrel Shifter & ALU Flag Logic

All data processing instructions can pass their second operand through the hardware barrel shifter before the ALU computes the final result:

- **LSL (Logical Shift Left)**: Zeros shifted in at LSB.
- **LSR (Logical Shift Right)**: Zeros shifted in at MSB. Immediate `#0` encodes shift by 32.
- **ASR (Arithmetic Shift Right)**: Sign bit replicated into MSB. Immediate `#0` encodes shift by 32.
- **ROR (Rotate Right)**: Bits shifted off at LSB wrap into MSB.
- **RRX (Rotate Right with Extend)**: 33-bit rotation using the previous Carry flag value.

#### Flag Calculations

- **Negative ($N$)**: Set to bit 31 of the result.
- **Zero ($Z$)**: Set if the 32-bit result is exactly `0`.
- **Carry ($C$)**:
  - For Addition (`ADD`, `ADC`): Set if unsigned addition overflowed 32 bits ($A + B + C_{in} > 2^{32}-1$).
  - For Subtraction (`SUB`, `SBC`, `CMP`): Set if unsigned subtraction did **not** borrow ($A \ge B + \text{borrow}$).
  - For Shifts: Carries the last bit shifted out of the shifter.
- **Overflow ($V$)**:
  - Set for signed addition if two operands of identical sign yield a result of opposing sign:
    $$V = (\sim(A \oplus B) \ \& \ (A \oplus \text{Result})) \ \& \ 0x80000000$$
  - Set for signed subtraction if operands of opposing sign yield a sign reversal:
    $$V = ((A \oplus B) \ \& \ (A \oplus \text{Result})) \ \& \ 0x80000000$$

### Dual Instruction Set Decoding (ARM & THUMB)

- **ARM Mode (32-bit)**:
  - Instructions are checked against the 4 condition bits `[31:28]` (`EQ`, `NE`, `CS`, `CC`, `MI`, `PL`, `VS`, `VC`, `HI`, `LS`, `GE`, `LT`, `GT`, `LE`, `AL`).
  - Bitwise dispatch handles: Branch & Exchange (`BX`), Branch with Link (`B`/`BL`), Single Data Transfer (`LDR`/`STR`), Halfword/Signed Transfer (`LDRH`/`STRH`/`LDRSB`/`LDRSH`), Block Data Transfer (`LDM`/`STM`), Multiply/Multiply-Accumulate (`MUL`/`MLA`), Long Multiply (`UMULL`/`SMULL`/`UMLAL`/`SMLAL`), Swap (`SWP`), Status Register Transfer (`MRS`/`MSR`), and Software Interrupt (`SWI`).
- **THUMB Mode (16-bit)**:
  - All 19 THUMB instruction formats are decoded directly into native high-speed C routines, supporting compact code density without pipeline overhead.

---

## Memory Bus & Addressing Subsystem

The memory bus controller (`src/bus.c`) arbitrates all 8-bit, 16-bit, and 32-bit read and write operations across the 32-bit address space.

### Memory Map & Mirroring

```
0x00000000 ┌──────────────────────────────────┐ 16 KB
           │ BIOS (System ROM)                │
0x02000000 ├──────────────────────────────────┤ 256 KB (Mirrored to 0x02FFFFFF)
           │ EWRAM (On-Board Work RAM)        │
0x03000000 ├──────────────────────────────────┤ 32 KB (Mirrored to 0x03FFFFFF)
           │ IWRAM (On-Chip Work RAM)         │
0x04000000 ├──────────────────────────────────┤ 1 KB
           │ I/O Registers                    │
0x05000000 ├──────────────────────────────────┤ 1 KB (Mirrored to 0x05FFFFFF)
           │ Palette RAM (PRam)               │
0x06000000 ├──────────────────────────────────┤ 96 KB (Mirrored to 0x06FFFFFF)
           │ VRAM (Video RAM)                 │
0x07000000 ├──────────────────────────────────┤ 1 KB (Mirrored to 0x07FFFFFF)
           │ OAM (Object Attribute Memory)    │
0x08000000 ├──────────────────────────────────┤ Up to 32 MB
           │ Game Pak ROM Waitstate 0         │
0x0A000000 ├──────────────────────────────────┤ Waitstate 1
           │ Game Pak ROM Waitstate 1         │
0x0C000000 ├──────────────────────────────────┤ Waitstate 2
           │ Game Pak ROM Waitstate 2         │
0x0E000000 ├──────────────────────────────────┤ Up to 64 KB
           │ Cartridge SRAM / Backup Storage  │
0xFFFFFFFF └──────────────────────────────────┘
```

### Unaligned Memory Access Mechanics

The ARMv4T architecture does not fault on unaligned memory access:
1. **Unaligned Word Load (`LDR`)**:
   - The memory bus forces address alignment: $A_{\text{aligned}} = A \ \& \ \sim 3$.
   - The 32-bit aligned word is loaded from memory and **rotated right** by $(A \ \& \ 3) \times 8$ bits:
     $$\text{Result} = (\text{Word} \gg \text{shift}) \ | \ (\text{Word} \ll (32 - \text{shift}))$$
2. **Unaligned Halfword Load (`LDRH`)**:
   - In ARMv4T, if bit 0 is 1, the aligned halfword is read and rotated or extracted depending on sign extension (`LDRSH`).
3. **Unaligned Stores (`STR`, `STRH`)**:
   - Address bits 0 and 1 are masked off (`addr &= ~3` for 32-bit, `addr &= ~1` for 16-bit).

### Hardware Bus Quirks & Open Bus

- **Palette RAM 8-bit Writes**: The GBA palette bus is 16 bits wide. An 8-bit byte written to Palette RAM is written to **both** bytes of the addressed 16-bit halfword (`(val << 8) | val`).
- **VRAM 8-bit Writes**: An 8-bit write to Background VRAM similarly duplicates the byte into both halves of the halfword. 8-bit writes directed to Sprite/OBJ VRAM (`0x06010000`+) are ignored by the hardware.
- **OAM 8-bit Writes**: 8-bit writes to OAM are ignored; only 16-bit and 32-bit writes can update sprite attributes.
- **BIOS Protection**: Reading from the BIOS area (`0x00000000`–`0x00003FFF`) while the CPU Program Counter is outside the BIOS region returns open-bus values to prevent ROMs from dumping the proprietary bootstrap.

### Cartridge Backup Persistence (SRAM & Serial EEPROM)

Commercial GBA cartridges utilize several distinct backup storage technologies. Zeff Station implements low-level cycle-accurate hardware emulation for both parallel battery-backed SRAM and high-capacity serial EEPROM:

1. **Battery-Backed SRAM (`0x0E000000`–`0x0E00FFFF`)**:
   - 32 KB or 64 KB 8-bit non-volatile RAM.
   - Reads and writes access battery-backed memory directly.
   - Writes set `sram_dirty = true`, automatically flushed into a `.sav` file alongside the ROM.

2. **Cartridge Serial EEPROM (`0x0D000000`–`0x0DFFFFFF`)**:
   - Many titles (e.g. *Super Mario Advance 2*, *Pokémon*, *The Legend of Zelda*) communicate with an on-cartridge **4Kbit (512-byte)** or **64Kbit (8-Kbyte)** serial EEPROM chip.
   - Because the Game Pak data bus connects the serial EEPROM to **Bit 0**, communication is performed serially (1 bit per halfword access) via **DMA3** or bit-banging:
     - **4Kbit EEPROM Read Request (9 bits)**: Command `11` (2 bits) + 6-bit address (MSB first) + `0` terminator.
     - **4Kbit EEPROM Write Request (73 bits)**: Command `10` (2 bits) + 6-bit address + 64 bits data + `0` terminator.
     - **64Kbit EEPROM Read Request (17 bits)**: Command `11` (2 bits) + 14-bit address (10 bits utilized) + `0` terminator.
     - **64Kbit EEPROM Write Request (81 bits)**: Command `10` (2 bits) + 14-bit address + 64 bits data + `0` terminator.
     - **Read Response Stream (68 bits)**: Transferred via DMA3 from `0x0D000000` into IWRAM/EWRAM. The first 4 halfwords return dummy zeros; the subsequent 64 halfwords deliver the 8-byte page data bit-by-bit in Bit 0.
   - **Ready Polling**: Halfword reads from `0x0D000000` (e.g., via `LDRH`) return Bit 0 as `1` when the EEPROM programming cycle is idle/complete.
   - Save data from EEPROM is persisted into `.sav` files on cartridge flush.

### Game Pak ROM Mirroring & Waitstates

- **Waitstate Regions**: The Game Pak ROM space spans 3 waitstate regions:
  - Waitstate 0: `0x08000000`–`0x09FFFFFF` (32 MB)
  - Waitstate 1: `0x0A000000`–`0x0BFFFFFF` (32 MB)
  - Waitstate 2: `0x0C000000`–`0x0DFFFFFF` (32 MB)
- **Mirroring Formula**: Cartridge ROMs smaller than 32 MB are continuously mirrored throughout the waitstate regions:
  $$\text{ROM Offset} = (A \ \& \ \text{0x01FFFFFF}) \pmod{\text{ROM Size}}$$
  This ensures games that jump between mirrored waitstate addresses (e.g., for custom bus access timings) read identical instruction and graphic data.

---

## Picture Processing Unit (PPU)

The PPU rasterizer (`src/ppu.c`) renders the display scanline-by-scanline matching exact hardware timings.

### Scanline Timing & Refresh Cycle

| Phase | Scanlines | Cycles per Line | Subsystem Activity |
| :--- | :--- | :--- | :--- |
| **Visible Draw (VDraw)** | 0 – 159 | 960 cycles | PPU renders 240 pixels (HDraw) |
| **Horizontal Blank (HBlank)** | 0 – 159 | 272 cycles | HBlank flag set; HBlank IRQ & DMA trigger |
| **Vertical Blank (VBlank)** | 160 – 227 | 1232 cycles | VBlank flag set; VBlank IRQ & DMA trigger |

- **Total Cycles per Scanline**: $960 + 272 = 1232$ cycles.
- **Total Lines per Frame**: $160 + 68 = 228$ lines.
- **Total Cycles per Frame**: $1232 \times 228 = 280,896$ cycles.
- **Frame Rate**: $\frac{16,777,216\text{ Hz}}{280,896\text{ cycles}} \approx 59.7275\text{ Hz}$.

### Video Modes (Modes 0 through 5)

| Mode | Type | Layers | Resolution | Colors | Double Buffered |
| :---: | :--- | :--- | :---: | :---: | :---: |
| **0** | Tile / Text | BG0, BG1, BG2, BG3 | 240×160 | 16 or 256 | No |
| **1** | Mixed | BG0, BG1 (Text), BG2 (Affine) | 240×160 | 16 / 256 / 256 | No |
| **2** | Affine | BG2, BG3 (Affine) | 240×160 | 256 | No |
| **3** | Bitmap | BG2 | 240×160 | 32,768 (RGB555) | No |
| **4** | Bitmap | BG2 | 240×160 | 256 (Palette) | Yes (Pages 0/1) |
| **5** | Bitmap | BG2 | 160×128 | 32,768 (RGB555) | Yes (Pages 0/1) |

### Affine Transformation Mathematics

Affine backgrounds (Modes 1 and 2) and affine sprites utilize 2D matrix transformation with 28-bit signed fixed-point (19.8) reference coordinates and 16-bit signed fixed-point (8.8) matrix coefficients:

$$\begin{bmatrix} x' \\ y' \end{bmatrix} = \begin{bmatrix} PA & PB \\ PC & PD \end{bmatrix} \begin{bmatrix} x - x_0 \\ y - y_0 \end{bmatrix} + \begin{bmatrix} x_{\text{ref}} \\ y_{\text{ref}} \end{bmatrix}$$

- Internal reference latches (`bgx_internal`, `bgy_internal`) reload from I/O registers at the beginning of frame line 0.
- At each visible scanline, the internal coordinates advance by the vertical steps:
  $$x_{\text{ref}} \leftarrow x_{\text{ref}} + PB, \quad y_{\text{ref}} \leftarrow y_{\text{ref}} + PD$$

### Sprite Engine (OAM) & Priority Sorting

- **128 Hardware Sprites**: Stored in OAM (`0x07000000`).
- **Shapes & Sizes**: Square ($8\times 8$ to $64\times 64$), Horizontal ($16\times 8$ to $64\times 32$), and Vertical ($8\times 16$ to $32\times 64$).
- **Affine Sprites**: 32 affine transformation matrices in OAM permit real-time rotation, scaling, and double-size bounding boxes.
- **Sprite Overlap Tie-Breaking**: When two sprites overlap on the same scanline pixel:
  1. The sprite with the higher priority (lower priority numerical value $0..3$) is drawn in front.
  2. If both sprites share the identical priority value, the sprite with the **lower OAM index** ($0..127$) takes precedence.
- **Scanline Compositing & Layer Precedence**:
  At each pixel on the scanline, the compositor evaluates priorities 0 through 3. According to GBA hardware specifications, when an OBJ pixel and a Background pixel have the identical priority value, **OBJ is displayed in front**:
  $$\text{OBJ} > \text{BG0} > \text{BG1} > \text{BG2} > \text{BG3}$$
  This strict ordering prevents opaque background fill cards (such as those used on BG1/BG2 in *Super Mario Advance 2*) from occluding priority-matching title sprites, HUD elements, or characters. The top two visible layers after priority resolution are passed to the alpha blender.

### Color Special Effects, Blending, & Windowing

- **Alpha Blending (`BLDCNT`, `BLDALPHA`)**:
  Combines 1st target pixel $A$ and 2nd target pixel $B$ with coefficients $EVA$ and $EVB$ ($0 \le EV \le 16$):
  $$C = \min\left(31, \frac{C_A \times EVA + C_B \times EVB}{16}\right)$$
- **Brightness Increase (`BLDY`)**:
  $$C = C_A + \frac{(31 - C_A) \times EVY}{16}$$
- **Brightness Decrease (`BLDY`)**:
  $$C = C_A - \frac{C_A \times EVY}{16}$$
- **Windowing**: Window 0 (`WIN0`), Window 1 (`WIN1`), and Object Window (`OBJWIN`) allow rectangular masks where specific layers and blending effects can be selectively enabled or disabled.

---

## Direct Memory Access (DMA)

The DMA controller (`src/dma.c`) provides 4 independent high-speed channels with priority order $\text{DMA0} > \text{DMA1} > \text{DMA2} > \text{DMA3}$:

- **Bus-Mastering**: When triggered, DMA steals CPU bus cycles to transfer 16-bit halfwords or 32-bit words at 1 cycle per transfer.
- **Trigger Modes**: Immediate, VBlank start, HBlank start, or Special (DirectSound FIFO A/B requests).
- **Address Adjustments**: Increment, Decrement, Fixed, or Reload Destination on repeat.
- **Sound FIFO DMA**: Channels 1 and 2 automatically transfer 4 words (16 bytes) into the sound FIFO registers whenever the APU buffer drops below 16 samples.

---

## Timers & Interrupt Controller

- **Timers 0–3 (`src/timer.c`)**:
  - Independent 16-bit counters with reload latches.
  - Prescalers: System clock divided by 1, 64, 256, or 1024.
  - **Cascade Mode**: Timers 1, 2, and 3 can be configured to increment only when the previous timer overflows, forming 32-bit or 64-bit timing chains.
  - Timer 0 and 1 overflows pace the DirectSound sample rates.
- **Interrupt Controller (`src/interrupt.c`)**:
  - Master switch (`IME`), interrupt enable mask (`IE`), and request flags (`IF`).
  - Prioritized handling for VBlank, HBlank, VCount Match, Timers 0–3, DMA 0–3, Keypad, and Serial interrupts.
  - **HALT Wake-Up Gating**: The ARM7TDMI processor only unhalts from low-power `HALT` state when an interrupt enabled in `IE` is signaled (`(ic->ie & ic->if_) != 0`). Unconditional unhalting on disabled IRQs (such as HBlank IRQ when only VBlank is requested) would wake the CPU 160 times per frame prematurely, breaking game timing loops.

### Low-Level IRQ Vector & User Handler Trampoline

Commercial GBA games register their game loop and rendering interrupt handler by writing a function pointer to IWRAM address `0x03007FFC` (mirrored at `0x03FFFFFC`). 

1. **Hardware Vector (`0x00000018`)**:
   - When an IRQ fires and is unmasked (`IME == 1`, `(IE & IF) != 0`, CPSR `I == 0`), the ARM7TDMI hardware automatically:
     1. Switches to `MODE_IRQ` (`0x12`).
     2. Saves pre-interrupt CPSR into `SPSR_irq`.
     3. Saves return address into `LR_irq` ($PC+4$ in THUMB, $PC$ in ARM).
     4. Sets CPSR `I` bit to disable nested IRQs and switches to ARM state.
     5. Branches directly to hardware vector `0x00000018`.

2. **Custom BIOS IRQ Trampoline**:
   - Because Zeff Station runs without requiring an external proprietary BIOS ROM, it pre-installs an ultra-fast 9-instruction ARM trampoline at `0x00000018`:
     ```arm
     0x18: stmdb sp!, {r0-r3, r12, lr}   ; Save caller-saved registers & LR to IRQ stack
     0x1C: mov   r0, #0x04000000         ; Base I/O address
     0x20: ldr   r1, [r0, #-4]           ; Load user handler pointer from 0x03007FFC
     0x24: cmp   r1, #0                  ; Guard against null handlers
     0x28: beq   +4                      ; Skip invocation if NULL
     0x2C: add   lr, pc, #0              ; Setup LR return address to 0x34
     0x30: bx    r1                      ; Jump to user IRQ handler (supports ARM or THUMB)
     0x34: ldmia sp!, {r0-r3, r12, lr}   ; Restore registers from IRQ stack
     0x38: subs  pc, lr, #4              ; Atomically restore PC and CPSR from SPSR
     ```
   - **Atomic SPSR & Pipeline Refill**: On `subs pc, lr, #4`, the CPU restores CPSR from SPSR (returning to the interrupted mode and re-enabling interrupts if previously unmasked). Crucially, the CPU checks the restored CPSR `T` bit: if the interrupted program was running THUMB instructions, the pipeline is immediately refilled from THUMB mode; if ARM, from ARM mode.
   - **BIOS Wait Synchronization (`SWI 0x04` & `0x05`)**: On every IRQ dispatch, the emulator ORs the triggered interrupt bits into the BIOS check flags at `0x03007FF8` (`*(uint16_t*)&iwram[0x7FF8] |= (IE & IF)`). When a game calls `VBlankIntrWait` (`SWI 0x05`), old pending flags are discarded before halting, ensuring the CPU sleeps until the next genuine VBlank IRQ occurs.

---

## Audio Processing Unit (APU)

The sound subsystem (`src/apu.c`) generates high-fidelity stereo audio streamed at **44,100 Hz**:

1. **DMG Channel 1**: Square wave with programmable frequency sweep, duty cycle (12.5%, 25%, 50%, 75%), volume envelope, and length counter.
2. **DMG Channel 2**: Square wave with duty cycle and volume envelope.
3. **DMG Channel 3**: Programmable 32-sample 4-bit wave RAM.
4. **DMG Channel 4**: Pseudo-random white noise generator using a 7-bit or 15-bit linear-feedback shift register (LFSR).
5. **DirectSound A & B**: Dual 8-bit signed PCM channels backed by 32-byte circular FIFOs, paced by Timer 0 or Timer 1, with independent stereo panning and volume scaling.

---

## High-Level BIOS (HLE) & Zeff Station Boot Sequence

Zeff Station features a built-in **High-Level BIOS** (`src/bios.c`), meaning ROMs boot and run out-of-the-box **without requiring a copyrighted external BIOS dump**:

- **SWI Routines**: Implements `SoftReset` (`0x00`), `RegisterRamReset` (`0x01`), `Halt` (`0x02`), `Stop` (`0x03`), `IntrWait` (`0x04`), `VBlankIntrWait` (`0x05`), `Div` (`0x06`), `DivArm` (`0x07`), `Sqrt` (`0x08`), `ArcTan` (`0x09`), `ArcTan2` (`0x0A`), `CpuSet` (`0x0B`), `CpuFastSet` (`0x0C`), `BgAffineSet` (`0x0E`), `ObjAffineSet` (`0x0F`), `BitUnPack` (`0x10`), `LZ77UnCompWram`/`Vram` (`0x11`/`0x12`), `RLUnCompWram`/`Vram` (`0x14`/`0x15`), and `SoundBias` (`0x19`).
- **VRAM Decompression Buffering**: Because the GBA VRAM bus strictly forbids 8-bit writes (duplicating byte lanes for BG VRAM and discarding byte writes to OBJ VRAM), `LZ77UnCompVram` and `RLUnCompVram` decompress byte streams into an internal staging buffer before committing full 16-bit halfwords into video memory.
- **Zeff Station Boot Sequence**:
  - Instead of the traditional boot logo, Zeff Station presents an animated starry intro with metallic gold/cyan typography proclaiming **ZEFF STATION ADVANCE SYSTEM**.
  - An authentic two-tone retro harmonic jingle plays through the APU pulse synthesizers during boot.
  - Pressing `Start` or `A` (or passing `--skip-intro`) instantly skips directly into game execution.
- **External BIOS Support**: Users who wish to run an official binary can pass `--bios <path>` to load a 16 KB BIOS image.

---

## Host Frontend (macOS Cocoa & CoreAudio)

The host frontend (`src/frontend_cocoa.m`) is built directly against native Apple frameworks:

- **AppKit / Cocoa**: Pure Objective-C windowing and event loops (`NSApplication`, `NSWindow`, `NSView`).
- **CoreGraphics Rendering**: Direct framebuffer presentation with zero interpolation (`kCGInterpolationNone`) to maintain razor-sharp integer pixel art.
- **CoreAudio / AudioQueue**: Asynchronous double-buffered audio streaming at 44.1 kHz with zero audio tearing or buffer underruns.
- **Retina Scaling**: Clean 1x, 2x, 3x, or 4x scaling factors with window resizing and real-time FPS counter in the title bar.

---

## Hardware Quirks & Commercial Title Case Studies

Building a 1:1 cycle-accurate GBA emulator requires emulating obscure hardware behaviors that commercial games rely upon to function:

### Super Mario Advance 2: Compositor Priority Occlusion

* **Symptom**: The title screen, menu files, and dialog cutscenes showed solid white backgrounds or purple blocks, completely occluding Mario, Yoshi, cursors, and text boxes.
* **Low-Level Root Cause**: In the GBA PPU scanline compositing pipeline, both background layers (`BG0`–`BG3`) and sprite layers (`OBJ`) carry a 2-bit priority attribute ($0..3$, where $0$ is highest priority). In *Super Mario Advance 2*, the developers placed white/purple backdrop cards on `BG1` and `BG2` with priority 1, while placing Mario, Yoshi, and the UI text sprites in `OBJ` also with priority 1. If an emulator resolves equal priority by drawing backgrounds after or over sprites, the background completely occludes the sprites.
* **Hardware Rule**: According to GBA hardware specifications, when an OBJ pixel and a Background pixel have the **identical priority level**, the OBJ layer takes precedence:
  $$\text{OBJ} > \text{BG0} > \text{BG1} > \text{BG2} > \text{BG3}$$
  Fixing the compositor loop in `src/ppu.c` to evaluate OBJ in front of matching BG priorities instantly restored 100% visual fidelity to the title sequence, file selection screen, and gameplay.

### Dragon Ball - Advanced Adventure: IRQ Return, Pipeline Refills, & VRAM Bus Quirks

* **Symptom**: The game booted to a blank white screen, hanging within the first few frames without producing audio or video.
* **Low-Level Root Causes & Hardware Resolutions**:
  1. **IRQ Return Pipeline Mode Invalidation (`SUBS PC, LR, #4`)**:
     Commercial games switch dynamically between 32-bit ARM mode and 16-bit THUMB mode. *Dragon Ball* executes its main loop in THUMB mode. When a VBlank or Timer IRQ triggers, the CPU hardware switches to ARM mode and jumps to the IRQ vector. Upon completing the interrupt handler, the code executes `SUBS PC, LR, #4` to return to the interrupted user code and restore `CPSR` from `SPSR`.
     If the emulator always refills its instruction pipeline in ARM mode upon an exception return, it attempts to decode 16-bit THUMB opcodes as 32-bit ARM instructions, crashing into undefined instruction traps at frame 4. The fix checks the restored `CPSR` bit 5 (`FLAG_T`): if set, `arm7tdmi_fill_pipeline_thumb` is invoked; otherwise, `arm7tdmi_fill_pipeline_arm`.
  2. **Interrupt Wake-Up Gating on `HALT`**:
     *Dragon Ball* utilizes `SWI 0x05` (`VBlankIntrWait`) to pace game logic to exactly 1 VBlank per 60 Hz frame. If the emulator unhalts the CPU whenever *any* interrupt condition is triggered (including disabled HBlank IRQs), the CPU wakes up 160 times per frame instead of once at VBlank, completely corrupting game timing and causing the internal wait loops to stall. The emulator now enforces the strict hardware condition: the CPU only wakes from `HALT` when an unmasked interrupt enabled in `IE` is signaled (`(ic->ie & ic->if_) != 0`).
  3. **VRAM 8-bit Bus Write Quirks in SWI Decompression**:
     The title screen graphics for *Dragon Ball* are compressed with LZ77 and decompressed via `SWI 0x12` (`LZ77UnCompVram`). On real GBA hardware, 8-bit byte writes to Background VRAM write the byte twice across the 16-bit bus, and 8-bit writes to Sprite/OBJ VRAM (`0x06010000`+) are completely discarded. Naive byte-by-byte decompression directly to VRAM corrupts adjacent bytes and loses sprite data entirely. `LZ77UnCompVram` and `RLUnCompVram` now buffer the decompressed byte stream in an internal buffer and commit full 16-bit halfwords to VRAM.
  4. **BIOS Wait Synchronization (`SWI 0x04` & `SWI 0x05`)**:
     When `VBlankIntrWait` is called, previous pending interrupt flags at IWRAM `0x03007FF8` are cleared so the routine sleeps until a *new* VBlank IRQ arrives.
  5. **Missing SWI Handlers**:
     Implemented `SWI 0x0F` (`ObjAffineSet`) and `SWI 0x10` (`BitUnPack`), which *Dragon Ball* calls during early tile generation.

---

## Building, Running, & Testing

### Prerequisites

- macOS (Intel or Apple Silicon)
- Apple Clang (included with Xcode or Command Line Tools: `xcode-select --install`)
- `make` and `python3` (for generating test ROMs)

### Compilation

```bash
# Build the emulator binary
make

# Run the automated test suite (verifies bus, CPU, THUMB, DMA, PPU)
make test

# Clean build artifacts
make clean
```

### Running Games

```bash
# Launch a GBA ROM
./build/zeff_station roms/game.gba

# Launch with 4x scale (960x640)
./build/zeff_station --scale 4 roms/game.gba

# Skip the Zeff Station boot intro
./build/zeff_station --skip-intro roms/game.gba

# Run automated headless benchmark for 600 frames
./build/zeff_station --headless --frames 600 roms/game.gba
```

---

## Controls & Command Line Options

### Keyboard Controls

| GBA Button | Host Keyboard Key |
| :---: | :---: |
| **A** | `Z` / `X` |
| **B** | `A` / `S` |
| **L Trigger** | `Q` |
| **R Trigger** | `W` / `E` |
| **Start** | `Return` / `Enter` |
| **Select** | `Space` / `Backspace` / `Shift` |
| **D-Pad Up** | `Up Arrow` |
| **D-Pad Down** | `Down Arrow` |
| **D-Pad Left** | `Left Arrow` |
| **D-Pad Right** | `Right Arrow` |
| **Fast Forward (4x Turbo)** | `Tab` (Toggle) |

### CLI Flags

```
Usage: ./build/zeff_station [options] <rom.gba>

Options:
  --bios <path>       Path to custom BIOS file (optional)
  --scale <factor>    Window scale factor 1-6 (default: 3 -> 720x480)
  --skip-intro        Skip Zeff Station boot intro sequence
  --headless          Run without display window (for headless tests)
  --frames <n>        Stop execution after N frames (default: unlimited)
  -h, --help          Show this help message
```
