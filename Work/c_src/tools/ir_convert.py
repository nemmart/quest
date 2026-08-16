#!/usr/bin/env python3
"""
ir_convert.py — Convert MV-32 basic blocks to readable pseudocode.

Reads basic block format:
    addr:
    instruction;
    instruction;
    n addr [addr...]  |  c addr n addr  |  s NNN n [addr...]  |  j addr n addr  |  u

Outputs annotated pseudocode with pattern recognition.
"""

import re
import sys

# ── Known addresses ──────────────────────────────────────────────
KNOWN_GLOBALS = {
    0x70000210: "SD_PTR",
    0x70000212: "OBJ_PTR",
    0x70000214: "CAS_PTR",
    0x70000216: "PLAYER_NUM",
    0x7000021C: "IN_BUFFER",
    0x70000260: "OUT_CHAN",
    0x70000262: "IN_CHAN",
    0x7000026C: "CITY_NUM",
    0x7000026E: "TERR_NUM",
    0x70000270: "MOVES_LEFT",
}

# Player field names (signed offset → name)
# signed = raw - 0x8000 for 0x7xxx, then rec = signed + 642
PLAYER_FIELDS = {
    0x7DB1: "status_bits_0",
    0x7DB2: "status_bits_1",
    0x7DB3: "x",
    0x7DB4: "y",
    0x7E7A: "inventory_base",
    0x7E85: "intelligence",
    0x7E86: "experience",
    0x7E87: "current_hp",
    0x7E88: "max_hp",
    0x7E89: "vision",
    0x7E8A: "perception",
    0x7E8B: "unknown_269",
    0x7E8C: "wealth",
    0x7E8E: "dragons_slain",
    0x7EF7: "status_field",
    0x7F00: "catapult_base",
    0x7F02: "arrows",
    0x7F03: "poison",
    0x7F54: "unknown_470",
    0x7F55: "unknown_471",
    0x7F56: "siege_castle_index",
    0x7FA7: "quest_level",
    0x7FA8: "castle_count",
    0x7FB3: "cache_intelligence",
    0x7FB4: "cache_experience",
    0x7FB5: "cache_hp",
    0x7FB7: "cache_vision",
    0x7FB8: "cache_perception",
    0x7FB9: "cache_wealth",
    0x7FBA: "cache_castle_count",
    0x7FBB: "cache_arrows",
    0x7FBC: "cache_poison",
    0x7FBD: "cache_quest_level",
    0x7FC0: "cache_status",
    0x7FC2: "cache_display_bits",
    0x002A: "player_class",
}

# Bit flag names (raw WNADI offset → name)
BIT_FLAGS = {
    0xDB10: "INACTIVE",
    0xDB11: "HAS_VIEWPORT",
    0xDB17: "FAMILIAR_ACTIVE",
    0xDB18: "ACTIVITY_SAILING",
    0xDB19: "ACTIVITY_FLYING",
    0xDB1A: "ACTIVITY_AT_HOME",
    0xDB1C: "DRAGON_SLAIN",
    0xDB1E: "MAGIC_BOOTS",
    0xDB1F: "CATAPULT",
    0xDB20: "ARMOR",
    0xDB21: "SPECIAL_ITEM",
    0xDB22: "TELEPORT_RING",
    0xDB23: "INVIS_RING",
    0xDB24: "ONE_RING",
    0xDB25: "SIGNET_RING",
    0xDB26: "ACTIVITY_EXPLORING",
    0xDB28: "ACTIVITY_CARRYING",
}

# ── Register names ───────────────────────────────────────────────
REG_NAMES = {0: "ac0", 1: "ac1", 2: "ac2", 3: "ac3"}

def reg(n):
    return REG_NAMES.get(n, f"r{n}")

# ── Operand formatting ───────────────────────────────────────────

