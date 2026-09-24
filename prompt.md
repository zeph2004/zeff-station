# Specification & Prompt: Build 1:1 GBA Emulator in C for macOS

You are tasked with building a cycle-accurate, 1:1 Game Boy Advance (GBA) emulator written in C99 and Objective-C from first principles. It must run natively on macOS with **zero external third-party dependencies** (no SDL, no external libraries; use native macOS Cocoa/AppKit and CoreAudio frameworks).

Instead of standard Nintendo branding, the emulator features custom branding, an animated retro intro sequence, an authentic synthesized harmonic boot chime, and high-performance macOS desktop integration. It must be capable of booting commercial GBA titles (including *Super Mario Advance 2*, *Dragon Ball: Advanced Adventure*, etc.) with pixel-perfect visual fidelity, accurate audio, and full save game persistence.

---

## 0. MANDATORY PRE-EXECUTION STEP: Ask for Custom Branding Name

> [!IMPORTANT]
> **BEFORE writing any files, creating directories, or executing code**, you MUST pause and ask the user for their preferred custom branding name.

### Prompt to Present to User:
> *"Before we begin building your GBA emulator, what custom branding name would you like to use for the system? (Default: **Zeff Station**)"*

### How to Apply the User's Choice:
1. **Fallback / Default**: If the user provides a blank response, presses enter, or confirms they want the default, use **Zeff Station**.
2. **Project-Wide Integration**:
   - **Boot Intro Sequence**: In the animated retro BIOS boot sequence in `src/bios.c`, render the user's chosen branding name (e.g. `"<BRAND> ADVANCE SYSTEM"`) in the glowing 5x7 font.
   - **CLI Banner & Help**: Display the chosen branding name on startup in `src/main.c`.
   - **Desktop Window Title**: Use the branding name in the Cocoa title bar in `src/frontend_cocoa.m` (e.g. `"<BRAND> - [Game Title]"`).
   - **Documentation & Makefile**: Reflect the chosen brand name in `README.md` and the binary executable name (e.g. `build/<brand_slug>` with symlink `build/gba_emulator`).

---

## 1. Project Structure & Build Requirements

The codebase must be structured as follows:

```
├── Makefile
├── README.md
├── prompt.md
├── .gitignore
├── include/
│   ├── apu.h
│   ├── arm7tdmi.h
│   ├── arm_instructions.h
│   ├── bios.h
│   ├── bus.h
│   ├── dma.h
│   ├── frontend.h
│   ├── gba.h
│   ├── interrupt.h
│   ├── keypad.h
│   ├── ppu.h
│   ├── thumb_instructions.h
│   └── timer.h
├── src/
│   ├── apu.c
│   ├── arm7tdmi.c
│   ├── arm_instructions.c
│   ├── bios.c
│   ├── bus.c
│   ├── dma.c
│   ├── frontend_cocoa.m
│   ├── gba.c
│   ├── interrupt.c
│   ├── keypad.c
│   ├── main.c
│   ├── ppu.c
│   ├── thumb_instructions.c
│   └── timer.c
├── tests/
│   ├── generate_test_roms.py
│   └── test_runner.c
└── roms/
    └── .gitkeep
```

### Compiler & Flags
- C compiler: `clang -std=c11 -Wall -Wextra -O2 -Iinclude -g`
- Objective-C frontend: `clang -fobjc-arc -Iinclude -O2 -g -c src/frontend_cocoa.m`
- Link flags: `-framework Cocoa -framework AudioToolbox -lm`
- Binary output: `build/zeff_station` (with symlink `build/gba_emulator`)
- Include a `Makefile` supporting:
  - `make`: Compiles emulator binary
  - `make test`: Builds test runner, executes `python3 tests/generate_test_roms.py`, runs 18 automated unit tests verifying bus, CPU, THUMB, DMA, and PPU
  - `make clean`: Cleans build directory

---

## 2. Processor Core (ARM7TDMI) Specifications

Implement an **ARMv4T** architecture 32-bit RISC core with a 3-stage instruction pipeline (Fetch, Decode, Execute):

1. **Pipeline Prefetch Mechanics**:
   - Maintain 2 prefetch slots (`pipeline[0]`, `pipeline[1]`) and a `cpu->branched` flag.
   - When an instruction at address $A$ is executing, PC register (`R15`) reads as **$A + 8$** in 32-bit ARM state and **$A + 4$** in 16-bit THUMB state.
   - Provide `arm7tdmi_fill_pipeline_arm(cpu)` and `arm7tdmi_fill_pipeline_thumb(cpu)` to refill pipeline upon branches, exceptions, and mode switches.

