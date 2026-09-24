#!/usr/bin/env python3
import struct
import os

def create_gba_header(title="ZEFF_GBA", game_code="ZEFF", maker_code="01"):
    # 0x00..0x03: Branch to 0x080000C0
    # ARM instruction: B 0x080000C0 -> offset = (0xC0 - 8) / 4 = 0x2E
    branch_instr = struct.pack("<I", 0xEA00002E)
    
    # 0x04..0x9F: Logo data (156 bytes)
    logo = bytes([
        0x24, 0xFF, 0xAE, 0x51, 0x69, 0x9A, 0xA2, 0x21, 0x3D, 0x84, 0x82, 0x0A, 0x84, 0xE4, 0x09, 0xAD,
        0x11, 0x24, 0x8B, 0x98, 0xC0, 0x81, 0x7F, 0x21, 0xA3, 0x52, 0xBE, 0x19, 0x93, 0x09, 0xCE, 0x20,
        0x10, 0x46, 0x4A, 0x4A, 0xF8, 0x27, 0x31, 0xEC, 0x58, 0xC7, 0xE8, 0x33, 0x82, 0xE3, 0xCE, 0xBF,
        0x85, 0xF4, 0xDF, 0x94, 0xCE, 0x4B, 0x09, 0x2B, 0x94, 0xA0, 0x8B, 0x45, 0x67, 0x82, 0x90, 0x86,
        0x78, 0x2A, 0x45, 0x82, 0x48, 0x3E, 0x4B, 0x21, 0x3D, 0x05, 0x01, 0x0E, 0x82, 0x48, 0x3E, 0x4B,
        0x01, 0x0E, 0x82, 0x48, 0x3E, 0x4B, 0x01, 0x0E, 0x82, 0x48, 0x3E, 0x4B, 0x01, 0x0E, 0x82, 0x48,
        0x3E, 0x4B, 0x01, 0x0E, 0x82, 0x48, 0x3E, 0x4B, 0x01, 0x0E, 0x82, 0x48, 0x3E, 0x4B, 0x01, 0x0E,
        0x82, 0x48, 0x3E, 0x4B, 0x01, 0x0E, 0x82, 0x48, 0x3E, 0x4B, 0x01, 0x0E, 0x82, 0x48, 0x3E, 0x4B,
        0x01, 0x0E, 0x82, 0x48, 0x3E, 0x4B, 0x01, 0x0E, 0x82, 0x48, 0x3E, 0x4B, 0x01, 0x0E, 0x82, 0x48,
        0x3E, 0x4B, 0x01, 0x0E, 0x82, 0x48, 0x3E, 0x4B, 0x37, 0x1A, 0x84, 0x48
    ])
    if len(logo) < 156:
        logo = logo.ljust(156, b'\x00')
    else:
        logo = logo[:156]

    raw_title = title.encode('ascii')[:12].ljust(12, b'\x00')
    raw_code = game_code.encode('ascii')[:4].ljust(4, b'\x00')
    raw_maker = maker_code.encode('ascii')[:2].ljust(2, b'\x00')
    fixed_byte = b'\x96'
    main_unit = b'\x00'
    device_type = b'\x00'
    reserved = b'\x00' * 7
    version = b'\x00'
    
    header_so_far = raw_title + raw_code + raw_maker + fixed_byte + main_unit + device_type + reserved + version
    chk = 0
    for b in header_so_far:
        chk = (chk - b) & 0xFF
    chk = (chk - 0x19) & 0xFF
    chk_byte = bytes([chk])
    reserved2 = b'\x00\x00'
    
    return branch_instr + logo + header_so_far + chk_byte + reserved2

def arm_b(offset):
    imm = (offset >> 2) & 0x00FFFFFF
    return 0xEA000000 | imm

def arm_mov_imm(rd, imm):
    return 0xE3A00000 | (rd << 12) | (imm & 0xFF)

def arm_add_imm(rd, rn, imm):
    return 0xE2800000 | (rn << 16) | (rd << 12) | (imm & 0xFF)