def format_mem_ref(text):
    """Make memory references more readable."""
    # [ac3+0xFFF4] → arg1, [ac3+0xFFF2] → arg2, etc.
    # These are always frame-relative (arg area is part of the frame)
    arg_offsets = {
        "0xFFF4": "arg1", "0xFFF2": "arg2", "0xFFF0": "arg3",
        "0xFFEE": "arg4", "0xFFEC": "arg5", "0xFFEA": "arg6",
        "0xFFE8": "arg7", "0xFFE6": "arg8",
    }
    for off, name in arg_offsets.items():
        text = text.replace(f"[ac3+{off}]", f"[{name}]")

    # Known globals
    for addr, name in KNOWN_GLOBALS.items():
        text = text.replace(f"[0x{addr:08X}]", f"[{name}]")
        text = text.replace(f"[0x{addr:08x}]", f"[{name}]")

    # Player fields [ac2+0x7xxx]
    m = re.search(r'\[ac2\+(0x[0-9A-Fa-f]+)\]', text)
    if m:
        raw = int(m.group(1), 16)
        if raw in PLAYER_FIELDS:
            text = text.replace(m.group(0), f"[player.{PLAYER_FIELDS[raw]}]")

    # Frame slots [ac3+0xNN] → s[0xNN] ONLY when ac3 is frame pointer
    if _ac3_is_frame:
        text = re.sub(r'\[ac3\+(0x[0-9A-Fa-f]+)\]', r's[\1]', text)

    return text

# Module-level ac3 tracking state
_ac3_is_frame = True

def format_bit_offset(val):
    """Convert raw bit offset to name."""
    if val in BIT_FLAGS:
        return f"bit:{BIT_FLAGS[val]}"
    return f"bit:0x{val:04X}"

# ── Instruction conversion ───────────────────────────────────────

def ac3_kills_frame(line):
    """Does this instruction make ac3 stop being the frame pointer?"""
    line = line.strip().rstrip(';').strip()
    parts = line.split(None, 1)
    if not parts:
        return False, False  # (kills, restores)
    op = parts[0]
    operands = parts[1] if len(parts) > 1 else ""

    # LDAFP 3 restores frame pointer
    if op == "LDAFP" and operands.startswith("3"):
        return False, True

    # WSAVS / WSAVR set up a new frame — ac3 becomes frame pointer
    if op in ("WSAVS", "WSAVR"):
        return False, True

    # WCMV / WBLM advance ac3 (block copy source pointer)
    if op in ("WCMV", "WBLM"):
        return True, False

    # Instructions with dest as FIRST operand, "3,..."
    if op in ("XNLDA", "XWLDA", "LNLDA", "LWLDA",
              "XLEFB", "LLEFB", "XLEF", "LLEF",
              "XLDB", "WLDB",
              "XWADD", "XNADD", "XNSUB", "XWSUB", "XWMUL",
              "XNSBI", "XNADI", "XWADI"):
        m = re.match(r'3[,\s]', operands)
        if m:
            return True, False

    # Instructions with dest as SECOND operand, "X,3"
    if op in ("WMOV", "WMOVR", "WADD", "WSUB", "WMUL", "WDIV",
              "WAND", "WIOR", "WXOR", "WCOM", "WNEG", "WHLV",
              "WINC", "WSBI", "NADD", "NSUB", "NMUL", "NNEG", "NSBI"):
        m = re.match(r'\d\s*,\s*3', operands)
        if m:
            return True, False

    # WXCH — either position swaps into ac3
    if op == "WXCH":
        if re.match(r'3\s*,', operands) or re.search(r',\s*3', operands):
            return True, False

    # Immediate-to-reg with dest=3: NLDAI val,3
    if op == "NLDAI":
        if re.search(r',\s*3\s*$', operands):
            return True, False

    # Immediate ops where FIRST operand is reg: WNADI 3,imm  WADI 3,imm etc.
    if op in ("WNADI", "WADI", "WADDI", "NADDI", "NADI",
              "WANDI", "WLSI", "WLSHI"):
        m = re.match(r'3[,\s]', operands)
        if m:
            return True, False

    # Single-reg ops: WPOP 3, LDASP 3
    if op in ("WPOP", "LDASP"):
        m = re.match(r'3', operands)
        if m:
            return True, False

    # MOV/ADD variants with dest=3 (second operand)
    if op in ("MOV.L#", "MOV.#", "COM.#", "ADD.O#", "ADD.O",
              "ADD.#", "ADD", "ADC", "ADC.C", "SUB.CL", "MOV.L"):
        m = re.match(r'\d\s*,\s*3', operands)
        if m:
            return True, False

    return False, False