2. **Operating Modes & Register Banking**:
   - Emulate all 7 processor modes: User (`0x10`), FIQ (`0x11`), IRQ (`0x12`), Supervisor (`0x13`), Abort (`0x17`), Undefined (`0x1B`), and System (`0x1F`).
   - Implement banked registers: `R8_fiq`–`R14_fiq`, `SPSR_fiq`, `R13_irq`, `R14_irq`, `SPSR_irq`, `R13_svc`, `R14_svc`, `SPSR_svc`, `R13_abt`, `R14_abt`, `SPSR_abt`, `R13_und`, `R14_und`, `SPSR_und`.
   - `arm7tdmi_switch_mode()` must save and restore banked registers and SPSR based on the mode transition.

3. **ALU, Barrel Shifter, & Status Flags**:
   - Shifter operations: Logical Left (`LSL`), Logical Right (`LSR`), Arithmetic Right (`ASR`), Rotate Right (`ROR`), and Rotate Right with Extend (`RRX`). Handle immediate `#0` shifts correctly (LSR #0 = 32, ASR #0 = 32).
   - ALU flags: Negative ($N$), Zero ($Z$), Carry ($C$), and Overflow ($V$).
   - Addition Carry: Unsigned addition overflow ($A + B + C_{in} > 2^{32} - 1$).
   - Subtraction Carry: Set if no borrow occurred ($A \ge B + \text{borrow}$).
   - Overflow ($V$):
     - Addition: $V = (\sim(A \oplus B) \ \& \ (A \oplus \text{Result})) \ \& \ 0x80000000$
     - Subtraction: $V = ((A \oplus B) \ \& \ (A \oplus \text{Result})) \ \& \ 0x80000000$

4. **Instruction Decoding**:
   - Complete 32-bit ARM instruction set: Branch (`B`, `BL`), Branch & Exchange (`BX`), ALU data processing (16 opcodes with condition execution and `S` flag), Single Data Transfer (`LDR`, `STR`), Halfword/Signed Transfer (`LDRH`, `STRH`, `LDRSB`, `LDRSH`), Block Data Transfer (`LDM`, `STM`), Multiply (`MUL`, `MLA`, `UMULL`, `SMULL`, `UMLAL`, `SMLAL`), Swap (`SWP`), Status Register Transfer (`MRS`, `MSR`), and Software Interrupt (`SWI`).
   - Complete 16-bit THUMB instruction set: Formats 1–19 decoded directly into fast C handlers.

5. **CRITICAL: IRQ Return Pipeline Refill**:
   - In `SUBS PC, LR, #4` (returning from an interrupt vector):
     - Restore `CPSR` from `SPSR`.
     - **Inspect restored CPSR bit 5 (`FLAG_T`)**: If set, refill the pipeline in **THUMB mode** (`arm7tdmi_fill_pipeline_thumb`). If cleared, refill in **ARM mode** (`arm7tdmi_fill_pipeline_arm`). Failing to check this causes games that run their main loop in THUMB mode (e.g. *Dragon Ball: Advanced Adventure*) to crash on frame 4.

---

## 3. Memory Bus & Cartridge Subsystem

Address space mapping:
- `0x00000000`–`0x00003FFF`: BIOS (16 KB)
- `0x02000000`–`0x0203FFFF`: On-Board Work RAM (EWRAM 256 KB, mirrored to `0x02FFFFFF`)
- `0x03000000`–`0x03007FFF`: On-Chip Work RAM (IWRAM 32 KB, mirrored to `0x03FFFFFF`)
- `0x04000000`–`0x040003FF`: I/O Registers (1 KB)
- `0x05000000`–`0x050003FF`: Palette RAM (1 KB, mirrored to `0x05FFFFFF`)
- `0x06000000`–`0x06017FFF`: VRAM (96 KB, mirrored to `0x06FFFFFF`)
- `0x07000000`–`0x070003FF`: OAM (1 KB, mirrored to `0x07FFFFFF`)
- `0x08000000`–`0x09FFFFFF`: Game Pak ROM Waitstate 0 (up to 32 MB)
- `0x0A000000`–`0x0BFFFFFF`: Game Pak ROM Waitstate 1
- `0x0C000000`–`0x0DFFFFFF`: Game Pak ROM Waitstate 2
- `0x0E000000`–`0x0E00FFFF`: Game Pak SRAM / Backup Memory

### Hardware Bus Quirks
1. **Unaligned Accesses**:
   - Unaligned `LDR`: Read aligned 32-bit word and rotate right by $(addr \ \& \ 3) \times 8$.
   - Unaligned `LDRH`: If bit 0 is set, read aligned halfword and rotate right by 8.
   - Unaligned stores mask off address alignment bits (`addr &= ~3` for 32-bit, `addr &= ~1` for 16-bit).
2. **Palette 8-bit Writes**: An 8-bit write to Palette RAM expands the byte across the 16-bit halfword: `(val << 8) | val`.
3. **VRAM 8-bit Writes**: An 8-bit write to Background VRAM (`< 0x06010000`) expands the byte into both halves of the halfword. 8-bit writes to Sprite/OBJ VRAM (`>= 0x06010000`) are **completely ignored** by hardware.
4. **OAM 8-bit Writes**: 8-bit writes to OAM are ignored; only 16-bit and 32-bit writes are permitted.
5. **BIOS Open-Bus Protection**: Reading BIOS memory when $PC \ge 0x00004000$ returns open-bus values.
6. **Cartridge ROM Mirroring**: ROMs smaller than 32 MB mirror continuously across `0x08000000`–`0x0DFFFFFF`:
   $$\text{ROM Offset} = (addr \ \& \ 0x01FFFFFF) \pmod{\text{ROM Size}}$$

### Cartridge Save Persistence (SRAM & EEPROM)
1. **Battery-Backed SRAM (`0x0E000000`–`0x0E00FFFF`)**:
   - 32 KB / 64 KB storage. Writes set `sram_dirty` flag and flush to `<rom_path>.sav`.
2. **Cartridge Serial EEPROM (`0x0D000000`–`0x0DFFFFFF`)**:
   - Emulate 4Kbit (512-byte) and 64Kbit (8-KB) EEPROM via bit 0 serial bus over DMA3.
   - Detect read requests (command `11` + address + `0`) and write requests (command `10` + address + 64 bits data + `0`).
   - For read requests, respond via DMA3 with a 68-halfword stream: 4 dummy zero halfwords followed by 64 halfwords carrying the 8-byte page data bit-by-bit in Bit 0.
   - Polling reads from `0x0D000000` return bit 0 as `1` (ready).

---

## 4. Picture Processing Unit (PPU)

Implement a scanline-based rasterizer matching GBA hardware timings:
- Resolution: $240 \times 160$ pixels.
- Scanline timing: 1232 cycles total per line (960 cycles HDraw + 272 cycles HBlank).
- Frame timing: 228 scanlines total (160 visible VDraw lines + 68 VBlank lines).
- Frame rate: $\approx 59.7275\text{ Hz}$ ($16,777,216 / 280,896$).
- Trigger HBlank and VBlank interrupts and DMA events at their exact cycle and scanline transitions.

### Video Modes (0 through 5)
- **Mode 0**: 4 Text backgrounds (`BG0`–`BG3`). 16-color or 256-color tiles.
- **Mode 1**: 2 Text backgrounds (`BG0`, `BG1`) + 1 Affine background (`BG2`).
- **Mode 2**: 2 Affine backgrounds (`BG2`, `BG3`).
- **Mode 3**: 16-bit RGB555 direct color bitmap ($240 \times 160$).
- **Mode 4**: 8-bit palette-indexed bitmap ($240 \times 160$) with page flipping (bit 4 of `DISPCNT`).
- **Mode 5**: 16-bit RGB555 direct color bitmap ($160 \times 128$) with page flipping.

### Affine Background Math
- Background registers `BG2PA`–`BG2PD` and `BG3PA`–`BG3PD` (8.8 fixed-point).
- Reference coordinates `BG2X`, `BG2Y`, `BG3X`, `BG3Y` (19.8 fixed-point).
- Latch internal reference coordinates at line 0, and step each scanline:
  $$x_{\text{ref}} \leftarrow x_{\text{ref}} + PB, \quad y_{\text{ref}} \leftarrow y_{\text{ref}} + PD$$

### 128 Hardware Sprites (OAM)
- Normal and Affine sprites (up to 32 affine transformation matrices).
- Shapes: Square, Horizontal, Vertical (sizes $8\times 8$ to $64\times 64$).
- 1D and 2D tile-to-OAM mapping.
- **Sprite Tie-Breaking Rule**: When two sprites overlap on the same scanline pixel and share the same priority value ($0..3$), the sprite with the **lower OAM index ($0..127$)** has higher precedence and is displayed in front.

### CRITICAL: Compositor Layer Priority Precedence Rule
- Each scanline pixel composites `BG0`–`BG3` and `OBJ` according to their priority ($0..3$, where $0$ is highest).
- **Rule**: When an OBJ pixel and a Background pixel have the **identical priority level**, **OBJ takes precedence**:
  $$\text{OBJ} > \text{BG0} > \text{BG1} > \text{BG2} > \text{BG3}$$
  *Failure to prioritize OBJ over BG at equal priority causes menu cards and dialog backdrops in games like Super Mario Advance 2 to occlude Mario and UI text.*
- Special effects: Alpha Blending (`BLDCNT`, `BLDALPHA` with $EVA/EVB$), Brightness Increase/Decrease (`BLDY` with $EVY$), and rectangular Windowing (`WIN0`, `WIN1`, `OBJWIN`).

---

## 5. Direct Memory Access (DMA) & Timers

### DMA Controller (4 Channels)
- Channels 0–3 with strict priority arbitration ($\text{DMA0} > \text{DMA1} > \text{DMA2} > \text{DMA3}$).
- Transfer 16-bit halfwords or 32-bit words at 1 bus cycle per unit.
- Address modes: Increment, Decrement, Fixed, and Reload Destination on repeat.
- Start triggers: Immediate, VBlank, HBlank, and Special (DirectSound FIFO A/B requests on DMA1/DMA2).

### Timers (0 through 3)
- 16-bit counters with reload values.
- Prescalers: System clock divided by 1, 64, 256, or 1024.
- Cascade mode: Timers 1–3 increment when the previous timer overflows.
- Generate timer IRQs on overflow and pace DirectSound APU FIFO sample consumption.

---

## 6. Interrupt Controller & Low-Level IRQ Dispatch

1. **Registers**: Master enable (`IME`), enable mask (`IE`), request flags (`IF`).
2. **CRITICAL: HALT Wake-Up Condition**:
   - The CPU enters low-power `HALT` mode via `SWI 0x02` or `SWI 0x05`.
   - The CPU must **ONLY** unhalt when an interrupt enabled in `IE` is signaled:
     $$\text{Unhalt Condition} = (IE \ \& \ IF) \ne 0$$
   - *Never unhalt the CPU unconditionally on any interrupt. Waking up on disabled HBlank IRQs prematurely breaks frame pacing in commercial titles.*
3. **Built-in IRQ Trampoline at `0x00000018`**:
   - Pre-install an ARM trampoline at `0x00000018`:
     ```arm
     0x18: stmdb sp!, {r0-r3, r12, lr}
     0x1C: mov   r0, #0x04000000
     0x20: ldr   r1, [r0, #-4]         ; Load handler from 0x03007FFC
     0x24: cmp   r1, #0
     0x28: beq   +4
     0x2C: add   lr, pc, #0
     0x30: bx    r1                    ; Jump to user handler
     0x34: ldmia sp!, {r0-r3, r12, lr}
     0x38: subs  pc, lr, #4            ; Atomic return + SPSR restore
     ```
   - On every IRQ dispatch, OR the triggered interrupt bits into the BIOS check flags at IWRAM `0x03007FF8`:
     $$*(uint16\_t*)\&iwram[0x7FF8] \ |= \ (IE \ \& \ IF)$$
   - In `SWI 0x05` (`VBlankIntrWait`) and `SWI 0x04` (`IntrWait`), clear stale flags before halting so the CPU sleeps until the next genuine interrupt occurs.

---

## 7. Audio Processing Unit (APU)

Generate stereo audio at **44,100 Hz**:
1. **DMG Channel 1**: Square wave with frequency sweep, duty cycles (12.5%, 25%, 50%, 75%), volume envelope, and length counter.
2. **DMG Channel 2**: Square wave with duty cycles and volume envelope.
3. **DMG Channel 3**: Programmable 32-sample 4-bit wave table.
4. **DMG Channel 4**: Pseudo-random noise generator with 7-bit or 15-bit LFSR.
5. **DirectSound A & B**: Dual 8-bit signed PCM channels with 32-byte circular FIFOs paced by Timer 0 or 1 overflows, featuring stereo panning and volume control.

---

## 8. High-Level BIOS (HLE) & Zeff Station Branding

Implement built-in HLE BIOS routines:
- `SoftReset` (`0x00`), `RegisterRamReset` (`0x01`), `Halt` (`0x02`), `Stop` (`0x03`), `IntrWait` (`0x04`), `VBlankIntrWait` (`0x05`), `Div` (`0x06`), `DivArm` (`0x07`), `Sqrt` (`0x08`), `ArcTan` (`0x09`), `ArcTan2` (`0x0A`), `CpuSet` (`0x0B`), `CpuFastSet` (`0x0C`), `BgAffineSet` (`0x0E`), `ObjAffineSet` (`0x0F`), `BitUnPack` (`0x10`), `LZ77UnCompWram`/`Vram` (`0x11`/`0x12`), `RLUnCompWram`/`Vram` (`0x14`/`0x15`), `SoundBias` (`0x19`).
- **CRITICAL: VRAM Decompression Buffering**:
  `LZ77UnCompVram` (`0x12`) and `RLUnCompVram` (`0x15`) must decompress into a RAM staging buffer and commit full **16-bit halfwords** to VRAM, avoiding 8-bit write corruption on the VRAM bus.
- **Custom Boot Sequence (Using Selected Branding Name)**:
  - Render an animated 90-frame starry boot sequence displaying the chosen branding (e.g. **<BRAND> ADVANCE SYSTEM**, or default **ZEFF STATION ADVANCE SYSTEM**) with gold and cyan glowing typography using an embedded 5x7 bitmap font.
  - Synthesize a harmonic 2-tone chime through APU Channel 1 during frames 10–50.
  - Allow skipping the intro immediately via `Start`, `A`, or the `--skip-intro` CLI flag.
  - Setup post-boot CPU registers (`SP_usr = 0x03007F00`, `SP_irq = 0x03007FA0`, `SP_svc = 0x03007FE0`, `CPSR = MODE_SYS`, `PC = 0x08000000`).

---

## 9. macOS Cocoa & CoreAudio Frontend

Implement `src/frontend_cocoa.m` using native Apple frameworks without SDL:
1. **Windowing & Events**:
   - `NSApplication`, `NSWindow`, and custom `NSView` subclass.
   - Handle key down/up events for GBA controls:
     - `A`: `Z` / `X`
     - `B`: `A` / `S`
     - `L Trigger`: `Q`
     - `R Trigger`: `W` / `E`
     - `Start`: `Return` / `Enter`
     - `Select`: `Space` / `Backspace` / `Shift`
     - `D-Pad`: Arrow keys
     - `Fast-Forward (4x Turbo)`: `Tab` (Toggle)
2. **CoreGraphics Rendering**:
   - Create `CGImageRef` with `CGDataProviderCreateWithData`.
   - **Byte Order**: Use `kCGImageAlphaNoneSkipLast | kCGBitmapByteOrder32Big` so that in-memory `[R, G, B, 0xFF]` matches CoreGraphics RGB components.
   - Draw with `kCGInterpolationNone` to preserve sharp pixel art across retina displays.
3. **CoreAudio AudioQueue**:
   - Double-buffered `AudioQueue` streaming stereo 16-bit signed PCM at 44.1 kHz.
   - Paced smoothly to the 59.73 Hz video refresh rate with zero buffer underruns.

---

## 10. Automated Verification & Testing

Create `tests/test_runner.c` and `tests/generate_test_roms.py` verifying 18 core hardware functions:
1. EWRAM memory mirroring at `0x02041000`.
2. IWRAM memory mirroring at `0x03008500`.
3. Palette 8-bit write halfword expansion.
4. Aligned and unaligned 32-bit bus reads.
5. DMA0 32-bit block transfers.
6. EEPROM 64-bit block writes via DMA3.
7. EEPROM 68-bit serial read stream via DMA3.
8. Timer 0 overflow and IRQ flag setting.
9. BIOS IRQ vector trampoline and user handler dispatch.
10. ARM arithmetic, barrel shifter, and branch instructions with synthetic ROM.
11. THUMB instruction set push/pop, math, and branches with synthetic ROM.
12. PPU Mode 3 bitmap rasterization.

Execute `make test` to verify all 18 unit tests pass with zero errors.