def arm_sub_imm(rd, rn, imm):
    return 0xE2400000 | (rn << 16) | (rd << 12) | (imm & 0xFF)

def arm_cmp_imm(rn, imm):
    return 0xE3500000 | (rn << 16) | (imm & 0xFF)

def arm_bne(offset):
    imm = (offset >> 2) & 0x00FFFFFF
    return 0x1A000000 | imm

def arm_ldr_pc(rd, offset):
    return 0xE59F0000 | (rd << 12) | (offset & 0xFFF)

def arm_str_imm(rd, rn, offset):
    return 0xE5800000 | (rn << 16) | (rd << 12) | (offset & 0xFFF)

def arm_strh_imm(rd, rn, offset):
    return 0xE1C000B0 | (rn << 16) | (rd << 12) | ((offset & 0xF0) << 4) | (offset & 0xF)

def arm_bx(rn):
    return 0xE12FFF10 | (rn & 0xF)

def build_mode3_rom():
    header = create_gba_header(title="ZEFF_COLORS", game_code="ZCLR")
    code = []
    
    # Pool constants
    pool = [
        0x04000000,
        0x00000403,
        0x06000000,
        38400
    ]
    
    # 14 instructions total = 56 bytes
    # Instruction 0: LDR R0, pool[0] (addr 0x080000C0, PC=0xC8, target=0x080000F8, offset = 0x30 = 48)
    code.append(arm_ldr_pc(0, 48))
    # Instruction 1: LDR R1, pool[1] (addr 0x080000C4, PC=0xCC, target=0x080000FC, offset = 0x30 = 48)
    code.append(arm_ldr_pc(1, 48))
    code.append(arm_strh_imm(1, 0, 0)) # STRH R1, [R0]
    
    # Instruction 3: LDR R2, pool[2] (addr 0x080000CC, PC=0xD4, target=0x08000100, offset = 0x2C = 44)
    code.append(arm_ldr_pc(2, 44))
    # Instruction 4: LDR R3, pool[3] (addr 0x080000D0, PC=0xD8, target=0x08000104, offset = 0x2C = 44)
    code.append(arm_ldr_pc(3, 44))
    code.append(arm_mov_imm(4, 0))     # MOV R4, #0
    
    # Loop (starts at index 6, addr 0x080000D8)
    code.append(0xE204501F) # AND R5, R4, #0x1F
    code.append(0xE1A061A4) # LSR R6, R4, #3
    code.append(0xE206601F) # AND R6, R6, #0x1F
    code.append(0xE1A06286) # LSL R6, R6, #5
    code.append(0xE1855006) # ORR R5, R5, R6
    code.append(0xE1A06324) # LSR R6, R4, #6
    code.append(0xE206601F) # AND R6, R6, #0x1F
    code.append(0xE1A06506) # LSL R6, R6, #10
    code.append(0xE1855006) # ORR R5, R5, R6
    
    # STRH R5, [R2], #2 = post-indexed store halfword
    code.append(0xE0C250B2) # STRH R5, [R2], #2
    code.append(arm_add_imm(4, 4, 1)) # ADD R4, R4, #1
    code.append(0xE1540003)           # CMP R4, R3
    # BNE loop (current PC = 0x08000118 + 8 = 0x120. Loop start = 0xD8. Offset = 0xD8 - 0x120 = -72 bytes)
    code.append(arm_bne(-72))
    
    # Halt / wait loop
    code.append(arm_b(-8))   # B self
    
    bin_code = b"".join(struct.pack("<I", instr) for instr in code)
    bin_pool = b"".join(struct.pack("<I", val) for val in pool)
    
    rom_data = header + bin_code + bin_pool
    if len(rom_data) < 32768:
        rom_data = rom_data.ljust(32768, b'\x00')
    return rom_data