def convert_instruction(line):
    """Convert a single instruction to readable form.
    Returns readable_string or None. Updates _ac3_is_frame state."""
    global _ac3_is_frame

    line = line.strip().rstrip(';').strip()
    if not line:
        return None

    # Split into opcode and operands
    parts = line.split(None, 1)
    op = parts[0]
    operands = parts[1] if len(parts) > 1 else ""

    # Remove trailing comments from operands but keep # function names
    comment = ""
    if "# " in operands:
        idx = operands.index("# ")
        comment = operands[idx+2:].strip()
        operands = operands[:idx].strip()

    operands = operands.rstrip(',').strip()

    # ── Load immediate ──
    if op == "NLDAI":
        m = re.match(r'(\d+)\s*\(0x[0-9A-Fa-f]+\)\s*,\s*(\d)', operands)
        if m:
            val, r = int(m.group(1)), int(m.group(2))
            return f"{reg(r)}={val}"

    if op == "WLDAI":
        m = re.match(r'(0x[0-9A-Fa-f]+)', operands)
        if m:
            return f"ac0:ac1={m.group(1)}"

    # ── Register moves ──
    if op == "WMOV":
        m = re.match(r'(\d),(\d)', operands)
        if m:
            return f"{reg(int(m.group(2)))}={reg(int(m.group(1)))}"

    if op == "WMOVR":
        m = re.match(r'(\d),(\d)', operands)
        if m:
            return f"{reg(int(m.group(1)))}={reg(int(m.group(2)))}"

    # ── Load from memory ──
    if op in ("XNLDA", "XWLDA"):
        width = "16" if op == "XNLDA" else "32"
        m = re.match(r'(\d),(.+)', operands)
        if m:
            r, addr = int(m.group(1)), m.group(2).strip()
            if addr.startswith("@"):
                addr = addr[1:]
                addr = format_mem_ref(addr).replace("s[", f"s{width}[")
                return f"{reg(r)}=*{addr}"
            addr = format_mem_ref(addr).replace("s[", f"s{width}[")
            return f"{reg(r)}={addr}"

    if op == "LNLDA":
        m = re.match(r'(\d),(.+)', operands)
        if m:
            r, addr = int(m.group(1)), format_mem_ref(m.group(2).strip()).replace("s[", "s16[")
            if addr.startswith("@"):
                addr = addr[1:]
                return f"{reg(r)}=*{addr}"
            return f"{reg(r)}={addr}"

    if op == "LWLDA":
        m = re.match(r'(\d),(.+)', operands)
        if m:
            r, addr = int(m.group(1)), format_mem_ref(m.group(2).strip()).replace("s[", "s32[")
            return f"{reg(r)}={addr}"

    # ── Store to memory ──
    if op in ("XNSTA", "XWSTA"):
        width = "16" if op == "XNSTA" else "32"
        m = re.match(r'(\d),(.+)', operands)
        if m:
            r, addr = int(m.group(1)), format_mem_ref(m.group(2).strip()).replace("s[", f"s{width}[")
            return f"{addr}={reg(r)}"

    if op == "LNSTA":
        m = re.match(r'(\d),(.+)', operands)
        if m:
            r, addr = int(m.group(1)), format_mem_ref(m.group(2).strip()).replace("s[", "s16[")
            return f"{addr}={reg(r)}"

    if op == "LWSTA":
        m = re.match(r'(\d),(.+)', operands)
        if m:
            r, addr = int(m.group(1)), format_mem_ref(m.group(2).strip()).replace("s[", "s32[")
            return f"{addr}={reg(r)}"

    # ── Arithmetic ──
    if op == "WADD":
        m = re.match(r'(\d),(\d)', operands)
        if m:
            s, d = int(m.group(1)), int(m.group(2))
            if s == d:
                return f"{reg(d)}={reg(d)} * 2  // WADD self"
            return f"{reg(d)}={reg(d)} + {reg(s)}"

    if op == "XWADD":
        m = re.match(r'(\d),(.+)', operands)
        if m:
            r, addr = int(m.group(1)), format_mem_ref(m.group(2).strip())
            return f"{reg(r)}={reg(r)} + {addr}"

    if op == "WSUB":
        m = re.match(r'(\d),(\d)', operands)
        if m:
            s, d = int(m.group(1)), int(m.group(2))
            if s == d:
                return f"{reg(d)}=0  // WSUB self"
            return f"{reg(d)}={reg(d)} - {reg(s)}"

    if op == "XNSUB":
        m = re.match(r'(\d),(.+)', operands)
        if m:
            r, addr = int(m.group(1)), format_mem_ref(m.group(2).strip())
            return f"{reg(r)}={reg(r)} - {addr}"

    if op == "WMUL":
        m = re.match(r'(\d),(\d)', operands)
        if m:
            s, d = int(m.group(1)), int(m.group(2))
            return f"{reg(d)}={reg(d)} * {reg(s)}"

    if op == "XWMUL":
        m = re.match(r'(\d),(.+)', operands)
        if m:
            r, addr = int(m.group(1)), format_mem_ref(m.group(2).strip())
            return f"{reg(r)}={reg(r)} * {addr}"

    if op == "WDIV":
        m = re.match(r'(\d),(\d)', operands)
        if m:
            return f"{reg(int(m.group(2)))}={reg(int(m.group(2)))} / {reg(int(m.group(1)))}"

    if op == "WINC":
        m = re.match(r'(\d),(\d)', operands)
        if m:
            return f"{reg(int(m.group(2)))}++"

    if op in ("WNADI", "WADI", "WADDI"):
        m = re.match(r'(\d),(\d+)\s*\(0x([0-9A-Fa-f]+)\)', operands)
        if m:
            r, dec_val, hex_val = int(m.group(1)), int(m.group(2)), int(m.group(3), 16)
            # Check for bit offset pattern
            if hex_val in BIT_FLAGS:
                return f"{reg(r)}={reg(r)} + {format_bit_offset(hex_val)}  // bit addr"
            # Sign extend 16-bit
            if hex_val >= 0x8000:
                signed = hex_val - 0x10000
                return f"{reg(r)}={reg(r)} + ({signed})  // 0x{hex_val:04X}"
            return f"{reg(r)}={reg(r)} + {dec_val}"

    if op == "LWADD":
        m = re.match(r'(\d),(.+)', operands)
        if m:
            r, addr = int(m.group(1)), format_mem_ref(m.group(2).strip())
            return f"{reg(r)}={reg(r)} + {addr}  // wide"

    if op == "WHLV":
        m = re.match(r'(\d),(\d)', operands)
        if m:
            return f"{reg(int(m.group(2)))}={reg(int(m.group(1)))} >> 1"

    if op == "NNEG":
        m = re.match(r'(\d),(\d)', operands)
        if m:
            return f"{reg(int(m.group(2)))}=-{reg(int(m.group(1)))}  // narrow"

    if op == "WNEG":
        m = re.match(r'(\d),(\d)', operands)
        if m:
            return f"{reg(int(m.group(2)))}=-{reg(int(m.group(1)))}"

    # ── Logic / Bit ──
    if op == "WAND":
        m = re.match(r'(\d),(\d)', operands)
        if m:
            return f"{reg(int(m.group(2)))}={reg(int(m.group(2)))} & {reg(int(m.group(1)))}"

    if op == "WANDI":
        m = re.match(r'(\d),(\d+)\s*\(0x([0-9A-Fa-f]+)\)', operands)
        if m:
            return f"{reg(int(m.group(1)))}={reg(int(m.group(1)))} & 0x{m.group(3)}"

    if op == "WIOR":
        m = re.match(r'(\d),(\d)', operands)
        if m:
            return f"{reg(int(m.group(2)))}={reg(int(m.group(2)))} | {reg(int(m.group(1)))}"

    if op == "WXOR":
        m = re.match(r'(\d),(\d)', operands)
        if m:
            return f"{reg(int(m.group(2)))}={reg(int(m.group(2)))} ^ {reg(int(m.group(1)))}"

    if op == "WCOM":
        m = re.match(r'(\d),(\d)', operands)
        if m:
            return f"{reg(int(m.group(2)))}=~{reg(int(m.group(1)))}"

    if op == "WLSI":
        m = re.match(r'(\d),(\d)', operands)
        if m:
            return f"{reg(int(m.group(2)))}={reg(int(m.group(1)))} << {reg(int(m.group(2)))}"

    # ── Bit set / clear (these are assignments, not skips) ──
    if op == "WBTO":
        m = re.match(r'(\d),(\d)', operands)
        if m:
            return f"set_bit({reg(int(m.group(1)))}, {reg(int(m.group(2)))})"

    if op == "WBTZ":
        m = re.match(r'(\d),(\d)', operands)
        if m:
            return f"clear_bit({reg(int(m.group(1)))}, {reg(int(m.group(2)))})"

    # ── Skip / Compare / Branch / Control ──
    # Left as raw MV-32 — skip semantics are subtle and error-prone.
    # The basic block structure already captures control flow.

    # ── Frame / Stack ──
    if op == "WSAVS":
        return f"ENTER locals={operands}"

    if op == "WRTN":
        return "RETURN"

    if op == "LDAFP":
        m = re.match(r'(\d)', operands)
        if m:
            return f"{reg(int(m.group(1)))}=frame_ptr"

    # ── Address calculation ──
    if op in ("XLEFB", "LLEFB"):
        m = re.match(r'(\d),(.+)', operands)
        if m:
            r, addr = int(m.group(1)), m.group(2).strip()
            # pc-relative string reference with resolved address
            if "[pc+" in addr:
                # Extract resolved address: (0xADDRESS:N)
                m2 = re.search(r'\(0x([0-9A-Fa-f]+):(\d+)\)', addr)
                if m2:
                    resolved = f"0x{m2.group(1)}"
                    byte_off = m2.group(2)
                    if byte_off == "1":
                        resolved += "+1"
                    return f"{reg(r)}=byte_addr({resolved})  // string"
                return f"{reg(r)}=byte_addr({addr})  // string ref"
            addr = format_mem_ref(addr)
            return f"{reg(r)}=byte_addr({addr})"

    if op in ("XLEF", "LLEF"):
        m = re.match(r'(\d),(.+)', operands)
        if m:
            r, addr = int(m.group(1)), format_mem_ref(m.group(2).strip())
            return f"{reg(r)}=word_addr({addr})"

    # ── XPEF / LPEF (push arg address) — handle in block context ──
    if op in ("XPEF", "LPEF", "XPEFB", "LPEFB"):
        addr = format_mem_ref(operands)
        # Indirect ref: @[...] → *[...]
        if addr.startswith("@"):
            addr = "*" + addr[1:]
        # Clean up [GLOBAL] → GLOBAL for known globals (no ac prefix)
        m2 = re.match(r'^\[([A-Z_]+)\]$', addr)
        if m2:
            addr = m2.group(1)
        return f"push_arg &{addr}"

    # ── Call ──
    if op == "LCALL":
        m = re.match(r'\[0x([0-9A-Fa-f]+)\],(\d+)', operands)
        if m:
            target = m.group(1)
            argc = int(m.group(2))
            name = comment if comment else f"0x{target}"
            return f"call {name}({argc} args)"

    if op == "XCALL":
        m = re.match(r'\[0x([0-9A-Fa-f]+)\],(\d+)', operands)
        if m:
            name = comment if comment else f"0x{m.group(1)}"
            return f"xcall {name}({m.group(2)} args)"
        return f"xcall {operands}"

    # ── Block move ──
    if op == "WCMV":
        return "block_copy(ac2_dst, ac3_src, ac0_len)  // WCMV: after, ac0=0, ac2/ac3 advanced"

    if op == "WBLM":
        return "block_move(ac2_dst, ac3_src, ac0_len)  // WBLM: after, ac0=0, ac2/ac3 advanced"

    # ── Loop (XNDO/XWDO) — left as raw, involves skip semantics ──

    # ── Convert ──
    if op == "CVWN":
        return "// CVWN: narrow→wide sign check"

    if op == "SEX":
        return "ac0 = sign_extend_16(ac0)"

    # ── Byte operations ──
    if op == "XLDB":
        m = re.match(r'(\d),(.+)', operands)
        if m:
            return f"{reg(int(m.group(1)))}=load_byte({format_mem_ref(m.group(2).strip())})"

    if op in ("XSTB", "WSTB"):
        m = re.match(r'(\d),(\d)', operands)
        if m:
            return f"store_byte({reg(int(m.group(1)))}, {reg(int(m.group(2)))})"

    # ── Misc ──
    if op == "WSBI":
        m = re.match(r'(\d),(\d)', operands)
        if m:
            return f"{reg(int(m.group(2)))}={reg(int(m.group(2)))} - {reg(int(m.group(1)))}"

    if op == "WPSH":
        m = re.match(r'(\d)', operands)
        if m:
            return f"push {reg(int(m.group(1)))}"

    if op == "WPOP":
        m = re.match(r'(\d)', operands)
        if m:
            return f"{reg(int(m.group(1)))}=pop"

    if op == "SYSCALL":
        return f"SYSCALL {operands}"

    if op == "VALID":
        return f"// VALID page check"

    if op == "LDSP":
        return f"dispatch_table {operands}"

    # ── Fallthrough: return original ──
    return f"/* {op} {operands} */"

# ── Skip-to-if conversion ──────────────────────────────────────

# W-skip instruction conditions: skip if condition is TRUE
# addr1 = fall-through (condition FALSE), addr2 = skip (condition TRUE)
SKIP_CONDITIONS = {
    # reg,reg compares
    "WSEQ":  ("==", "rr"),
    "WSNE":  ("!=", "rr"),
    "WSLT":  ("<",  "rr"),
    "WSLE":  ("<=", "rr"),
    "WSGT":  (">",  "rr"),
    "WSGE":  (">=", "rr"),
    "WUSGT": (">u", "rr"),
    "WUSGE": (">=u","rr"),
    # reg,imm compares
    "WSEQI": ("==", "ri"),
    "WSNEI": ("!=", "ri"),
    "WSGTI": (">",  "ri"),
    "WSLEI": ("<=", "ri"),
    "WUGTI": (">u", "ri"),
    "WULEI": ("<=u","ri"),
    # bit test: skip if zero
    "WSZB":  ("bit==0", "rr"),
    "WSZBO": ("bit==0", "rr"),
}

def parse_skip_operands(op, operands, form):
    """Parse skip instruction operands into condition string."""
    if form == "rr":
        m = re.match(r'(\d),(\d)', operands)
        if m:
            return f"ac{m.group(1)}", f"ac{m.group(2)}"
    elif form == "ri":
        m = re.match(r'(\d),(\d+)', operands)
        if m:
            return f"ac{m.group(1)}", m.group(2)
    return None, None