def build_cpu_arm_rom():
    header = create_gba_header(title="ARM_CPU_TEST", game_code="ZARM")
    
    # We will assemble with exact addresses
    # Base: 0x080000C0
    code = []
    
    # Step 1: Arithmetic test
    code.append(arm_mov_imm(0, 10))      # 0x0C0
    code.append(arm_add_imm(0, 0, 15))   # 0x0C4 -> R0 = 25
    code.append(arm_sub_imm(0, 0, 5))    # 0x0C8 -> R0 = 20
    code.append(arm_cmp_imm(0, 20))      # 0x0CC
    code.append(arm_bne(52))             # 0x0D0: PC=0xD8. Fail is at 0x10C -> offset = 52
    
    # Step 2: Barrel shift
    code.append(arm_mov_imm(1, 3))       # 0x0D4
    code.append(0xE1A02181)              # 0x0D8: LSL R2, R1, #3 -> R2 = 24
    code.append(arm_cmp_imm(2, 24))      # 0x0DC
    code.append(arm_bne(40))             # 0x0E0: PC=0xE8. Fail at 0x10C -> offset = 36 -> 36 bytes
    
    # Step 3: Multiply
    code.append(arm_mov_imm(3, 7))       # 0x0E4
    code.append(arm_mov_imm(4, 6))       # 0x0E8
    code.append(0xE0050493)              # 0x0EC: MUL R5, R3, R4 -> R5 = 42
    code.append(arm_cmp_imm(5, 42))      # 0x0F0
    code.append(arm_bne(20))             # 0x0F4: PC=0xFC. Fail at 0x10C -> offset = 16 bytes
    
    # Step 4: Memory write & read in IWRAM
    # Pool starts at 0x118:
    # pool[0] = 0x03000000 at 0x118
    # pool[1] = 0xCAFEBABE at 0x11C
    # 0x0F8: LDR R6, pool[0]. PC=0x100. Target=0x118. Offset = 24 bytes
    code.append(arm_ldr_pc(6, 24))       # 0x0F8
    # 0x0FC: LDR R7, pool[1]. PC=0x104. Target=0x11C. Offset = 24 bytes
    code.append(arm_ldr_pc(7, 24))       # 0x0FC
    code.append(arm_str_imm(7, 6, 0))    # 0x100: STR R7, [R6]
    code.append(0xE5968000)              # 0x104: LDR R8, [R6]
    code.append(0xE1570008)              # 0x108: CMP R7, R8
    # If equal, skip over fail block to infinite loop!
    code.append(arm_bne(8))              # 0x10C: PC=0x114. If fail -> 0x114
    code.append(arm_b(-8))               # 0x110: Pass! B self
    
    # Fail block at 0x114:
    code.append(arm_mov_imm(0, 0xFF))    # 0x114
    
    pool = [
        0x03000000,
        0xCAFEBABE
    ]
    
    bin_code = b"".join(struct.pack("<I", instr) for instr in code)
    bin_pool = b"".join(struct.pack("<I", val) for val in pool)
    
    rom_data = header + bin_code + bin_pool
    if len(rom_data) < 32768:
        rom_data = rom_data.ljust(32768, b'\x00')
    return rom_data