def parse_nova_skip(text):
    """Parse novaCompute skip: 'MOV.L# 2,2,SNC' → condition string.
    Returns (condition, True) or (None, False)."""
    m = re.match(r'(MOV|COM|ADD|SUB|NEG|AND|IOR)([.#OLZ]*)\s+(\d),(\d),(\w+)', text)
    if not m:
        return None
    base_op = m.group(1)
    modifiers = m.group(2)
    src = int(m.group(3))
    dst = int(m.group(4))
    skip_code = m.group(5)

    # For MOV R,R — test the register value
    if base_op == "MOV" and src == dst:
        if "#" in modifiers:
            # No write, just test
            if "L" in modifiers:
                # Tests sign bit via carry
                if skip_code == "SNC":
                    return f"ac{dst} < 0"
                elif skip_code == "SZC":
                    return f"ac{dst} >= 0"
            else:
                if skip_code == "SZR":
                    return f"ac{dst} == 0"
                elif skip_code == "SNR":
                    return f"ac{dst} != 0"

    # For COM R,R — complement and test
    if base_op == "COM" and src == dst and "#" in modifiers:
        if skip_code == "SZR":
            return f"ac{dst} == -1"

    # For ADD R,R — double and test sign/overflow
    if base_op == "ADD" and src == dst:
        if "O" in modifiers and "#" in modifiers:
            if skip_code == "SBN":
                return f"ac{dst} > 0"
            elif skip_code == "SEZ":
                return f"ac{dst} == 0"

    return None

def convert_skip_to_if(output_lines, terminator):
    """If the last instruction is a skip and terminator has 2 addrs,
    combine into if-goto. Returns converted string or None."""
    # Check terminator has exactly 2 successors
    m = re.match(r'n\s+([0-9a-fA-F]+)\s+([0-9a-fA-F]+)$', terminator)
    if not m:
        return None
    addr_false = m.group(1)  # fall-through (skip NOT taken)
    addr_true = m.group(2)   # skip taken (condition TRUE)

    # Find last non-empty instruction line
    last_idx = None
    for i in range(len(output_lines) - 1, -1, -1):
        s = output_lines[i].strip()
        if s and s.startswith('/*') and s.endswith('*/'):
            last_idx = i
            break
        elif s and not s.endswith(':'):
            break  # Hit a non-comment instruction — skip is not last

    if last_idx is None:
        return None

    skip_text = output_lines[last_idx].strip()
    # Extract instruction from /* ... */
    inner = skip_text[2:-2].strip()

    # Try W-skip instructions
    parts = inner.split(None, 1)
    if not parts:
        return None
    op = parts[0]
    operands = parts[1] if len(parts) > 1 else ""
    # Strip parenthetical hex from immediate operands: "10 (0x000A)" → "10"
    operands_clean = re.sub(r'\s*\(0x[0-9A-Fa-f]+\)', '', operands).strip()

    if op in SKIP_CONDITIONS:
        cond_op, form = SKIP_CONDITIONS[op]
        lhs, rhs = parse_skip_operands(op, operands_clean, form)
        if lhs is not None:
            # Remove the skip comment from output
            output_lines.pop(last_idx)
            if cond_op == "bit==0":
                return f"if (!bit({lhs}, {rhs})) goto {addr_true} else {addr_false}"
            else:
                return f"if ({lhs} {cond_op} {rhs}) goto {addr_true} else {addr_false}"

    # Try novaCompute skips
    cond = parse_nova_skip(inner)
    if cond is not None:
        output_lines.pop(last_idx)
        return f"if ({cond}) goto {addr_true} else {addr_false}"

    return None