def build_cpu_thumb_rom():
    header = create_gba_header(title="THUMB_TEST", game_code="ZTHM")
    code = []
    
    # ARM stub that switches to THUMB at 0x080000C8:
    # 0x080000C0: ADD R0, PC, #1 (PC = 0xC8, so R0 = 0xC9)
    # 0x080000C4: BX R0 (switches to THUMB at 0xC8!)
    code.append(arm_add_imm(0, 15, 1))
    code.append(arm_bx(0))
    
    # Starts at 0x080000C8 in THUMB:
    # 0x0C8: MOV R0, #50
    # 0x0CA: ADD R0, #25 -> R0 = 75
    # 0x0CC: PUSH {R0}
    # 0x0CE: MOV R0, #0
    # 0x0D0: POP {R1} -> R1 = 75
    # 0x0D2: CMP R1, #75
    # 0x0D4: BNE fail (at 0x0E0)
    # Pass:
    # 0x0D6: LDR R0, [PC, #8] -> pool[0] at 0x0E4
    # 0x0D8: LDR R1, [PC, #12] -> pool[1] at 0x0E8
    # 0x0DA: STR R1, [R0, #0]
    # 0x0DC: B self
    # Fail:
    # 0x0DE: B self
    
    thumb_code = [
        0x2032, # 0x0C8: MOV R0, #50
        0x3019, # 0x0CA: ADD R0, #25
        0xB401, # 0x0CC: PUSH {R0}
        0x2000, # 0x0CE: MOV R0, #0
        0xBC02, # 0x0D0: POP {R1}
        0x294B, # 0x0D2: CMP R1, #75
        0xD104, # 0x0D4: BNE +8 (to 0x0DE)
        # Pass:
        0x4802, # 0x0D6: LDR R0, [PC, #8] (PC=0xDA & ~2 = 0xD8 + 8 = 0xE0) -> target 0xE0
        0x4902, # 0x0D8: LDR R1, [PC, #8] (PC=0xDC & ~2 = 0xDC + 8 = 0xE4) -> target 0xE4
        0x6001, # 0x0DA: STR R1, [R0, #0]
        0xE7FE, # 0x0DC: B self
        # Fail:
        0xE7FE, # 0x0DE: B self
    ]
    
    bin_arm = b"".join(struct.pack("<I", instr) for instr in code)
    bin_thumb = b"".join(struct.pack("<H", instr) for instr in thumb_code)
    # Current thumb size = 22 bytes. 0xC8 + 22 = 0xDE.
    # Align to 4 bytes: add 2 bytes padding so pool starts at 0xE0!
    if (len(bin_arm) + len(bin_thumb)) % 4 != 0:
        bin_thumb += b"\x00\x00"
    
    pool = [
        0x03000004, # Target 0xE0
        0x1337BEEF  # Target 0xE4
    ]
    bin_pool = b"".join(struct.pack("<I", val) for val in pool)
    
    rom_data = header + bin_arm + bin_thumb + bin_pool
    if len(rom_data) < 32768:
        rom_data = rom_data.ljust(32768, b'\x00')
    return rom_data

def build_timer_irq_rom():
    header = create_gba_header(title="TIMER_TEST", game_code="ZTMR")
    code = []
    
    code.append(arm_ldr_pc(0, 48))
    code.append(arm_ldr_pc(1, 48))
    code.append(arm_strh_imm(1, 0, 0x100))
    code.append(arm_ldr_pc(1, 44))
    code.append(arm_strh_imm(1, 0, 0x102))
    code.append(arm_mov_imm(1, 8))
    code.append(arm_strh_imm(1, 0, 0x200))
    code.append(arm_mov_imm(1, 1))
    code.append(arm_strh_imm(1, 0, 0x208))
    code.append(arm_b(-8))
    
    pool = [
        0x04000000,
        0x0000FF00,
        0x000000C1
    ]
    
    bin_code = b"".join(struct.pack("<I", instr) for instr in code)
    bin_pool = b"".join(struct.pack("<I", val) for val in pool)
    
    rom_data = header + bin_code + bin_pool
    if len(rom_data) < 32768:
        rom_data = rom_data.ljust(32768, b'\x00')
    return rom_data

def main():
    os.makedirs("roms", exist_ok=True)
    with open("roms/mode3_colors.gba", "wb") as f:
        f.write(build_mode3_rom())
    print("[+] Generated roms/mode3_colors.gba")
    
    with open("roms/cpu_arm_test.gba", "wb") as f:
        f.write(build_cpu_arm_rom())
    print("[+] Generated roms/cpu_arm_test.gba")
    
    with open("roms/cpu_thumb_test.gba", "wb") as f:
        f.write(build_cpu_thumb_rom())
    print("[+] Generated roms/cpu_thumb_test.gba")
    
    with open("roms/timer_irq_test.gba", "wb") as f:
        f.write(build_timer_irq_rom())
    print("[+] Generated roms/timer_irq_test.gba")

if __name__ == "__main__":
    main()