# ── Block processing ─────────────────────────────────────────────

def process_block(addr, instructions, terminator, entry_ac3=True):
    """Process a basic block, collapsing XPEF sequences into call args."""
    global _ac3_is_frame
    _ac3_is_frame = entry_ac3  # set from dataflow analysis
    output = [f"\n{addr}:"]

    # First pass (raw text): find call and first push indices
    call_idx = None
    first_push_idx = None
    for i, inst in enumerate(instructions):
        stripped = inst.strip().rstrip(';').strip()
        parts = stripped.split(None, 1)
        if not parts:
            continue
        op = parts[0]
        if op in ("XPEF", "LPEF", "XPEFB", "LPEFB") and first_push_idx is None:
            first_push_idx = i
        if op in ("LCALL", "XCALL"):
            call_idx = i
            break

    def emit(inst):
        """Convert one instruction, update ac3 state, return IR string."""
        global _ac3_is_frame
        ir = convert_instruction(inst)
        # Update ac3 state AFTER conversion (instruction uses old state)
        kills, restores = ac3_kills_frame(inst)
        if restores:
            _ac3_is_frame = True
        elif kills:
            _ac3_is_frame = False
        return ir

    if call_idx is not None and first_push_idx is not None:
        # Emit everything before the first push
        for i in range(first_push_idx):
            ir = emit(instructions[i])
            if ir:
                output.append(f"    {ir}")

        # Collect pushes and interleaved computation
        pending_args = []
        interleaved = []
        for i in range(first_push_idx, call_idx):
            ir = emit(instructions[i])
            if ir is None:
                continue
            if ir.startswith("push_arg"):
                pending_args.append(ir.replace("push_arg ", ""))
            else:
                interleaved.append(ir)

        # Emit interleaved computation first
        for line in interleaved:
            output.append(f"    {line}")

        # Emit collapsed call
        call_ir = emit(instructions[call_idx])
        if pending_args:
            args_str = ", ".join(reversed(pending_args))
            call_ir = re.sub(r'\(\d+ args\)', f'({args_str})', call_ir)
        output.append(f"    {call_ir}")

        # Emit anything after the call
        for i in range(call_idx + 1, len(instructions)):
            ir = emit(instructions[i])
            if ir:
                output.append(f"    {ir}")
    else:
        # No call in this block — emit everything normally
        for inst in instructions:
            ir = emit(inst)
            if ir is None:
                continue
            if ir.startswith("push_arg"):
                output.append(f"    {ir}")
            else:
                output.append(f"    {ir}")

    # Convert skip + two-successor terminator to if-goto
    if terminator:
        converted = convert_skip_to_if(output, terminator)
        if converted:
            output.append(f"    {converted}")
        else:
            output.append(f"    {terminator}")

    return "\n".join(output)

# ── Parser ───────────────────────────────────────────────────────

def parse_blocks(text):
    """Parse basic block format into structured blocks."""
    blocks = []
    current_addr = None
    current_insts = []

    for line in text.split('\n'):
        line = line.strip()
        if not line:
            continue

        # Block header: addr:
        m = re.match(r'^([0-9a-fA-F]+):$', line)
        if m:
            if current_addr is not None:
                blocks.append((current_addr, current_insts, None))
            current_addr = m.group(1)
            current_insts = []
            continue

        # Terminators: n, c, s, j, u
        if re.match(r'^[ncsju]\b', line):
            if current_addr is not None:
                blocks.append((current_addr, current_insts, line))
                current_addr = None
                current_insts = []
            continue

        # Instruction
        if current_addr is not None:
            current_insts.append(line)

    if current_addr is not None:
        blocks.append((current_addr, current_insts, None))

    return blocks

# ── Main ─────────────────────────────────────────────────────────

def main():
    if len(sys.argv) > 1:
        with open(sys.argv[1]) as f:
            text = f.read()
    else:
        text = sys.stdin.read()

    # Run dataflow analysis to get per-block ac3 state
    from dataflow import parse_blocks as df_parse, build_cfg, analyze_ac3
    df_blocks = df_parse(text)
    build_cfg(df_blocks)
    ac3_states = analyze_ac3(df_blocks)

    # Convert blocks with correct ac3 entry state
    blocks = parse_blocks(text)

    for addr, instructions, terminator in blocks:
        # Look up ac3 entry state from dataflow analysis
        entry_ac3 = True  # default: assume frame
        addr_lower = addr.lower()
        if addr_lower in ac3_states:
            entry_ac3, _ = ac3_states[addr_lower]

        result = process_block(addr, instructions, terminator, entry_ac3)
        print(result)

if __name__ == "__main__":
    main()
